#include "qianli-a3.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorQianliA3 {
    GObject parent_instance;
    
    /* Qianli A3 specific state */
    gchar *device_path;
    gboolean stream_enabled;
    guint32 last_current_raw;
    guint32 last_voltage_raw;
    
    /* Buffer for parsing */
    GByteArray *rx_buffer;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3, G_TYPE_OBJECT)

static void
usb_tester_monitor_qianli_a3_finalize (GObject *object)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (object);
    
    g_free (self->device_path);
    if (self->rx_buffer) {
        g_byte_array_unref (self->rx_buffer);
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_qianli_a3_parent_class)->finalize (object);
}

static void
usb_tester_monitor_qianli_a3_class_init (UsbTesterMonitorQianliA3Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_qianli_a3_finalize;
}

static void
usb_tester_monitor_qianli_a3_init (UsbTesterMonitorQianliA3 *self)
{
    self->stream_enabled = FALSE;
    self->last_current_raw = 0;
    self->last_voltage_raw = 0;
    self->rx_buffer = g_byte_array_new ();
}

UsbTesterMonitorQianliA3 *
usb_tester_monitor_qianli_a3_new (const gchar *device_path)
{
    UsbTesterMonitorQianliA3 *self = g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3, NULL);
    self->device_path = g_strdup (device_path);
    return self;
}
