#include "main-window.h"
#include "application.h"
#include "device-manager.h"
#include "device-protocol.h"
#include "data-display.h"
#include "chart-view.h"
#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib.h>

/* Number of recent voltage samples used for the ripple (peak-to-peak) window */
#define RIPPLE_WINDOW_SAMPLES 64

/* Status dot colors in the device panel */
#define DOT_CONNECTED "#35b042"
#define DOT_DISCONNECTED "#9a9996"

typedef struct {
    UsbTesterMonitorDevice *device;    /* strong reference */

    /* Main area page for this device */
    GtkWidget *page;
    UsbTesterMonitorDataDisplay *data_display;
    UsbTesterMonitorChartView *chart_view;

    /* Sidebar row */
    GtkListBoxRow *row;
    GtkLabel *row_dot;

    /* Statistics */
    gdouble min_voltage;
    gdouble max_voltage;
    gdouble min_current;
    gdouble max_current;
    gboolean has_data;

    /* Recent voltages for ripple calculation */
    GArray *ripple_window;
} DevicePage;

struct _UsbTesterMonitorMainWindow {
    AdwApplicationWindow parent_instance;

    /* UI widgets */
    GtkLabel *title_label;
    GtkToggleButton *panel_button;
    GtkButton *add_button;
    GtkToggleButton *connect_button;
    GtkPopover *add_popover;
    GtkBox *add_popover_box;

    /* Left-side device panel */
    GtkRevealer *device_panel;
    GtkListBox *device_list;

    /* One page per device */
    GtkStack *device_stack;

    /* Device management */
    UsbTesterMonitorDeviceManager *device_manager;

    /* identifier -> DevicePage */
    GHashTable *pages;

    /* Currently selected device identifier (NULL when none) */
    gchar *active_identifier;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorMainWindow, usb_tester_monitor_main_window, ADW_TYPE_APPLICATION_WINDOW)

static void update_selection_ui (UsbTesterMonitorMainWindow *self);

static void
page_free (gpointer data)
{
    DevicePage *page = data;

    g_object_unref (page->device);
    g_array_unref (page->ripple_window);
    g_free (page);
}

static DevicePage *
find_page (UsbTesterMonitorMainWindow *self,
           const gchar *identifier)
{
    return g_hash_table_lookup (self->pages, identifier);
}

static void
row_dot_set_connected (GtkLabel *dot,
                       gboolean connected)
{
    const gchar *color = connected ? DOT_CONNECTED : DOT_DISCONNECTED;
    gchar *markup = g_strdup_printf ("<span foreground='%s'>\xe2\x97\x8f</span>", color);
    gtk_label_set_markup (dot, markup);
    g_free (markup);
}

static void
on_reading (const gchar *identifier,
            gdouble voltage,
            gdouble current,
            gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    DevicePage *page = find_page (self, identifier);
    if (page == NULL) {
        return;
    }

    gdouble power = voltage * current;

    /* Track min/max */
    if (!page->has_data) {
        page->min_voltage = voltage;
        page->max_voltage = voltage;
        page->min_current = current;
        page->max_current = current;
        page->has_data = TRUE;
    } else {
        if (voltage < page->min_voltage) page->min_voltage = voltage;
        if (voltage > page->max_voltage) page->max_voltage = voltage;
        if (current < page->min_current) page->min_current = current;
        if (current > page->max_current) page->max_current = current;
    }

    usb_tester_monitor_data_display_set_voltage (page->data_display, voltage);
    usb_tester_monitor_data_display_set_current (page->data_display, current);
    usb_tester_monitor_data_display_set_power (page->data_display, power);
    usb_tester_monitor_data_display_set_min_max_voltage (page->data_display,
                                                         page->min_voltage,
                                                         page->max_voltage);
    usb_tester_monitor_data_display_set_min_max_current (page->data_display,
                                                         page->min_current,
                                                         page->max_current);

    /* Ripple: peak-to-peak voltage across the recent window */
    g_array_append_vals (page->ripple_window, &voltage, 1);
    while (page->ripple_window->len > RIPPLE_WINDOW_SAMPLES) {
        g_array_remove_index (page->ripple_window, 0);
    }

    gdouble lo = G_MAXDOUBLE;
    gdouble hi = -G_MAXDOUBLE;
    for (guint i = 0; i < page->ripple_window->len; i++) {
        gdouble value = g_array_index (page->ripple_window, gdouble, i);
        if (value < lo) lo = value;
        if (value > hi) hi = value;
    }
    usb_tester_monitor_data_display_set_ripple (page->data_display, hi - lo);

    usb_tester_monitor_chart_view_add_reading (page->chart_view, voltage, current);
}

