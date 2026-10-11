#include "data-logger.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <time.h>

struct _UsbTesterMonitorDataLogger {
    GObject parent_instance;
    
    /* Logging configuration */
    gchar *log_dir;
    gchar *log_file;
    guint interval_ms;
    
    /* Logging state */
    gboolean is_logging;
    guint timeout_id;
    
    /* Data storage */
    GList *entries;
    
    /* File output */
    GFile *file;
    GFileOutputStream *output_stream;
    
    /* Callback */
    UsbTesterMonitorLogCallback callback;
    gpointer user_data;
    GDestroyNotify notify;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorDataLogger, usb_tester_monitor_data_logger, G_TYPE_OBJECT)

static void
usb_tester_monitor_data_logger_finalize (GObject *object)
{
    UsbTesterMonitorDataLogger *self = USB_TESTER_MONITOR_DATA_LOGGER (object);
    
    /* Stop logging if active */
    if (self->is_logging) {
        usb_tester_monitor_data_logger_stop (self);
    }
    
    g_free (self->log_dir);
    g_free (self->log_file);
    
    /* Close file */
    if (self->output_stream) {
        g_output_stream_close (G_OUTPUT_STREAM (self->output_stream), NULL, NULL);
        g_object_unref (self->output_stream);
    }
    if (self->file) {
        g_object_unref (self->file);
    }
    
    /* Free entries */
    g_list_free_full (self->entries, g_free);
    
    /* Clean up callback */
    if (self->notify && self->user_data) {
        self->notify (self->user_data);
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_data_logger_parent_class)->finalize (object);
}

static gboolean
write_log_entry (UsbTesterMonitorDataLogger *self)
{
    if (!self->is_logging || !self->output_stream) {
        return G_SOURCE_CONTINUE;
    }
    
    /* Get current time */
    GDateTime *now = g_date_time_new_now_local ();
    gchar *timestamp_str = g_date_time_format (now, "%Y-%m-%d %H:%M:%S.%f");
    
    /* Get latest entry */
    if (self->entries) {
        UsbTesterMonitorLogEntry *entry = self->entries->data;
        
        gchar *line = g_strdup_printf ("%s,%.3f,%.3f,%.3f\n",
                                       timestamp_str,
                                       entry->voltage,
                                       entry->current,
                                       entry->power);
        
        /* Write to file */
        gsize bytes_written;
        g_output_stream_write_all (G_OUTPUT_STREAM (self->output_stream),
                                   line,
                                   strlen (line),
                                   &bytes_written,
                                   NULL,
                                   NULL);
        
        g_free (line);
    }
    
    g_free (timestamp_str);
    g_date_time_unref (now);
    
    return G_SOURCE_CONTINUE;
}

static void
usb_tester_monitor_data_logger_class_init (UsbTesterMonitorDataLoggerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_data_logger_finalize;
}

static void
usb_tester_monitor_data_logger_init (UsbTesterMonitorDataLogger *self)
{
    self->interval_ms = 100;
    self->is_logging = FALSE;
    self->entries = NULL;
    self->timeout_id = 0;
}

UsbTesterMonitorDataLogger *
usb_tester_monitor_data_logger_new (const gchar *log_dir)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_DATA_LOGGER,
                        "log-dir", log_dir,
                        NULL);
}

UsbTesterMonitorDataLogger *
usb_tester_monitor_data_logger_new_with_file (const gchar *log_file)
{
    UsbTesterMonitorDataLogger *self = g_object_new (USB_TESTER_MONITOR_TYPE_DATA_LOGGER, NULL);
    self->log_file = g_strdup (log_file);
    return self;
}

static gboolean
ensure_log_file (UsbTesterMonitorDataLogger *self)
{
    if (self->log_file) {
        /* Use specified file */
        self->file = g_file_new_for_path (self->log_file);
    } else if (self->log_dir) {
        /* Create file in log directory */
        GDateTime *now = g_date_time_new_now_local ();
        gchar *timestamp = g_date_time_format (now, "%Y%m%d_%H%M%S");
        
        gchar *filename = g_strdup_printf ("usb_tester_%s.csv", timestamp);
        gchar *filepath = g_build_filename (self->log_dir, filename, NULL);
        
        self->file = g_file_new_for_path (filepath);
        
        g_free (filename);
        g_free (filepath);
        g_free (timestamp);
        g_date_time_unref (now);
    } else {
        /* Use default location */
        const gchar *home_dir = g_get_home_dir ();
        gchar *filepath = g_build_filename (home_dir, "usb_tester_log.csv", NULL);
        self->file = g_file_new_for_path (filepath);
        g_free (filepath);
    }
    
    /* Create parent directory if needed */
    GFile *parent = g_file_get_parent (self->file);
    if (parent) {
        g_file_make_directory_with_parents (parent, NULL, NULL);
        g_object_unref (parent);
    }
    
    /* Open file for appending */
    GError *error = NULL;
    self->output_stream = g_file_append_to (self->file, G_FILE_CREATE_NONE, NULL, &error);
    
    if (!self->output_stream) {
        g_warning ("Failed to open log file: %s", error ? error->message : "unknown error");
        if (error) g_error_free (error);
        return FALSE;
    }
    
    /* Write CSV header */
    gchar *header = "timestamp,voltage,current,power\n";
    gsize bytes_written;
    g_output_stream_write_all (G_OUTPUT_STREAM (self->output_stream),
                               header,
                               strlen (header),
                               &bytes_written,
                               NULL,
                               NULL);
    
    return TRUE;
}

