#include "device-manager.h"
#include "device-protocol.h"
#include <glib.h>
#include <gio/gio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* USB IDs of the Qianli iBridge A3 (XHSC CDC serial) */
#define QIANLI_A3_USB_VENDOR_ID "2e88"
#define QIANLI_A3_USB_PRODUCT_ID "4603"

struct _UsbTesterMonitorDeviceManager {
    GObject parent_instance;

    /* List of detected devices */
    GListStore *devices;

    /* Open connections: device identifier -> UsbTesterMonitorDeviceProtocol.
     * Multiple devices can be connected at the same time. */
    GHashTable *connections;

    /* Device scanning */
    GList *serial_ports;

    /* Reading callback forwarded to the active protocol */
    UsbTesterMonitorManagerReadingCallback reading_callback;
    gpointer reading_user_data;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDeviceManager, usb_tester_monitor_device_manager, G_TYPE_OBJECT)
G_DEFINE_TYPE (UsbTesterMonitorDevice, usb_tester_monitor_device, G_TYPE_OBJECT)

static void
usb_tester_monitor_device_finalize (GObject *object)
{
    UsbTesterMonitorDevice *device = USB_TESTER_MONITOR_DEVICE (object);

    g_free (device->name);
    g_free (device->identifier);

    G_OBJECT_CLASS (usb_tester_monitor_device_parent_class)->finalize (object);
}

static void
usb_tester_monitor_device_class_init (UsbTesterMonitorDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = usb_tester_monitor_device_finalize;
}

static void
usb_tester_monitor_device_init (UsbTesterMonitorDevice *self)
{
}

UsbTesterMonitorDevice *
usb_tester_monitor_device_new (const gchar *name,
                               const gchar *identifier,
                               UsbTesterMonitorDeviceType type)
{
    UsbTesterMonitorDevice *device = g_object_new (USB_TESTER_MONITOR_TYPE_DEVICE, NULL);

    device->name = g_strdup (name);
    device->identifier = g_strdup (identifier);
    device->type = type;
    device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;

    return device;
}

static void
usb_tester_monitor_device_manager_finalize (GObject *object)
{
    UsbTesterMonitorDeviceManager *self = USB_TESTER_MONITOR_DEVICE_MANAGER (object);

    if (self->connections) {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, self->connections);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
            UsbTesterMonitorDeviceProtocol *protocol = value;

            usb_tester_monitor_device_protocol_stop_streaming (protocol);
            usb_tester_monitor_device_protocol_disconnect (protocol);
            g_object_unref (protocol);

            g_hash_table_iter_steal (&iter);
            g_free (key);
        }

        g_hash_table_unref (self->connections);
    }

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
    self->devices = g_list_store_new (USB_TESTER_MONITOR_TYPE_DEVICE);
    /* Keys are owned copies of the identifier, values are owned protocols */
    self->connections = g_hash_table_new (g_str_hash, g_str_equal);
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

/* Walk up the sysfs device chain looking for idVendor/idProduct files.
 * Fills vendor/product with freshly allocated strings when found. */
static gboolean
usb_tester_monitor_device_manager_find_usb_ids (const gchar *tty_name,
                                                 gchar **vendor,
                                                 gchar **product)
{
    gchar *device_link = g_build_filename ("/sys/class/tty", tty_name, "device", NULL);
    char resolved[PATH_MAX];

    if (!realpath (device_link, resolved)) {
        g_free (device_link);
        return FALSE;
    }
    g_free (device_link);

    gchar *dir = g_strdup (resolved);
    for (gint depth = 0; depth < 6 && dir != NULL; depth++) {
        gchar *vendor_file = g_build_filename (dir, "idVendor", NULL);
        gboolean found = g_file_test (vendor_file, G_FILE_TEST_EXISTS);
        g_free (vendor_file);

        if (found) {
            *vendor = NULL;
            *product = NULL;

            if (!g_file_get_contents (g_build_filename (dir, "idVendor", NULL), vendor, NULL, NULL) ||
                !g_file_get_contents (g_build_filename (dir, "idProduct", NULL), product, NULL, NULL)) {
                g_free (*vendor);
                g_free (*product);
                g_free (dir);
                return FALSE;
            }

            g_strstrip (*vendor);
            g_strstrip (*product);
            g_free (dir);
            return TRUE;
        }

        gchar *parent = g_path_get_dirname (dir);
        g_free (dir);
        dir = NULL;
        if (g_strcmp0 (parent, "/") == 0) {
            g_free (parent);
            break;
        }
        dir = parent;
    }

    g_free (dir);
    return FALSE;
}

