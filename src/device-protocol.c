#include "device-protocol.h"
#include "qianli-a3.h"
#include <glib.h>
#include <gio/gio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

struct _UsbTesterMonitorDeviceProtocol {
    GObject parent_instance;

    gchar *device_path;
    gchar *device_type;
    guint baud_rate;

    UsbTesterMonitorProtocolState state;
    gchar *device_name;
    gchar *device_info;

    /* Serial port connection */
    GIOChannel *channel;
    guint source_id;

    /* Data callback */
    UsbTesterMonitorDataCallback data_callback;
    gpointer user_data;
    GDestroyNotify notify;

    /* Device-specific implementation */
    gpointer device_impl;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDeviceProtocol, usb_tester_monitor_device_protocol, G_TYPE_OBJECT)

static void usb_tester_monitor_device_protocol_close_channel (UsbTesterMonitorDeviceProtocol *self);

static void
usb_tester_monitor_device_protocol_finalize (GObject *object)
{
    UsbTesterMonitorDeviceProtocol *self = USB_TESTER_MONITOR_DEVICE_PROTOCOL (object);

    g_free (self->device_path);
    g_free (self->device_type);
    g_free (self->device_name);
    g_free (self->device_info);

    usb_tester_monitor_device_protocol_close_channel (self);

    if (self->notify && self->user_data) {
        self->notify (self->user_data);
    }

    if (self->device_impl) {
        g_object_unref (self->device_impl);
    }

    G_OBJECT_CLASS (usb_tester_monitor_device_protocol_parent_class)->finalize (object);
}

static void
usb_tester_monitor_device_protocol_class_init (UsbTesterMonitorDeviceProtocolClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = usb_tester_monitor_device_protocol_finalize;
}

static void
usb_tester_monitor_device_protocol_init (UsbTesterMonitorDeviceProtocol *self)
{
    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED;
    self->baud_rate = 115200;
    self->source_id = 0;
}

UsbTesterMonitorDeviceProtocol *
usb_tester_monitor_device_protocol_new (const gchar *device_path,
                                       guint baud_rate)
{
    UsbTesterMonitorDeviceProtocol *self;
    self = g_object_new (USB_TESTER_MONITOR_TYPE_DEVICE_PROTOCOL, NULL);
    self->device_path = g_strdup (device_path);
    self->baud_rate = baud_rate;
    return self;
}

UsbTesterMonitorDeviceProtocol *
usb_tester_monitor_device_protocol_new_for_type (const gchar *device_path,
                                                 const gchar *device_type)
{
    UsbTesterMonitorDeviceProtocol *self;
    
    self = g_object_new (USB_TESTER_MONITOR_TYPE_DEVICE_PROTOCOL, NULL);
    self->device_path = g_strdup (device_path);
    self->device_type = g_strdup (device_type);
    
    /* Create device-specific implementation based on type */
    if (g_strcmp0 (device_type, "qianli-a3") == 0) {
        self->device_impl = g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3,
                                         NULL);
    }
    
    return self;
}

static void
usb_tester_monitor_device_protocol_close_channel (UsbTesterMonitorDeviceProtocol *self)
{
    if (self->source_id > 0) {
        g_source_remove (self->source_id);
        self->source_id = 0;
    }

    if (self->channel) {
        gint fd = g_io_channel_unix_get_fd (self->channel);
        g_io_channel_shutdown (self->channel, FALSE, NULL);
        g_io_channel_unref (self->channel);
        self->channel = NULL;
        if (fd >= 0) {
            close (fd);
        }
    }
}

static void
usb_tester_monitor_device_protocol_on_reading (gdouble voltage,
                                                gdouble current,
                                                gpointer user_data)
{
    UsbTesterMonitorDeviceProtocol *self = USB_TESTER_MONITOR_DEVICE_PROTOCOL (user_data);

    if (self->data_callback) {
        UsbTesterMonitorReading reading;
        reading.voltage = voltage;
        reading.current = current;
        reading.power = voltage * current;
        reading.timestamp = g_get_real_time ();
        self->data_callback (self, &reading, self->user_data);
    }
}

static gboolean
usb_tester_monitor_device_protocol_write_packet (UsbTesterMonitorDeviceProtocol *self,
                                                  guint8 model,
                                                  guint8 cmd,
                                                  guint8 param1,
                                                  guint8 param2,
                                                  const guint8 *payload,
                                                  guint16 payload_len)
{
    g_return_val_if_fail (self->channel != NULL, FALSE);

    guint8 *packet = NULL;
    gsize packet_len = 0;
    GError *error = NULL;
    gsize bytes_written = 0;
    gboolean ok = FALSE;

    if (!usb_tester_monitor_qianli_a3_build_packet (model, cmd, param1, param2,
                                                    payload, payload_len,
                                                    &packet, &packet_len)) {
        return FALSE;
    }

    GIOStatus status = g_io_channel_write_chars (self->channel, (const gchar *) packet,
                                                 packet_len, &bytes_written, &error);
    g_io_channel_flush (self->channel, NULL);
    g_free (packet);

    if (error) {
        g_error_free (error);
    }

    ok = (status == G_IO_STATUS_NORMAL);

    return ok;
}

