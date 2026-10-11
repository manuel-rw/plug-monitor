#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_QIANLI_A3 (usb_tester_monitor_qianli_a3_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3,
                      USB_TESTER_MONITOR, QIANLI_A3, GObject)

/* Qianli iBridge A3 protocol constants */
#define QIANLI_A3_MAGIC_BYTE 0xDA
#define QIANLI_A3_DEFAULT_BAUD_RATE 115200

/* Device model and command IDs */
#define QIANLI_A3_MODEL_ID 0x04
#define QIANLI_A3_CMD_ENABLE_STREAM 0x05
#define QIANLI_A3_CMD_DISABLE_STREAM 0x05

/* Payload constants */
#define QIANLI_A3_PAYLOAD_ENABLE 0x00
#define QIANLI_A3_PAYLOAD_DISABLE 0x01

/* Stream payload is 8 bytes: current (u32 BE, 1/10000 A) + voltage (u32 BE, 1/1000 V) */
#define QIANLI_A3_STREAM_PAYLOAD_SIZE 8
#define QIANLI_A3_CURRENT_SCALE 10000.0
#define QIANLI_A3_VOLTAGE_SCALE 1000.0
#define QIANLI_A3_MAX_PAYLOAD_SIZE 1024

typedef void (*UsbTesterMonitorQianliReadingCallback) (gdouble voltage,
                                                      gdouble current,
                                                      gpointer user_data);

UsbTesterMonitorQianliA3 *usb_tester_monitor_qianli_a3_new (const gchar *device_path);

/* Build a protocol packet. Free the returned buffer with g_free().
 * Returns FALSE on invalid arguments. */
gboolean usb_tester_monitor_qianli_a3_build_packet (guint8 model,
                                                    guint8 cmd,
                                                    guint8 param1,
                                                    guint8 param2,
                                                    const guint8 *payload,
                                                    guint16 payload_len,
                                                    guint8 **packet,
                                                    gsize *packet_len);

/* Feed raw received bytes into the frame parser. Complete, checksum-valid
 * stream frames are forwarded to the registered reading callback. */
void usb_tester_monitor_qianli_a3_feed (UsbTesterMonitorQianliA3 *self,
                                        const guint8 *data,
                                        gsize length);

void usb_tester_monitor_qianli_a3_set_reading_callback (UsbTesterMonitorQianliA3 *self,
                                                        UsbTesterMonitorQianliReadingCallback callback,
                                                        gpointer user_data);

G_END_DECLS
