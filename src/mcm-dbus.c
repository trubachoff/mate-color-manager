/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2010 Richard Hughes <richard@hughsie.com>
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
#include <mateconf/mateconf-client.h>

#include "egg-debug.h"

#include "mcm-utils.h"
#include "mcm-dbus.h"
#include "mcm-device-xrandr.h"
#include "mcm-client.h"
#include "mcm-profile.h"
#include "mcm-profile-store.h"

static void     mcm_dbus_finalize	(GObject	*object);

#define MCM_DBUS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DBUS, McmDbusPrivate))

struct McmDbusPrivate
{
	MateConfClient		*mateconf_client;
	McmClient		*client;
	McmProfileStore		*profile_store;
	GTimer			*timer;
	gchar			*rendering_intent_display;
	gchar			*rendering_intent_softproof;
	gchar			*colorspace_rgb;
	gchar			*colorspace_cmyk;
};

enum {
	PROP_0,
	PROP_RENDERING_INTENT_DISPLAY,
	PROP_RENDERING_INTENT_SOFTPROOF,
	PROP_COLORSPACE_RGB,
	PROP_COLORSPACE_CMYK,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST,
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (McmDbus, mcm_dbus, G_TYPE_OBJECT)

/**
 * mcm_dbus_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
mcm_dbus_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("mcm_dbus_error");
	return quark;
}

/**
 * mcm_dbus_error_get_type:
 **/
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
GType
mcm_dbus_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] =
		{
			ENUM_ENTRY (MCM_DBUS_ERROR_FAILED, "Failed"),
			ENUM_ENTRY (MCM_DBUS_ERROR_INTERNAL_ERROR, "InternalError"),
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("McmDbusError", values);
	}
	return etype;
}


/**
 * mcm_dbus_get_property:
 **/
static void
mcm_dbus_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmDbus *dbus = MCM_DBUS (object);
	switch (prop_id) {
	case PROP_RENDERING_INTENT_DISPLAY:
		g_value_set_string (value, dbus->priv->rendering_intent_display);
		break;
	case PROP_COLORSPACE_RGB:
		g_value_set_string (value, dbus->priv->colorspace_rgb);
		break;
	case PROP_COLORSPACE_CMYK:
		g_value_set_string (value, dbus->priv->colorspace_cmyk);
		break;
	case PROP_RENDERING_INTENT_SOFTPROOF:
		g_value_set_string (value, dbus->priv->rendering_intent_softproof);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* reset time */
	g_timer_reset (dbus->priv->timer);
}

/**
 * mcm_dbus_set_property:
 **/
static void
mcm_dbus_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_dbus_get_idle_time:
 **/
guint
mcm_dbus_get_idle_time (McmDbus	*dbus)
{
	guint idle;
	idle = (guint) g_timer_elapsed (dbus->priv->timer, NULL);
	egg_debug ("we've been idle for %is", idle);
	return idle;
}

#define MCM_DBUS_STRUCT_STRING_STRING (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))

/**
 * mcm_dbus_get_profiles_for_device_internal:
 **/
static GPtrArray *
mcm_dbus_get_profiles_for_device_internal (McmDbus *dbus, const gchar *device_id_with_prefix)
{
	gboolean ret;
	const gchar *filename;
	const gchar *device_id;
	const gchar *device_id_tmp;
	guint i;
	gboolean use_native_device = FALSE;
	McmDevice *device;
	McmProfile *profile;
	GFile *file;
	GError *error = NULL;
	GPtrArray *array;
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

	/* create a temp array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get list */
	egg_debug ("query=%s [%s] %i", device_id_with_prefix, device_id, use_native_device);
	array_devices = mcm_client_get_devices (dbus->priv->client);
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

			/* we have a profile? */
			filename = mcm_device_get_profile_filename (device);
			if (filename == NULL) {
				egg_warning ("%s does not have a profile set", device_id);
				continue;
			}

			/* open and parse filename */
			profile = mcm_profile_default_new ();
			file = g_file_new_for_path (filename);
			ret = mcm_profile_parse (profile, file, &error);
			if (!ret) {
				egg_warning ("failed to parse %s: %s", filename, error->message);
				g_clear_error (&error);
			} else {
				g_ptr_array_add (array, g_object_ref (profile));
			}

			/* unref */
			g_object_unref (file);
			g_object_unref (profile);
		}
	}

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
	return array;
}

/**
 * mcm_dbus_get_profiles_for_kind_internal:
 **/
static GPtrArray *
mcm_dbus_get_profiles_for_kind_internal (McmDbus *dbus, McmDeviceKind kind)
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
	profile_array = mcm_profile_store_get_array (dbus->priv->profile_store);
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
 * mcm_dbus_get_devices:
 **/
