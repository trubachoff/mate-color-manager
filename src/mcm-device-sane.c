/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <sane/sane.h>

#include "mcm-device-sane.h"
#include "mcm-enum.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_device_sane_finalize	(GObject     *object);

#define MCM_DEVICE_SANE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DEVICE_SANE, McmDeviceSanePrivate))

/**
 * McmDeviceSanePrivate:
 *
 * Private #McmDeviceSane data
 **/
struct _McmDeviceSanePrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (McmDeviceSane, mcm_device_sane, MCM_TYPE_DEVICE)

typedef struct {
	gchar	*key;
	gchar	*value;
} McmDeviceSaneKeyPair;

/**
 * mcm_device_sane_free_key_pair:
 **/
static void
mcm_device_sane_free_key_pair (McmDeviceSaneKeyPair *key_pair)
{
	g_free (key_pair->key);
	g_free (key_pair->value);
	g_free (key_pair);
}

/**
 * mcm_device_sane_get_key_pairs_from_filename:
 **/
static GPtrArray *
mcm_device_sane_get_key_pairs_from_filename (const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *contents = NULL;
	GPtrArray *array = NULL;
	guint i;
	gchar **split = NULL;
	McmDeviceSaneKeyPair *key_pair;

	/* get contents */
	ret = g_file_get_contents (filename, &contents, NULL, error);
	if (!ret)
		goto out;

	/* parse */
	split = g_strsplit (contents, "\n", -1);
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) mcm_device_sane_free_key_pair);
	for (i=0; split[i] != NULL; i+=2) {
		key_pair = g_new0 (McmDeviceSaneKeyPair, 1);
		if (split[i] == NULL || split[i+1] == NULL)
			break;
		key_pair->key = g_strdup (split[i]);
		key_pair->value = g_strdup (split[i+1]);
		g_ptr_array_add (array, key_pair);
	}
out:
	g_free (contents);
	g_strfreev (split);
	return array;
}

/**
 * mcm_device_sane_get_key_pairs_to_filename:
 **/
static gboolean
mcm_device_sane_get_key_pairs_to_filename (const gchar *filename, GPtrArray *array, GError **error)
{
	gboolean ret;
	GString *string;
	guint i;
	McmDeviceSaneKeyPair *key_pair;

	/* turn the array into a string */
	string = g_string_new ("");
	for (i=0; i<array->len; i++) {
		key_pair = g_ptr_array_index (array, i);
		g_string_append (string, key_pair->key);
		g_string_append_c (string, '\n');
		g_string_append (string, key_pair->value);
		g_string_append_c (string, '\n');
	}

	/* save to file */
	ret = g_file_set_contents (filename, string->str, -1, error);
	if (!ret)
		goto out;
out:
	g_string_free (string, TRUE);
	return ret;
}

/**
 * mcm_device_sane_set_key_pair_value:
 **/
static void
mcm_device_sane_set_key_pair_value (GPtrArray *array, const gchar *key, const gchar *value)
{
	guint i;
	McmDeviceSaneKeyPair *key_pair;

	/* find and replace */
	for (i=0; i<array->len; i++) {
		key_pair = g_ptr_array_index (array, i);
		if (g_strcmp0 (key_pair->key, key) == 0) {
			g_free (key_pair->value);
			key_pair->value = g_strdup (value);
			goto out;
		}
	}

	/* not found, create new */
	key_pair = g_new0 (McmDeviceSaneKeyPair, 1);
	key_pair->key = g_strdup (key);
	key_pair->value = g_strdup (value);
	g_ptr_array_add (array, key_pair);
out:
	return;
}

/**
 * mcm_device_sane_set_from_device:
 **/
