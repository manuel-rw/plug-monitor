#include "chart-view.h"
#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib.h>
#include <cairo.h>
#include <math.h>

#define MAX_DATA_POINTS 800
#define PADDING 20

/* Series colors (kept in sync with the CSS classes in data/css/style.css) */
#define COLOR_VOLTAGE "#35b042"
#define COLOR_CURRENT "#f57900"
#define COLOR_POWER   "#3584e4"

struct _UsbTesterMonitorChartView {
    GtkDrawingArea parent_instance;

    /* Data storage */
    GArray *voltage_data;
    GArray *current_data;
    GArray *power_data;

    /* Auto-computed display ranges */
    gdouble min_voltage;
    gdouble max_voltage;
    gdouble min_current;
    gdouble max_current;
    gdouble min_power;
    gdouble max_power;

    /* Colors */
    GdkRGBA voltage_color;
    GdkRGBA current_color;
    GdkRGBA power_color;
    GdkRGBA background_color;
    GdkRGBA grid_color;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorChartView, usb_tester_monitor_chart_view, GTK_TYPE_DRAWING_AREA)

static void
usb_tester_monitor_chart_view_finalize (GObject *object)
{
    UsbTesterMonitorChartView *self = USB_TESTER_MONITOR_CHART_VIEW (object);

    g_array_unref (self->voltage_data);
    g_array_unref (self->current_data);
    g_array_unref (self->power_data);

    G_OBJECT_CLASS (usb_tester_monitor_chart_view_parent_class)->finalize (object);
}

static void
recompute_range (GArray *data,
                 gdouble *min_out,
                 gdouble *max_out,
                 gdouble fallback)
{
    if (data->len == 0) {
        *min_out = 0.0;
        *max_out = fallback;
        return;
    }

    gdouble lo = G_MAXDOUBLE;
    gdouble hi = -G_MAXDOUBLE;

    for (guint i = 0; i < data->len; i++) {
        gdouble value = g_array_index (data, gdouble, i);
        if (value < lo) lo = value;
        if (value > hi) hi = value;
    }

    if (hi - lo < 1e-6) {
        /* Flat line: give it some vertical breathing room */
        lo -= 0.01;
        hi += 0.01;
    }

    *min_out = lo;
    *max_out = hi;
}

static void
draw_series (cairo_t *cr,
             GArray *data,
             gdouble min_value,
             gdouble max_value,
             gint width,
             gint height,
             const GdkRGBA *color)
{
    if (data->len < 2) {
        return;
    }

    gdouble range = max_value - min_value;
    if (range <= 0) range = 1.0;

    gdk_cairo_set_source_rgba (cr, color);
    cairo_set_line_width (cr, 2.0);

    for (guint i = 0; i < data->len; i++) {
        gdouble value = g_array_index (data, gdouble, i);
        gdouble x = PADDING + (i * (width - 2 * PADDING) / (data->len - 1));
        gdouble y = height - PADDING - ((value - min_value) / range * (height - 2 * PADDING));

        if (i == 0) {
            cairo_move_to (cr, x, y);
        } else {
            cairo_line_to (cr, x, y);
        }
    }
    cairo_stroke (cr);
}

static void
draw_legend (cairo_t *cr)
{
    const gchar *labels[] = { "Voltage", "Current", "Power" };
    const gchar *colors[] = { COLOR_VOLTAGE, COLOR_CURRENT, COLOR_POWER };

    cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 12.0);

    gdouble x = PADDING + 8;
    const gdouble y = 14;

    for (gint i = 0; i < 3; i++) {
        GdkRGBA color;
        gdk_rgba_parse (&color, colors[i]);

        /* Colored marker */
        gdk_cairo_set_source_rgba (cr, &color);
        cairo_rectangle (cr, x, y - 8, 10, 10);
        cairo_fill (cr);

        /* Label */
        cairo_set_source_rgba (cr, 0.94, 0.94, 0.94, 1.0);
        cairo_text_extents_t extents;
        cairo_text_extents (cr, labels[i], &extents);
        cairo_move_to (cr, x + 14, y + 1);
        cairo_show_text (cr, labels[i]);

        x += 14 + extents.x_advance + 16;
    }
}

static void
usb_tester_monitor_chart_view_snapshot (GtkWidget *widget,
                                        GtkSnapshot *snapshot)
{
    UsbTesterMonitorChartView *self = USB_TESTER_MONITOR_CHART_VIEW (widget);

    gint width = gtk_widget_get_width (widget);
    gint height = gtk_widget_get_height (widget);

    if (width <= 0 || height <= 0) {
        return;
    }

    cairo_t *cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

    /* Draw background */
    gdk_cairo_set_source_rgba (cr, &self->background_color);
    cairo_paint (cr);

    /* Draw grid */
    gdk_cairo_set_source_rgba (cr, &self->grid_color);
    cairo_set_line_width (cr, 1.0);

    for (int i = 0; i <= 5; i++) {
        gdouble y = height - PADDING - (i * (height - 2 * PADDING) / 5.0);
        cairo_move_to (cr, PADDING, y);
        cairo_line_to (cr, width - PADDING, y);
        cairo_stroke (cr);
    }

    for (int i = 0; i <= 10; i++) {
        gdouble x = PADDING + (i * (width - 2 * PADDING) / 10.0);
        cairo_move_to (cr, x, PADDING);
        cairo_line_to (cr, x, height - PADDING);
        cairo_stroke (cr);
    }

    /* Draw data series */
    draw_series (cr, self->voltage_data, self->min_voltage, self->max_voltage,
                 width, height, &self->voltage_color);
    draw_series (cr, self->current_data, self->min_current, self->max_current,
                 width, height, &self->current_color);
    draw_series (cr, self->power_data, self->min_power, self->max_power,
                 width, height, &self->power_color);

    /* Draw legend markers */
    draw_legend (cr);

    cairo_destroy (cr);
}

