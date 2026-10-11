#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_STATS_CARD (usb_tester_monitor_stats_card_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorStatsCard, usb_tester_monitor_stats_card,
                      USB_TESTER_MONITOR, STATS_CARD, GtkBox)

UsbTesterMonitorStatsCard *usb_tester_monitor_stats_card_new (void);

void usb_tester_monitor_stats_card_set_min_max_voltage (UsbTesterMonitorStatsCard *self,
                                                        gdouble min_voltage,
                                                        gdouble max_voltage);

void usb_tester_monitor_stats_card_set_min_max_current (UsbTesterMonitorStatsCard *self,
                                                        gdouble min_current,
                                                        gdouble max_current);

/* Ripple (peak-to-peak) voltage in volts */
void usb_tester_monitor_stats_card_set_ripple (UsbTesterMonitorStatsCard *self,
                                              gdouble ripple_voltage);

G_END_DECLS
