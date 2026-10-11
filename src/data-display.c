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
    
    /* Status widget */
    GtkLabel *status_label;
    
    /* Min/Max widgets */
    GtkLabel *min_voltage_label;
    GtkLabel *max_voltage_label;
    GtkLabel *min_current_label;
    GtkLabel *max_current_label;

    /* Ripple widget */
    GtkLabel *ripple_label;
    
    /* Main grid */
    GtkGrid *grid;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDataDisplay, usb_tester_monitor_data_display, GTK_TYPE_BOX)

static void
usb_tester_monitor_data_display_finalize (GObject *object)
{
    G_OBJECT_CLASS (usb_tester_monitor_data_display_parent_class)->finalize (object);
}

static void
usb_tester_monitor_data_display_class_init (UsbTesterMonitorDataDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_data_display_finalize;
}

static void
usb_tester_monitor_data_display_init (UsbTesterMonitorDataDisplay *self)
{
    /* Set up main box */
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing (GTK_BOX (self), 12);
    gtk_widget_set_margin_top (GTK_WIDGET (self), 24);
    gtk_widget_set_margin_bottom (GTK_WIDGET (self), 24);
    gtk_widget_set_margin_start (GTK_WIDGET (self), 24);
    gtk_widget_set_margin_end (GTK_WIDGET (self), 24);
    
    /* Create main grid */
    self->grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_column_spacing (self->grid, 24);
    gtk_grid_set_row_spacing (self->grid, 12);
    gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->grid));
    
    /* Voltage display */
    GtkLabel *voltage_title = GTK_LABEL (gtk_label_new ("Voltage"));
    gtk_label_set_xalign (voltage_title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (voltage_title), "h3");
    gtk_widget_add_css_class (GTK_WIDGET (voltage_title), "voltage-title");
    gtk_grid_insert_column (self->grid, 0);
    gtk_grid_attach (self->grid, GTK_WIDGET (voltage_title), 0, 0, 1, 1);
    
    GtkBox *voltage_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    self->voltage_label = GTK_LABEL (gtk_label_new ("0.000"));
    gtk_label_set_xalign (self->voltage_label, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->voltage_label), "voltage-value");
    gtk_box_append (voltage_box, GTK_WIDGET (self->voltage_label));
    
    self->voltage_unit = GTK_LABEL (gtk_label_new ("V"));
    gtk_label_set_xalign (self->voltage_unit, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->voltage_unit), "voltage-unit");
    gtk_box_append (voltage_box, GTK_WIDGET (self->voltage_unit));
    
    gtk_grid_attach (self->grid, GTK_WIDGET (voltage_box), 0, 1, 1, 1);
    
    /* Current display */
    GtkLabel *current_title = GTK_LABEL (gtk_label_new ("Current"));
    gtk_label_set_xalign (current_title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (current_title), "h3");
    gtk_widget_add_css_class (GTK_WIDGET (current_title), "current-title");
    gtk_grid_attach (self->grid, GTK_WIDGET (current_title), 1, 0, 1, 1);
    
    GtkBox *current_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    self->current_label = GTK_LABEL (gtk_label_new ("0.000"));
    gtk_label_set_xalign (self->current_label, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->current_label), "current-value");
    gtk_box_append (current_box, GTK_WIDGET (self->current_label));
    
    self->current_unit = GTK_LABEL (gtk_label_new ("A"));
    gtk_label_set_xalign (self->current_unit, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->current_unit), "current-unit");
    gtk_box_append (current_box, GTK_WIDGET (self->current_unit));
    
    gtk_grid_attach (self->grid, GTK_WIDGET (current_box), 1, 1, 1, 1);
    
    /* Power display */
    GtkLabel *power_title = GTK_LABEL (gtk_label_new ("Power"));
    gtk_label_set_xalign (power_title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (power_title), "h3");
    gtk_widget_add_css_class (GTK_WIDGET (power_title), "power-title");
    gtk_grid_attach (self->grid, GTK_WIDGET (power_title), 2, 0, 1, 1);
    
    GtkBox *power_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    self->power_label = GTK_LABEL (gtk_label_new ("0.000"));
    gtk_label_set_xalign (self->power_label, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->power_label), "power-value");
    gtk_box_append (power_box, GTK_WIDGET (self->power_label));
    
    self->power_unit = GTK_LABEL (gtk_label_new ("W"));
    gtk_label_set_xalign (self->power_unit, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (self->power_unit), "power-unit");
    gtk_box_append (power_box, GTK_WIDGET (self->power_unit));
    
    gtk_grid_attach (self->grid, GTK_WIDGET (power_box), 2, 1, 1, 1);
    
    /* Status display */
    self->status_label = GTK_LABEL (gtk_label_new ("Disconnected"));
    gtk_label_set_xalign (self->status_label, 0.5);
    gtk_widget_add_css_class (GTK_WIDGET (self->status_label), "status");
    gtk_grid_attach (self->grid, GTK_WIDGET (self->status_label), 0, 2, 3, 1);
    
    /* Min/Max stats */
    GtkLabel *stats_title = GTK_LABEL (gtk_label_new ("Statistics"));
    gtk_label_set_xalign (stats_title, 0.0);
    gtk_widget_add_css_class (GTK_WIDGET (stats_title), "h4");
    gtk_grid_attach (self->grid, GTK_WIDGET (stats_title), 0, 3, 3, 1);
    
    /* Voltage min/max */
    GtkBox *voltage_stats = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12));
    
    GtkLabel *voltage_min_label = GTK_LABEL (gtk_label_new ("Min:"));
    gtk_box_append (voltage_stats, GTK_WIDGET (voltage_min_label));
    
    self->min_voltage_label = GTK_LABEL (gtk_label_new ("0.000 V"));
    gtk_widget_add_css_class (GTK_WIDGET (self->min_voltage_label), "min-value");
    gtk_box_append (voltage_stats, GTK_WIDGET (self->min_voltage_label));
    
    GtkLabel *voltage_max_label = GTK_LABEL (gtk_label_new ("Max:"));
    gtk_box_append (voltage_stats, GTK_WIDGET (voltage_max_label));
    
    self->max_voltage_label = GTK_LABEL (gtk_label_new ("0.000 V"));
    gtk_widget_add_css_class (GTK_WIDGET (self->max_voltage_label), "max-value");
    gtk_box_append (voltage_stats, GTK_WIDGET (self->max_voltage_label));
    
    gtk_grid_attach (self->grid, GTK_WIDGET (voltage_stats), 0, 4, 1, 1);
    
    /* Current min/max */
    GtkBox *current_stats = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12));
    
    GtkLabel *current_min_label = GTK_LABEL (gtk_label_new ("Min:"));
    gtk_box_append (current_stats, GTK_WIDGET (current_min_label));
    
    self->min_current_label = GTK_LABEL (gtk_label_new ("0.000 A"));
    gtk_widget_add_css_class (GTK_WIDGET (self->min_current_label), "min-value");
    gtk_box_append (current_stats, GTK_WIDGET (self->min_current_label));
    
    GtkLabel *current_max_label = GTK_LABEL (gtk_label_new ("Max:"));
    gtk_box_append (current_stats, GTK_WIDGET (current_max_label));
    
    self->max_current_label = GTK_LABEL (gtk_label_new ("0.000 A"));
    gtk_widget_add_css_class (GTK_WIDGET (self->max_current_label), "max-value");
    gtk_box_append (current_stats, GTK_WIDGET (self->max_current_label));
    
    gtk_grid_attach (self->grid, GTK_WIDGET (current_stats), 1, 4, 1, 1);

    /* Ripple (peak-to-peak voltage over the recent window) */
    GtkBox *ripple_stats = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12));

    GtkLabel *ripple_caption = GTK_LABEL (gtk_label_new ("Ripple:"));
    gtk_box_append (ripple_stats, GTK_WIDGET (ripple_caption));

    self->ripple_label = GTK_LABEL (gtk_label_new ("0.0 mV"));
    gtk_widget_add_css_class (GTK_WIDGET (self->ripple_label), "ripple-value");
    gtk_box_append (ripple_stats, GTK_WIDGET (self->ripple_label));

    gtk_grid_attach (self->grid, GTK_WIDGET (ripple_stats), 2, 4, 1, 1);
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

