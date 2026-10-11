#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_DEVICE_PROTOCOL (usb_tester_monitor_device_protocol_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorDeviceProtocol, usb_tester_monitor_device_protocol,
                      USB_TESTER_MONITOR, DEVICE_PROTOCOL, GObject)

typedef enum {
    USB_TESTER_MONITOR_PROTOCOL_STATE_DISCONNECTED,
    USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED,
    USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING,
    USB_TESTER_MONITOR_PROTOCOL_STATE_ERROR
} UsbTesterMonitorProtocolState;

typedef struct {
    gdouble voltage;
    gdouble current;
    gdouble power;
    /* Extra channels: NAN when the device does not provide them */
    gdouble temperature; /* degrees Celsius */
    gdouble battery;     /* percent, 0-100 */
    guint64 timestamp;
} UsbTesterMonitorReading;

typedef void (*UsbTesterMonitorDataCallback) (UsbTesterMonitorDeviceProtocol *protocol,
                                               UsbTesterMonitorReading *reading,
                                               gpointer user_data);

UsbTesterMonitorDeviceProtocol *usb_tester_monitor_device_protocol_new (const gchar *device_path,
                                                                       guint baud_rate);

UsbTesterMonitorDeviceProtocol *usb_tester_monitor_device_protocol_new_for_type (const gchar *device_path,
                                                                                 const gchar *device_type);

gboolean usb_tester_monitor_device_protocol_connect (UsbTesterMonitorDeviceProtocol *self,
                                                     GError **error);

void usb_tester_monitor_device_protocol_disconnect (UsbTesterMonitorDeviceProtocol *self);

void usb_tester_monitor_device_protocol_start_streaming (UsbTesterMonitorDeviceProtocol *self);

void usb_tester_monitor_device_protocol_stop_streaming (UsbTesterMonitorDeviceProtocol *self);

void usb_tester_monitor_device_protocol_set_data_callback (UsbTesterMonitorDeviceProtocol *self,
                                                           UsbTesterMonitorDataCallback callback,
                                                           gpointer user_data,
                                                           GDestroyNotify notify);

UsbTesterMonitorProtocolState usb_tester_monitor_device_protocol_get_state (UsbTesterMonitorDeviceProtocol *self);

const gchar *usb_tester_monitor_device_protocol_get_device_name (UsbTesterMonitorDeviceProtocol *self);

const gchar *usb_tester_monitor_device_protocol_get_device_info (UsbTesterMonitorDeviceProtocol *self);

G_END_DECLS