void
mcm_dbus_get_devices (McmDbus *dbus, DBusGMethodInvocation *context)
{
	McmDevice *device;
	guint i;
	gchar **devices;
	GPtrArray *array;

	egg_debug ("getting list of devices");

	/* copy the device id */
	array = mcm_client_get_devices (dbus->priv->client);
	devices = g_new0 (gchar *, array->len + 1);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		devices[i] = g_strdup (mcm_device_get_id (device));
	}

	/* return devices */
	dbus_g_method_return (context, devices);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	/* unref list of devices */
	g_strfreev (devices);
	g_ptr_array_unref (array);
}

/**
 * mcm_dbus_get_profiles_for_device:
 **/
void
mcm_dbus_get_profiles_for_device (McmDbus *dbus, const gchar *device_id, const gchar *options, DBusGMethodInvocation *context)
{
	GPtrArray *array_profiles;
	McmProfile *profile;
	const gchar *title;
	const gchar *filename;
	guint i;
	GPtrArray *array_structs;
	GValue *value;

	egg_debug ("getting profiles for %s", device_id);

	/* get array of profile filenames */
	array_profiles = mcm_dbus_get_profiles_for_device_internal (dbus, device_id);

	/* copy data to dbus struct */
	array_structs = g_ptr_array_sized_new (array_profiles->len);
	for (i=0; i<array_profiles->len; i++) {
		profile = (McmProfile *) g_ptr_array_index (array_profiles, i);

		value = g_new0 (GValue, 1);
		g_value_init (value, MCM_DBUS_STRUCT_STRING_STRING);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (MCM_DBUS_STRUCT_STRING_STRING));
		title = mcm_profile_get_description (profile);
		filename = mcm_profile_get_filename (profile);
		dbus_g_type_struct_set (value, 0, title, 1, filename, -1);
		g_ptr_array_add (array_structs, g_value_get_boxed (value));
		g_free (value);
	}

	/* return profiles */
	dbus_g_method_return (context, array_structs);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	g_ptr_array_unref (array_profiles);
}

/**
 * mcm_dbus_get_profiles_for_type:
 **/
void
mcm_dbus_get_profiles_for_type (McmDbus *dbus, const gchar *kind, const gchar *options, DBusGMethodInvocation *context)
{
	GPtrArray *array_profiles;
	McmProfile *profile;
	const gchar *title;
	const gchar *filename;
	guint i;
	GPtrArray *array_structs;
	GValue *value;
	McmDeviceKind kind_enum;

	egg_debug ("getting profiles for %s", kind);

	/* get array of profile filenames */
	kind_enum = mcm_device_kind_from_string (kind);
	array_profiles = mcm_dbus_get_profiles_for_kind_internal (dbus, kind_enum);

	/* copy data to dbus struct */
	array_structs = g_ptr_array_sized_new (array_profiles->len);
	for (i=0; i<array_profiles->len; i++) {
		profile = (McmProfile *) g_ptr_array_index (array_profiles, i);

		/* get the data */
		value = g_new0 (GValue, 1);
		g_value_init (value, MCM_DBUS_STRUCT_STRING_STRING);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (MCM_DBUS_STRUCT_STRING_STRING));
		title = mcm_profile_get_description (profile);
		filename = mcm_profile_get_filename (profile);
		dbus_g_type_struct_set (value, 0, title, 1, filename, -1);
		g_ptr_array_add (array_structs, g_value_get_boxed (value));
		g_free (value);
	}

	/* return profiles */
	dbus_g_method_return (context, array_structs);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	g_ptr_array_unref (array_profiles);
}

/**
 * mcm_dbus_get_profile_for_window:
 **/
void
mcm_dbus_get_profile_for_window (McmDbus *dbus, guint xid, DBusGMethodInvocation *context)
{
	GError *error;
	McmDevice *device;
	GdkWindow *window;
	const gchar *filename;

	egg_debug ("getting profile for %i", xid);

	/* get window for xid */
	window = gdk_window_foreign_new (xid);
	if (window == NULL) {
		error = g_error_new (1, 0, "failed to find window with xid %i", xid);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		goto out;
	}

	/* get device for this window */
	device = mcm_client_get_device_by_window (dbus->priv->client, window);
	if (device == NULL) {
		error = g_error_new (1, 0, "no device found for xid %i", xid);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		goto out;
	}

	/* get the data */
	filename = mcm_device_get_profile_filename (device);
	if (filename == NULL) {
		error = g_error_new (1, 0, "no profiles found for xid %i", xid);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		goto out;
	}

	/* return profiles */
	dbus_g_method_return (context, filename);

out:
	/* reset time */
	g_timer_reset (dbus->priv->timer);
	if (window != NULL)
		g_object_unref (window);
}

/**
 * mcm_dbus_class_init:
 * @klass: The McmDbusClass
 **/
