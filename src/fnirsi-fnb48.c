#include "fnirsi-fnb48.h"
#include <glib.h>
#include <math.h>
#include <string.h>

struct _UsbTesterMonitorFnirsiFnb48 {
    GObject parent_instance;

    gchar *device_path;

    /* Buffer for parsing */
    GByteArray *rx_buffer;

    /* Reading callback */
    UsbTesterMonitorFnirsiReadingCallback reading_callback;
    gpointer reading_user_data;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorFnirsiFnb48, usb_tester_monitor_fnirsi_fnb48, G_TYPE_OBJECT)

static void
usb_tester_monitor_fnirsi_fnb48_finalize (GObject *object)
{
    UsbTesterMonitorFnirsiFnb48 *self = USB_TESTER_MONITOR_FNIRSI_FNB48 (object);

    g_free (self->device_path);
    if (self->rx_buffer) {
        g_byte_array_unref (self->rx_buffer);
    }

    G_OBJECT_CLASS (usb_tester_monitor_fnirsi_fnb48_parent_class)->finalize (object);
}

static void
usb_tester_monitor_fnirsi_fnb48_class_init (UsbTesterMonitorFnirsiFnb48Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = usb_tester_monitor_fnirsi_fnb48_finalize;
}

static void
usb_tester_monitor_fnirsi_fnb48_init (UsbTesterMonitorFnirsiFnb48 *self)
{
    self->rx_buffer = g_byte_array_new ();
}

UsbTesterMonitorFnirsiFnb48 *
usb_tester_monitor_fnirsi_fnb48_new (const gchar *device_path)
{
    UsbTesterMonitorFnirsiFnb48 *self = g_object_new (USB_TESTER_MONITOR_TYPE_FNIRSI_FNB48, NULL);
    self->device_path = g_strdup (device_path);
    return self;
}

guint8
usb_tester_monitor_fnirsi_fnb48_crc8 (const guint8 *data, gsize length)
{
    guint8 crc = 0x42;

    for (gsize i = 0; i < length; i++) {
        crc ^= data[i];
        for (gint bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (guint8) ((crc << 1) ^ 0x39);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

gboolean
usb_tester_monitor_fnirsi_fnb48_build_request (guint8 command,
                                               guint8 **report,
                                               gsize *report_len)
{
    g_return_val_if_fail (report != NULL, FALSE);
    g_return_val_if_fail (report_len != NULL, FALSE);

    guint8 *out = g_malloc0 (FNIRSI_FNB48_REPORT_SIZE);
    out[0] = FNIRSI_FNB48_REPORT_PREFIX;
    out[1] = command;
    /* bytes 2..62 stay zero */
    out[FNIRSI_FNB48_REPORT_SIZE - 1] =
        usb_tester_monitor_fnirsi_fnb48_crc8 (out + 1, FNIRSI_FNB48_REPORT_SIZE - 2);

    *report = out;
    *report_len = FNIRSI_FNB48_REPORT_SIZE;
    return TRUE;
}

void
usb_tester_monitor_fnirsi_fnb48_set_reading_callback (UsbTesterMonitorFnirsiFnb48 *self,
                                                      UsbTesterMonitorFnirsiReadingCallback callback,
                                                      gpointer user_data)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_FNIRSI_FNB48 (self));

    self->reading_callback = callback;
    self->reading_user_data = user_data;
}

static guint32
read_le32 (const guint8 *bytes)
{
    return (guint32) bytes[0] | ((guint32) bytes[1] << 8) |
           ((guint32) bytes[2] << 16) | ((guint32) bytes[3] << 24);
}

static guint16
read_le16 (const guint8 *bytes)
{
    return (guint16) (bytes[0] | (bytes[1] << 8));
}

/* Decode the newest sample of a CRC-verified data report and forward it.
 * The device sends 4 samples every 40 ms (100 sps); emitting only the last
 * one keeps the UI update rate at 25 readings per second. */
static void
usb_tester_monitor_fnirsi_fnb48_emit_sample (UsbTesterMonitorFnirsiFnb48 *self,
                                             const guint8 *report)
{
    const guint8 *sample = report + 2 +
                           (FNIRSI_FNB48_SAMPLES_PER_REPORT - 1) * FNIRSI_FNB48_SAMPLE_SIZE;

    gdouble voltage = (gdouble) read_le32 (sample + 0) / FNIRSI_FNB48_VOLTAGE_SCALE;
    gdouble current = (gdouble) read_le32 (sample + 4) / FNIRSI_FNB48_CURRENT_SCALE;
    gdouble temperature = (gdouble) read_le16 (sample + 13) / FNIRSI_FNB48_TEMP_SCALE;

    if (self->reading_callback) {
        self->reading_callback (voltage, current, temperature, self->reading_user_data);
    }
}

void
usb_tester_monitor_fnirsi_fnb48_feed (UsbTesterMonitorFnirsiFnb48 *self,
                                      const guint8 *data,
                                      gsize length)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_FNIRSI_FNB48 (self));
    g_return_if_fail (data != NULL || length == 0);

    if (length == 0) {
        return;
    }

    g_byte_array_append (self->rx_buffer, data, length);

    while (self->rx_buffer->len >= FNIRSI_FNB48_REPORT_SIZE) {
        const guint8 *buf = self->rx_buffer->data;

        /* Resync: drop bytes until the start of a data report.
         * The scan is content-based, so it also copes with a report-ID byte
         * that the kernel or the device may prepend to the report payload. */
        if (buf[0] != FNIRSI_FNB48_REPORT_PREFIX || buf[1] != FNIRSI_FNB48_REPORT_TYPE_DATA) {
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        guint8 checksum = usb_tester_monitor_fnirsi_fnb48_crc8 (buf + 1,
                                                               FNIRSI_FNB48_REPORT_SIZE - 2);

        if (checksum != buf[FNIRSI_FNB48_REPORT_SIZE - 1]) {
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        usb_tester_monitor_fnirsi_fnb48_emit_sample (self, buf);

        g_byte_array_remove_range (self->rx_buffer, 0, FNIRSI_FNB48_REPORT_SIZE);
    }

    /* Never let a desynchronized stream grow the buffer unbounded */
    if (self->rx_buffer->len > FNIRSI_FNB48_MAX_BUFFER_SIZE) {
        g_byte_array_remove_range (self->rx_buffer, 0,
                                   self->rx_buffer->len - FNIRSI_FNB48_REPORT_SIZE);
    }
}
