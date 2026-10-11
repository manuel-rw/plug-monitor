#include "application.h"

int
main (int argc, char **argv)
{
    UsbTesterMonitorApplication *app;
    int status;

    app = usb_tester_monitor_application_new ("org.gnome.USBTesterMonitor",
                                               G_APPLICATION_DEFAULT_FLAGS);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}
