#include "main-window.h"
#include "application.h"
#include "device-manager.h"
#include "device-protocol.h"
#include "device-permissions.h"
#include "data-display.h"
#include "stats-card.h"
#include "chart-view.h"
#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <math.h>

/* Number of recent voltage samples used for the ripple (peak-to-peak) window */
#define RIPPLE_WINDOW_SAMPLES 64

/* Status dot colors in the device panel */
#define DOT_CONNECTED "#35b042"
#define DOT_DISCONNECTED "#9a9996"

/* Countdown before a recording starts */
#define RECORD_COUNTDOWN_SECONDS 3

/* Recording channels */
enum {
    REC_VOLTAGE     = 1 << 0,
    REC_CURRENT     = 1 << 1,
    REC_POWER       = 1 << 2,
    REC_TEMPERATURE = 1 << 3,
    REC_BATTERY     = 1 << 4,
};

typedef struct {
    guint64 timestamp_us;
    gdouble voltage;
    gdouble current;
    gdouble power;
    gdouble temperature;
    gdouble battery;
} RecordSample;

typedef struct {
    UsbTesterMonitorDevice *device;    /* strong reference */

    /* Main area page for this device */
    GtkWidget *page;
    UsbTesterMonitorDataDisplay *data_display;
    UsbTesterMonitorChartView *chart_view;
    UsbTesterMonitorStatsCard *stats_card;

    /* Sidebar row */
    GtkListBoxRow *row;
    GtkLabel *row_dot;
    GtkBox *row_name_box;   /* caption lines live here */
    GtkLabel *row_battery;  /* created lazily when the device reports a battery */

    /* Statistics */
    gdouble min_voltage;
    gdouble max_voltage;
    gdouble min_current;
    gdouble max_current;
    gboolean has_data;

    /* Recent voltages for ripple calculation */
    GArray *ripple_window;

    /* Extra channels (NAN when unavailable) */
    gdouble temperature;
    gdouble battery;
} DevicePage;

struct _UsbTesterMonitorMainWindow {
    AdwApplicationWindow parent_instance;

    /* UI widgets */
    GtkLabel *title_label;
    GtkToggleButton *panel_button;
    GtkMenuButton *add_button;
    GtkToggleButton *connect_button;
    GtkLabel *temp_dot;
    GtkLabel *temp_label;
    GtkButton *record_button;
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

    /* Recording state */
    gchar *recording_identifier;   /* NULL when idle */
    GArray *recording_samples;      /* RecordSample */
    guint recording_channels;
    guint recording_duration_s;
    guint countdown_source;
    guint autostop_source;
    gint countdown_remaining;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorMainWindow, usb_tester_monitor_main_window, ADW_TYPE_APPLICATION_WINDOW)

static void update_selection_ui (UsbTesterMonitorMainWindow *self);
static void recording_set_idle (UsbTesterMonitorMainWindow *self);

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

static DevicePage *
active_page (UsbTesterMonitorMainWindow *self)
{
    return self->active_identifier != NULL
        ? find_page (self, self->active_identifier)
        : NULL;
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
show_error_dialog (UsbTesterMonitorMainWindow *self,
                   const gchar *message)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new ("%s", message);
    gtk_alert_dialog_show (dialog, GTK_WINDOW (self));
    g_object_unref (dialog);
}

static void
connect_button_set_connected (UsbTesterMonitorMainWindow *self,
                              gboolean connected)
{
    gtk_button_set_label (GTK_BUTTON (self->connect_button),
                          connected ? "Disconnect" : "Connect");
}

