#include "main-window.h"
#include "application.h"
#include "device-manager.h"
#include "device-protocol.h"
#include "data-display.h"
#include "chart-view.h"
#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib.h>

struct _UsbTesterMonitorMainWindow {
    AdwApplicationWindow parent_instance;
    
    /* UI widgets */
    GtkBox *main_box;
    GtkHeaderBar *header_bar;
    GtkBox *header_box;
    GtkToggleButton *connect_button;
    GtkLabel *device_label;
    
    /* Device management */
    UsbTesterMonitorDeviceManager *device_manager;
    
    /* Display components */
    UsbTesterMonitorDataDisplay *data_display;
    UsbTesterMonitorChartView *chart_view;
    
    /* Current readings */
    gdouble current_voltage;
    gdouble current_current;
    gdouble current_power;
    gdouble min_voltage;
    gdouble max_voltage;
    gdouble min_current;
    gdouble max_current;
    
    /* Device state */
    gboolean is_connected;
    gboolean is_streaming;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorMainWindow, usb_tester_monitor_main_window, ADW_TYPE_APPLICATION_WINDOW)

static void
on_connect_button_toggled (GtkToggleButton *button,
                           gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);
    
    if (gtk_toggle_button_get_active (button)) {
        /* Connect to first available device for now */
        GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);
        
        if (g_list_model_get_n_items (devices) > 0) {
            UsbTesterMonitorDevice *device = g_list_model_get_item (devices, 0);
            usb_tester_monitor_device_manager_connect_device (self->device_manager,
                                                             device->identifier);
            
            gtk_label_set_text (self->device_label, device->identifier);
            usb_tester_monitor_data_display_set_device_status (self->data_display,
                                                                    "Connected");
            
            self->is_connected = TRUE;
        }
    } else {
        /* Disconnect */
        GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);
        
        if (g_list_model_get_n_items (devices) > 0) {
            UsbTesterMonitorDevice *device = g_list_model_get_item (devices, 0);
            usb_tester_monitor_device_manager_disconnect_device (self->device_manager,
                                                                 device->identifier);
        }
        
        self->is_connected = FALSE;
        
        /* Reset display */
        usb_tester_monitor_data_display_set_voltage (self->data_display, 0.0);
        usb_tester_monitor_data_display_set_current (self->data_display, 0.0);
        usb_tester_monitor_data_display_set_power (self->data_display, 0.0);
        usb_tester_monitor_data_display_set_device_status (self->data_display,
                                                            "Disconnected");
        
        /* Clear chart */
        usb_tester_monitor_chart_view_clear (self->chart_view);
        
        /* Reset min/max */
        self->min_voltage = 0;
        self->max_voltage = 0;
        self->min_current = 0;
        self->max_current = 0;
        
        gtk_label_set_text (self->device_label, "No device");
    }
}

static void
setup_header_bar (UsbTesterMonitorMainWindow *self)
{
    /* Create header bar */
    self->header_bar = GTK_HEADER_BAR (gtk_header_bar_new ());
    gtk_window_set_titlebar (GTK_WINDOW (self), GTK_WIDGET (self->header_bar));
    
    /* Title */
    GtkLabel *title_label = GTK_LABEL (gtk_label_new ("USB Tester Monitor"));
    gtk_header_bar_set_title_widget (self->header_bar, GTK_WIDGET (title_label));
    gtk_header_bar_set_show_title_buttons (self->header_bar, TRUE);
    
    /* Device selector and connect button */
    self->header_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
    
    self->device_label = GTK_LABEL (gtk_label_new ("No device"));
    gtk_box_append (self->header_box, GTK_WIDGET (self->device_label));
    
    self->connect_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
    gtk_button_set_label (GTK_BUTTON (self->connect_button), "Connect");
    gtk_box_append (self->header_box, GTK_WIDGET (self->connect_button));
    
    g_signal_connect (self->connect_button, "toggled",
                      G_CALLBACK (on_connect_button_toggled), self);
    
    gtk_header_bar_pack_end (self->header_bar, GTK_WIDGET (self->header_box));
}

static void
setup_main_content (UsbTesterMonitorMainWindow *self)
{
    /* Main box */
    self->main_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    gtk_window_set_child (GTK_WINDOW (self), GTK_WIDGET (self->main_box));
    
    /* Create data display */
    self->data_display = usb_tester_monitor_data_display_new ();
    gtk_box_append (self->main_box, GTK_WIDGET (self->data_display));
    
    /* Create chart view */
    self->chart_view = usb_tester_monitor_chart_view_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (self->chart_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (self->chart_view), TRUE);
    gtk_box_append (self->main_box, GTK_WIDGET (self->chart_view));
}

static void
usb_tester_monitor_main_window_dispose (GObject *object)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (object);
    
    if (self->device_manager) {
        g_object_unref (self->device_manager);
        self->device_manager = NULL;
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_main_window_parent_class)->dispose (object);
}

static void
usb_tester_monitor_main_window_class_init (UsbTesterMonitorMainWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->dispose = usb_tester_monitor_main_window_dispose;
}

static void
usb_tester_monitor_main_window_init (UsbTesterMonitorMainWindow *self)
{
    /* Initialize values */
    self->current_voltage = 0.0;
    self->current_current = 0.0;
    self->current_power = 0.0;
    self->min_voltage = 0.0;
    self->max_voltage = 0.0;
    self->min_current = 0.0;
    self->max_current = 0.0;
    self->is_connected = FALSE;
    self->is_streaming = FALSE;
    
    /* Set up window properties */
    gtk_window_set_default_size (GTK_WINDOW (self), 800, 600);
    gtk_window_set_title (GTK_WINDOW (self), "USB Tester Monitor");
    
    /* Create device manager */
    self->device_manager = usb_tester_monitor_device_manager_new ();
    usb_tester_monitor_device_manager_scan_devices (self->device_manager);
    
    /* Set up UI */
    setup_header_bar (self);
    setup_main_content (self);
    
    /* Update display with initial values */
    usb_tester_monitor_data_display_set_voltage (self->data_display, 0.0);
    usb_tester_monitor_data_display_set_current (self->data_display, 0.0);
    usb_tester_monitor_data_display_set_power (self->data_display, 0.0);
    usb_tester_monitor_data_display_set_device_status (self->data_display, "Disconnected");
}

UsbTesterMonitorMainWindow *
usb_tester_monitor_main_window_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_MAIN_WINDOW, NULL);
}
