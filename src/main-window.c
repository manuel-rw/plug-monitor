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
    AdwLeaflet *leaflet;
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
    
    /* Data callback */
    guint data_handler_id;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorMainWindow, usb_tester_monitor_main_window, ADW_TYPE_APPLICATION_WINDOW)

static void
on_data_received (UsbTesterMonitorDeviceProtocol *protocol,
                  UsbTesterMonitorReading *reading,
                  gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);
    
    /* Update current readings */
    self->current_voltage = reading->voltage;
    self->current_current = reading->current;
    self->current_power = reading->power;
    
    /* Update min/max */
    if (self->min_voltage == 0 || reading->voltage < self->min_voltage) {
        self->min_voltage = reading->voltage;
    }
    if (reading->voltage > self->max_voltage) {
        self->max_voltage = reading->voltage;
    }
    
    if (self->min_current == 0 || reading->current < self->min_current) {
        self->min_current = reading->current;
    }
    if (reading->current > self->max_current) {
        self->max_current = reading->current;
    }
    
    /* Update display */
    usb_tester_monitor_data_display_set_voltage (self->data_display, reading->voltage);
    usb_tester_monitor_data_display_set_current (self->data_display, reading->current);
    usb_tester_monitor_data_display_set_power (self->data_display, reading->power);
    
    usb_tester_monitor_data_display_set_min_max_voltage (self->data_display,
                                                          self->min_voltage,
                                                          self->max_voltage);
    usb_tester_monitor_data_display_set_min_max_current (self->data_display,
                                                          self->min_current,
                                                          self->max_current);
    
    /* Update chart */
    usb_tester_monitor_chart_view_add_voltage_reading (self->chart_view, reading->voltage);
    usb_tester_monitor_chart_view_add_current_reading (self->chart_view, reading->current);
    
    /* Update status */
    gchar *status = g_strdup_printf ("Connected | %.3f V | %.3f A | %.3f W",
                                     reading->voltage, reading->current, reading->power);
    usb_tester_monitor_data_display_set_device_status (self->data_display, status);
    g_free (status);
}

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
            
            /* Get the active protocol */
            UsbTesterMonitorDeviceProtocol *protocol = 
                usb_tester_monitor_device_manager_get_active_protocol (self->device_manager);
            
            if (protocol) {
                /* Set up data callback */
                usb_tester_monitor_device_protocol_set_data_callback (
                    protocol, on_data_received, self, NULL
                );
                
                /* Start streaming */
                usb_tester_monitor_device_protocol_start_streaming (protocol);
                
                self->is_connected = TRUE;
                self->is_streaming = TRUE;
                
                /* Update UI */
                gtk_label_set_text (self->device_label, device->identifier);
                usb_tester_monitor_data_display_set_device_status (self->data_display,
                                                                    "Connected");
            }
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
        self->is_streaming = FALSE;
        
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
    gtk_header_bar_set_title_widget (self->header_bar, gtk_label_new ("USB Tester Monitor"));
    gtk_header_bar_set_show_title_buttons (self->header_bar, TRUE);
    
    /* Device selector and connect button */
    GtkBox *header_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
    
    self->device_label = GTK_LABEL (gtk_label_new ("No device"));
    gtk_box_append (header_box, GTK_WIDGET (self->device_label));
    
    self->connect_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
    gtk_button_set_label (GTK_BUTTON (self->connect_button), "Connect");
    gtk_box_append (header_box, GTK_WIDGET (self->connect_button));
    
    g_signal_connect (self->connect_button, "toggled",
                      G_CALLBACK (on_connect_button_toggled), self);
    
    gtk_header_bar_pack_end (self->header_bar, GTK_WIDGET (header_box));
}

static void
setup_main_content (UsbTesterMonitorMainWindow *self)
{
    /* Main box */
    self->main_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    gtk_window_set_child (GTK_WINDOW (self), GTK_WIDGET (self->main_box));
    
    /* Leaflet for different views */
    self->leaflet = ADW_LEAFLET (adw_leaflet_new ());
    gtk_box_append (self->main_box, GTK_WIDGET (self->leaflet));
    
    /* Create data display */
    self->data_display = usb_tester_monitor_data_display_new ();
    
    /* Create chart view */
    self->chart_view = usb_tester_monitor_chart_view_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (self->chart_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (self->chart_view), TRUE);
    
    /* Add to leaflet */
    adw_leaflet_append (self->leaflet, GTK_WIDGET (self->data_display));
    adw_leaflet_append (self->leaflet, GTK_WIDGET (self->chart_view));
    
    /* Set initial page */
    adw_leaflet_set_visible_child (self->leaflet, GTK_WIDGET (self->data_display));
}

static void
usb_tester_monitor_main_window_dispose (GObject *object)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (object);
    
    /* Disconnect device if connected */
    if (self->is_connected) {
        GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);
        if (g_list_model_get_n_items (devices) > 0) {
            UsbTesterMonitorDevice *device = g_list_model_get_item (devices, 0);
            usb_tester_monitor_device_manager_disconnect_device (self->device_manager,
                                                                 device->identifier);
        }
    }
    
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
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    
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
