#include "qianli-a3.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorQianliA3 {
    GObject parent_instance;

    /* Qianli A3 specific state */
    gchar *device_path;
    gboolean stream_enabled;
    guint32 last_current_raw;
    guint32 last_voltage_raw;

    /* Buffer for parsing */
    GByteArray *rx_buffer;

    /* Reading callback */
    UsbTesterMonitorQianliReadingCallback reading_callback;
    gpointer reading_user_data;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3, G_TYPE_OBJECT)

static void
usb_tester_monitor_qianli_a3_finalize (GObject *object)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (object);
    
    g_free (self->device_path);
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

UsbTesterMonitorQianliA3 *
usb_tester_monitor_qianli_a3_new (const gchar *device_path)
{
    UsbTesterMonitorQianliA3 *self = g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3, NULL);
    self->device_path = g_strdup (device_path);
    return self;
}

gboolean
usb_tester_monitor_qianli_a3_build_packet (guint8 model,
                                           guint8 cmd,
                                           guint8 param1,
                                           guint8 param2,
                                           const guint8 *payload,
                                           guint16 payload_len,
                                           guint8 **packet,
                                           gsize *packet_len)
{
    g_return_val_if_fail (packet != NULL, FALSE);
    g_return_val_if_fail (packet_len != NULL, FALSE);

    /* Firmware requires a payload of at least one byte */
    guint16 length = payload_len;
    if (length == 0) {
        length = 1;
    }

    guint8 *out = g_malloc (8 + length);
    out[0] = QIANLI_A3_MAGIC_BYTE;
    out[1] = length & 0xFF;          /* payload length, little endian */
    out[2] = (length >> 8) & 0xFF;
    out[3] = model;
    out[4] = cmd;
    out[5] = param1;
    out[6] = param2;

    guint8 checksum = 0;
    for (guint i = 0; i < length; i++) {
        guint8 byte = (payload != NULL && payload_len > 0) ? payload[i] : 0x00;
        out[8 + i] = byte;
        checksum ^= byte;
    }
    out[7] = checksum;

    *packet = out;
    *packet_len = 8 + length;
    return TRUE;
}

void
usb_tester_monitor_qianli_a3_set_reading_callback (UsbTesterMonitorQianliA3 *self,
                                                   UsbTesterMonitorQianliReadingCallback callback,
                                                   gpointer user_data)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_QIANLI_A3 (self));

    self->reading_callback = callback;
    self->reading_user_data = user_data;
}

static guint32
read_be32 (const guint8 *bytes)
{
    return ((guint32) bytes[0] << 24) | ((guint32) bytes[1] << 16) |
           ((guint32) bytes[2] << 8) | (guint32) bytes[3];
}

void
usb_tester_monitor_qianli_a3_feed (UsbTesterMonitorQianliA3 *self,
                                   const guint8 *data,
                                   gsize length)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_QIANLI_A3 (self));
    g_return_if_fail (data != NULL || length == 0);

    if (length == 0) {
        return;
    }

    g_byte_array_append (self->rx_buffer, data, length);

    while (self->rx_buffer->len >= 8) {
        const guint8 *buf = self->rx_buffer->data;

        /* Resync: drop bytes until the magic byte */
        if (buf[0] != QIANLI_A3_MAGIC_BYTE) {
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        guint payload_len = buf[1] | (buf[2] << 8);
        gsize total_len = 8 + payload_len;

        if (payload_len > QIANLI_A3_MAX_PAYLOAD_SIZE) {
            /* Implausible length: treat magic byte as noise */
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        if (self->rx_buffer->len < total_len) {
            break; /* wait for more data */
        }

        /* Checksum is the XOR of the payload bytes */
        guint8 checksum = 0;
        for (guint i = 0; i < payload_len; i++) {
            checksum ^= buf[8 + i];
        }

        if (checksum != buf[7]) {
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        guint8 model = buf[3];
        guint8 cmd = buf[4];

        if (model == QIANLI_A3_MODEL_ID && cmd == QIANLI_A3_CMD_ENABLE_STREAM &&
            payload_len == QIANLI_A3_STREAM_PAYLOAD_SIZE) {
            self->last_current_raw = read_be32 (buf + 8);
            self->last_voltage_raw = read_be32 (buf + 12);

            gdouble current = (gdouble) self->last_current_raw / QIANLI_A3_CURRENT_SCALE;
            gdouble voltage = (gdouble) self->last_voltage_raw / QIANLI_A3_VOLTAGE_SCALE;

            if (self->reading_callback) {
                self->reading_callback (voltage, current, self->reading_user_data);
            }
        }

        g_byte_array_remove_range (self->rx_buffer, 0, total_len);
    }
}