static void
mcm_dbus_class_init (McmDbusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_dbus_finalize;
	object_class->get_property = mcm_dbus_get_property;
	object_class->set_property = mcm_dbus_set_property;
	g_type_class_add_private (klass, sizeof (McmDbusPrivate));

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * McmDbus:rendering-intent-display:
	 */
	g_object_class_install_property (object_class,
					 PROP_RENDERING_INTENT_DISPLAY,
					 g_param_spec_string ("rendering-intent-display",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * McmDbus:rendering-intent-softproof:
	 */
	g_object_class_install_property (object_class,
					 PROP_RENDERING_INTENT_SOFTPROOF,
					 g_param_spec_string ("rendering-intent-softproof",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * McmDbus:colorspace-rgb:
	 */
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE_RGB,
					 g_param_spec_string ("colorspace-rgb",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * McmDbus:colorspace-cmyk:
	 */
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE_CMYK,
					 g_param_spec_string ("colorspace-cmyk",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
}

/**
 * mcm_dbus_mateconf_key_changed_cb:
 *
 * We might have to do things when the mateconf keys change; do them here.
 **/
static void
mcm_dbus_mateconf_key_changed_cb (MateConfClient *client, guint cnxn_id, MateConfEntry *entry, McmDbus *dbus)
{
	/* just emit signal */
	g_signal_emit (dbus, signals[SIGNAL_CHANGED], 0);
}

/**
 * mcm_dbus_client_changed_cb:
 **/
static void
mcm_dbus_client_changed_cb (McmClient *client, McmDevice *device, McmDbus *dbus)
{
	/* just emit signal */
	g_signal_emit (dbus, signals[SIGNAL_CHANGED], 0);
}

/**
 * mcm_dbus_init:
 * @dbus: This class instance
 **/
static void
mcm_dbus_init (McmDbus *dbus)
{
	gboolean ret;
	GError *error = NULL;

	dbus->priv = MCM_DBUS_GET_PRIVATE (dbus);
	dbus->priv->mateconf_client = mateconf_client_get_default ();
	dbus->priv->client = mcm_client_new ();
	g_signal_connect (dbus->priv->client, "added", G_CALLBACK (mcm_dbus_client_changed_cb), dbus);
	g_signal_connect (dbus->priv->client, "removed", G_CALLBACK (mcm_dbus_client_changed_cb), dbus);
	g_signal_connect (dbus->priv->client, "changed", G_CALLBACK (mcm_dbus_client_changed_cb), dbus);

	mcm_client_set_use_threads (dbus->priv->client, TRUE);
	dbus->priv->profile_store = mcm_profile_store_new ();
	dbus->priv->timer = g_timer_new ();

	/* notify on changes */
	mateconf_client_notify_add (dbus->priv->mateconf_client, MCM_SETTINGS_DIR,
				 (MateConfClientNotifyFunc) mcm_dbus_mateconf_key_changed_cb,
				 dbus, NULL, NULL);

	/* coldplug */
	dbus->priv->rendering_intent_display = mateconf_client_get_string (dbus->priv->mateconf_client, MCM_SETTINGS_RENDERING_INTENT_DISPLAY, NULL);
	dbus->priv->rendering_intent_softproof = mateconf_client_get_string (dbus->priv->mateconf_client, MCM_SETTINGS_RENDERING_INTENT_SOFTPROOF, NULL);
	dbus->priv->colorspace_rgb = mateconf_client_get_string (dbus->priv->mateconf_client, MCM_SETTINGS_COLORSPACE_RGB, NULL);
	dbus->priv->colorspace_cmyk = mateconf_client_get_string (dbus->priv->mateconf_client, MCM_SETTINGS_COLORSPACE_CMYK, NULL);

	/* get all devices */
	ret = mcm_client_add_connected (dbus->priv->client, MCM_CLIENT_COLDPLUG_ALL, &error);
	if (!ret) {
		egg_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
	}
}

/**
 * mcm_dbus_finalize:
 * @object: The object to finalize
 **/
static void
mcm_dbus_finalize (GObject *object)
{
	McmDbus *dbus;
	g_return_if_fail (PK_IS_DBUS (object));

	dbus = MCM_DBUS (object);
	g_return_if_fail (dbus->priv != NULL);
	g_free (dbus->priv->rendering_intent_display);
	g_free (dbus->priv->rendering_intent_softproof);
	g_free (dbus->priv->colorspace_rgb);
	g_free (dbus->priv->colorspace_cmyk);
	g_object_unref (dbus->priv->client);
	g_object_unref (dbus->priv->profile_store);
	g_object_unref (dbus->priv->mateconf_client);
	g_timer_destroy (dbus->priv->timer);

	G_OBJECT_CLASS (mcm_dbus_parent_class)->finalize (object);
}

/**
 * mcm_dbus_new:
 *
 * Return value: a new McmDbus object.
 **/
McmDbus *
mcm_dbus_new (void)
{
	McmDbus *dbus;
	dbus = g_object_new (MCM_TYPE_DBUS, NULL);
	return MCM_DBUS (dbus);
}

