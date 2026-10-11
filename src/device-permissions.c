#include "device-permissions.h"
#include <gio/gio.h>
#include <glib.h>

#define RULE_TMP_PATH "/tmp/usb-tester-monitor-fnb48.rules"
#define RULE_DEST_PATH "/etc/udev/rules.d/90-usb-tester-monitor-fnb48.rules"

static const gchar *rule_contents =
    "# Fnirsi FNB-48 (0483:003a) - installed by USB Tester Monitor\n"
    "SUBSYSTEM==\"hidraw\", ATTRS{idVendor}==\"0483\", ATTRS{idProduct}==\"003a\", "
    "MODE=\"0660\", GROUP=\"dialout\", TAG+=\"uaccess\"\n";

static const gchar *install_command =
    "install -m 644 " RULE_TMP_PATH " " RULE_DEST_PATH
    " && udevadm control --reload"
    " && udevadm trigger --subsystem-match=hidraw";

typedef struct {
    UsbTesterMonitorPermissionsGrantedCallback callback;
    gpointer user_data;
} GrantData;

static void
on_pkexec_finished (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
    GSubprocess *process = G_SUBPROCESS (source);
    GrantData *data = user_data;

    gboolean success = g_subprocess_wait_check_finish (process, result, NULL);

    if (data->callback) {
        data->callback (success, data->user_data);
    }

    g_object_unref (process);
    g_free (data);
}

static void
grant_failed (GrantData *data)
{
    if (data->callback) {
        data->callback (FALSE, data->user_data);
    }
    g_free (data);
}

void
usb_tester_monitor_device_permissions_grant_fnirsi_fnb48_async (
    UsbTesterMonitorPermissionsGrantedCallback callback,
    gpointer user_data)
{
    GrantData *data = g_new (GrantData, 1);
    data->callback = callback;
    data->user_data = user_data;

    gchar *pkexec = g_find_program_in_path ("pkexec");
    if (pkexec == NULL) {
        grant_failed (data);
        return;
    }
    g_free (pkexec);

    GError *error = NULL;
    if (!g_file_set_contents (RULE_TMP_PATH, rule_contents, -1, &error)) {
        g_error_free (error);
        grant_failed (data);
        return;
    }

    GSubprocess *process = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, &error,
                                             "pkexec", "/bin/sh", "-c",
                                             install_command, NULL);
    if (process == NULL) {
        g_error_free (error);
        grant_failed (data);
        return;
    }

    g_subprocess_wait_check_async (process, NULL, on_pkexec_finished, data);
}
