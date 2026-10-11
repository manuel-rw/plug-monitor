#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_POWERZ_KM003C (usb_tester_monitor_powerz_km003c_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorPowerzKm003c, usb_tester_monitor_powerz_km003c,
                      USB_TESTER_MONITOR, POWERZ_KM003C, GObject)

/* ChargerLAB POWER-Z KM002C / KM003C protocol constants (HID interface).
 * Every exchange is a 64-byte report; requests carry a 4-byte little-endian
 * MsgHeader (type:7, extend:1, id:8, :1, att:15), replies have the attribute
 * in the low 15 bits of their first word. */
#define POWERZ_REPORT_SIZE 64

/* Command types (MsgHeader.ctrl.type) */
#define POWERZ_CMD_GET_DATA 0x0C
#define POWERZ_CMD_PUT_DATA 0x41

/* Attribute types */
#define POWERZ_ATT_ADC 0x001

/* USB IDs */
#define POWERZ_VENDOR_ID "5fc9"
#define POWERZ_KM002C_PRODUCT_ID "0061"
#define POWERZ_KM003C_PRODUCT_ID "0063"

/* AdcData block in an ATT_ADC reply, starting at report offset 8 */
#define POWERZ_ADC_VBUS 8   /* int32_le, 1e-6 V */
#define POWERZ_ADC_IBUS 12  /* int32_le, 1e-6 A, negative = reverse flow */
#define POWERZ_ADC_TEMP 32  /* int16_le, 1/128 C, 0x8000 = invalid */
#define POWERZ_ADC_DP 38    /* uint16_le, 1e-4 V */
#define POWERZ_ADC_DN 40     /* uint16_le, 1e-4 V */
#define POWERZ_TEMP_INVALID_RAW (-32768)

/* Rate limit for the single-shot ADC polling loop (25 readings/s) */
#define POWERZ_POLL_INTERVAL_MS 40

typedef void (*UsbTesterMonitorPowerzReadingCallback) (gdouble voltage,
                                                       gdouble current,
                                                       gdouble temperature,
                                                       gdouble dp_voltage,
                                                       gdouble dn_voltage,
                                                       gpointer user_data);

UsbTesterMonitorPowerzKm003c *usb_tester_monitor_powerz_km003c_new (const gchar *device_path);

/* Build the 4-byte GET_DATA/ATT_ADC request, zero padded to 64 bytes.
 * Free the returned buffer with g_free(). */
guint8 *usb_tester_monitor_powerz_km003c_build_adc_request (void);

/* Feed raw received bytes into the report parser. Each ATT_ADC reply
 * triggers the reading callback with the decoded values. */
void usb_tester_monitor_powerz_km003c_feed (UsbTesterMonitorPowerzKm003c *self,
                                            const guint8 *data,
                                            gsize length);

void usb_tester_monitor_powerz_km003c_set_reading_callback (UsbTesterMonitorPowerzKm003c *self,
                                                            UsbTesterMonitorPowerzReadingCallback callback,
                                                            gpointer user_data);

G_END_DECLS
