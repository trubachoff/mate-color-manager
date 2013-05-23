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
#include <gtk/gtk.h>
#include <locale.h>
#ifdef HAVE_NOTIFY
#include <libnotify/notify.h>
#endif
#include "egg-debug.h"
#include "mcm-client.h"
#include "mcm-device-xrandr.h"
#include "mcm-exif.h"
#include "mcm-device.h"
#include "mcm-utils.h"
#include "mcm-client.h"
#include "mcm-profile-store.h"

static GMainLoop *loop = NULL;
static GSettings *settings = NULL;
static GDBusNodeInfo *introspection = NULL;
static McmClient *client = NULL;
static McmProfileStore *profile_store = NULL;
static GTimer *timer = NULL;
static GDBusConnection *connection = NULL;

#define MCM_SESSION_IDLE_EXIT		60 /* seconds */
#define MCM_SESSION_NOTIFY_TIMEOUT	30000 /* ms */

/**
 * mcm_session_check_idle_cb:
 **/
static gboolean
mcm_session_check_idle_cb (gpointer user_data)
{
	guint idle;

	/* get the idle time */
	idle = (guint) g_timer_elapsed (timer, NULL);
	egg_debug ("we've been idle for %is", idle);
	if (idle > MCM_SESSION_IDLE_EXIT) {
		egg_debug ("exiting loop as idle");
		g_main_loop_quit (loop);
		return FALSE;
	}
	/* continue to poll */
	return TRUE;
}


#ifdef HAVE_NOTIFY
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
#endif

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
mcm_session_added_cb (McmClient *client_, McmDevice *device, gpointer user_data)
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
	profile = mcm_device_get_default_profile_filename (device);
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
 * mcm_session_get_profile_for_window:
 **/
static const gchar *
mcm_session_get_profile_for_window (guint xid, GError **error)
{
	McmDevice *device;
	GdkWindow *window;
	const gchar *filename = NULL;

	egg_debug ("getting profile for %i", xid);

	/* get window for xid */
	window = gdk_window_foreign_new (xid);
	if (window == NULL) {
		g_set_error (error, 1, 0, "failed to find window with xid %i", xid);
		goto out;
	}

	/* get device for this window */
	device = mcm_client_get_device_by_window (client, window);
	if (device == NULL) {
		g_set_error (error, 1, 0, "no device found for xid %i", xid);
		goto out;
	}

	/* get the data */
	filename = mcm_device_get_default_profile_filename (device);
	if (filename == NULL) {
		g_set_error (error, 1, 0, "no profiles found for xid %i", xid);
		goto out;
	}
out:
	if (window != NULL)
		g_object_unref (window);
	return filename;
}

/**
 * mcm_session_get_profiles_for_kind:
 **/
static GPtrArray *
mcm_session_get_profiles_for_kind (McmDeviceKind kind, GError **error)
{
	guint i;
	McmProfile *profile;
	McmProfileKind profile_kind;
	McmProfileKind kind_tmp;
	GPtrArray *array;
	GPtrArray *profile_array;

	/* create a temp array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get the correct profile kind for the device kind */
	profile_kind = mcm_utils_device_kind_to_profile_kind (kind);

	/* get list */
	profile_array = mcm_profile_store_get_array (profile_store);
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* compare what we have against what we were given */
		kind_tmp = mcm_profile_get_kind (profile);
		if (kind_tmp == profile_kind)
			g_ptr_array_add (array, g_object_ref (profile));
	}

	/* unref profile list */
	g_ptr_array_unref (profile_array);
	return array;
}

/**
 * mcm_session_variant_from_profile_array:
 **/
static GVariant *
mcm_session_variant_from_profile_array (GPtrArray *array)
{
	guint i;
	McmProfile *profile;
	GVariantBuilder *builder;
	GVariant *value;

	/* create builder */
	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	/* add each tuple to the array */
	for (i=0; i<array->len; i++) {
		profile = g_ptr_array_index (array, i);
		g_variant_builder_add (builder, "(ss)",
				       mcm_profile_get_filename (profile),
				       mcm_profile_get_description (profile));
	}

	value = g_variant_builder_end (builder);
	g_variant_builder_unref (builder);
	return value;
}

/**
 * mcm_session_get_profiles_for_file:
 **/
static GPtrArray *
mcm_session_get_profiles_for_file (const gchar *filename, GError **error)
{
	guint i;
	gboolean ret;
	McmExif *exif;
	McmDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices;
	GFile *file;

	/* get file type */
	exif = mcm_exif_new ();
	file = g_file_new_for_path (filename);
	ret = mcm_exif_parse (exif, file, error);
	if (!ret)
		goto out;

	/* get list */
	egg_debug ("query=%s", filename);
	array_devices = mcm_client_get_devices (client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* match up critical parts */
		if (g_strcmp0 (mcm_device_get_manufacturer (device), mcm_exif_get_manufacturer (exif)) == 0 &&
		    g_strcmp0 (mcm_device_get_model (device), mcm_exif_get_model (exif)) == 0 &&
		    g_strcmp0 (mcm_device_get_serial (device), mcm_exif_get_serial (exif)) == 0) {

			array = mcm_device_get_profiles (device);
			break;
		}
	}

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
out:
	g_object_unref (file);
	g_object_unref (exif);
	return array;
}

