#include "application.h"
#include "main-window.h"
#include <gtk/gtk.h>

struct _UsbTesterMonitorApplication {
    AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorApplication, usb_tester_monitor_application, ADW_TYPE_APPLICATION)

/* Series colors used across the app: green voltage, orange current, blue power.
 * Keep in sync with the chart colors in chart-view.c. */
static const char *app_css =
    ".voltage-value, .voltage-unit, .voltage-title { color: #35b042; }"
    ".current-value, .current-unit, .current-title { color: #f57900; }"
    ".power-value, .power-unit, .power-title { color: #3584e4; }"
    ".voltage-value, .current-value, .power-value {"
    "  font-size: 32px;"
    "  font-weight: 700;"
    "}"
    ".voltage-unit, .current-unit, .power-unit {"
    "  font-size: 16px;"
    "  font-weight: 700;"
    "  opacity: 0.85;"
    "}"
    ".ripple-value { color: #35b042; font-weight: 700; }"
    ".stats-card {"
    "  font-size: 12px;"
    "  background-color: @card_bg_color;"
    "  border-radius: 12px;"
    "  padding: 14px 18px;"
    "}"
    ".stats-value { font-weight: 700; }"

    /* Temperature readout: monospace so digits do not shift the layout */
    ".temp-value { font-family: monospace; }"

    /* Left-side device panel: floating card in the GNOME 50 style */
    ".device-panel {"
    "  background-color: @sidebar_bg_color;"
    "  border-radius: 12px;"
    "  box-shadow: 0 1px 6px 1px rgba(0, 0, 0, 0.25);"
    "}"
    ".device-panel list {"
    "  background-color: transparent;"
    "}"
    ".device-panel list row {"
    "  border-radius: 8px;"
    "  margin: 1px 2px;"
    "  padding: 6px 4px;"
    "}";

static void
usb_tester_monitor_application_startup (GApplication *app)
{
    G_APPLICATION_CLASS (usb_tester_monitor_application_parent_class)->startup (app);

    GtkCssProvider *provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (provider, app_css);
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);
}

static void
usb_tester_monitor_application_activate (GApplication *app)
{
    UsbTesterMonitorMainWindow *window;

    window = usb_tester_monitor_main_window_new ();
    gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (app));
    gtk_window_present (GTK_WINDOW (window));
}

static void
usb_tester_monitor_application_class_init (UsbTesterMonitorApplicationClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

    app_class->startup = usb_tester_monitor_application_startup;
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
