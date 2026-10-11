#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_FNIRSI_FNB48 (usb_tester_monitor_fnirsi_fnb48_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorFnirsiFnb48, usb_tester_monitor_fnirsi_fnb48,
                      USB_TESTER_MONITOR, FNIRSI_FNB48, GObject)

/* Fnirsi FNB-48 protocol constants (HID interface, 64-byte reports) */
#define FNIRSI_FNB48_REPORT_PREFIX 0xAA /* vendor constant at the start of every report */
#define FNIRSI_FNB48_REPORT_TYPE_DATA 0x04 /* measurement report from the device */

/* Host -> device request types; the trailing byte of each request is its CRC-8 */
#define FNIRSI_FNB48_CMD_INIT_1 0x81
#define FNIRSI_FNB48_CMD_INIT_2 0x82
#define FNIRSI_FNB48_CMD_POLL 0x83

#define FNIRSI_FNB48_REPORT_SIZE 64
#define FNIRSI_FNB48_SAMPLE_SIZE 15
#define FNIRSI_FNB48_SAMPLES_PER_REPORT 4
#define FNIRSI_FNB48_MAX_BUFFER_SIZE 4096

/* Data report layout: prefix, type, 4 x 15-byte samples, unknown byte, CRC-8.
 * Each sample: voltage (u32 LE, 1e-5 V), current (u32 LE, 1e-5 A),
 * D+ (u16 LE, 1e-3 V), D- (u16 LE, 1e-3 V), unknown byte,
 * temperature (u16 LE, 0.1 degrees Celsius). */
#define FNIRSI_FNB48_VOLTAGE_SCALE 100000.0
#define FNIRSI_FNB48_CURRENT_SCALE 100000.0
#define FNIRSI_FNB48_TEMP_SCALE 10.0

typedef void (*UsbTesterMonitorFnirsiReadingCallback) (gdouble voltage,
                                                      gdouble current,
                                                      gdouble temperature,
                                                      gpointer user_data);

UsbTesterMonitorFnirsiFnb48 *usb_tester_monitor_fnirsi_fnb48_new (const gchar *device_path);

/* CRC-8 used by the FNB-48 protocol: poly 0x39, init 0x42, MSB first, no xorout */
guint8 usb_tester_monitor_fnirsi_fnb48_crc8 (const guint8 *data, gsize length);

/* Build a 64-byte host request report for the given command.
 * Free the returned buffer with g_free().
 * Returns FALSE on invalid arguments. */
gboolean usb_tester_monitor_fnirsi_fnb48_build_request (guint8 command,
                                                        guint8 **report,
                                                        gsize *report_len);

/* Feed raw received bytes into the report parser. For every CRC-valid data
 * report the newest of its 4 samples is forwarded to the reading callback. */
void usb_tester_monitor_fnirsi_fnb48_feed (UsbTesterMonitorFnirsiFnb48 *self,
                                           const guint8 *data,
                                           gsize length);

void usb_tester_monitor_fnirsi_fnb48_set_reading_callback (UsbTesterMonitorFnirsiFnb48 *self,
                                                           UsbTesterMonitorFnirsiReadingCallback callback,
                                                           gpointer user_data);

G_END_DECLS
