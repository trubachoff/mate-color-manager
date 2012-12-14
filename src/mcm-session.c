/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <libmatenotify/notify.h>

#include "egg-debug.h"
#include "mcm-dbus.h"
#include "mcm-client.h"
#include "mcm-device.h"
#include "mcm-utils.h"
#include "org.mate.ColorManager.h"

static GMainLoop *loop = NULL;
static GSettings *settings = NULL;

#define MCM_SESSION_IDLE_EXIT		60 /* seconds */
#define MCM_SESSION_NOTIFY_TIMEOUT	30000 /* ms */

/**
 * mcm_session_object_register:
 * @connection: What we want to register to
 * @object: The GObject we want to register
 *
 * Return value: success
 **/
static gboolean
mcm_session_object_register (DBusGConnection *connection, GObject *object)
{
	DBusGProxy *bus_proxy = NULL;
	GError *error = NULL;
	guint request_name_result;
	gboolean ret;

	/* connect to the bus */
	bus_proxy = dbus_g_proxy_new_for_name (connection, DBUS_SERVICE_DBUS,
					       DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	/* get our name */
	ret = dbus_g_proxy_call (bus_proxy, "RequestName", &error,
				 G_TYPE_STRING, MCM_DBUS_SERVICE,
				 G_TYPE_UINT, DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
					      DBUS_NAME_FLAG_REPLACE_EXISTING |
					      DBUS_NAME_FLAG_DO_NOT_QUEUE,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &request_name_result,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		egg_warning ("RequestName failed: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* free the bus_proxy */
	g_object_unref (G_OBJECT (bus_proxy));

	/* already running */
	if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		return FALSE;

	dbus_g_object_type_install_info (MCM_TYPE_DBUS, &dbus_glib_mcm_dbus_object_info);
	dbus_g_error_domain_register (MCM_DBUS_ERROR, NULL, MCM_DBUS_TYPE_ERROR);
	dbus_g_connection_register_g_object (connection, MCM_DBUS_PATH, object);

	return TRUE;
}

/**
 * mcm_session_check_idle_cb:
 **/
static gboolean
mcm_session_check_idle_cb (McmDbus *dbus)
{
	guint idle;

	/* get the idle time */
	idle = mcm_dbus_get_idle_time (dbus);
	if (idle > MCM_SESSION_IDLE_EXIT) {
		egg_debug ("exiting loop as idle");
		g_main_loop_quit (loop);
		return FALSE;
	}
	/* continue to poll */
	return TRUE;
}

/**
 * mcm_session_notify_cb:
 **/
static void
mcm_session_notify_cb (NotifyNotification *notification, gchar *action, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	if (g_strcmp0 (action, "display") == 0) {
		g_settings_set_int (settings, MCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD, 0);
	} else if (g_strcmp0 (action, "printer") == 0) {
		g_settings_set_int (settings, MCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD, 0);
	} else if (g_strcmp0 (action, "recalibrate") == 0) {
		ret = g_spawn_command_line_async ("mcm-prefs", &error);
		if (!ret) {
			egg_warning ("failed to spawn: %s", error->message);
			g_error_free (error);
		}
	}
}

/**
 * mcm_session_notify_recalibrate:
 **/
static gboolean
mcm_session_notify_recalibrate (const gchar *title, const gchar *message, McmDeviceKind kind)
{
	gboolean ret;
	GError *error = NULL;
	NotifyNotification *notification;

	/* show a bubble */
	notification = notify_notification_new (title, message, MCM_STOCK_ICON, NULL);
	notify_notification_set_timeout (notification, MCM_SESSION_NOTIFY_TIMEOUT);
	notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);

	/* TRANSLATORS: button: this is to open MCM */
	notify_notification_add_action (notification, "recalibrate", _("Recalibrate now"), mcm_session_notify_cb, NULL, NULL);

	/* TRANSLATORS: button: this is to ignore the recalibrate notifications */
	notify_notification_add_action (notification, mcm_device_kind_to_string (kind), _("Ignore"), mcm_session_notify_cb, NULL, NULL);

	ret = notify_notification_show (notification, &error);
	if (!ret) {
		egg_warning ("failed to show notification: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * mcm_session_notify_device:
 **/
static void
mcm_session_notify_device (McmDevice *device)
{
	gchar *message;
	const gchar *title;
	McmDeviceKind kind;
	glong since;
	GTimeVal timeval;
	gint threshold;

	/* get current time */
	g_get_current_time (&timeval);

	/* TRANSLATORS: this is when the device has not been recalibrated in a while */
	title = _("Recalibration required");

	/* check we care */
	kind = mcm_device_get_kind (device);
	if (kind == MCM_DEVICE_KIND_DISPLAY) {

		/* get from GSettings */
		threshold = g_settings_get_int (settings, MCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the display has not been recalibrated in a while */
		message = g_strdup_printf (_("The display '%s' should be recalibrated soon."), mcm_device_get_title (device));
	} else {

		/* get from GSettings */
		threshold = g_settings_get_int (settings, MCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the printer has not been recalibrated in a while */
		message = g_strdup_printf (_("The printer '%s' should be recalibrated soon."), mcm_device_get_title (device));
	}

	/* check if we need to notify */
	since = timeval.tv_sec - mcm_device_get_modified_time (device);
	if (threshold > since)
		mcm_session_notify_recalibrate (title, message, kind);
	g_free (message);
}

/**
 * mcm_session_added_cb:
 **/
static void
mcm_session_added_cb (McmClient *client, McmDevice *device, gpointer user_data)
{
	McmDeviceKind kind;
	const gchar *profile;
	gchar *basename = NULL;
	gboolean allow_notifications;

	/* check we care */
	kind = mcm_device_get_kind (device);
	if (kind != MCM_DEVICE_KIND_DISPLAY &&
	    kind != MCM_DEVICE_KIND_PRINTER)
		return;

	/* ensure we have a profile */
	profile = mcm_device_get_profile_filename (device);
	if (profile == NULL) {
		egg_debug ("no profile set for %s", mcm_device_get_id (device));
		goto out;
	}

	/* ensure it's a profile generated by us */
	basename = g_path_get_basename (profile);
	if (!g_str_has_prefix (basename, "MCM")) {
		egg_debug ("not a MCM profile for %s: %s", mcm_device_get_id (device), profile);
		goto out;
	}

	/* do we allow notifications */
	allow_notifications = g_settings_get_boolean (settings, MCM_SETTINGS_SHOW_NOTIFICATIONS);
	if (!allow_notifications)
		goto out;

	/* handle device */
	mcm_session_notify_device (device);
out:
	g_free (basename);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean no_timed_exit = FALSE;
	McmDbus *dbus = NULL;
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 0;
	DBusGConnection *connection;
	McmClient *client = NULL;

	const GOptionEntry options[] = {
		{ "no-timed-exit", '\0', 0, G_OPTION_ARG_NONE, &no_timed_exit,
		  _("Do not exit after the request has been processed"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();
	g_type_init ();
	notify_init ("mate-color-manager");

	/* TRANSLATORS: program name, a session wide daemon to watch for updates and changing system state */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	gtk_init (&argc, &argv);

	/* get the settings */
	settings = g_settings_new (MCM_SETTINGS_SCHEMA);

	/* monitor devices as they are added */
	client = mcm_client_new ();
	g_signal_connect (client, "added", G_CALLBACK (mcm_session_added_cb), NULL);

	/* create new objects */
	dbus = mcm_dbus_new ();
	loop = g_main_loop_new (NULL, FALSE);

	/* get the bus */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		egg_warning ("%s", error->message);
		g_error_free (error);
		retval = 1;
		goto out;
	}

	/* try to register */
	ret = mcm_session_object_register (connection, G_OBJECT (dbus));
	if (!ret) {
		egg_warning ("failed to replace running instance.");
		retval = 1;
		goto out;
	}

	/* only timeout if we have specified iton the command line */
	if (!no_timed_exit)
		g_timeout_add_seconds (5, (GSourceFunc) mcm_session_check_idle_cb, dbus);

	/* wait */
	g_main_loop_run (loop);
out:
	g_object_unref (settings);
	g_object_unref (client);
	g_main_loop_unref (loop);
	g_object_unref (dbus);
	return retval;
}

