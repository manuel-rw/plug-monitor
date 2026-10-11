#include "stats-card.h"
#include <gtk/gtk.h>
#include <glib.h>

struct _UsbTesterMonitorStatsCard {
    GtkBox parent_instance;

    GtkLabel *voltage_value;
    GtkLabel *current_value;
    GtkLabel *ripple_value;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorStatsCard, usb_tester_monitor_stats_card, GTK_TYPE_BOX)

static GtkLabel *
make_title (const gchar *text)
{
    GtkLabel *label = GTK_LABEL (gtk_label_new (text));
    gtk_widget_add_css_class (GTK_WIDGET (label), "dim-label");
    gtk_widget_add_css_class (GTK_WIDGET (label), "caption");
    gtk_label_set_xalign (label, 0.0);
    return label;
}

static void
usb_tester_monitor_stats_card_init (UsbTesterMonitorStatsCard *self)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class (GTK_WIDGET (self), "card");
    gtk_widget_add_css_class (GTK_WIDGET (self), "stats-card");
    gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);

    gtk_widget_set_margin_top (GTK_WIDGET (self), 12);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self), 12);
    gtk_widget_set_margin_start (GTK_WIDGET (self), 12);
    gtk_widget_set_margin_end (GTK_WIDGET (self), 12);

    GtkGrid *grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_column_spacing (grid, 48);
    gtk_grid_set_row_spacing (grid, 2);
    gtk_grid_set_column_homogeneous (grid, TRUE);
    gtk_widget_set_halign (GTK_WIDGET (grid), GTK_ALIGN_FILL);
    gtk_widget_set_valign (GTK_WIDGET (grid), GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand (GTK_WIDGET (grid), FALSE);
    gtk_widget_set_hexpand (GTK_WIDGET (grid), TRUE);
    gtk_box_append (GTK_BOX (self), GTK_WIDGET (grid));

    /* Voltage */
    gtk_grid_attach (grid, GTK_WIDGET (make_title ("Voltage")), 0, 0, 1, 1);
    self->voltage_value = GTK_LABEL (gtk_label_new ("Min 0.000 V  Max 0.000 V"));
    gtk_widget_add_css_class (GTK_WIDGET (self->voltage_value), "stats-value");
    gtk_label_set_ellipsize (self->voltage_value, PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (self->voltage_value, 0.0);
    gtk_grid_attach (grid, GTK_WIDGET (self->voltage_value), 0, 1, 1, 1);

    /* Current */
    gtk_grid_attach (grid, GTK_WIDGET (make_title ("Current")), 1, 0, 1, 1);
    self->current_value = GTK_LABEL (gtk_label_new ("Min 0.000 A  Max 0.000 A"));
    gtk_widget_add_css_class (GTK_WIDGET (self->current_value), "stats-value");
    gtk_label_set_ellipsize (self->current_value, PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (self->current_value, 0.0);
    gtk_grid_attach (grid, GTK_WIDGET (self->current_value), 1, 1, 1, 1);

    /* Ripple */
    gtk_grid_attach (grid, GTK_WIDGET (make_title ("Ripple")), 2, 0, 1, 1);
    self->ripple_value = GTK_LABEL (gtk_label_new ("0.0 mV"));
    gtk_widget_add_css_class (GTK_WIDGET (self->ripple_value), "stats-value");
    gtk_label_set_ellipsize (self->ripple_value, PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign (self->ripple_value, 0.0);
    gtk_grid_attach (grid, GTK_WIDGET (self->ripple_value), 2, 1, 1, 1);
}

static void
usb_tester_monitor_stats_card_class_init (UsbTesterMonitorStatsCardClass *klass)
{
}

UsbTesterMonitorStatsCard *
usb_tester_monitor_stats_card_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_STATS_CARD, NULL);
}

void
usb_tester_monitor_stats_card_set_min_max_voltage (UsbTesterMonitorStatsCard *self,
                                                   gdouble min_voltage,
                                                   gdouble max_voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_STATS_CARD (self));

    gchar *text = g_strdup_printf ("Min %.3f V  Max %.3f V", min_voltage, max_voltage);
    gtk_label_set_text (self->voltage_value, text);
    g_free (text);
}

void
usb_tester_monitor_stats_card_set_min_max_current (UsbTesterMonitorStatsCard *self,
                                                   gdouble min_current,
                                                   gdouble max_current)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_STATS_CARD (self));

    gchar *text = g_strdup_printf ("Min %.3f A  Max %.3f A", min_current, max_current);
    gtk_label_set_text (self->current_value, text);
    g_free (text);
}

void
usb_tester_monitor_stats_card_set_ripple (UsbTesterMonitorStatsCard *self,
                                          gdouble ripple_voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_STATS_CARD (self));

    gchar *text = g_strdup_printf ("%.1f mV", ripple_voltage * 1000.0);
    gtk_label_set_text (self->ripple_value, text);
    g_free (text);
}