static void
select_page (UsbTesterMonitorMainWindow *self,
             DevicePage *page)
{
    gtk_stack_set_visible_child (self->device_stack, page->page);
    gtk_list_box_select_row (self->device_list, page->row);

    g_free (self->active_identifier);
    self->active_identifier = g_strdup (page->device->identifier);

    update_selection_ui (self);
}

static void
on_device_row_selected (GtkListBox *box,
                        GtkListBoxRow *row,
                        gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    if (row == NULL) {
        return;
    }

    DevicePage *page = g_object_get_data (G_OBJECT (row), "page");
    if (page != NULL) {
        select_page (self, page);
    }
}

static void
on_connect_button_toggled (GtkToggleButton *button,
                           gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    DevicePage *page = self->active_identifier != NULL
        ? find_page (self, self->active_identifier)
        : NULL;

    if (page == NULL) {
        /* Nothing selected: release the toggle again */
        g_signal_handlers_block_by_func (button, on_connect_button_toggled, self);
        gtk_toggle_button_set_active (button, FALSE);
        g_signal_handlers_unblock_by_func (button, on_connect_button_toggled, self);
        return;
    }

    const gchar *identifier = page->device->identifier;

    if (gtk_toggle_button_get_active (button)) {
        gboolean connected =
            usb_tester_monitor_device_manager_connect_device (self->device_manager,
                                                               identifier);

        if (connected) {
            row_dot_set_connected (page->row_dot, TRUE);
            usb_tester_monitor_data_display_set_device_status (page->data_display,
                                                               "Connected");
        } else {
            usb_tester_monitor_data_display_set_device_status (page->data_display,
                                                               "Connection failed");
            g_signal_handlers_block_by_func (button, on_connect_button_toggled, self);
            gtk_toggle_button_set_active (button, FALSE);
            g_signal_handlers_unblock_by_func (button, on_connect_button_toggled, self);
        }
    } else {
        usb_tester_monitor_device_manager_disconnect_device (self->device_manager,
                                                             identifier);
        row_dot_set_connected (page->row_dot, FALSE);

        /* Reset the display and chart of this device */
        usb_tester_monitor_data_display_set_voltage (page->data_display, 0.0);
        usb_tester_monitor_data_display_set_current (page->data_display, 0.0);
        usb_tester_monitor_data_display_set_power (page->data_display, 0.0);
        usb_tester_monitor_data_display_set_device_status (page->data_display,
                                                           "Disconnected");
        usb_tester_monitor_data_display_set_ripple (page->data_display, 0.0);
        usb_tester_monitor_chart_view_clear (page->chart_view);

        page->has_data = FALSE;
        page->min_voltage = 0.0;
        page->max_voltage = 0.0;
        page->min_current = 0.0;
        page->max_current = 0.0;
        g_array_set_size (page->ripple_window, 0);
    }
}

static void
update_selection_ui (UsbTesterMonitorMainWindow *self)
{
    DevicePage *page = self->active_identifier != NULL
        ? find_page (self, self->active_identifier)
        : NULL;

    if (page != NULL) {
        gtk_label_set_text (self->title_label, page->device->name);
        gtk_window_set_title (GTK_WINDOW (self), page->device->name);

        gboolean connected =
            usb_tester_monitor_device_manager_is_connected (self->device_manager,
                                                            page->device->identifier);

        g_signal_handlers_block_by_func (self->connect_button,
                                         on_connect_button_toggled, self);
        gtk_toggle_button_set_active (self->connect_button, connected);
        g_signal_handlers_unblock_by_func (self->connect_button,
                                           on_connect_button_toggled, self);
    } else {
        gtk_label_set_text (self->title_label, "USB Tester Monitor");
        gtk_window_set_title (GTK_WINDOW (self), "USB Tester Monitor");

        g_signal_handlers_block_by_func (self->connect_button,
                                         on_connect_button_toggled, self);
        gtk_toggle_button_set_active (self->connect_button, FALSE);
        g_signal_handlers_unblock_by_func (self->connect_button,
                                           on_connect_button_toggled, self);
    }
}

