#include "chart-view.h"
#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>

#define MAX_DATA_POINTS 800
#define PADDING 20

struct _UsbTesterMonitorChartView {
    GtkDrawingArea parent_instance;
    
    /* Data storage */
    GArray *voltage_data;
    GArray *current_data;
    
    /* Display settings */
    guint time_range_seconds;
    gdouble min_voltage;
    gdouble max_voltage;
    gdouble min_current;
    gdouble max_current;
    
    /* Colors */
    GdkRGBA voltage_color;
    GdkRGBA current_color;
    GdkRGBA background_color;
    GdkRGBA grid_color;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorChartView, usb_tester_monitor_chart_view, GTK_TYPE_DRAWING_AREA)

static void
usb_tester_monitor_chart_view_finalize (GObject *object)
{
    UsbTesterMonitorChartView *self = USB_TESTER_MONITOR_CHART_VIEW (object);
    
    if (self->voltage_data) {
        g_array_unref (self->voltage_data);
    }
    if (self->current_data) {
        g_array_unref (self->current_data);
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_chart_view_parent_class)->finalize (object);
}

static void
usb_tester_monitor_chart_view_snapshot (GtkWidget *widget,
                                        GtkSnapshot *snapshot)
{
    UsbTesterMonitorChartView *self = USB_TESTER_MONITOR_CHART_VIEW (widget);
    
    /* Get widget dimensions */
    gint width = gtk_widget_get_width (widget);
    gint height = gtk_widget_get_height (widget);
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    /* Create cairo context */
    cairo_t *cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
    
    /* Draw background */
    gdk_cairo_set_source_rgba (cr, &self->background_color);
    cairo_paint (cr);
    
    /* Draw grid */
    gdk_cairo_set_source_rgba (cr, &self->grid_color);
    cairo_set_line_width (cr, 1.0);
    
    /* Horizontal grid lines */
    for (int i = 0; i <= 5; i++) {
        gdouble y = height - PADDING - (i * (height - 2 * PADDING) / 5.0);
        cairo_move_to (cr, PADDING, y);
        cairo_line_to (cr, width - PADDING, y);
        cairo_stroke (cr);
    }
    
    /* Vertical grid lines */
    for (int i = 0; i <= 10; i++) {
        gdouble x = PADDING + (i * (width - 2 * PADDING) / 10.0);
        cairo_move_to (cr, x, PADDING);
        cairo_line_to (cr, x, height - PADDING);
        cairo_stroke (cr);
    }
    
    /* Draw data if available */
    if (self->voltage_data->len > 1) {
        /* Draw voltage line */
        gdk_cairo_set_source_rgba (cr, &self->voltage_color);
        cairo_set_line_width (cr, 2.0);
        
        cairo_move_to (cr, PADDING, height - PADDING);
        
        gdouble voltage_range = self->max_voltage - self->min_voltage;
        if (voltage_range == 0) voltage_range = 1.0;
        
        for (guint i = 0; i < self->voltage_data->len; i++) {
            gdouble value = g_array_index (self->voltage_data, gdouble, i);
            gdouble x = PADDING + (i * (width - 2 * PADDING) / (self->voltage_data->len - 1));
            gdouble y = height - PADDING - ((value - self->min_voltage) / voltage_range * (height - 2 * PADDING));
            
            cairo_line_to (cr, x, y);
        }
        cairo_stroke (cr);
    }
    
    if (self->current_data->len > 1) {
        /* Draw current line */
        gdk_cairo_set_source_rgba (cr, &self->current_color);
        cairo_set_line_width (cr, 2.0);
        
        cairo_move_to (cr, PADDING, height - PADDING);
        
        gdouble current_range = self->max_current - self->min_current;
        if (current_range == 0) current_range = 1.0;
        
        for (guint i = 0; i < self->current_data->len; i++) {
            gdouble value = g_array_index (self->current_data, gdouble, i);
            gdouble x = PADDING + (i * (width - 2 * PADDING) / (self->current_data->len - 1));
            gdouble y = height - PADDING - ((value - self->min_current) / current_range * (height - 2 * PADDING));
            
            cairo_line_to (cr, x, y);
        }
        cairo_stroke (cr);
    }
    
    cairo_destroy (cr);
}

static void
usb_tester_monitor_chart_view_class_init (UsbTesterMonitorChartViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_chart_view_finalize;
    widget_class->snapshot = usb_tester_monitor_chart_view_snapshot;
}

static void
usb_tester_monitor_chart_view_init (UsbTesterMonitorChartView *self)
{
    /* Initialize data arrays */
    self->voltage_data = g_array_new (FALSE, FALSE, sizeof(gdouble));
    self->current_data = g_array_new (FALSE, FALSE, sizeof(gdouble));
    
    /* Set default time range */
    self->time_range_seconds = 30;
    
    /* Set default ranges */
    self->min_voltage = 0.0;
    self->max_voltage = 5.0;
    self->min_current = 0.0;
    self->max_current = 2.0;
    
    /* Set colors */
    gdk_rgba_parse (&self->voltage_color, "#35b042"); /* Green for voltage */
    gdk_rgba_parse (&self->current_color, "#f57900"); /* Orange for current */
    gdk_rgba_parse (&self->background_color, "#1e1e1e"); /* Dark background */
    gdk_rgba_parse (&self->grid_color, "#404040"); /* Grid color */
    
    /* Set widget properties */
    gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
    gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

UsbTesterMonitorChartView *
usb_tester_monitor_chart_view_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_CHART_VIEW, NULL);
}

void
usb_tester_monitor_chart_view_add_voltage_reading (UsbTesterMonitorChartView *self,
                                                   gdouble voltage)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));
    
    /* Add new reading */
    gdouble val = voltage;
    g_array_append_vals (self->voltage_data, &val, 1);
    
    /* Update range */
    if (voltage < self->min_voltage || self->voltage_data->len == 1) {
        self->min_voltage = voltage;
    }
    if (voltage > self->max_voltage) {
        self->max_voltage = voltage;
    }
    
    /* Limit data points */
    while (self->voltage_data->len > MAX_DATA_POINTS) {
        g_array_remove_index (self->voltage_data, 0);
    }
    
    /* Request redraw */
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
usb_tester_monitor_chart_view_add_current_reading (UsbTesterMonitorChartView *self,
                                                   gdouble current)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));
    
    /* Add new reading */
    gdouble val = current;
    g_array_append_vals (self->current_data, &val, 1);
    
    /* Update range */
    if (current < self->min_current || self->current_data->len == 1) {
        self->min_current = current;
    }
    if (current > self->max_current) {
        self->max_current = current;
    }
    
    /* Limit data points */
    while (self->current_data->len > MAX_DATA_POINTS) {
        g_array_remove_index (self->current_data, 0);
    }
    
    /* Request redraw */
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
usb_tester_monitor_chart_view_clear (UsbTesterMonitorChartView *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));
    
    g_array_set_size (self->voltage_data, 0);
    g_array_set_size (self->current_data, 0);
    
    self->min_voltage = 0.0;
    self->max_voltage = 5.0;
    self->min_current = 0.0;
    self->max_current = 2.0;
    
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
usb_tester_monitor_chart_view_set_time_range (UsbTesterMonitorChartView *self,
                                               guint seconds)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));
    
    self->time_range_seconds = seconds;
    
    /* TODO: Implement time-based filtering */
    
    gtk_widget_queue_draw (GTK_WIDGET (self));
}
