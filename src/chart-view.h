#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_CHART_VIEW (usb_tester_monitor_chart_view_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorChartView, usb_tester_monitor_chart_view,
                      USB_TESTER_MONITOR, CHART_VIEW, GtkDrawingArea)

UsbTesterMonitorChartView *usb_tester_monitor_chart_view_new (void);

void usb_tester_monitor_chart_view_add_voltage_reading (UsbTesterMonitorChartView *self,
                                                        gdouble voltage);

void usb_tester_monitor_chart_view_add_current_reading (UsbTesterMonitorChartView *self,
                                                        gdouble current);

void usb_tester_monitor_chart_view_clear (UsbTesterMonitorChartView *self);

void usb_tester_monitor_chart_view_set_time_range (UsbTesterMonitorChartView *self,
                                                    guint seconds);

G_END_DECLS