static void
add_device (UsbTesterMonitorMainWindow *self,
            UsbTesterMonitorDevice *device)
{
    /* Takes ownership of the device reference it is given */
    DevicePage *page = g_new0 (DevicePage, 1);
    page->device = device;
    page->ripple_window = g_array_new (FALSE, FALSE, sizeof (gdouble));

    /* Page in the main stack */
    GtkBox *page_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

    page->data_display = usb_tester_monitor_data_display_new ();
    gtk_box_append (page_box, GTK_WIDGET (page->data_display));

    page->chart_view = usb_tester_monitor_chart_view_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (page->chart_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (page->chart_view), TRUE);
    gtk_box_append (page_box, GTK_WIDGET (page->chart_view));

    page->page = GTK_WIDGET (page_box);
    gtk_stack_add_child (self->device_stack, page->page);

    /* Row in the device panel */
    GtkBox *row_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8));
    gtk_widget_set_margin_start (GTK_WIDGET (row_box), 8);
    gtk_widget_set_margin_end (GTK_WIDGET (row_box), 8);
    gtk_widget_set_margin_top (GTK_WIDGET (row_box), 6);
    gtk_widget_set_margin_bottom (GTK_WIDGET (row_box), 6);

    page->row_dot = GTK_LABEL (gtk_label_new (""));
    row_dot_set_connected (page->row_dot, FALSE);
    gtk_box_append (row_box, GTK_WIDGET (page->row_dot));

    GtkBox *name_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 2));
    GtkLabel *name_label = GTK_LABEL (gtk_label_new (device->name));
    gtk_label_set_xalign (name_label, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (name_label), "heading");
    gtk_box_append (name_box, GTK_WIDGET (name_label));

    GtkLabel *path_label = GTK_LABEL (gtk_label_new (device->identifier));
    gtk_label_set_xalign (path_label, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (path_label), "caption");
    gtk_widget_add_css_class (GTK_WIDGET (path_label), "dim-label");
    gtk_box_append (name_box, GTK_WIDGET (path_label));

    gtk_box_append (row_box, GTK_WIDGET (name_box));

    page->row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
    gtk_list_box_row_set_child (page->row, GTK_WIDGET (row_box));
    g_object_set_data (G_OBJECT (page->row), "page", page);

    gtk_list_box_append (self->device_list, GTK_WIDGET (page->row));

    g_hash_table_insert (self->pages, g_strdup (device->identifier), page);

    select_page (self, page);
}

static void
on_popover_device_activated (GtkButton *button,
                             gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    const gchar *identifier = g_object_get_data (G_OBJECT (button), "identifier");

    usb_tester_monitor_device_manager_scan_devices (self->device_manager);
    GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);

    for (guint i = 0; i < g_list_model_get_n_items (devices); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (devices, i);
        if (g_strcmp0 (device->identifier, identifier) == 0) {
            gtk_popover_popdown (self->add_popover);

            if (find_page (self, identifier) == NULL) {
                add_device (self, device);
            } else {
                g_object_unref (device);
            }
            return;
        }
        g_object_unref (device);
    }

    gtk_popover_popdown (self->add_popover);
}