static void
temp_label_set_value (UsbTesterMonitorMainWindow *self,
                      gdouble temperature)
{
    /* Status colors: green below 35 degrees, yellow below 50, red above */
    const gchar *dot_color = "#9a9996"; /* gray: no temperature available */

    if (isnan (temperature)) {
        gtk_label_set_text (self->temp_label, "\u2014 \u00b0C");
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->temp_label),
                                     "Temperature (not available on this device)");
    } else {
        if (temperature < 35.0) {
            dot_color = "#35b042";
        } else if (temperature < 50.0) {
            dot_color = "#f5c211";
        } else {
            dot_color = "#ef2929";
        }

        gchar *text = g_strdup_printf ("%.1f \u00b0C", temperature);
        gtk_label_set_text (self->temp_label, text);
        g_free (text);
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->temp_label),
                                     "Temperature of the selected device");
    }

    gchar *dot = g_strdup_printf ("<span foreground='%s'>\xe2\x97\x8f</span>", dot_color);
    gtk_label_set_markup (self->temp_dot, dot);
    g_free (dot);
}

/* ---------------------------------------------------------------- Recording */

static void
recording_set_idle (UsbTesterMonitorMainWindow *self)
{
    g_clear_pointer (&self->recording_identifier, g_free);

    if (self->recording_samples != NULL) {
        g_array_unref (self->recording_samples);
        self->recording_samples = NULL;
    }

    if (self->countdown_source > 0) {
        g_source_remove (self->countdown_source);
        self->countdown_source = 0;
    }
    if (self->autostop_source > 0) {
        g_source_remove (self->autostop_source);
        self->autostop_source = 0;
    }

    gtk_button_set_label (GTK_BUTTON (self->record_button), "Record");
    gtk_widget_remove_css_class (GTK_WIDGET (self->record_button), "destructive-action");
}

static void recording_stop_and_save (UsbTesterMonitorMainWindow *self);

static gboolean
on_autostop (gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    self->autostop_source = 0;
    recording_stop_and_save (self);
    return G_SOURCE_REMOVE;
}

static void
recording_begin (UsbTesterMonitorMainWindow *self)
{
    DevicePage *page = active_page (self);
    if (page == NULL) {
        recording_set_idle (self);
        return;
    }

    self->recording_identifier = g_strdup (page->device->identifier);
    self->recording_samples =
        g_array_new (FALSE, FALSE, sizeof (RecordSample));

    gtk_button_set_label (GTK_BUTTON (self->record_button), "Stop Recording");
    gtk_widget_add_css_class (GTK_WIDGET (self->record_button), "destructive-action");

    /* Auto-stop when the configured duration has elapsed */
    self->autostop_source =
        g_timeout_add_seconds (self->recording_duration_s, on_autostop, self);
}

static gboolean
on_countdown_tick (gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    self->countdown_remaining--;

    if (self->countdown_remaining > 0) {
        gchar *text = g_strdup_printf ("Cancel (%d)", self->countdown_remaining);
        gtk_button_set_label (GTK_BUTTON (self->record_button), text);
        g_free (text);
        return G_SOURCE_CONTINUE;
    }

    self->countdown_source = 0;
    recording_begin (self);
    return G_SOURCE_REMOVE;
}

static gboolean
write_recording_csv (UsbTesterMonitorMainWindow *self,
                     GFile *file,
                     GError **error)
{
    GString *csv = g_string_new ("timestamp_ms");

    if (self->recording_channels & REC_VOLTAGE) g_string_append (csv, ",voltage_V");
    if (self->recording_channels & REC_CURRENT) g_string_append (csv, ",current_A");
    if (self->recording_channels & REC_POWER) g_string_append (csv, ",power_W");
    if (self->recording_channels & REC_TEMPERATURE) g_string_append (csv, ",temperature_C");
    if (self->recording_channels & REC_BATTERY) g_string_append (csv, ",battery_pct");
    g_string_append (csv, "\n");

    for (guint i = 0; i < self->recording_samples->len; i++) {
        RecordSample s = g_array_index (self->recording_samples, RecordSample, i);

        g_string_append_printf (csv, "%" G_GUINT64_FORMAT, s.timestamp_us / 1000);

        if (self->recording_channels & REC_VOLTAGE) g_string_append_printf (csv, ",%.4f", s.voltage);
        if (self->recording_channels & REC_CURRENT) g_string_append_printf (csv, ",%.4f", s.current);
        if (self->recording_channels & REC_POWER) g_string_append_printf (csv, ",%.4f", s.power);
        if (self->recording_channels & REC_TEMPERATURE) g_string_append_printf (csv, ",%.2f", s.temperature);
        if (self->recording_channels & REC_BATTERY) g_string_append_printf (csv, ",%.1f", s.battery);
        g_string_append (csv, "\n");
    }

    gboolean ok = g_file_replace_contents (file, csv->str, csv->len, NULL, FALSE,
                                           G_FILE_CREATE_NONE, NULL, NULL, error);
    g_string_free (csv, TRUE);
    return ok;
}