static gboolean
usb_tester_monitor_chart_view_query_tooltip (GtkWidget *widget,
                                             gint x,
                                             gint y,
                                             gboolean keyboard_tooltip,
                                             GtkTooltip *tooltip)
{
    UsbTesterMonitorChartView *self = USB_TESTER_MONITOR_CHART_VIEW (widget);
    (void) y;
    (void) keyboard_tooltip;

    gint width = gtk_widget_get_width (widget);

    if (self->voltage_data->len == 0 || width <= 2 * PADDING) {
        return FALSE;
    }

    /* Map the horizontal position to a sample index */
    gdouble fraction = (gdouble) (x - PADDING) / (width - 2 * PADDING);
    fraction = CLAMP (fraction, 0.0, 1.0);
    gint index = (gint) round (fraction * (self->voltage_data->len - 1));

    gdouble voltage = g_array_index (self->voltage_data, gdouble, index);
    gdouble current = g_array_index (self->current_data, gdouble, index);
    gdouble power = g_array_index (self->power_data, gdouble, index);

    gchar *markup =
        g_strdup_printf ("<b>Sample %d</b>\n"
                         "<span foreground='%s'>Voltage  %.3f V</span>\n"
                         "<span foreground='%s'>Current  %.3f A</span>\n"
                         "<span foreground='%s'>Power    %.3f W</span>",
                         index,
                         COLOR_VOLTAGE, voltage,
                         COLOR_CURRENT, current,
                         COLOR_POWER, power);
    gtk_tooltip_set_markup (tooltip, markup);
    g_free (markup);

    return TRUE;
}

static void
usb_tester_monitor_chart_view_class_init (UsbTesterMonitorChartViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = usb_tester_monitor_chart_view_finalize;
    widget_class->snapshot = usb_tester_monitor_chart_view_snapshot;
    widget_class->query_tooltip = usb_tester_monitor_chart_view_query_tooltip;
}

static void
usb_tester_monitor_chart_view_init (UsbTesterMonitorChartView *self)
{
    self->voltage_data = g_array_new (FALSE, FALSE, sizeof (gdouble));
    self->current_data = g_array_new (FALSE, FALSE, sizeof (gdouble));
    self->power_data = g_array_new (FALSE, FALSE, sizeof (gdouble));

    self->min_voltage = 0.0;
    self->max_voltage = 5.0;
    self->min_current = 0.0;
    self->max_current = 2.0;
    self->min_power = 0.0;
    self->max_power = 10.0;

    gdk_rgba_parse (&self->voltage_color, COLOR_VOLTAGE);
    gdk_rgba_parse (&self->current_color, COLOR_CURRENT);
    gdk_rgba_parse (&self->power_color, COLOR_POWER);
    gdk_rgba_parse (&self->background_color, "#1e1e1e");
    gdk_rgba_parse (&self->grid_color, "#404040");

    gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
    gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);
}

UsbTesterMonitorChartView *
usb_tester_monitor_chart_view_new (void)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_CHART_VIEW, NULL);
}

void
usb_tester_monitor_chart_view_add_reading (UsbTesterMonitorChartView *self,
                                           gdouble voltage,
                                           gdouble current)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));

    gdouble power = voltage * current;

    g_array_append_vals (self->voltage_data, &voltage, 1);
    g_array_append_vals (self->current_data, &current, 1);
    g_array_append_vals (self->power_data, &power, 1);

    /* Limit data points */
    while (self->voltage_data->len > MAX_DATA_POINTS) {
        g_array_remove_index (self->voltage_data, 0);
    }
    while (self->current_data->len > MAX_DATA_POINTS) {
        g_array_remove_index (self->current_data, 0);
    }
    while (self->power_data->len > MAX_DATA_POINTS) {
        g_array_remove_index (self->power_data, 0);
    }

    /* Recompute display ranges */
    recompute_range (self->voltage_data, &self->min_voltage, &self->max_voltage, 5.0);
    recompute_range (self->current_data, &self->min_current, &self->max_current, 2.0);
    recompute_range (self->power_data, &self->min_power, &self->max_power, 10.0);

    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
usb_tester_monitor_chart_view_clear (UsbTesterMonitorChartView *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));

    g_array_set_size (self->voltage_data, 0);
    g_array_set_size (self->current_data, 0);
    g_array_set_size (self->power_data, 0);

    self->min_voltage = 0.0;
    self->max_voltage = 5.0;
    self->min_current = 0.0;
    self->max_current = 2.0;
    self->min_power = 0.0;
    self->max_power = 10.0;

    gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
usb_tester_monitor_chart_view_set_time_range (UsbTesterMonitorChartView *self,
                                              guint seconds)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_CHART_VIEW (self));

    /* Kept for API compatibility */
    (void) self;
    (void) seconds;
}