static void
rebuild_add_popover (UsbTesterMonitorMainWindow *self)
{
    /* Remove previous entries */
    GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->add_popover_box));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling (child);
        gtk_box_remove (self->add_popover_box, child);
        child = next;
    }

    usb_tester_monitor_device_manager_scan_devices (self->device_manager);
    GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);

    guint added = 0;
    for (guint i = 0; i < g_list_model_get_n_items (devices); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (devices, i);

        if (find_page (self, device->identifier) != NULL) {
            g_object_unref (device);
            continue;
        }

        GtkButton *entry = GTK_BUTTON (gtk_button_new ());
        gtk_widget_add_css_class (GTK_WIDGET (entry), "flat");

        GtkBox *entry_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8));
        GtkLabel *name = GTK_LABEL (gtk_label_new (device->name));
        gtk_label_set_xalign (name, 0.0);
        gtk_box_append (entry_box, GTK_WIDGET (name));

        GtkLabel *path = GTK_LABEL (gtk_label_new (device->identifier));
        gtk_widget_add_css_class (GTK_WIDGET (path), "caption");
        gtk_widget_add_css_class (GTK_WIDGET (path), "dim-label");
        gtk_box_append (entry_box, GTK_WIDGET (path));

        gtk_button_set_child (entry, GTK_WIDGET (entry_box));
        g_object_set_data_full (G_OBJECT (entry), "identifier",
                                g_strdup (device->identifier), g_free);
        g_signal_connect (entry, "clicked",
                          G_CALLBACK (on_popover_device_activated), self);

        gtk_box_append (self->add_popover_box, GTK_WIDGET (entry));
        added++;
        g_object_unref (device);
    }

    if (added == 0) {
        GtkLabel *empty = GTK_LABEL (gtk_label_new ("No new devices found"));
        gtk_widget_set_margin_start (GTK_WIDGET (empty), 12);
        gtk_widget_set_margin_end (GTK_WIDGET (empty), 12);
        gtk_widget_set_margin_top (GTK_WIDGET (empty), 12);
        gtk_widget_set_margin_bottom (GTK_WIDGET (empty), 12);
        gtk_box_append (self->add_popover_box, GTK_WIDGET (empty));
    }
}

static void
on_add_popover_map (GtkWidget *popover,
                    gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);
    (void) popover;

    rebuild_add_popover (self);
}

static void
on_panel_button_toggled (GtkToggleButton *button,
                         gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    gtk_revealer_set_reveal_child (self->device_panel,
                                   gtk_toggle_button_get_active (button));
}

static void
setup_header_bar (UsbTesterMonitorMainWindow *self,
                  AdwToolbarView *toolbar_view)
{
    GtkHeaderBar *header_bar = GTK_HEADER_BAR (gtk_header_bar_new ());
    gtk_header_bar_set_show_title_buttons (header_bar, TRUE);

    /* Device name as window title */
    self->title_label = GTK_LABEL (gtk_label_new ("USB Tester Monitor"));
    gtk_widget_add_css_class (GTK_WIDGET (self->title_label), "heading");
    gtk_header_bar_set_title_widget (header_bar, GTK_WIDGET (self->title_label));

    /* "+" button (top left) to add devices */
    self->add_button = GTK_BUTTON (gtk_menu_button_new ());
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->add_button), "list-add-symbolic");
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->add_button), "Add device");
    gtk_header_bar_pack_start (header_bar, GTK_WIDGET (self->add_button));

    /* Button toggling the device panel on the left side */
    self->panel_button =
        GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
    GtkWidget *panel_icon = gtk_image_new_from_icon_name ("view-list-symbolic");
    gtk_button_set_child (GTK_BUTTON (self->panel_button), panel_icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->panel_button), "Devices");
    g_signal_connect (self->panel_button, "toggled",
                      G_CALLBACK (on_panel_button_toggled), self);
    gtk_header_bar_pack_start (header_bar, GTK_WIDGET (self->panel_button));

    /* Connect button (top right) */
    self->connect_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
    gtk_button_set_label (GTK_BUTTON (self->connect_button), "Connect");
    g_signal_connect (self->connect_button, "toggled",
                      G_CALLBACK (on_connect_button_toggled), self);
    gtk_header_bar_pack_end (header_bar, GTK_WIDGET (self->connect_button));

    adw_toolbar_view_add_top_bar (toolbar_view, GTK_WIDGET (header_bar));
}