static void
on_save_dialog_result (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);

    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish (dialog, result, &error);

    if (file != NULL) {
        GError *io_error = NULL;
        if (!write_recording_csv (self, file, &io_error)) {
            show_error_dialog (self, io_error ? io_error->message : "Failed to save recording");
            g_clear_error (&io_error);
        }
        g_object_unref (file);
    } else {
        /* User cancelled the save: the recording is discarded */
        g_clear_error (&error);
    }

    g_object_unref (dialog);
    recording_set_idle (self);
}

static void
recording_stop_and_save (UsbTesterMonitorMainWindow *self)
{
    if (self->autostop_source > 0) {
        g_source_remove (self->autostop_source);
        self->autostop_source = 0;
    }

    if (self->recording_samples == NULL || self->recording_samples->len == 0) {
        recording_set_idle (self);
        return;
    }

    /* Build a suggested file name before freezing the capture */
    GDateTime *now = g_date_time_new_now_local ();
    gchar *stamp = g_date_time_format (now, "%Y%m%d_%H%M%S");
    g_date_time_unref (now);

    DevicePage *page = find_page (self, self->recording_identifier);
    gchar *name;
    if (page != NULL) {
        gchar *device_name = g_strdup (page->device->name);
        for (gchar *c = device_name; *c != '\0'; c++) {
            if (*c == ' ') *c = '_';
        }
        name = g_strdup_printf ("%s_%s.csv", device_name, stamp);
        g_free (device_name);
    } else {
        name = g_strdup_printf ("recording_%s.csv", stamp);
    }
    g_free (stamp);

    /* Freeze the dataset: no further samples are appended */
    g_clear_pointer (&self->recording_identifier, g_free);

    /* Ask where to save the file */
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, "Save recording");
    gtk_file_dialog_set_initial_name (dialog, name);
    g_free (name);

    gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL,
                          on_save_dialog_result, self);
}

static void
on_record_dialog_response (AdwAlertDialog *dialog,
                           gchar *response,
                           gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    if (g_strcmp0 (response, "start") != 0) {
        return;
    }

    GtkCheckButton *check;
    GtkSpinButton *spin;
    guint channels = 0;

    check = g_object_get_data (G_OBJECT (dialog), "check-voltage");
    if (gtk_check_button_get_active (check)) channels |= REC_VOLTAGE;
    check = g_object_get_data (G_OBJECT (dialog), "check-current");
    if (gtk_check_button_get_active (check)) channels |= REC_CURRENT;
    check = g_object_get_data (G_OBJECT (dialog), "check-power");
    if (gtk_check_button_get_active (check)) channels |= REC_POWER;
    check = g_object_get_data (G_OBJECT (dialog), "check-temperature");
    if (gtk_check_button_get_active (check)) channels |= REC_TEMPERATURE;
    check = g_object_get_data (G_OBJECT (dialog), "check-battery");
    if (gtk_check_button_get_active (check)) channels |= REC_BATTERY;

    spin = g_object_get_data (G_OBJECT (dialog), "spin-duration");
    guint duration = (guint) gtk_spin_button_get_value_as_int (spin);

    if (channels == 0 || duration == 0) {
        return;
    }

    self->recording_channels = channels;
    self->recording_duration_s = duration;

    /* Countdown before the recording starts */
    self->countdown_remaining = RECORD_COUNTDOWN_SECONDS;
    gchar *text = g_strdup_printf ("Cancel (%d)", self->countdown_remaining);
    gtk_button_set_label (GTK_BUTTON (self->record_button), text);
    g_free (text);
    self->countdown_source = g_timeout_add_seconds (1, on_countdown_tick, self);
}

