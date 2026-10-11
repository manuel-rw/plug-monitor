#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define USB_TESTER_MONITOR_TYPE_APPLICATION (usb_tester_monitor_application_get_type())

G_DECLARE_FINAL_TYPE (UsbTesterMonitorApplication, usb_tester_monitor_application, 
                      USB_TESTER_MONITOR, APPLICATION, AdwApplication)

UsbTesterMonitorApplication *usb_tester_monitor_application_new (const char *application_id,
                                                               GApplicationFlags flags);

G_END_DECLS
