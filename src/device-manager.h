#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_DEVICE_MANAGER (usb_tester_monitor_device_manager_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorDeviceManager, usb_tester_monitor_device_manager,
                      USB_TESTER_MONITOR, DEVICE_MANAGER, GObject)

typedef struct _UsbTesterMonitorDevice UsbTesterMonitorDevice;

typedef enum {
    USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED,
    USB_TESTER_MONITOR_DEVICE_STATE_CONNECTED,
    USB_TESTER_MONITOR_DEVICE_STATE_CONNECTING,
    USB_TESTER_MONITOR_DEVICE_STATE_ERROR
} UsbTesterMonitorDeviceState;

typedef enum {
    USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3,
    USB_TESTER_MONITOR_DEVICE_TYPE_UNKNOWN
} UsbTesterMonitorDeviceType;

struct _UsbTesterMonitorDevice {
    gchar *name;
    gchar *identifier;
    UsbTesterMonitorDeviceType type;
    UsbTesterMonitorDeviceState state;
    gpointer device_data; /* Device-specific data */
};

UsbTesterMonitorDeviceManager *usb_tester_monitor_device_manager_new (void);

GListModel *usb_tester_monitor_device_manager_get_devices (UsbTesterMonitorDeviceManager *self);

void usb_tester_monitor_device_manager_scan_devices (UsbTesterMonitorDeviceManager *self);

void usb_tester_monitor_device_manager_connect_device (UsbTesterMonitorDeviceManager *self,
                                                        const gchar *identifier);

void usb_tester_monitor_device_manager_disconnect_device (UsbTesterMonitorDeviceManager *self,
                                                           const gchar *identifier);

UsbTesterMonitorDeviceProtocol *usb_tester_monitor_device_manager_get_active_protocol (UsbTesterMonitorDeviceManager *self);

G_END_DECLS