static void
show_record_dialog (UsbTesterMonitorMainWindow *self)
{
    AdwAlertDialog *dialog =
        ADW_ALERT_DIALOG (adw_alert_dialog_new ("Start recording",
                                                "Choose the channels and duration to record."));

    GtkBox *box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));
    gtk_widget_set_margin_top (GTK_WIDGET (box), 12);
    gtk_widget_set_margin_bottom (GTK_WIDGET (box), 12);
    gtk_widget_set_margin_start (GTK_WIDGET (box), 12);
    gtk_widget_set_margin_end (GTK_WIDGET (box), 12);

    GtkLabel *channels_title = GTK_LABEL (gtk_label_new ("Channels"));
    gtk_widget_add_css_class (GTK_WIDGET (channels_title), "h4");
    gtk_label_set_xalign (channels_title, 0.0);
    gtk_box_append (box, GTK_WIDGET (channels_title));

    struct {
        const gchar *id;
        const gchar *label;
        gboolean active;
        gboolean available;
    } channel_defs[] = {
        { "check-voltage",     "Voltage",     TRUE,  TRUE  },
        { "check-current",     "Current",     TRUE,  TRUE  },
        { "check-power",       "Power",       FALSE, TRUE  },
        { "check-temperature", "Temperature", FALSE, FALSE },
        { "check-battery",     "Battery",     FALSE, FALSE },
    };

    for (gsize i = 0; i < G_N_ELEMENTS (channel_defs); i++) {
        GtkCheckButton *check =
            GTK_CHECK_BUTTON (gtk_check_button_new_with_label (channel_defs[i].label));
        gtk_check_button_set_active (check, channel_defs[i].active);
        gtk_widget_set_halign (GTK_WIDGET (check), GTK_ALIGN_START);

        if (!channel_defs[i].available) {
            gtk_widget_set_sensitive (GTK_WIDGET (check), FALSE);
            gtk_widget_set_tooltip_text (GTK_WIDGET (check),
                                         "Not available on this device");
        }

        g_object_set_data (G_OBJECT (dialog), channel_defs[i].id, check);
        gtk_box_append (box, GTK_WIDGET (check));
    }

    GtkLabel *duration_title = GTK_LABEL (gtk_label_new ("Duration"));
    gtk_widget_add_css_class (GTK_WIDGET (duration_title), "h4");
    gtk_label_set_xalign (duration_title, 0.0);
    gtk_box_append (box, GTK_WIDGET (duration_title));

    GtkSpinButton *spin = GTK_SPIN_BUTTON (gtk_spin_button_new_with_range (1, 86400, 1));
    gtk_spin_button_set_value (spin, 60.0);
    gtk_widget_set_halign (GTK_WIDGET (spin), GTK_ALIGN_START);
    g_object_set_data (G_OBJECT (dialog), "spin-duration", spin);
    gtk_box_append (box, GTK_WIDGET (spin));

    adw_alert_dialog_set_extra_child (dialog, GTK_WIDGET (box));
    adw_alert_dialog_add_responses (dialog,
                                    "cancel", "_Cancel",
                                    "start", "_Start Recording",
                                    NULL);
    adw_alert_dialog_set_response_appearance (dialog, "start", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response (dialog, "start");

    g_signal_connect (dialog, "response", G_CALLBACK (on_record_dialog_response), self);
    adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
on_record_button_clicked (GtkButton *button,
                          gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);
    (void) button;

    if (self->countdown_source > 0) {
        /* Cancel the pending countdown */
        recording_set_idle (self);
        return;
    }

    if (self->recording_identifier != NULL) {
        /* Stop and ask where to save */
        recording_stop_and_save (self);
        return;
    }

    DevicePage *page = active_page (self);
    if (page == NULL ||
        !usb_tester_monitor_device_manager_is_connected (self->device_manager,
                                                          page->device->identifier)) {
        show_error_dialog (self, "Connect a device before recording");
        return;
    }

    show_record_dialog (self);
}