/**
 * mcm_session_get_profiles_for_device:
 **/
static GPtrArray *
mcm_session_get_profiles_for_device (const gchar *device_id_with_prefix, GError **error)
{
	const gchar *device_id;
	const gchar *device_id_tmp;
	guint i;
	gboolean use_native_device = FALSE;
	McmDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices;

	/* strip the prefix, if there is any */
	device_id = g_strstr_len (device_id_with_prefix, -1, ":");
	if (device_id == NULL) {
		device_id = device_id_with_prefix;
	} else {
		device_id++;
		use_native_device = TRUE;
	}

	/* use the sysfs path to be backwards compatible */
	if (g_str_has_prefix (device_id_with_prefix, "/"))
		use_native_device = TRUE;

	/* get list */
	egg_debug ("query=%s [%s] %i", device_id_with_prefix, device_id, use_native_device);
	array_devices = mcm_client_get_devices (client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* get the id for this device */
		if (use_native_device && MCM_IS_DEVICE_XRANDR (device)) {
			device_id_tmp = mcm_device_xrandr_get_native_device (MCM_DEVICE_XRANDR (device));
		} else {
			device_id_tmp = mcm_device_get_id (device);
		}

		/* wrong kind of device */
		if (device_id_tmp == NULL)
			continue;

		/* compare what we have against what we were given */
		egg_debug ("comparing %s with %s", device_id_tmp, device_id);
		if (g_strcmp0 (device_id_tmp, device_id) == 0) {

			array = mcm_device_get_profiles (device);
			break;
		}
	}

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
	return array;
}

/**
 * mcm_session_handle_method_call:
 **/
