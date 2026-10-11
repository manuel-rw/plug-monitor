#include "application.h"
#include "main-window.h"

struct _UsbTesterMonitorApplication {
    AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorApplication, usb_tester_monitor_application, ADW_TYPE_APPLICATION)

static void
usb_tester_monitor_application_activate (GApplication *app)
{
    UsbTesterMonitorApplication *self = USB_TESTER_MONITOR_APPLICATION (app);
    UsbTesterMonitorMainWindow *window;

    window = usb_tester_monitor_main_window_new ();
    gtk_window_present (GTK_WINDOW (window));
}

static void
usb_tester_monitor_application_class_init (UsbTesterMonitorApplicationClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->activate = usb_tester_monitor_application_activate;
}

static void
usb_tester_monitor_application_init (UsbTesterMonitorApplication *self)
{
}

UsbTesterMonitorApplication *
usb_tester_monitor_application_new (const char *application_id,
                                   GApplicationFlags flags)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_APPLICATION,
                        "application-id", application_id,
                        "flags", flags,
                        NULL);
}
