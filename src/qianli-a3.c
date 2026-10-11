#include "qianli-a3.h"
#include "device-protocol.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorQianliA3 {
    UsbTesterMonitorDeviceProtocol parent_instance;
    
    /* Qianli A3 specific state */
    gboolean stream_enabled;
    guint32 last_current_raw;
    guint32 last_voltage_raw;
    
    /* Buffer for parsing */
    GByteArray *rx_buffer;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3, 
                     USB_TESTER_MONITOR_TYPE_DEVICE_PROTOCOL)

static void
usb_tester_monitor_qianli_a3_finalize (GObject *object)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (object);
    
    if (self->rx_buffer) {
        g_byte_array_unref (self->rx_buffer);
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_qianli_a3_parent_class)->finalize (object);
}

static void
usb_tester_monitor_qianli_a3_class_init (UsbTesterMonitorQianliA3Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_qianli_a3_finalize;
}

static void
usb_tester_monitor_qianli_a3_init (UsbTesterMonitorQianliA3 *self)
{
    self->stream_enabled = FALSE;
    self->last_current_raw = 0;
    self->last_voltage_raw = 0;
    self->rx_buffer = g_byte_array_new ();
}

/* Calculate checksum for XOR */
static guint8
calculate_xor_checksum (const guint8 *data, gsize len)
{
    guint8 checksum = 0;
    for (gsize i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/* Send command to device */
static gboolean
send_qianli_command (UsbTesterMonitorQianliA3 *self, 
                     guint8 model, guint8 cmd, 
                     const gchar *payload, gsize payload_len,
                     GError **error)
{
    UsbTesterMonitorDeviceProtocol *protocol = USB_TESTER_MONITOR_DEVICE_PROTOCOL (self);
    
    if (protocol->state != USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                     "Device not connected");
        return FALSE;
    }
    
    GByteArray *packet = g_byte_array_new ();
    
    /* Magic byte */
    g_byte_array_append (packet, (guint8[]){QIANLI_A3_MAGIC_BYTE}, 1);
    
    /* Length (little endian) */
    guint16 length = payload_len;
    g_byte_array_append (packet, (guint8[]){length & 0xFF, (length >> 8) & 0xFF}, 2);
    
    /* Model */
    g_byte_array_append (packet, (guint8[]){model}, 1);
    
    /* Command */
    g_byte_array_append (packet, (guint8[]){cmd}, 1);
    
    /* Parameters (usually 0x00 0x00) */
    g_byte_array_append (packet, (guint8[]){0x00, 0x00}, 2);
    
    /* Checksum (XOR of payload) */
    guint8 checksum = 0;
    if (payload_len > 0) {
        checksum = calculate_xor_checksum ((guint8 *)payload, payload_len);
    }
    g_byte_array_append (packet, (guint8[]){checksum}, 1);
    
    /* Payload */
    if (payload_len > 0) {
        g_byte_array_append (packet, (guint8 *)payload, payload_len);
    }
    
    /* Write packet to serial port */
    gsize bytes_written;
    GError *local_error = NULL;
    GIOStatus status = g_io_channel_write_chars (protocol->channel,
                                                  (gchar *)packet->data,
                                                  packet->len,
                                                  &bytes_written,
                                                  &local_error);
    
    g_byte_array_unref (packet);
    
    if (status != G_IO_STATUS_NORMAL) {
        g_propagate_error (error, local_error);
        return FALSE;
    }
    
    return TRUE;
}

UsbTesterMonitorQianliA3 *
usb_tester_monitor_qianli_a3_new (const gchar *device_path)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3,
                        "device-path", device_path,
                        NULL);
}