void
usb_tester_monitor_data_display_set_device_status (UsbTesterMonitorDataDisplay *self,
                                                    const gchar *status)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));
    
    gtk_label_set_text (self->status_label, status);
}

void
usb_tester_monitor_data_display_set_min_max_voltage (UsbTesterMonitorDataDisplay *self,
                                                        gdouble min_voltage,
                                                        gdouble max_voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));
    
    gchar *min_text = g_strdup_printf ("%.3f V", min_voltage);
    gchar *max_text = g_strdup_printf ("%.3f V", max_voltage);
    
    gtk_label_set_text (self->min_voltage_label, min_text);
    gtk_label_set_text (self->max_voltage_label, max_text);
    
    g_free (min_text);
    g_free (max_text);
}

void
usb_tester_monitor_data_display_set_min_max_current (UsbTesterMonitorDataDisplay *self,
                                                        gdouble min_current,
                                                        gdouble max_current)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));
    
    gchar *min_text = g_strdup_printf ("%.3f A", min_current);
    gchar *max_text = g_strdup_printf ("%.3f A", max_current);
    
    gtk_label_set_text (self->min_current_label, min_text);
    gtk_label_set_text (self->max_current_label, max_text);

    g_free (min_text);
    g_free (max_text);
}

void
usb_tester_monitor_data_display_set_ripple (UsbTesterMonitorDataDisplay *self,
                                            gdouble ripple_voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_DISPLAY (self));

    gchar *text = g_strdup_printf ("%.1f mV", ripple_voltage * 1000.0);
    gtk_label_set_text (self->ripple_label, text);
    g_free (text);
}