void
usb_tester_monitor_device_manager_scan_devices (UsbTesterMonitorDeviceManager *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));

    /* Clear existing devices */
    g_list_store_remove_all (self->devices);

    GDir *dir = g_dir_open ("/sys/class/tty", 0, NULL);
    if (dir == NULL) {
        return;
    }

    const gchar *name;
    while ((name = g_dir_read_name (dir)) != NULL) {
        gchar *vendor = NULL;
        gchar *product = NULL;

        if (!usb_tester_monitor_device_manager_find_usb_ids (name, &vendor, &product)) {
            continue;
        }

        if (g_strcmp0 (vendor, QIANLI_A3_USB_VENDOR_ID) == 0 &&
            g_strcmp0 (product, QIANLI_A3_USB_PRODUCT_ID) == 0) {
            gchar *identifier = g_strdup_printf ("/dev/%s", name);
            g_list_store_append (self->devices,
                                 usb_tester_monitor_device_new ("Qianli iBridge A3",
                                                                identifier,
                                                                USB_TESTER_MONITOR_DEVICE_TYPE_QIANLI_A3));
            g_free (identifier);
        }

        g_free (vendor);
        g_free (product);
    }

    g_dir_close (dir);
}

static void
usb_tester_monitor_device_manager_on_reading (UsbTesterMonitorDeviceProtocol *protocol,
                                               UsbTesterMonitorReading *reading,
                                               gpointer user_data)
{
    UsbTesterMonitorDeviceManager *self = USB_TESTER_MONITOR_DEVICE_MANAGER (user_data);

    if (self->reading_callback) {
        /* Each protocol carries the identifier of the device it serves,
         * so readings can be routed to the right device in the UI. */
        const gchar *identifier = g_object_get_data (G_OBJECT (protocol),
                                                    "device-identifier");
        self->reading_callback (identifier, reading->voltage, reading->current,
                                 self->reading_user_data);
    }
}

static UsbTesterMonitorDevice *
usb_tester_monitor_device_manager_find_device (UsbTesterMonitorDeviceManager *self,
                                                const gchar *identifier)
{
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            return device;
        }
        g_object_unref (device);
    }
    return NULL;
}

gboolean
usb_tester_monitor_device_manager_connect_device (UsbTesterMonitorDeviceManager *self,
                                                   const gchar *identifier)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self), FALSE);
    g_return_val_if_fail (identifier != NULL, FALSE);

    UsbTesterMonitorDevice *device = usb_tester_monitor_device_manager_find_device (self, identifier);
    if (device == NULL) {
        return FALSE;
    }

    /* Already connected: nothing to do */
    if (g_hash_table_contains (self->connections, identifier)) {
        g_object_unref (device);
        return TRUE;
    }

    UsbTesterMonitorDeviceProtocol *protocol =
        usb_tester_monitor_device_protocol_new_for_type (identifier, "qianli-a3");
    usb_tester_monitor_device_protocol_set_data_callback (protocol,
                                                          usb_tester_monitor_device_manager_on_reading,
                                                          self, NULL);

    GError *error = NULL;
    if (!usb_tester_monitor_device_protocol_connect (protocol, &error)) {
        g_warning ("Failed to connect to %s: %s", identifier,
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        g_object_unref (protocol);
        device->state = USB_TESTER_MONITOR_DEVICE_STATE_ERROR;
        g_object_unref (device);
        return FALSE;
    }

    /* Associate the protocol with its identifier so readings can be routed */
    g_object_set_data_full (G_OBJECT (protocol), "device-identifier",
                            g_strdup (identifier), g_free);

    usb_tester_monitor_device_protocol_start_streaming (protocol);

    g_hash_table_insert (self->connections, g_strdup (identifier), protocol);

    device->state = USB_TESTER_MONITOR_DEVICE_STATE_CONNECTED;
    g_object_unref (device);

    return TRUE;
}

void
usb_tester_monitor_device_manager_disconnect_device (UsbTesterMonitorDeviceManager *self,
                                                      const gchar *identifier)
{
    g_return_if_fail (USB_TESTER_MONITOR_DEVICE_MANAGER (self));

    if (identifier == NULL) {
        return;
    }

    gpointer key = NULL;
    gpointer value = NULL;

    if (g_hash_table_steal_extended (self->connections, identifier, &key, &value)) {
        UsbTesterMonitorDeviceProtocol *protocol = value;

        usb_tester_monitor_device_protocol_stop_streaming (protocol);
        usb_tester_monitor_device_protocol_disconnect (protocol);
        g_object_unref (protocol);
        g_free (key);
    }

    /* Update device state */
    if (identifier != NULL) {
        for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->devices)); i++) {
            UsbTesterMonitorDevice *device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
            if (g_strcmp0 (device->identifier, identifier) == 0) {
                device->state = USB_TESTER_MONITOR_DEVICE_STATE_DISCONNECTED;
            }
            g_object_unref (device);
        }
    }
}

void
usb_tester_monitor_device_manager_set_reading_callback (UsbTesterMonitorDeviceManager *self,
                                                        UsbTesterMonitorManagerReadingCallback callback,
                                                        gpointer user_data)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DEVICE_MANAGER (self));

    self->reading_callback = callback;
    self->reading_user_data = user_data;
}

gboolean
usb_tester_monitor_device_manager_is_connected (UsbTesterMonitorDeviceManager *self,
                                                const gchar *identifier)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_DEVICE_MANAGER (self), FALSE);
    g_return_val_if_fail (identifier != NULL, FALSE);

    return g_hash_table_contains (self->connections, identifier);
}