static void
setup_main_content (UsbTesterMonitorMainWindow *self,
                    AdwToolbarView *toolbar_view)
{
    /* One page per device */
    self->device_stack = GTK_STACK (gtk_stack_new ());
    gtk_stack_set_transition_type (self->device_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    GtkLabel *placeholder = GTK_LABEL (gtk_label_new ("No device selected.\n"
                                                      "Use the + button to add a device."));
    gtk_widget_add_css_class (GTK_WIDGET (placeholder), "dim-label");
    gtk_stack_add_named (self->device_stack, GTK_WIDGET (placeholder), "empty");
    gtk_stack_set_visible_child_name (self->device_stack, "empty");

    /* Left-side device panel shown as an overlay popup */
    GtkOverlay *overlay = GTK_OVERLAY (gtk_overlay_new ());
    gtk_overlay_set_child (overlay, GTK_WIDGET (self->device_stack));

    GtkBox *panel_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_add_css_class (GTK_WIDGET (panel_box), "device-panel");
    gtk_widget_set_margin_top (GTK_WIDGET (panel_box), 6);
    gtk_widget_set_margin_bottom (GTK_WIDGET (panel_box), 6);
    gtk_widget_set_margin_start (GTK_WIDGET (panel_box), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (panel_box), 6);
    gtk_widget_set_size_request (GTK_WIDGET (panel_box), 260, -1);

    GtkLabel *panel_title = GTK_LABEL (gtk_label_new ("Devices"));
    gtk_label_set_xalign (panel_title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (panel_title), "heading");
    gtk_widget_set_margin_start (GTK_WIDGET (panel_title), 12);
    gtk_widget_set_margin_end (GTK_WIDGET (panel_title), 12);
    gtk_widget_set_margin_top (GTK_WIDGET (panel_title), 6);
    gtk_box_append (panel_box, GTK_WIDGET (panel_title));

    self->device_list = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_selection_mode (self->device_list, GTK_SELECTION_BROWSE);
    g_signal_connect (self->device_list, "row-selected",
                      G_CALLBACK (on_device_row_selected), self);

    GtkScrolledWindow *scrolled =
        GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
    gtk_scrolled_window_set_policy (scrolled,
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child (scrolled, GTK_WIDGET (self->device_list));
    gtk_widget_set_vexpand (GTK_WIDGET (scrolled), TRUE);
    gtk_box_append (panel_box, GTK_WIDGET (scrolled));

    self->device_panel = GTK_REVEALER (gtk_revealer_new ());
    gtk_revealer_set_transition_type (self->device_panel,
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_reveal_child (self->device_panel, FALSE);
    gtk_revealer_set_child (self->device_panel, GTK_WIDGET (panel_box));
    gtk_widget_set_halign (GTK_WIDGET (self->device_panel), GTK_ALIGN_START);
    gtk_widget_set_valign (GTK_WIDGET (self->device_panel), GTK_ALIGN_FILL);

    gtk_overlay_add_overlay (overlay, GTK_WIDGET (self->device_panel));

    adw_toolbar_view_set_content (toolbar_view, GTK_WIDGET (overlay));
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (self),
                                        GTK_WIDGET (toolbar_view));
}

static void
usb_tester_monitor_main_window_dispose (GObject *object)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (object);

    g_clear_pointer (&self->pages, g_hash_table_unref);
    g_clear_pointer (&self->active_identifier, g_free);

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
    self->pages = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, page_free);

    gtk_window_set_default_size (GTK_WINDOW (self), 900, 650);
    gtk_window_set_title (GTK_WINDOW (self), "USB Tester Monitor");

    /* Create device manager */
    self->device_manager = usb_tester_monitor_device_manager_new ();
    usb_tester_monitor_device_manager_scan_devices (self->device_manager);
    usb_tester_monitor_device_manager_set_reading_callback (self->device_manager,
                                                            on_reading,
                                                            self);

    /* Set up UI */
    AdwToolbarView *toolbar_view = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
    setup_header_bar (self, toolbar_view);
    setup_main_content (self, toolbar_view);

    /* Add-device popover */
    self->add_popover = GTK_POPOVER (gtk_popover_new ());
    self->add_popover_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_margin_top (GTK_WIDGET (self->add_popover_box), 6);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self->add_popover_box), 6);
    gtk_popover_set_child (self->add_popover, GTK_WIDGET (self->add_popover_box));
    g_signal_connect (self->add_popover, "map",
                      G_CALLBACK (on_add_popover_map), self);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->add_button),
                                 GTK_WIDGET (self->add_popover));

    /* Add every device that is already connected to the system */
    GListModel *devices = usb_tester_monitor_device_manager_get_devices (self->device_manager);
    for (guint i = 0; i < g_list_model_get_n_items (devices); i++) {
        UsbTesterMonitorDevice *device = g_list_model_get_item (devices, i);
        add_device (self, device);
    }

    /* Show the device panel initially */
    gtk_revealer_set_reveal_child (self->device_panel, TRUE);
    gtk_toggle_button_set_active (self->panel_button, TRUE);
}

UsbTesterMonitorMainWindow *
usb_tester_monitor_main_window_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_MAIN_WINDOW, NULL);
}
