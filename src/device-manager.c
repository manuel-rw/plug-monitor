#include "device-manager.h"
#include "device-protocol.h"
#include "qianli-a3.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorDeviceManager {
    GObject parent_instance;
    
    /* List of detected devices */
    GListStore *devices;
    
    /* Currently connected device */
    UsbTesterMonitorDeviceProtocol *active_protocol;
    
    /* Device scanning */
    GList *serial_ports;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDeviceManager, usb_tester_monitor_device_manager, G_TYPE_OBJECT)

static void
usb_tester_monitor_device_free (UsbTesterMonitorDevice *device)
{
    g_free (device->name);
    g_free (device->identifier);
    g_free (device);
}

static void
usb_tester_monitor_device_manager_finalize (GObject *object)
{
    UsbTesterMonitorDeviceManager *self = USB_TESTER_MONITOR_DEVICE_MANAGER (object);
    
    if (self->devices) {
        g_object_unref (self->devices);
    }
    
    if (self->active_protocol) {
        usb_tester_monitor_device_protocol_disconnect (self->active_protocol);
        g_object_unref (self->active_protocol);
    }
    
    g_list_free_full (self->serial_ports, g_free);
    
    G_OBJECT_CLASS (usb_tester_monitor_device_manager_parent_class)->finalize (object);
}

static void
usb_tester_monitor_device_manager_class_init (UsbTesterMonitorDeviceManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_device_manager_finalize;
}

static void
usb_tester_monitor_device_manager_init (UsbTesterMonitorDeviceManager *self)
{
    self->devices = g_list_store_new (G_TYPE_POINTER);
    self->serial_ports = NULL;
}

/* Detect available serial ports */
static GList *
detect_serial_ports (void)
{
    GList *ports = NULL;
    
    /* Common serial port patterns */
    const gchar *patterns[] = {
        "/dev/ttyUSB*",
        "/dev/ttyACM*",
        "/dev/ttyS*",
        "COM*",
        NULL
    };
    
    /* Try to find serial ports */
    /* This is a simplified approach - in production, use libserialport or similar */
    
    /* For now, return an empty list */
    /* Device detection will be enhanced in future versions */
    
    return ports;
}

/* Check if a port might be a Qianli A3 device */
static gboolean
is_qianli_a3_device (const gchar *port_path)
{
    /* Try to open and identify the device */
    /* This is a placeholder - actual detection would involve */
    /* sending identification commands and checking responses */
    
    /* For now, assume common USB serial devices might be Qianli A3 */
    if (g_str_has_prefix (port_path, "/dev/ttyUSB") ||
        g_str_has_prefix (port_path, "/dev/ttyACM") ||
        g_str_has_prefix (port_path, "COM")) {
        return TRUE;
    }
    
    return FALSE;
}

UsbTesterMonitorDeviceManager *
usb_tester_monitor_device_manager_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_DEVICE_MANAGER, NULL);
}

GListModel *
usb_tester_monitor_device_manager_get_devices (UsbTesterMonitorDeviceManager *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self), NULL);
    
    return G_LIST_MODEL (self->devices);
}

void
usb_tester_monitor_device_manager_scan_devices (UsbTesterMonitorDeviceManager *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));
    
    /* Clear existing devices */
    g_list_store_remove_all (self->devices);
    
    /* Detect serial ports */
    self->serial_ports = detect_serial_ports ();
    
    /* For each port, check if it might be a supported device */
    /* This is a simplified approach - real implementation would try to */
    /* communicate with each device to identify it */
    
    /* Add some common test patterns */
    /* In a real implementation, these would be detected devices */
    
    /* Add Qianli A3 devices */
    /* For now, add some placeholder devices for testing */
    
    UsbTesterMonitorDevice *device;
    
    /* Test device 1 */
    device = g_new0 (UsbTesterMonitorDevice, 1);
    device->name = g_strdup ("Qianli iBridge A3");
    device->identifier = g_strdup ("/dev/ttyUSB0");
    device->type = USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3;
    device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
    g_list_store_append (self->devices, device);
    
    /* Test device 2 */
    device = g_new0 (UsbTesterMonitorDevice, 1);
    device->name = g_strdup ("Qianli iBridge A3");
    device->identifier = g_strdup ("/dev/ttyUSB1");
    device->type = USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3;
    device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
    g_list_store_append (self->devices, device);
    
    /* Test device 3 (Windows) */
    device = g_new0 (UsbTesterMonitorDevice, 1);
    device->name = g_strdup ("Qianli iBridge A3");
    device->identifier = g_strdup ("COM3");
    device->type = USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3;
    device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
    g_list_store_append (self->devices, device);
}

void
usb_tester_monitor_device_manager_connect_device (UsbTesterMonitorDeviceManager *self,
                                                   const gchar *identifier)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));
    g_return_if_fail (identifier != NULL);
    
    /* Disconnect current device if connected */
    if (self->active_protocol) {
        usb_tester_monitor_device_protocol_disconnect (self->active_protocol);
        g_object_unref (self->active_protocol);
        self->active_protocol = NULL;
    }
    
    /* Create appropriate protocol for the device */
    /* For now, assume all devices are Qianli A3 */
    self->active_protocol = g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3,
                                          "device-path", identifier,
                                          NULL);
    
    /* Try to connect */
    GError *error = NULL;
    if (!usb_tester_monitor_device_protocol_connect (self->active_protocol, &error)) {
        g_warning ("Failed to connect to device %s: %s", identifier, error->message);
        g_error_free (error);
        g_object_unref (self->active_protocol);
        self->active_protocol = NULL;
        return;
    }
    
    /* Update device state */
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            device->state = USB_TESTER_MONITOR_DEVICE_STATE_CONNECTED;
            g_object_notify (G_OBJECT (self), "devices");
            break;
        }
    }
}

void
usb_tester_monitor_device_manager_disconnect_device (UsbTesterMonitorDeviceManager *self,
                                                      const gchar *identifier)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));
    
    if (!self->active_protocol) {
        return;
    }
    
    /* Stop streaming if active */
    if (usb_tester_monitor_device_protocol_get_state (self->active_protocol) == 
        USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING) {
        usb_tester_monitor_device_protocol_stop_streaming (self->active_protocol);
    }
    
    /* Disconnect */
    usb_tester_monitor_device_protocol_disconnect (self->active_protocol);
    g_object_unref (self->active_protocol);
    self->active_protocol = NULL;
    
    /* Update device state */
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
            g_object_notify (G_OBJECT (self), "devices");
            break;
        }
    }
}

/* Get the active protocol */
UsbTesterMonitorDeviceProtocol *
usb_tester_monitor_device_manager_get_active_protocol (UsbTesterMonitorDeviceManager *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self), NULL);
    
    return self->active_protocol;
}
