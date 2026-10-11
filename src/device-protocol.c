#include "device-protocol.h"
#include "qianli-a3.h"
#include <glib.h>
#include <gio/gio.h>

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

static void
usb_tester_monitor_device_protocol_finalize (GObject *object)
{
    UsbTesterMonitorDeviceProtocol *self = USB_TESTER_MONITOR_DEVICE_PROTOCOL (object);

    g_free (self->device_path);
    g_free (self->device_type);
    g_free (self->device_name);
    g_free (self->device_info);

    if (self->channel) {
        g_io_channel_unref (self->channel);
    }

    if (self->source_id > 0) {
        g_source_remove (self->source_id);
    }

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
    
    /* Open serial port */
    GError *local_error = NULL;
    self->channel = g_io_channel_new_file (self->device_path, "r+", &local_error);
    
    if (!self->channel) {
        g_propagate_error (error, local_error);
        return FALSE;
    }
    
    /* Configure serial port */
    g_io_channel_set_encoding (self->channel, NULL, &local_error);
    
    if (local_error) {
        g_io_channel_unref (self->channel);
        self->channel = NULL;
        g_propagate_error (error, local_error);
        return FALSE;
    }
    
    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED;
    
    return TRUE;
}

void
usb_tester_monitor_device_protocol_disconnect (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));
    
    if (self->source_id > 0) {
        g_source_remove (self->source_id);
        self->source_id = 0;
    }
    
    if (self->channel) {
        g_io_channel_shutdown (self->channel, TRUE, NULL);
        g_io_channel_unref (self->channel);
        self->channel = NULL;
    }
    
    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED;
}

void
usb_tester_monitor_device_protocol_start_streaming (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));
    
    if (self->state != USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED) {
        return;
    }
    
    self->state = USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING;
}

void
usb_tester_monitor_device_protocol_stop_streaming (UsbTesterMonitorDeviceProtocol *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_PROTOCOL (self));
    
    if (self->state != USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING) {
        return;
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