/* ------------------------------------------------------------------ Readings */

static void
on_reading (const gchar *identifier,
            gdouble voltage,
            gdouble current,
            gdouble temperature,
            gdouble battery,
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
    usb_tester_monitor_stats_card_set_min_max_voltage (page->stats_card,
                                                       page->min_voltage,
                                                       page->max_voltage);
    usb_tester_monitor_stats_card_set_min_max_current (page->stats_card,
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
    usb_tester_monitor_stats_card_set_ripple (page->stats_card, hi - lo);

    usb_tester_monitor_chart_view_add_reading (page->chart_view, voltage, current);

    /* Extra channels */
    page->temperature = temperature;
    page->battery = battery;

    if (!isnan (battery)) {
        if (page->row_battery == NULL) {
            page->row_battery = GTK_LABEL (gtk_label_new (""));
            gtk_label_set_xalign (page->row_battery, 0.0);
            gtk_widget_add_css_class (GTK_WIDGET (page->row_battery), "caption");
            gtk_widget_add_css_class (GTK_WIDGET (page->row_battery), "dim-label");
            gtk_box_append (page->row_name_box, GTK_WIDGET (page->row_battery));
        }
        gchar *text = g_strdup_printf ("Battery %.0f %%", battery);
        gtk_label_set_text (page->row_battery, text);
        g_free (text);
    }

    /* Temperature of the selected device is shown in the header */
    if (g_strcmp0 (identifier, self->active_identifier) == 0) {
        temp_label_set_value (self, temperature);
    }

    /* Capture samples while a recording is running for this device */
    if (self->recording_identifier != NULL &&
        g_strcmp0 (identifier, self->recording_identifier) == 0) {
        RecordSample sample = {
            .timestamp_us = g_get_real_time (),
            .voltage = voltage,
            .current = current,
            .power = power,
            .temperature = temperature,
            .battery = battery,
        };
        g_array_append_vals (self->recording_samples, &sample, 1);
    }
}

/* -------------------------------------------------------------- Selection UI */

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

/* ------------------------------------------------------ Device permissions */

static void on_connect_button_toggled (GtkToggleButton *button, gpointer user_data);

typedef struct {
    UsbTesterMonitorMainWindow *self; /* strong reference */
    gchar *identifier;
    gint grant_response;
} PermissionRequest;

static void
permission_request_free (PermissionRequest *request)
{
    g_object_unref (request->self);
    g_free (request->identifier);
    g_free (request);
}

static void
release_connect_toggle (UsbTesterMonitorMainWindow *self)
{
    g_signal_handlers_block_by_func (self->connect_button, on_connect_button_toggled, self);
    gtk_toggle_button_set_active (self->connect_button, FALSE);
    g_signal_handlers_unblock_by_func (self->connect_button, on_connect_button_toggled, self);
}

static gboolean
retry_connect_after_grant_idle (gpointer user_data)
{
    PermissionRequest *request = user_data;
    UsbTesterMonitorMainWindow *self = request->self;

    DevicePage *page = find_page (self, request->identifier);
    if (page != NULL) {
        GError *error = NULL;
        if (usb_tester_monitor_device_manager_connect_device (self->device_manager,
                                                              request->identifier,
                                                              &error)) {
            row_dot_set_connected (page->row_dot, TRUE);
            connect_button_set_connected (self, TRUE);

            g_signal_handlers_block_by_func (self->connect_button, on_connect_button_toggled, self);
            gtk_toggle_button_set_active (self->connect_button, TRUE);
            g_signal_handlers_unblock_by_func (self->connect_button, on_connect_button_toggled, self);
        } else {
            show_error_dialog (self, error != NULL ? error->message
                                                   : "Could not connect to the device");
            g_clear_error (&error);
            release_connect_toggle (self);
        }
    }

    permission_request_free (request);
    return G_SOURCE_REMOVE;
}

static void
retry_connect_after_grant (gboolean granted,
                           gpointer user_data)
{
    PermissionRequest *request = user_data;
    UsbTesterMonitorMainWindow *self = request->self;

    if (granted) {
        /* udev needs a moment to apply the new node permissions */
        g_timeout_add (500, retry_connect_after_grant_idle, request);
        return;
    }

    show_error_dialog (self,
                       "Access could not be granted. Install the udev rule "
                       "described in the README manually, then reconnect.");
    release_connect_toggle (self);
    permission_request_free (request);
}

static void
on_permission_dialog_response (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
    PermissionRequest *request = user_data;
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG (source);

    gint response = gtk_alert_dialog_choose_finish (dialog, result, NULL);

    if (response == request->grant_response) {
        usb_tester_monitor_device_permissions_grant_fnirsi_fnb48_async (
            retry_connect_after_grant, request);
    } else {
        release_connect_toggle (request->self);
        permission_request_free (request);
    }

    g_object_unref (dialog);
}

static void
show_permission_dialog (UsbTesterMonitorMainWindow *self,
                        DevicePage *page)
{
    PermissionRequest *request = g_new (PermissionRequest, 1);
    request->self = g_object_ref (self);
    request->identifier = g_strdup (page->device->identifier);

    GtkAlertDialog *dialog = gtk_alert_dialog_new ("Permission required");
    gchar *detail = g_strdup_printf (
        "Access to %s is restricted to the administrator. "
        "USB Tester Monitor can install a udev rule that grants you access to "
        "this device permanently. Your password will be requested once.",
        page->device->identifier);
    gtk_alert_dialog_set_detail (dialog, detail);
    g_free (detail);

    const char *buttons[] = { "Cancel", "Grant Access", NULL };
    gtk_alert_dialog_set_buttons (dialog, buttons);
    request->grant_response = 1;
    gtk_alert_dialog_set_cancel_button (dialog, 0);
    gtk_alert_dialog_set_default_button (dialog, request->grant_response);

    gtk_alert_dialog_choose (dialog, GTK_WINDOW (self), NULL,
                             on_permission_dialog_response, request);
}

/* -------------------------------------------------------------- Connect UI */

static void
on_connect_button_toggled (GtkToggleButton *button,
                           gpointer user_data)
{
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    DevicePage *page = active_page (self);

    if (page == NULL) {
        /* Nothing selected: release the toggle again */
        g_signal_handlers_block_by_func (button, on_connect_button_toggled, self);
        gtk_toggle_button_set_active (button, FALSE);
        g_signal_handlers_unblock_by_func (button, on_connect_button_toggled, self);
        return;
    }

    const gchar *identifier = page->device->identifier;

    if (gtk_toggle_button_get_active (button)) {
        GError *error = NULL;
        gboolean connected =
            usb_tester_monitor_device_manager_connect_device (self->device_manager,
                                                               identifier, &error);

        if (connected) {
            row_dot_set_connected (page->row_dot, TRUE);
            connect_button_set_connected (self, TRUE);
        } else if (error != NULL &&
                   g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED) &&
                   page->device->type == USB_TESTER_MONITOR_DEVICE_TYPE_FNIRSI_FNB48) {
            g_error_free (error);

            /* Release the toggle; the retry after granting re-activates it */
            g_signal_handlers_block_by_func (button, on_connect_button_toggled, self);
            gtk_toggle_button_set_active (button, FALSE);
            g_signal_handlers_unblock_by_func (button, on_connect_button_toggled, self);

            show_permission_dialog (self, page);
        } else {
            gchar *message = g_strdup_printf ("Could not connect to %s%s%s",
                                              page->device->name,
                                              error != NULL ? ": " : "",
                                              error != NULL ? error->message : "");
            show_error_dialog (self, message);
            g_free (message);
            g_clear_error (&error);

            g_signal_handlers_block_by_func (button, on_connect_button_toggled, self);
            gtk_toggle_button_set_active (button, FALSE);
            g_signal_handlers_unblock_by_func (button, on_connect_button_toggled, self);
        }
    } else {
        usb_tester_monitor_device_manager_disconnect_device (self->device_manager,
                                                             identifier);
        row_dot_set_connected (page->row_dot, FALSE);
        connect_button_set_connected (self, FALSE);

        /* Reset the display and chart of this device */
        usb_tester_monitor_data_display_set_voltage (page->data_display, 0.0);
        usb_tester_monitor_data_display_set_current (page->data_display, 0.0);
        usb_tester_monitor_data_display_set_power (page->data_display, 0.0);
        usb_tester_monitor_chart_view_clear (page->chart_view);
        usb_tester_monitor_stats_card_set_min_max_voltage (page->stats_card, 0.0, 0.0);
        usb_tester_monitor_stats_card_set_min_max_current (page->stats_card, 0.0, 0.0);
        usb_tester_monitor_stats_card_set_ripple (page->stats_card, 0.0);

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
    DevicePage *page = active_page (self);

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
        connect_button_set_connected (self, connected);
        temp_label_set_value (self, page->temperature);
    } else {
        gtk_label_set_text (self->title_label, "USB Tester Monitor");
        gtk_window_set_title (GTK_WINDOW (self), "USB Tester Monitor");

        g_signal_handlers_block_by_func (self->connect_button,
                                         on_connect_button_toggled, self);
        gtk_toggle_button_set_active (self->connect_button, FALSE);
        g_signal_handlers_unblock_by_func (self->connect_button,
                                           on_connect_button_toggled, self);
        connect_button_set_connected (self, FALSE);
        temp_label_set_value (self, NAN);
    }
}

