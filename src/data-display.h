#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_DATA_DISPLAY (usb_tester_monitor_data_display_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorDataDisplay, usb_tester_monitor_data_display,
                      USB_TESTER_MONITOR, DATA_DISPLAY, GtkBox)

UsbTesterMonitorDataDisplay *usb_tester_monitor_data_display_new (void);

void usb_tester_monitor_data_display_set_voltage (UsbTesterMonitorDataDisplay *self,
                                                   gdouble voltage);

void usb_tester_monitor_data_display_set_current (UsbTesterMonitorDataDisplay *self,
                                                   gdouble current);

void usb_tester_monitor_data_display_set_power (UsbTesterMonitorDataDisplay *self,
                                                 gdouble power);

void usb_tester_monitor_data_display_set_device_status (UsbTesterMonitorDataDisplay *self,
                                                         const gchar *status);

void usb_tester_monitor_data_display_set_min_max_voltage (UsbTesterMonitorDataDisplay *self,
                                                           gdouble min_voltage,
                                                           gdouble max_voltage);

void usb_tester_monitor_data_display_set_min_max_current (UsbTesterMonitorDataDisplay *self,
                                                           gdouble min_current,
                                                           gdouble max_current);

/* Ripple (peak-to-peak) voltage in volts */
void usb_tester_monitor_data_display_set_ripple (UsbTesterMonitorDataDisplay *self,
                                                 gdouble ripple_voltage);

G_END_DECLS
