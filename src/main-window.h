#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_MAIN_WINDOW (usb_tester_monitor_main_window_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorMainWindow, usb_tester_monitor_main_window,
                      USB_TESTER_MONITOR, MAIN_WINDOW, AdwApplicationWindow)

UsbTesterMonitorMainWindow *usb_tester_monitor_main_window_new (void);

G_END_DECLS