/* ------------------------------------------------------------ Device adding */

static void
add_device (UsbTesterMonitorMainWindow *self,
            UsbTesterMonitorDevice *device)
{
    /* Takes ownership of the device reference it is given */
    DevicePage *page = g_new0 (DevicePage, 1);
    page->device = device;
    page->ripple_window = g_array_new (FALSE, FALSE, sizeof (gdouble));
    page->temperature = NAN;
    page->battery = NAN;

    /* Page in the main stack */
    GtkBox *page_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));

    page->data_display = usb_tester_monitor_data_display_new ();
    gtk_box_append (page_box, GTK_WIDGET (page->data_display));

    page->chart_view = usb_tester_monitor_chart_view_new ();
    gtk_widget_set_hexpand (GTK_WIDGET (page->chart_view), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (page->chart_view), TRUE);
    gtk_box_append (page_box, GTK_WIDGET (page->chart_view));

    page->stats_card = usb_tester_monitor_stats_card_new ();
    gtk_box_append (page_box, GTK_WIDGET (page->stats_card));

    page->page = GTK_WIDGET (page_box);
    gtk_stack_add_child (self->device_stack, page->page);

    /* Row in the device panel. The rounded hover/selection background is
     * applied by the CSS rule on the row; keep the inner margin small. */
    GtkBox *row_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8));
    gtk_widget_set_margin_start (GTK_WIDGET (row_box), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (row_box), 6);
    gtk_widget_set_margin_top (GTK_WIDGET (row_box), 2);
    gtk_widget_set_margin_bottom (GTK_WIDGET (row_box), 2);

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

    page->row_name_box = name_box;

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

