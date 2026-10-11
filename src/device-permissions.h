#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef void (*UsbTesterMonitorPermissionsGrantedCallback) (gboolean success,
                                                            gpointer user_data);

/* Installs a udev rule that grants the active user access to the hidraw
 * node of the Fnirsi FNB-48. Authorization happens through pkexec, which
 * shows the system's native authentication dialog. The callback is invoked
 * on the main loop and is always called, with success FALSE when pkexec is
 * unavailable, the user cancels the authentication, or the installation
 * fails. */
void usb_tester_monitor_device_permissions_grant_fnirsi_fnb48_async (
    UsbTesterMonitorPermissionsGrantedCallback callback,
    gpointer user_data);

G_END_DECLS