static void
mcm_session_handle_method_call (GDBusConnection *connection_, const gchar *sender,
				const gchar *object_path, const gchar *interface_name,
				const gchar *method_name, GVariant *parameters,
				GDBusMethodInvocation *invocation, gpointer user_data)
{
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	guint xid;
	gchar *device_id = NULL;
	gchar *filename = NULL;
	gchar *hints = NULL;
	gchar *type = NULL;
	GPtrArray *array = NULL;
	gchar **devices = NULL;
	McmDevice *device;
	GError *error = NULL;
	const gchar *profile_filename;
	guint i;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* copy the device id */
		array = mcm_client_get_devices (client);
		devices = g_new0 (gchar *, array->len + 1);
		for (i=0; i<array->len; i++) {
			device = g_ptr_array_index (array, i);
			devices[i] = g_strdup (mcm_device_get_id (device));
		}

		/* format the value */
		value = g_variant_new_strv ((const gchar * const *) devices, -1);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "GetProfileForWindow") == 0) {
		g_variant_get (parameters, "(u)", &xid);

		/* get the profile for a window */
		profile_filename = mcm_session_get_profile_for_window (xid, &error);
		if (profile_filename == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.mate.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = g_variant_new_string (profile_filename);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForDevice") == 0) {
		g_variant_get (parameters, "(ss)", &device_id, &hints);

		/* get array of profile filenames */
		array = mcm_session_get_profiles_for_device (device_id, &error);
		if (array == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.mate.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = mcm_session_variant_from_profile_array (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForType") == 0) {
		g_variant_get (parameters, "(ss)", &type, &hints);

		/* get array of profiles */
		array = mcm_session_get_profiles_for_kind (mcm_device_kind_from_string (type), &error);
		if (array == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.mate.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = mcm_session_variant_from_profile_array (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForFile") == 0) {
		g_variant_get (parameters, "(ss)", &filename, &hints);

		/* get array of profile filenames */
		array = mcm_session_get_profiles_for_file (device_id, &error);
		if (array == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.mate.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = mcm_session_variant_from_profile_array (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}
out:
	/* reset time */
	g_timer_reset (timer);

	if (array != NULL)
		g_ptr_array_unref (array);
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
	g_free (device_id);
	g_free (type);
	g_free (filename);
	g_free (hints);
	g_strfreev (devices);
	return;
}

/**
 * mcm_session_handle_get_property:
 **/
static GVariant *
mcm_session_handle_get_property (GDBusConnection *connection_, const gchar *sender,
				 const gchar *object_path, const gchar *interface_name,
				 const gchar *property_name, GError **error,
				 gpointer user_data)
{
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "RenderingIntentDisplay") == 0) {
		retval = g_settings_get_value (settings, MCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	} else if (g_strcmp0 (property_name, "RenderingIntentSoftproof") == 0) {
		retval = g_settings_get_value (settings, MCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	} else if (g_strcmp0 (property_name, "ColorspaceRgb") == 0) {
		retval = g_settings_get_value (settings, MCM_SETTINGS_COLORSPACE_RGB);
	} else if (g_strcmp0 (property_name, "ColorspaceCmyk") == 0) {
		retval = g_settings_get_value (settings, MCM_SETTINGS_COLORSPACE_CMYK);
	}

	/* reset time */
	g_timer_reset (timer);

	return retval;
}

/**
 * mcm_session_on_bus_acquired:
 **/
static void
mcm_session_on_bus_acquired (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {
		mcm_session_handle_method_call,
		mcm_session_handle_get_property,
		NULL
	};

	registration_id = g_dbus_connection_register_object (connection_,
							     MCM_DBUS_PATH,
							     introspection->interfaces[0],
							     &interface_vtable,
							     NULL,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * mcm_session_on_name_acquired:
 **/
static void
mcm_session_on_name_acquired (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	egg_debug ("acquired name: %s", name);
	connection = g_object_ref (connection_);
}

/**
 * mcm_session_on_name_lost:
 **/
static void
mcm_session_on_name_lost (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	egg_debug ("lost name: %s", name);
	g_main_loop_quit (loop);
}

/**
 * mcm_session_emit_changed:
 **/
static void
mcm_session_emit_changed (void)
{
	gboolean ret;
	GError *error = NULL;

	/* check we are connected */
	if (connection == NULL)
		return;

	/* just emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     MCM_DBUS_PATH,
					     MCM_DBUS_INTERFACE,
					     "Changed",
					     NULL,
					     &error);
	if (!ret) {
		egg_warning ("failed to emit signal: %s", error->message);
		g_error_free (error);
	}
}

/**
 * mcm_session_key_changed_cb:
 **/
static void
mcm_session_key_changed_cb (GSettings *settings_, const gchar *key, gpointer user_data)
{
	mcm_session_emit_changed ();
}

/**
 * mcm_session_client_changed_cb:
 **/
static void
mcm_session_client_changed_cb (McmClient *client_, McmDevice *device, gpointer user_data)
{
	mcm_session_emit_changed ();
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean no_timed_exit = FALSE;
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 1;
	guint owner_id = 0;
	guint poll_id = 0;
	GFile *file = NULL;
	gchar *introspection_data = NULL;

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
	g_signal_connect (settings, "changed", G_CALLBACK (mcm_session_key_changed_cb), NULL);

	/* monitor devices as they are added */
	client = mcm_client_new ();
	mcm_client_set_use_threads (client, TRUE);
	g_signal_connect (client, "added", G_CALLBACK (mcm_session_added_cb), NULL);
	g_signal_connect (client, "added", G_CALLBACK (mcm_session_client_changed_cb), NULL);
	g_signal_connect (client, "removed", G_CALLBACK (mcm_session_client_changed_cb), NULL);
	g_signal_connect (client, "changed", G_CALLBACK (mcm_session_client_changed_cb), NULL);

	/* have access to all profiles */
	profile_store = mcm_profile_store_new ();
	timer = g_timer_new ();

	/* get all connected devices */
	ret = mcm_client_coldplug (client, MCM_CLIENT_COLDPLUG_ALL, &error);
	if (!ret) {
		egg_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
	}

	/* create new objects */
	loop = g_main_loop_new (NULL, FALSE);

	/* load introspection from file */
	file = g_file_new_for_path (DATADIR "/dbus-1/interfaces/org.mate.ColorManager.xml");
	ret = g_file_load_contents (file, NULL, &introspection_data, NULL, NULL, &error);
	if (!ret) {
		egg_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* build introspection from XML */
	introspection = g_dbus_node_info_new_for_xml (introspection_data, &error);
	if (introspection == NULL) {
		egg_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* own the object */
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				   MCM_DBUS_SERVICE,
				   G_BUS_NAME_OWNER_FLAGS_NONE,
				   mcm_session_on_bus_acquired,
				   mcm_session_on_name_acquired,
				   mcm_session_on_name_lost,
				   NULL, NULL);

	/* only timeout if we have specified it on the command line */
	if (!no_timed_exit) {
		poll_id = g_timeout_add_seconds (5, (GSourceFunc) mcm_session_check_idle_cb, NULL);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (poll_id, "[McmSession] inactivity checker");
#endif
	}

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_free (introspection_data);
	if (poll_id != 0)
		g_source_remove (poll_id);
	if (file != NULL)
		g_object_unref (file);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (timer != NULL)
		g_timer_destroy (timer);
	if (connection != NULL)
		g_object_unref (connection);
	g_dbus_node_info_unref (introspection);
	g_object_unref (settings);
	g_object_unref (client);
	g_main_loop_unref (loop);
	return retval;
}