/* ------------------------------------------------------------------ About */

static void
on_info_button_clicked (GtkButton *button,
                        gpointer user_data)
{
    (void) button;
    UsbTesterMonitorMainWindow *self = USB_TESTER_MONITOR_MAIN_WINDOW (user_data);

    const gchar *developers[] = { "Manuel https://github.com/manuel-rw", NULL };

    AdwAboutDialog *about = ADW_ABOUT_DIALOG (adw_about_dialog_new ());
    adw_about_dialog_set_application_name (about, "USB Tester Monitor");
    adw_about_dialog_set_version (about, "0.1.0");
    adw_about_dialog_set_developer_name (about, "Manuel");
    adw_about_dialog_set_developers (about, developers);
    adw_about_dialog_set_comments (about,
                                   "Monitor USB power tester devices in real time.\n\n"
                                   "Supported devices:\n"
                                   "\xe2\x80\xa2 Qianli iBridge A3\n"
                                   "\xe2\x80\xa2 Fnirsi FNB-48");
    adw_about_dialog_set_website (about, "https://github.com/manuel-rw/plug-monitor");
    adw_about_dialog_set_issue_url (about, "https://github.com/manuel-rw/plug-monitor/issues");
    adw_about_dialog_set_license_type (about, GTK_LICENSE_GPL_3_0);

    adw_dialog_present (ADW_DIALOG (about), GTK_WIDGET (self));
}

