#include "qianli-a3.h"
#include "device-protocol.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

struct _UsbTesterMonitorQianliA3 {
    UsbTesterMonitorDeviceProtocol parent_instance;
    
    /* Qianli A3 specific state */
    gboolean stream_enabled;
    guint32 last_current_raw;
    guint32 last_voltage_raw;
    
    /* Buffer for parsing */
    GByteArray *rx_buffer;
};

G_DEFINE_FINAL_TYPE (UsbTesterMonitorQianliA3, usb_tester_monitor_qianli_a3, 
                     USB_TESTER_MONITOR_TYPE_DEVICE_PROTOCOL)

static void
usb_tester_monitor_qianli_a3_finalize (GObject *object)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (object);
    
    if (self->rx_buffer) {
        g_byte_array_unref (self->rx_buffer);
    }
    
    G_OBJECT_CLASS (usb_tester_monitor_qianli_a3_parent_class)->finalize (object);
}

static void
usb_tester_monitor_qianli_a3_class_init (UsbTesterMonitorQianliA3Class *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    
    object_class->finalize = usb_tester_monitor_qianli_a3_finalize;
}

static void
usb_tester_monitor_qianli_a3_init (UsbTesterMonitorQianliA3 *self)
{
    self->stream_enabled = FALSE;
    self->last_current_raw = 0;
    self->last_voltage_raw = 0;
    self->rx_buffer = g_byte_array_new ();
}

/* Packet building helper */
static GByteArray *
build_qianli_packet (guint8 model, guint8 cmd, const gchar *payload, gsize payload_len)
{
    GByteArray *packet = g_byte_array_new ();
    
    /* Magic byte */
    g_byte_array_append (packet, (guint8[]){QIANLI_A3_MAGIC_BYTE}, 1);
    
    /* Length (little endian) */
    guint16 length = payload_len;
    g_byte_array_append (packet, (guint8[]){length & 0xFF, (length >> 8) & 0xFF}, 2);
    
    /* Model */
    g_byte_array_append (packet, (guint8[]){model}, 1);
    
    /* Command */
    g_byte_array_append (packet, (guint8[]){cmd}, 1);
    
    /* Parameters (usually 0x00 0x00) */
    g_byte_array_append (packet, (guint8[]){0x00, 0x00}, 2);
    
    /* Checksum (XOR of payload) */
    guint8 checksum = 0;
    if (payload_len > 0) {
        for (gsize i = 0; i < payload_len; i++) {
            checksum ^= payload[i];
        }
    }
    g_byte_array_append (packet, (guint8[]){checksum}, 1);
    
    /* Payload */
    if (payload_len > 0) {
        g_byte_array_append (packet, (guint8 *)payload, payload_len);
    }
    
    return packet;
}

