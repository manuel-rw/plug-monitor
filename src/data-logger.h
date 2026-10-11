#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_DATA_LOGGER (usb_tester_monitor_data_logger_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorDataLogger, usb_tester_monitor_data_logger,
                      USB_TESTER_MONITOR, DATA_LOGGER, GObject)

typedef struct {
    gdouble voltage;
    gdouble current;
    gdouble power;
    guint64 timestamp;
} UsbTesterMonitorLogEntry;

typedef void (*UsbTesterMonitorLogCallback) (UsbTesterMonitorDataLogger *logger,
                                              const gchar *message,
                                              gpointer user_data);

UsbTesterMonitorDataLogger *usb_tester_monitor_data_logger_new (const gchar *log_dir);

UsbTesterMonitorDataLogger *usb_tester_monitor_data_logger_new_with_file (const gchar *log_file);

void usb_tester_monitor_data_logger_start (UsbTesterMonitorDataLogger *self);

void usb_tester_monitor_data_logger_stop (UsbTesterMonitorDataLogger *self);

void usb_tester_monitor_data_logger_log_reading (UsbTesterMonitorDataLogger *self,
                                                  gdouble voltage,
                                                  gdouble current,
                                                  gdouble power);

void usb_tester_monitor_data_logger_set_interval (UsbTesterMonitorDataLogger *self,
                                                    guint interval_ms);

void usb_tester_monitor_data_logger_set_callback (UsbTesterMonitorDataLogger *self,
                                                   UsbTesterMonitorLogCallback callback,
                                                   gpointer user_data,
                                                   GDestroyNotify notify);

gboolean usb_tester_monitor_data_logger_export_to_csv (UsbTesterMonitorDataLogger *self,
                                                       const gchar *filename,
                                                       GError **error);

GList *usb_tester_monitor_data_logger_get_entries (UsbTesterMonitorDataLogger *self);

void usb_tester_monitor_data_logger_clear (UsbTesterMonitorDataLogger *self);

G_END_DECLS
