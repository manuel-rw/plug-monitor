#include "data-display.h"
#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib.h>

struct _UsbTesterMonitorDataDisplay {
    GtkBox parent_instance;

    /* Voltage widgets */
    GtkLabel *voltage_label;
    GtkLabel *voltage_unit;

    /* Current widgets */
    GtkLabel *current_label;
    GtkLabel *current_unit;

    /* Power widgets */
    GtkLabel *power_label;
    GtkLabel *power_unit;

    /* Main grid */
    GtkGrid *grid;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDataDisplay, usb_tester_monitor_data_display, GTK_TYPE_BOX)

static void
usb_tester_monitor_data_display_class_init (UsbTesterMonitorDataDisplayClass *klass)
{
}

static GtkLabel *
make_title (GtkGrid *grid,
            gint column,
            const gchar *text,
            const gchar *css_class)
{
    GtkLabel *title = GTK_LABEL (gtk_label_new (text));
    gtk_label_set_xalign (title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (title), "h3");
    gtk_widget_add_css_class (GTK_WIDGET (title), css_class);
    gtk_grid_attach (grid, GTK_WIDGET (title), column, 0, 1, 1);
    return title;
}

static GtkBox *
make_value_box (GtkGrid *grid,
                gint column,
                GtkLabel **value_out,
                GtkLabel **unit_out,
                const gchar *value_class,
                const gchar *unit_class,
                const gchar *unit_text)
{
    GtkBox *box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));

    *value_out = GTK_LABEL (gtk_label_new ("0.000"));
    gtk_label_set_xalign (*value_out, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (*value_out), value_class);
    gtk_box_append (box, GTK_WIDGET (*value_out));

    *unit_out = GTK_LABEL (gtk_label_new (unit_text));
    gtk_label_set_xalign (*unit_out, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (*unit_out), unit_class);
    gtk_box_append (box, GTK_WIDGET (*unit_out));

    gtk_grid_attach (grid, GTK_WIDGET (box), column, 1, 1, 1);
    return box;
}

static void
usb_tester_monitor_data_display_init (UsbTesterMonitorDataDisplay *self)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing (GTK_BOX (self), 12);
    gtk_widget_set_margin_top (GTK_WIDGET (self), 24);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self), 12);
    gtk_widget_set_margin_start (GTK_WIDGET (self), 24);
    gtk_widget_set_margin_end (GTK_WIDGET (self), 24);

    self->grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_column_spacing (self->grid, 48);
    gtk_grid_set_row_spacing (self->grid, 6);
    gtk_widget_set_halign (GTK_WIDGET (self->grid), GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->grid));

    /* Voltage display */
    make_title (self->grid, 0, "Voltage", "voltage-title");
    make_value_box (self->grid, 0, &self->voltage_label, &self->voltage_unit,
                    "voltage-value", "voltage-unit", "V");

    /* Current display */
    make_title (self->grid, 1, "Current", "current-title");
    make_value_box (self->grid, 1, &self->current_label, &self->current_unit,
                    "current-value", "current-unit", "A");

    /* Power display */
    make_title (self->grid, 2, "Power", "power-title");
    make_value_box (self->grid, 2, &self->power_label, &self->power_unit,
                    "power-value", "power-unit", "W");
}

UsbTesterMonitorDataDisplay *
usb_tester_monitor_data_display_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_DATA_DISPLAY, NULL);
}

void
usb_tester_monitor_data_display_set_voltage (UsbTesterMonitorDataDisplay *self,
                                              gdouble voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));

    gchar *text = g_strdup_printf ("%.3f", voltage);
    gtk_label_set_text (self->voltage_label, text);
    g_free (text);
}

void
usb_tester_monitor_data_display_set_current (UsbTesterMonitorDataDisplay *self,
                                              gdouble current)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));

    gchar *text = g_strdup_printf ("%.3f", current);
    gtk_label_set_text (self->current_label, text);
    g_free (text);
}

void
usb_tester_monitor_data_display_set_power (UsbTesterMonitorDataDisplay *self,
                                            gdouble power)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));

    gchar *text = g_strdup_printf ("%.3f", power);
    gtk_label_set_text (self->power_label, text);
    g_free (text);
}
