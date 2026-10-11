#include "powerz-km003c.h"
#include <glib.h>
#include <math.h>
#include <string.h>

struct _UsbTesterMonitorPowerzKm003c {
    GObject parent_instance;

    gchar *device_path;

    /* Buffer for parsing */
    GByteArray *rx_buffer;

    /* Reading callback */
    UsbTesterMonitorPowerzReadingCallback reading_callback;
    gpointer reading_user_data;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorPowerzKm003c, usb_tester_monitor_powerz_km003c, G_TYPE_OBJECT)

static void
usb_tester_monitor_powerz_km003c_finalize (GObject *object)
{
    UsbTesterMonitorPowerzKm003c *self = USB_TESTER_MONITOR_POWERZ_KM003C (object);

    g_free (self->device_path);
    if (self->rx_buffer) {
        g_byte_array_unref (self->rx_buffer);
    }

    G_OBJECT_CLASS (usb_tester_monitor_powerz_km003c_parent_class)->finalize (object);
}

static void
usb_tester_monitor_powerz_km003c_class_init (UsbTesterMonitorPowerzKm003cClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = usb_tester_monitor_powerz_km003c_finalize;
}

static void
usb_tester_monitor_powerz_km003c_init (UsbTesterMonitorPowerzKm003c *self)
{
    self->rx_buffer = g_byte_array_new ();
}

UsbTesterMonitorPowerzKm003c *
usb_tester_monitor_powerz_km003c_new (const gchar *device_path)
{
    UsbTesterMonitorPowerzKm003c *self = g_object_new (USB_TESTER_MONITOR_TYPE_POWERZ_KM003C, NULL);
    self->device_path = g_strdup (device_path);
    return self;
}

guint8 *
usb_tester_monitor_powerz_km003c_build_adc_request (void)
{
    guint8 *request = g_malloc0 (POWERZ_REPORT_SIZE);

    /* MsgHeader ctrl bitfield, little-endian: type = GET_DATA (0x0C),
     * extend = 0, id = 0, unused bit = 0, att = ATT_ADC (0x001) in bits
     * 17..31. The resulting word is 0x0002000C. */
    request[0] = POWERZ_CMD_GET_DATA;
    request[1] = 0x00;
    request[2] = 0x02;
    request[3] = 0x00;

    return request;
}

void
usb_tester_monitor_powerz_km003c_set_reading_callback (UsbTesterMonitorPowerzKm003c *self,
                                                        UsbTesterMonitorPowerzReadingCallback callback,
                                                        gpointer user_data)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_POWERZ_KM003C (self));

    self->reading_callback = callback;
    self->reading_user_data = user_data;
}

static guint32
read_le32 (const guint8 *bytes)
{
    return (guint32) bytes[0] | ((guint32) bytes[1] << 8) |
           ((guint32) bytes[2] << 16) | ((guint32) bytes[3] << 24);
}

static gint16
read_le16_signed (const guint8 *bytes)
{
    return (gint16) (bytes[0] | (bytes[1] << 8));
}

/* Decode one 64-byte ATT_ADC reply and forward the values */
static void
usb_tester_monitor_powerz_km003c_emit (UsbTesterMonitorPowerzKm003c *self,
                                       const guint8 *report)
{
    gdouble voltage = (gdouble) (gint32) read_le32 (report + POWERZ_ADC_VBUS) / 1000000.0;
    gdouble current = (gdouble) (gint32) read_le32 (report + POWERZ_ADC_IBUS) / 1000000.0;

    gint16 temp_raw = read_le16_signed (report + POWERZ_ADC_TEMP);
    gdouble temperature = (temp_raw == POWERZ_TEMP_INVALID_RAW)
        ? NAN
        : (gdouble) temp_raw / 128.0;

    gdouble dp_voltage = (gdouble) read_le16_signed (report + POWERZ_ADC_DP) / 10000.0;
    gdouble dn_voltage = (gdouble) read_le16_signed (report + POWERZ_ADC_DN) / 10000.0;

    if (self->reading_callback) {
        self->reading_callback (voltage, current, temperature,
                                 dp_voltage, dn_voltage,
                                 self->reading_user_data);
    }
}

void
usb_tester_monitor_powerz_km003c_feed (UsbTesterMonitorPowerzKm003c *self,
                                       const guint8 *data,
                                       gsize length)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_POWERZ_KM003C (self));
    g_return_if_fail (data != NULL || length == 0);

    if (length == 0) {
        return;
    }

    g_byte_array_append (self->rx_buffer, data, length);

    while (self->rx_buffer->len >= POWERZ_REPORT_SIZE) {
        const guint8 *report = self->rx_buffer->data;

        /* Reply header: attribute in the low 15 bits of the first word.
         * Wait for a complete ATT_ADC reply; drop anything else. */
        if ((read_le32 (report) & 0x7FFF) != POWERZ_ATT_ADC) {
            g_byte_array_remove_index (self->rx_buffer, 0);
            continue;
        }

        usb_tester_monitor_powerz_km003c_emit (self, report);
        g_byte_array_remove_range (self->rx_buffer, 0, POWERZ_REPORT_SIZE);
    }

    /* Never let a desynchronized stream grow the buffer unbounded */
    if (self->rx_buffer->len > 4 * POWERZ_REPORT_SIZE) {
        g_byte_array_remove_range (self->rx_buffer, 0,
                                   self->rx_buffer->len - POWERZ_REPORT_SIZE);
    }
}
