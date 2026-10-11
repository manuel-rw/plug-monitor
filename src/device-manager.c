#include "device-manager.h"
#include "device-protocol.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorDeviceManager {
    GObject parent_instance;
    
    /* List of detected devices */
    GListStore *devices;
    
    /* Currently connected device */
    gpointer active_protocol;
    
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
    
    /* Update device state */
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            device->state = USB_TESTER_MONITOR_DEVICE_STATE_CONNECTED;
        }
    }
}

void
usb_tester_monitor_device_manager_disconnect_device (UsbTesterMonitorDeviceManager *self,
                                                      const gchar *identifier)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));
    
    /* Update device state */
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
        }
    }
}