/* ------------------------------------------------------------------ Layout */

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
    self->add_button = GTK_MENU_BUTTON (gtk_menu_button_new ());
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->add_button), "list-add-symbolic");
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->add_button), "Add device");
    gtk_header_bar_pack_start (header_bar, GTK_WIDGET (self->add_button));

    /* Button toggling the device panel on the left side */
    self->panel_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
    GtkWidget *panel_icon = gtk_image_new_from_icon_name ("view-list-symbolic");
    gtk_button_set_child (GTK_BUTTON (self->panel_button), panel_icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->panel_button), "Devices");
    g_signal_connect (self->panel_button, "toggled",
                      G_CALLBACK (on_panel_button_toggled), self);
    gtk_header_bar_pack_start (header_bar, GTK_WIDGET (self->panel_button));

    /* Top right: info, temperature, record, connect.
     * pack_end puts each new widget to the left of the previous one,
     * so the info button ends up on the far right. */
    GtkButton *info_button = GTK_BUTTON (gtk_button_new_from_icon_name ("dialog-information-symbolic"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (info_button), "About USB Tester Monitor");
    g_signal_connect (info_button, "clicked",
                      G_CALLBACK (on_info_button_clicked), self);
    gtk_header_bar_pack_end (header_bar, GTK_WIDGET (info_button));

    /* Temperature of the active device, with a colored status dot */
    GtkBox *temp_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
    self->temp_dot = GTK_LABEL (gtk_label_new (""));
    gtk_widget_add_css_class (GTK_WIDGET (self->temp_dot), "temp-dot");
    self->temp_label = GTK_LABEL (gtk_label_new ("\u2014 \u00b0C"));
    gtk_widget_add_css_class (GTK_WIDGET (self->temp_label), "temp-value");
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->temp_label),
                                 "Temperature (not available on this device)");
    temp_label_set_value (self, NAN);
    gtk_box_append (temp_box, GTK_WIDGET (self->temp_dot));
    gtk_box_append (temp_box, GTK_WIDGET (self->temp_label));
    gtk_header_bar_pack_end (header_bar, GTK_WIDGET (temp_box));

    self->record_button = GTK_BUTTON (gtk_button_new_with_label ("Record"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->record_button),
                                 "Record the selected device to a CSV file");
    g_signal_connect (self->record_button, "clicked",
                      G_CALLBACK (on_record_button_clicked), self);
    gtk_header_bar_pack_end (header_bar, GTK_WIDGET (self->record_button));

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
    gtk_widget_set_margin_top (GTK_WIDGET (panel_box), 8);
    gtk_widget_set_margin_bottom (GTK_WIDGET (panel_box), 8);
    gtk_widget_set_margin_start (GTK_WIDGET (panel_box), 8);
    gtk_widget_set_margin_end (GTK_WIDGET (panel_box), 8);
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

    GtkScrolledWindow *scrolled = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
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

    recording_set_idle (self);

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
    self->recording_channels = 0;
    self->recording_duration_s = 0;
    self->countdown_source = 0;
    self->autostop_source = 0;
    self->countdown_remaining = 0;

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
