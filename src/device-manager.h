#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#include "device-protocol.h"

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_DEVICE_MANAGER (usb_tester_monitor_device_manager_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorDeviceManager, usb_tester_monitor_device_manager,
                      USB_TESTER_MONITOR, DEVICE_MANAGER, GObject)

#define USB_TESTER_MONITOR_TYPE_DEVICE (usb_tester_monitor_device_get_type ())

#define USB_TESTER_MONITOR_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), USB_TESTER_MONITOR_TYPE_DEVICE, UsbTesterMonitorDevice))
#define USB_TESTER_MONITOR_IS_DEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), USB_TESTER_MONITOR_TYPE_DEVICE))

typedef struct _UsbTesterMonitorDevice UsbTesterMonitorDevice;
typedef struct _UsbTesterMonitorDeviceClass UsbTesterMonitorDeviceClass;

typedef enum {
    USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED,
    USB_TESTER_MONITOR_DEVICE_STATE_CONNECTED,
    USB_TESTER_MONITOR_DEVICE_STATE_CONNECTING,
    USB_TESTER_MONITOR_DEVICE_STATE_ERROR
} UsbTesterMonitorDeviceState;

typedef enum {
    USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3,
    /* Fnirsi device types must stay contiguous: the UI treats this range
     * as "the Fnirsi protocol family" (temperature and D+/D- channels) */
    USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB48,
    USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_C1,
    USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB48S,
    USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB58,
    /* POWER-Z types, likewise contiguous */
    USB_TESTER_MONITOR_DEVICE_TYPE_POWERZ_KM002C,
    USB_TESTER_MONITOR_DEVICE_TYPE_POWERZ_KM003C,
    USB_TESTER_MONITOR_DEVICE_TYPE_UNKNOWN
} UsbTesterMonitorDeviceType;

#define USB_TESTER_MONITOR_DEVICE_TYPE_IS_FNIRSI(type) \
    ((type) >= USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB48 && \
     (type) <= USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB58)

#define USB_TESTER_MONITOR_DEVICE_TYPE_IS_POWERZ(type) \
    ((type) >= USB_TESTER_MONITOR_DEVICE_TYPE_POWERZ_KM002C && \
     (type) <= USB_TESTER_MONITOR_DEVICE_TYPE_POWERZ_KM003C)

GType usb_tester_monitor_device_get_type (void);

UsbTesterMonitorDevice *usb_tester_monitor_device_new (const gchar *name,
                                                        const gchar *identifier,
                                                        UsbTesterMonitorDeviceType type);

struct _UsbTesterMonitorDeviceClass {
    GObjectClass parent_class;
};

struct _UsbTesterMonitorDevice {
    GObject parent_instance;

    gchar *name;
    gchar *identifier;
    UsbTesterMonitorDeviceType type;
    UsbTesterMonitorDeviceState state;
    gpointer device_data; /* Device-specific data */
};

UsbTesterMonitorDeviceManager *usb_tester_monitor_device_manager_new (void);

GListModel *usb_tester_monitor_device_manager_get_devices (UsbTesterMonitorDeviceManager *self);

void usb_tester_monitor_device_manager_scan_devices (UsbTesterMonitorDeviceManager *self);

/* Connect/disconnect by device identifier (e.g. "/dev/ttyACM0").
 * Returns FALSE when the device is unknown or the serial port cannot be
 * opened; error (when not NULL) is set with the failure reason. */
gboolean usb_tester_monitor_device_manager_connect_device (UsbTesterMonitorDeviceManager *self,
                                                        const gchar *identifier,
                                                        GError **error);

void usb_tester_monitor_device_manager_disconnect_device (UsbTesterMonitorDeviceManager *self,
                                                           const gchar *identifier);

typedef void (*UsbTesterMonitorManagerReadingCallback) (const gchar *identifier,
                                                        const UsbTesterMonitorReading *reading,
                                                        gpointer user_data);

void usb_tester_monitor_device_manager_set_reading_callback (UsbTesterMonitorDeviceManager *self,
                                                             UsbTesterMonitorManagerReadingCallback callback,
                                                             gpointer user_data);

/* Whether a connection is currently open for the given device identifier. */
gboolean usb_tester_monitor_device_manager_is_connected (UsbTesterMonitorDeviceManager *self,
                                                         const gchar *identifier);

G_END_DECLS
