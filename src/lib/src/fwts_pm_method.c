/*
 * Copyright (C) 2014-2026 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>
#include <gio/gio.h>

#include "fwts.h"
#include "fwts_pm_method.h"

#if FWTS_ENABLE_LOGIND
/*
 *  logind_do()
 *  call Logind to perform an action
 */
static gboolean logind_do(gpointer data)
{
	GError *error = NULL;
	fwts_pm_method_vars *fwts_settings = (fwts_pm_method_vars *)data;

	/*
	 * If the loop is not running, return TRUE so as to repeat
	 * the operation
	 */
	if (g_main_loop_is_running (fwts_settings->gmainloop)) {
		GVariant *reply;
		bool is_s3;
		char buffer[50];

		fwts_log_info(fwts_settings->fw, "Requesting %s action\n",
			fwts_settings->action);

		is_s3 = (strcmp(fwts_settings->action, PM_SUSPEND_LOGIND) == 0 ||
			 strcmp(fwts_settings->action, PM_SUSPEND_HYBRID_LOGIND) == 0);

		snprintf(buffer, sizeof(buffer), "Starting fwts %s\n",
			is_s3 ? "suspend" : "hibernate");
		(void)fwts_klog_write(fwts_settings->fw, buffer);
		snprintf(buffer, sizeof(buffer), "%s\n",
			fwts_settings->action);
		(void)fwts_klog_write(fwts_settings->fw, buffer);
		(void)time(&(fwts_settings->t_start));

		reply = g_dbus_proxy_call_sync(fwts_settings->logind_proxy,
			fwts_settings->action,
			g_variant_new("(b)", FALSE),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

		if (reply != NULL) {
			g_variant_unref(reply);
		} else {
			fwts_log_error(fwts_settings->fw,
				"Error from Logind: %s\n",
				error->message);
			g_error_free(error);
		}
		return FALSE;

	}
	fwts_log_info(fwts_settings->fw, "Glib loop not ready\n");
	return TRUE;
}

/*
 *  logind_signal_subscribe()
 *  subscribe to a signal coming from Logind
 */
static guint logind_signal_subscribe(
	GDBusConnection *connection,
	const gchar *logind_signal,
	GDBusSignalCallback callback,
	gpointer user_data)
{
	return g_dbus_connection_signal_subscribe (connection,
		"org.freedesktop.login1", /* sender */
		"org.freedesktop.login1.Manager",
		logind_signal,
		"/org/freedesktop/login1",
		NULL, /* arg0 */
		G_DBUS_SIGNAL_FLAGS_NONE,
		callback, user_data, NULL);
}

/*
 *  logind_signal_unsubscribe()
 *  unsubscribe from a signal coming from Logind
 */
static void logind_signal_unsubscribe(
	GDBusConnection *connection,
	guint subscription_id)
{
	g_dbus_connection_signal_unsubscribe(connection, subscription_id);
}

/*
 *  logind_on_signal()
 *  callback to handle suspend and resume events
 */
static void logind_on_signal(
	GDBusConnection *connection,
	const gchar *sender_name,
	const gchar *object_path,
	const gchar *interface_name,
	const gchar *signal_name,
	GVariant *parameters,
	gpointer user_data)
{
	gboolean status;
	fwts_pm_method_vars *fwts_settings = (fwts_pm_method_vars *)user_data;

	/* Prevent -Werror=unused-parameter from complaining */
	FWTS_UNUSED(connection);
	FWTS_UNUSED(sender_name);
	FWTS_UNUSED(object_path);
	FWTS_UNUSED(interface_name);
	FWTS_UNUSED(signal_name);

	if (!g_variant_is_of_type(parameters, G_VARIANT_TYPE ("(b)"))) {
		fwts_log_error(fwts_settings->fw, "Suspend type %s\n",
			g_variant_get_type_string(parameters));
		return;
	} else {
		g_variant_get(parameters, "(b)", &status);

		if (!status) {

			time(&(fwts_settings->t_end));
			(void)fwts_klog_write(fwts_settings->fw,
				FWTS_RESUME "\n");
			(void)fwts_klog_write(fwts_settings->fw,
				"Finished fwts resume\n");
			/*
			 * Let's give the system some time to get back from S3
			 * or Logind will refuse to suspend and shoot both
			 * events without doing anything
			 */
			if (fwts_settings->min_delay < 3) {
				fwts_log_info(fwts_settings->fw,
					"Skipping the minimum delay (%d) and "
					"using a 3 seconds delay instead\n",
					fwts_settings->min_delay);
				sleep(3);
			}
			g_main_loop_quit(fwts_settings->gmainloop);
		}
	}
}

/*
 *  logind_can_do_action()
 *  test supported Logind actions that reply with a string
 */
static bool logind_can_do_action(
	fwts_pm_method_vars *fwts_settings,
	const char* action)
{
	GVariant *reply;
	GError *error = NULL;
	bool status = false;
	gchar *response;

	if (glib_check_version(2, 26, 0) != NULL)
		return false;

	if (fwts_logind_init_proxy(fwts_settings) != 0)
		return false;

	reply = g_dbus_proxy_call_sync(fwts_settings->logind_proxy,
		action, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	if (reply != NULL) {
		if (!g_variant_is_of_type(reply, G_VARIANT_TYPE ("(s)"))) {
			fwts_log_error(fwts_settings->fw,
				"Unexpected response to %s action: %s\n",
				action,
				g_variant_get_type_string (reply));

			g_variant_unref(reply);
			return status;
		}

		g_variant_get(reply, "(&s)", &response);
		fwts_log_info(fwts_settings->fw, "Response to %s is %s\n",
			action, response);

		if (strcmp(response, "challenge") == 0) {
			fwts_log_error(fwts_settings->fw,
				"%s action available only after "
				"authorisation\n", action);
		} else if (strcmp(response, "yes") == 0) {
			fwts_log_info(fwts_settings->fw,
				"User allowed to execute the %s action\n",
				action);
			status = true;
		} else if (strcmp(response, "no") == 0) {
			fwts_log_error(fwts_settings->fw,
				"User not allowed to execute the %s action\n",
				action);
		} else if (strcmp(response, "na") == 0) {
			fwts_log_error(fwts_settings->fw,
				"Hardware doesn't support %s action\n",
				action);
		}

		g_variant_unref(reply);
	} else {
		fwts_log_error(fwts_settings->fw,
			"Invalid response from Logind on %s action\n",
			action);
		g_error_free(error);
	}
	return status;
}

/*
 *  fwts_logind_init_proxy()
 *  initialise the Dbus proxy for Logind Manager
 */
int fwts_logind_init_proxy(fwts_pm_method_vars *fwts_settings)
{
	int status = 0;

	if (fwts_settings->logind_connection == NULL)
		fwts_settings->logind_connection =
			g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

	if (fwts_settings->logind_connection == NULL) {
		status = 1;
		fwts_log_error(fwts_settings->fw,
			"Cannot establish a connection to Dbus\n");
		goto out;
	}

	if (fwts_settings->logind_proxy == NULL) {
		fwts_settings->logind_proxy =
			g_dbus_proxy_new_sync(fwts_settings->logind_connection,
				G_DBUS_PROXY_FLAGS_NONE,
				NULL, "org.freedesktop.login1",
				"/org/freedesktop/login1",
				"org.freedesktop.login1.Manager",
				NULL, NULL);
	}

	if (fwts_settings->logind_proxy == NULL) {
		status = 1;
		fwts_log_error(fwts_settings->fw,
			"Cannot establish a connection to login1.Manager\n");
		goto out;
	}

out:
	return status;
}

/*
 *  fwts_logind_wait_for_resume_from_action()
 *  start Glib mainloop and listen to suspend/resume events coming from Logind
 *  exit the loop and return the duration after an event.
 *
 *  "action" is one of the following actions supported by Logind:
 *    "Suspend", "Hibernate", "HybridSleep"
 *  "minimum_delay" is the minimum delay to use before the function returns
 *    the function after resume
 */
int fwts_logind_wait_for_resume_from_action(
	fwts_pm_method_vars *fwts_settings,
	const char *action,
	int minimum_delay)
{
	guint subscription_id = 0;
	int duration = 0;

	/* Check that the action is supported */
	if (!(strcmp(action, PM_SUSPEND_LOGIND) == 0 ||
		strcmp(action, PM_SUSPEND_HYBRID_LOGIND) == 0 ||
		strcmp(action, PM_HIBERNATE_LOGIND) == 0)) {
		fwts_log_error(fwts_settings->fw,
			"Unknown logind action: %s\n", action);
		return 0;
	}

	/* Initialise the proxy */
	if (fwts_logind_init_proxy(fwts_settings) != 0) {
		fwts_log_error(fwts_settings->fw,
			"Failed to initialise logind proxy\n");
		return 0;
	}

	/* Set the action to perform */
	fwts_settings->action = strdup(action);
	if (!fwts_settings->action) {
		fwts_log_error(fwts_settings->fw,
			"Failed to initialise logind action\n");
		return 0;
	}

	/* Set the minimum delay (this is needed for both S3 and S4) */
	fwts_settings->min_delay = minimum_delay;

	/* Subscribe to the signal that Logind sends on resume */
	subscription_id =
		logind_signal_subscribe(fwts_settings->logind_connection,
			"PrepareForSleep", logind_on_signal, fwts_settings);

	/* Start the main loop */
	fwts_settings->gmainloop = g_main_loop_new(NULL, FALSE);
	if (fwts_settings->gmainloop) {
		g_timeout_add(1, logind_do, fwts_settings);

		g_main_loop_run(fwts_settings->gmainloop);
		duration = (int)(fwts_settings->t_end - fwts_settings->t_start);

		/* Optional, as it will be freed together with the struct */
		g_main_loop_unref(fwts_settings->gmainloop);
		fwts_settings->gmainloop = NULL;
	} else {
		fwts_log_error(fwts_settings->fw,
			"Failed to start glib mainloop\n");
	}

	/* Unsubscribe from the signal */
	logind_signal_unsubscribe(fwts_settings->logind_connection,
		subscription_id);

	return duration;
}

/*
 *  fwts_logind_can_suspend()
 *  return a boolean that states whether suspend is a supported action or not
 */
bool fwts_logind_can_suspend(fwts_pm_method_vars *fwts_settings)
{
	return logind_can_do_action(fwts_settings, "CanSuspend");
}

/*
 *  fwts_logind_can_hybrid_suspend()
 *  return a boolean that states whether hybrid suspend is a
 *  supported action or not
 */
bool fwts_logind_can_hybrid_suspend(fwts_pm_method_vars *fwts_settings)
{
	return logind_can_do_action(fwts_settings, "CanHybridSleep");
}

/*
 *  fwts_logind_can_hibernate()
 *  return a boolean that states whether hibernate is a supported action or not
 */
bool fwts_logind_can_hibernate(fwts_pm_method_vars *fwts_settings)
{
	return logind_can_do_action(fwts_settings, "CanHibernate");
}
#endif

/*
 *  fwts_sysfs_can_suspend()
 *  return a boolean that states whether suspend is a supported action or not
 */
bool fwts_sysfs_can_suspend(const fwts_pm_method_vars *fwts_settings)
{
	return fwts_file_first_line_contains_string(fwts_settings->fw,
		"/sys/power/state", "mem");
}

/*
 *  fwts_sysfs_can_hybrid_suspend()
 *  return a boolean that states whether hybrid suspend is a supported action or not
 */
bool fwts_sysfs_can_hybrid_suspend(const fwts_pm_method_vars *fwts_settings)
{
	bool status;

	status = fwts_file_first_line_contains_string(fwts_settings->fw,
		"/sys/power/state", "disk");

	if (!status)
		return FALSE;

	return fwts_file_first_line_contains_string(fwts_settings->fw,
		"/sys/power/disk",
		"suspend");
}

/*
 *  fwts_sysfs_can_hibernate()
 *  return a boolean that states whether hibernate is a supported action or not
 */
bool fwts_sysfs_can_hibernate(const fwts_pm_method_vars *fwts_settings)
{
	return fwts_file_first_line_contains_string(fwts_settings->fw,
		"/sys/power/state", "disk");
}

/*
 *  fwts_sysfs_do_suspend()
 *  enter either S3 or hybrid S3
 *  return the exit status
 */
int fwts_sysfs_do_suspend(
	const fwts_pm_method_vars *fwts_settings,
	const bool s3_hybrid)
{
	int status;

	if (s3_hybrid) {
		status = fwts_write_string_file(fwts_settings->fw,
			"/sys/power/disk", "suspend");

		if (status != FWTS_OK)
			return status;

		status = fwts_write_string_file(fwts_settings->fw,
				"/sys/power/state", "disk");
	} else {
		status = fwts_write_string_file(fwts_settings->fw,
				"/sys/power/state", "mem");
	}
	return status;
}

/*
 *  fwts_sysfs_do_hibernate()
 *  enter S4
 *  return the exit status
 */
int fwts_sysfs_do_hibernate(const fwts_pm_method_vars *fwts_settings)
{
	return fwts_write_string_file(fwts_settings->fw,
		"/sys/power/state", "disk");
}

void free_pm_method_vars(fwts_pm_method_vars *var)
{
	if (!var)
		return;
#if FWTS_ENABLE_LOGIND
	if (var->logind_proxy)
		g_object_unref(var->logind_proxy);
	if (var->logind_connection)
		g_object_unref(var->logind_connection);
	if (var->gmainloop)
		g_main_loop_unref(var->gmainloop);
	if (var->action)
		free(var->action);
#endif
	free(var);
}