static gboolean
usb_tester_monitor_device_protocol_on_serial_data (GIOChannel *source,
                                                    GIOCondition condition,
                                                    gpointer user_data)
{
    UsbTesterMonitorDeviceProtocol *self = USB_TESTER_MONITOR_DEVICE_PROTOCOL (user_data);
    gchar buffer[4096];
    gsize bytes_read = 0;
    GError *error = NULL;

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        self->source_id = 0;
        usb_tester_monitor_device_protocol_close_channel (self);
        self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED;
        return FALSE;
    }

    GIOStatus status = g_io_channel_read_chars (source, buffer, sizeof (buffer),
                                                &bytes_read, &error);

    if (error) {
        g_error_free (error);
    }

    if (status == G_IO_STATUS_ERROR || status == G_IO_STATUS_EOF) {
        self->source_id = 0;
        usb_tester_monitor_device_protocol_close_channel (self);
        self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED;
        return FALSE;
    }

    if (status == G_IO_STATUS_NORMAL && bytes_read > 0 && self->device_impl) {
        usb_tester_monitor_qianli_a3_feed (USB_TESTER_MONITOR_QIANLI_A3 (self->device_impl),
                                           (const guint8 *) buffer, bytes_read);
    }

    return TRUE;
}

gboolean
usb_tester_monitor_device_protocol_connect (UsbTesterMonitorDeviceProtocol *self,
                                             GError **error)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self), FALSE);

    if (self->state != USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                     "Device is not in disconnected state");
        return FALSE;
    }

    gint fd = open (self->device_path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                     "Failed to open serial port '%s': %s",
                     self->device_path, g_strerror (errno));
        return FALSE;
    }

    struct termios tio;
    if (tcgetattr (fd, &tio) == 0) {
        cfmakeraw (&tio);
        tio.c_cflag |= CLOCAL | CREAD;
        speed_t speed = B115200;
        switch (self->baud_rate) {
            case 9600: speed = B9600; break;
            case 19200: speed = B19200; break;
            case 38400: speed = B38400; break;
            case 57600: speed = B57600; break;
            default: speed = B115200; break;
        }
        cfsetispeed (&tio, speed);
        cfsetospeed (&tio, speed);
        tcsetattr (fd, TCSANOW, &tio);
        tcflush (fd, TCIOFLUSH);
    }

    self->channel = g_io_channel_unix_new (fd);
    g_io_channel_set_encoding (self->channel, NULL, NULL);
    g_io_channel_set_buffered (self->channel, FALSE);

    /* Ensure a device implementation exists (Qianli A3 by default) */
    if (self->device_impl == NULL) {
        self->device_impl = usb_tester_monitor_qianli_a3_new (self->device_path);
    }
    usb_tester_monitor_qianli_a3_set_reading_callback (
        USB_TESTER_MONITOR_QIANLI_A3 (self->device_impl),
        usb_tester_monitor_device_protocol_on_reading, self);

    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED;

    return TRUE;
}

void
usb_tester_monitor_device_protocol_disconnect (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));

    usb_tester_monitor_device_protocol_close_channel (self);

    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED;
}

void
usb_tester_monitor_device_protocol_start_streaming (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));

    if (self->state != USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED) {
        return;
    }

    /* Enable data stream: Model 04, Cmd 05, Payload 00 */
    const guint8 payload_enable = QIANLI_A3_PAYLOAD_ENABLE;
    if (!usb_tester_monitor_device_protocol_write_packet (self,
                                                           QIANLI_A3_MODEL_ID,
                                                           QIANLI_A3_CMD_ENABLE_STREAM,
                                                           0x00, 0x00,
                                                           &payload_enable,
                                                           1)) {
        return;
    }

    self->source_id = g_io_add_watch (self->channel,
                                      G_IO_IN | G_IO_HUP | G_IO_ERR,
                                      usb_tester_monitor_device_protocol_on_serial_data,
                                      self);

    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING;
}

void
usb_tester_monitor_device_protocol_stop_streaming (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));

    if (self->state != USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING) {
        return;
    }

    /* Disable data stream: Model 04, Cmd 05, Payload 01 */
    if (self->channel) {
        const guint8 payload_disable = QIANLI_A3_PAYLOAD_DISABLE;
        usb_tester_monitor_device_protocol_write_packet (self,
                                                         QIANLI_A3_MODEL_ID,
                                                         QIANLI_A3_CMD_DISABLE_STREAM,
                                                         0x00, 0x00,
                                                         &payload_disable,
                                                         1);
    }

    if (self->source_id > 0) {
        g_source_remove (self->source_id);
        self->source_id = 0;
    }

    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED;
}

void
usb_tester_monitor_device_protocol_set_data_callback (UsbTesterMonitorDeviceProtocol *self,
                                                       UsbTesterMonitorDataCallback callback,
                                                       gpointer user_data,
                                                       GDestroyNotify notify)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));
    
    self->data_callback = callback;
    self->user_data = user_data;
    self->notify = notify;
}

UsbTesterMonitorProtocolState
usb_tester_monitor_device_protocol_get_state (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self), 
                         USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED);
    
    return self->state;
}

const gchar *
usb_tester_monitor_device_protocol_get_device_name (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self), NULL);
    
    return self->device_name;
}

const gchar *
usb_tester_monitor_device_protocol_get_device_info (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self), NULL);
    
    return self->device_info;
}