void
usb_tester_monitor_data_logger_start (UsbTesterMonitorDataLogger *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    if (self->is_logging) {
        return;
    }
    
    /* Ensure log file is ready */
    if (!ensure_log_file (self)) {
        g_warning ("Failed to create log file");
        return;
    }
    
    self->is_logging = TRUE;
    
    /* Set up periodic flushing */
    self->timeout_id = g_timeout_add (self->interval_ms,
                                     (GSourceFunc)write_log_entry,
                                     self);
}

void
usb_tester_monitor_data_logger_stop (UsbTesterMonitorDataLogger *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    if (!self->is_logging) {
        return;
    }
    
    self->is_logging = FALSE;
    
    if (self->timeout_id > 0) {
        g_source_remove (self->timeout_id);
        self->timeout_id = 0;
    }
    
    /* Write remaining entries */
    write_log_entry (self);
    
    /* Close file */
    if (self->output_stream) {
        g_output_stream_close (G_OUTPUT_STREAM (self->output_stream), NULL, NULL);
        g_object_unref (self->output_stream);
        self->output_stream = NULL;
    }
    
    if (self->file) {
        g_object_unref (self->file);
        self->file = NULL;
    }
}

void
usb_tester_monitor_data_logger_log_reading (UsbTesterMonitorDataLogger *self,
                                              gdouble voltage,
                                              gdouble current,
                                              gdouble power)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    UsbTesterMonitorLogEntry *entry = g_new0 (UsbTesterMonitorLogEntry, 1);
    entry->voltage = voltage;
    entry->current = current;
    entry->power = power;
    entry->timestamp = g_get_monotonic_time ();
    
    self->entries = g_list_append (self->entries, entry);
    
    /* Notify callback */
    if (self->callback) {
        gchar *message = g_strdup_printf ("Logged: %.3f V, %.3f A, %.3f W",
                                         voltage, current, power);
        self->callback (self, message, self->user_data);
        g_free (message);
    }
}

void
usb_tester_monitor_data_logger_set_interval (UsbTesterMonitorDataLogger *self,
                                                guint interval_ms)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    self->interval_ms = interval_ms;
}

void
usb_tester_monitor_data_logger_set_callback (UsbTesterMonitorDataLogger *self,
                                               UsbTesterMonitorLogCallback callback,
                                               gpointer user_data,
                                               GDestroyNotify notify)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    self->callback = callback;
    self->user_data = user_data;
    self->notify = notify;
}

gboolean
usb_tester_monitor_data_logger_export_to_csv (UsbTesterMonitorDataLogger *self,
                                               const gchar *filename,
                                               GError **error)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self), FALSE);
    
    GFile *export_file = g_file_new_for_path (filename);
    GError *local_error = NULL;
    GFileOutputStream *stream = g_file_create (export_file, G_FILE_CREATE_NONE, NULL, &local_error);
    
    if (!stream) {
        g_propagate_error (error, local_error);
        g_object_unref (export_file);
        return FALSE;
    }
    
    /* Write CSV header */
    gchar *header = "timestamp,voltage,current,power\n";
    gsize bytes_written;
    g_output_stream_write_all (G_OUTPUT_STREAM (stream), header, strlen (header), &bytes_written, NULL, &local_error);
    
    if (local_error) {
        g_propagate_error (error, local_error);
        g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
        g_object_unref (stream);
        g_object_unref (export_file);
        return FALSE;
    }
    
    /* Write data */
    for (GList *item = self->entries; item; item = item->next) {
        UsbTesterMonitorLogEntry *entry = item->data;
        GDateTime *dt = g_date_time_new_from_unix_local (entry->timestamp / 1000000);
        gchar *timestamp_str = g_date_time_format (dt, "%Y-%m-%d %H:%M:%S");
        
        gchar *line = g_strdup_printf ("%s,%.3f,%.3f,%.3f\n",
                                       timestamp_str,
                                       entry->voltage,
                                       entry->current,
                                       entry->power);
        
        g_output_stream_write_all (G_OUTPUT_STREAM (stream), line, strlen (line), &bytes_written, NULL, &local_error);
        
        g_free (line);
        g_free (timestamp_str);
        g_date_time_unref (dt);
        
        if (local_error) {
            g_propagate_error (error, local_error);
            break;
        }
    }
    
    g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
    g_object_unref (stream);
    g_object_unref (export_file);
    
    return TRUE;
}

GList *
usb_tester_monitor_data_logger_get_entries (UsbTesterMonitorDataLogger *self)
{
    g_return_val_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self), NULL);
    
    return self->entries;
}

void
usb_tester_monitor_data_logger_clear (UsbTesterMonitorDataLogger *self)
{
    g_return_if_fail (USB_TESTER_MONITOR_IS_DATA_LOGGER (self));
    
    g_list_free_full (self->entries, g_free);
    self->entries = NULL;
}
