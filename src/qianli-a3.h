#pragma once

#include <glib-object.h>
#include "device-protocol.h"

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_QIANLI_A3 (usb_tester_monitor_qianli_a3_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3,
                      USB_TESTER_MONITOR, QIANLI_A3, UsbTesterMonitorDeviceProtocol)

/* Qianli iBridge A3 protocol constants */
#define QIANLI_A3_MAGIC_BYTE 0xDA
#define QIANLI_A3_DEFAULT_BAUD_RATE 115200

/* Device model and command IDs */
#define QIANLI_A3_MODEL_ID 0x04
#define QIANLI_A3_CMD_ENABLE_STREAM 0x05
#define QIANLI_A3_CMD_DISABLE_STREAM 0x05
#define QIANLI_A3_CMD_PING 0x01
#define QIANLI_A3_CMD_VERSION 0x04

/* Payload constants */
#define QIANLI_A3_PAYLOAD_ENABLE 0x00
#define QIANLI_A3_PAYLOAD_DISABLE 0x01

UsbTesterMonitorQianliA3 *usb_tester_monitor_qianli_a3_new (const gchar *device_path);

G_END_DECLS