/* Calculate checksum for XOR */
static guint8
calculate_xor_checksum (const guint8 *data, gsize len)
{
    guint8 checksum = 0;
    for (gsize i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/* Parse incoming data */
static gboolean
parse_qianli_packet (UsbTesterMonitorQianliA3 *self, const guint8 *data, gsize len)
{
    GByteArray *buffer = self->rx_buffer;
    
    /* Add new data to buffer */
    g_byte_array_append (buffer, data, len);
    
    /* Try to parse packets */
    while (buffer->len >= 8) {
        /* Check magic byte */
        if (buffer->data[0] != QIANLI_A3_MAGIC_BYTE) {
            /* Remove invalid byte */
            g_byte_array_remove_index (buffer, 0);
            continue;
        }
        
        /* Parse length (little endian) */
        guint16 payload_len = buffer->data[1] | (buffer->data[2] << 8);
        guint total_len = 8 + payload_len;
        
        /* Check if we have complete packet */
        if (buffer->len < total_len) {
            break; /* Need more data */
        }
        
        /* Verify checksum */
        guint8 header_checksum = buffer->data[7];
        guint8 calculated_checksum = 0;
        
        if (payload_len > 0) {
            calculated_checksum = calculate_xor_checksum (&buffer->data[8], payload_len);
        }
        
        if (header_checksum != calculated_checksum) {
            /* Invalid checksum, remove magic byte and try again */
            g_byte_array_remove_index (buffer, 0);
            continue;
        }
        
        /* Valid packet */
        guint8 model = buffer->data[3];
        guint8 cmd = buffer->data[4];
        const guint8 *payload = &buffer->data[8];
        
        /* Handle data stream packet (Model 0x04, Cmd 0x05, 8-byte payload) */
        if (model == QIANLI_A3_MODEL_ID && cmd == QIANLI_A3_CMD_ENABLE_STREAM && payload_len == 8) {
            guint32 current_raw, voltage_raw;
            
            /* Parse big-endian values */
            current_raw = ((guint32)payload[0] << 24) | 
                          ((guint32)payload[1] << 16) | 
                          ((guint32)payload[2] << 8) | 
                          (guint32)payload[3];
            
            voltage_raw = ((guint32)payload[4] << 24) | 
                          ((guint32)payload[5] << 16) | 
                          ((guint32)payload[6] << 8) | 
                          (guint32)payload[7];
            
            /* Convert to actual values */
            gdouble current = current_raw / 10000.0;
            gdouble voltage = voltage_raw / 1000.0;
            gdouble power = voltage * current;
            guint64 timestamp = g_get_monotonic_time ();
            
            /* Store values */
            self->last_current_raw = current_raw;
            self->last_voltage_raw = voltage_raw;
            
            /* Notify callback if set */
            if (self->parent_instance.data_callback) {
                UsbTesterMonitorReading reading = {
                    .voltage = voltage,
                    .current = current,
                    .power = power,
                    .timestamp = timestamp
                };
                self->parent_instance.data_callback (
                    USB_TESTER_MONITOR_DEVICE_PROTOCOL (&self->parent_instance),
                    &reading,
                    self->parent_instance.user_data
                );
            }
        }
        
        /* Handle version response (Model 0x01) */
        else if (model == 0x01) {
            /* Parse version information */
            /* Format: 64 bytes for text info, 24 bytes for UID */
            if (payload_len >= 64) {
                gchar brand[25] = {0};
                gchar model_str[25] = {0};
                gchar hw_version[9] = {0};
                gchar fw_version[9] = {0};
                
                memcpy (brand, &payload[0], 24);
                memcpy (model_str, &payload[24], 24);
                memcpy (hw_version, &payload[48], 8);
                memcpy (fw_version, &payload[56], 8);
                
                /* Clean up strings */
                for (int i = 0; i < 24; i++) {
                    if (brand[i] == '\0') break;
                }
                for (int i = 0; i < 24; i++) {
                    if (model_str[i] == '\0') break;
                }
                
                g_free (self->parent_instance.device_name);
                self->parent_instance.device_name = g_strdup (model_str);
                
                g_free (self->parent_instance.device_info);
                self->parent_instance.device_info = g_strdup_printf (
                    "Brand: %s\nModel: %s\nHW: %s\nFW: %s",
                    brand, model_str, hw_version, fw_version
                );
            }
        }
        
        /* Remove processed packet from buffer */
        g_byte_array_remove_range (buffer, 0, total_len);
    }
    
    return TRUE;
}

/* Send command to device */
static gboolean
send_qianli_command (UsbTesterMonitorQianliA3 *self, 
                     guint8 model, guint8 cmd, 
                     const gchar *payload, gsize payload_len,
                     GError **error)
{
    UsbTesterMonitorDeviceProtocol *protocol = USB_TESTER_MONITOR_DEVICE_PROTOCOL (self);
    
    if (protocol->state != USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                     "Device not connected");
        return FALSE;
    }
    
    GByteArray *packet = build_qianli_packet (model, cmd, payload, payload_len);
    
    /* Write packet to serial port */
    gsize bytes_written;
    GIOStatus status = g_io_channel_write_chars (protocol->channel,
                                                  (gchar *)packet->data,
                                                  packet->len,
                                                  &bytes_written,
                                                  error);
    
    g_byte_array_unref (packet);
    
    if (status != G_IO_STATUS_NORMAL) {
        return FALSE;
    }
    
    return TRUE;
}

static gboolean
usb_tester_monitor_qianli_a3_connect (UsbTesterMonitorDeviceProtocol *protocol,
                                       GError **error)
{
    /* Chain up to parent connect */
    if (!USB_TESTER_MONITOR_DEVICE_PROTOCOL_CLASS (usb_tester_monitor_qianli_a3_parent_class)->connect (protocol, error)) {
        return FALSE;
    }
    
    /* Send ping command */
    gchar ping_payload = 0x00;
    send_qianli_command (USB_TESTER_MONITOR_QIANLI_A3 (protocol),
                         0x01, 0x01, &ping_payload, 1, error);
    
    return TRUE;
}

static void
usb_tester_monitor_qianli_a3_start_streaming (UsbTesterMonitorDeviceProtocol *protocol)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (protocol);
    
    /* Send enable stream command */
    gchar enable_payload = QIANLI_A3_PAYLOAD_ENABLE;
    GError *error = NULL;
    
    if (send_qianli_command (self, QIANLI_A3_MODEL_ID, QIANLI_A3_CMD_ENABLE_STREAM,
                             &enable_payload, 1, &error)) {
        self->stream_enabled = TRUE;
        protocol->state = USB_TESTER_MONITOR_PROTOCOL_STATE_STREAMING;
    } else {
        g_warning ("Failed to enable stream: %s", error->message);
        g_error_free (error);
    }
}

static void
usb_tester_monitor_qianli_a3_stop_streaming (UsbTesterMonitorDeviceProtocol *protocol)
{
    UsbTesterMonitorQianliA3 *self = USB_TESTER_MONITOR_QIANLI_A3 (protocol);
    
    /* Send disable stream command */
    gchar disable_payload = QIANLI_A3_PAYLOAD_DISABLE;
    GError *error = NULL;
    
    if (send_qianli_command (self, QIANLI_A3_MODEL_ID, QIANLI_A3_CMD_DISABLE_STREAM,
                             &disable_payload, 1, &error)) {
        self->stream_enabled = FALSE;
        protocol->state = USB_TESTER_MONITOR_PROTOCOL_STATE_CONNECTED;
    } else {
        g_warning ("Failed to disable stream: %s", error->message);
        g_error_free (error);
    }
}

UsbTesterMonitorQianliA3 *
usb_tester_monitor_qianli_a3_new (const gchar *device_path)
{
    return g_object_new (USB_TESTER_MONITOR_TYPE_QIANLI_A3,
                        "device-path", device_path,
                        NULL);
}