gboolean
mcm_device_sane_set_from_device (McmDevice *device, const SANE_Device *sane_device, GError **error)
{
	gchar *id = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *title = NULL;

	egg_debug ("name=%s", sane_device->name);
	egg_debug ("vendor=%s", sane_device->vendor);
	egg_debug ("model=%s", sane_device->model);
	egg_debug ("type=%s", sane_device->type);

	/* convert device_id 'plustek:libusb:004:002' to suitable id */
	id = g_strdup_printf ("sane_%s", sane_device->model);
	mcm_utils_alphanum_lcase (id);

	/* make safe strings */
	manufacturer = g_strdup (sane_device->vendor);
	model = g_strdup (sane_device->model);
	title = g_strdup_printf ("%s - %s", manufacturer, model);

	/* set properties on device */
	g_object_set (device,
		      "kind", MCM_DEVICE_KIND_SCANNER,
		      "colorspace", MCM_COLORSPACE_RGB,
		      "id", id,
		      "connected", TRUE,
//		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", sane_device->name,
		      NULL);

	g_free (manufacturer);
	g_free (model);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * mcm_device_sane_apply_global:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
mcm_device_sane_apply_global (McmDeviceSane *device_sane, GError **error)
{
	gboolean ret = FALSE;
	gchar *filename = NULL;
	GPtrArray *array;

	filename = g_build_filename (g_get_home_dir (), ".sane", "xsane", "xsane.rc", NULL);

	/* get existing file, if it exists */
	array = mcm_device_sane_get_key_pairs_from_filename (filename, error);
	if (array == NULL)
		goto out;

	/* set some keys */
	mcm_device_sane_set_key_pair_value (array, "\"display-icm-profile\"", "\"\"");
	mcm_device_sane_set_key_pair_value (array, "\"working-color-space-icm-profile\"", "\"\"");
	mcm_device_sane_set_key_pair_value (array, "\"auto-correct-colors\"", "1");

	/* ensure directory exists */
	mcm_utils_mkdir_for_filename  (filename, NULL);

	/* save new file */
	ret = mcm_device_sane_get_key_pairs_to_filename (filename, array, error);
	if (!ret)
		goto out;
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (filename);
	return ret;
}

/**
 * mcm_device_sane_remove_spaces:
 *
 * Return value: %TRUE for success;
 **/
static void
mcm_device_sane_remove_spaces (gchar *text)
{
	guint i;
	guint j = 0;

	for (i=0; text[i] != '\0'; i++) {
		if (i != j)
			text[j] = text[i];
		if (text[i] != ' ')
			j++;
	}
	text[j] = '\0';
}

/**
 * mcm_device_sane_apply_device:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
mcm_device_sane_apply_device (McmDeviceSane *device_sane, GError **error)
{
	gboolean ret = FALSE;
	gchar *filename = NULL;
	gchar *device_filename = NULL;
	const gchar *profile_filename = NULL;
	gchar *profile_filename_quoted = NULL;
	GPtrArray *array;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	McmDevice *device = MCM_DEVICE (device_sane);

	/* get properties from device */
	model = mcm_device_get_model (device);
	manufacturer = mcm_device_get_manufacturer (device);
	profile_filename = mcm_device_get_profile_filename (device);
	profile_filename_quoted = g_strdup_printf ("\"%s\"", profile_filename);

	device_filename = g_strdup_printf ("%s:%s.drc", manufacturer, model);
	g_strdelimit (device_filename, "/", '_');
	mcm_device_sane_remove_spaces (device_filename);
	egg_debug ("device_filename=%s", device_filename);

	filename = g_build_filename (g_get_home_dir (), ".sane", "xsane", device_filename, NULL);

	/* get existing file, if it exists */
	array = mcm_device_sane_get_key_pairs_from_filename (filename, error);
	if (array == NULL)
		goto out;

	/* set some keys */
	mcm_device_sane_set_key_pair_value (array, "\"xsane-scanner-default-color-icm-profile\"", profile_filename_quoted);
	mcm_device_sane_set_key_pair_value (array, "\"xsane-scanner-default-gray-icm-profile\"", profile_filename_quoted);
	mcm_device_sane_set_key_pair_value (array, "\"xsane-enable-color-management\"", "1");

	/* ensure directory exists */
	mcm_utils_mkdir_for_filename  (filename, NULL);

	/* save new file */
	ret = mcm_device_sane_get_key_pairs_to_filename (filename, array, error);
	if (!ret)
		goto out;
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (filename);
	g_free (device_filename);
	return ret;
}

/**
 * mcm_device_sane_apply:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
mcm_device_sane_apply (McmDevice *device, GError **error)
{
	gboolean ret;

	/* apply global settings for xsane */
	ret = mcm_device_sane_apply_global (MCM_DEVICE_SANE (device), error);
	if (!ret)
		goto out;

	/* apply device specific settings for xsane */
	ret = mcm_device_sane_apply_device (MCM_DEVICE_SANE (device), error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * mcm_device_sane_get_property:
 **/
static void
mcm_device_sane_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmDeviceSane *device_sane = MCM_DEVICE_SANE (object);
	McmDeviceSanePrivate *priv = device_sane->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_value_set_string (value, priv->native_device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_device_sane_set_property:
 **/
static void
mcm_device_sane_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmDeviceSane *device_sane = MCM_DEVICE_SANE (object);
	McmDeviceSanePrivate *priv = device_sane->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_free (priv->native_device);
		priv->native_device = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_device_sane_class_init:
 **/
static void
mcm_device_sane_class_init (McmDeviceSaneClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	McmDeviceClass *device_class = MCM_DEVICE_CLASS (klass);

	object_class->finalize = mcm_device_sane_finalize;
	object_class->get_property = mcm_device_sane_get_property;
	object_class->set_property = mcm_device_sane_set_property;

	device_class->apply = mcm_device_sane_apply;

	/**
	 * McmDeviceSane:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (McmDeviceSanePrivate));
}

/**
 * mcm_device_sane_init:
 **/
static void
mcm_device_sane_init (McmDeviceSane *device_sane)
{
	device_sane->priv = MCM_DEVICE_SANE_GET_PRIVATE (device_sane);
	device_sane->priv->native_device = NULL;
}

/**
 * mcm_device_sane_finalize:
 **/
static void
mcm_device_sane_finalize (GObject *object)
{
	McmDeviceSane *device_sane = MCM_DEVICE_SANE (object);
	McmDeviceSanePrivate *priv = device_sane->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (mcm_device_sane_parent_class)->finalize (object);
}

/**
 * mcm_device_sane_new:
 *
 * Return value: a new #McmDevice object.
 **/
McmDevice *
mcm_device_sane_new (void)
{
	McmDevice *device;
	device = g_object_new (MCM_TYPE_DEVICE_SANE, NULL);
	return MCM_DEVICE (device);
}

