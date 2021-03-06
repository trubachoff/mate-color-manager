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
#include <cups/cups.h>
#include <cups/ppd.h>

#include "mcm-device-cups.h"
#include "mcm-enum.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_device_cups_finalize	(GObject     *object);

#define MCM_DEVICE_CUPS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DEVICE_CUPS, McmDeviceCupsPrivate))

/**
 * McmDeviceCupsPrivate:
 *
 * Private #McmDeviceCups data
 **/
struct _McmDeviceCupsPrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (McmDeviceCups, mcm_device_cups, MCM_TYPE_DEVICE)

/**
 * mcm_device_cups_set_from_dest:
 **/
gboolean
mcm_device_cups_set_from_dest (McmDevice *device, http_t *http, cups_dest_t dest, GError **error)
{
	gint i;
	gboolean ret = TRUE;
	ppd_file_t *ppd_file = NULL;
	const gchar *ppd_file_location = NULL;
	gchar *id = NULL;
	gchar *device_id = NULL;
	gchar *title = NULL;
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *profile_filename = NULL;
	McmColorspace colorspace = MCM_COLORSPACE_UNKNOWN;

	egg_debug ("name: %s", dest.name);
	egg_debug ("instance: %s", dest.instance);
	egg_debug ("num_options: %i", dest.num_options);

	/* don't add Cups-PDF devices */
	if (g_strcmp0 (dest.name, "Cups-PDF") == 0) {
		g_set_error (error, 1, 0, "Not adding PDF device");
		ret = FALSE;
		goto out;
	}

	ppd_file_location = cupsGetPPD2 (http, dest.name);
	egg_debug ("ppd_file_location=%s", ppd_file_location);

	/* don't add devices without PPD */
	if (ppd_file_location == NULL) {
		g_set_error (error, 1, 0, "Not adding device without PPD");
		ret = FALSE;
		goto out;
	}

	/* try to open PPD file */
	ppd_file = ppdOpenFile (ppd_file_location);
	if (ppd_file == NULL) {
		g_set_error (error, 1, 0, "PPD open file failed");
		ret = FALSE;
		goto out;
	}

	for (i = 0; i < ppd_file->num_attrs; i++) {
		const gchar *keyword;
		const gchar *value;

		/* get the keyword and value */
		keyword = ppd_file->attrs[i]->name;
		value = ppd_file->attrs[i]->value;

		/* ignore some */
		if (g_strcmp0 (keyword, "Font") == 0)
			continue;
		if (g_strcmp0 (keyword, "Product") == 0)
			continue;
		if (g_strcmp0 (keyword, "ParamCustomPageSize") == 0)
			continue;

		/* check to see if there is anything interesting */
		if (g_strcmp0 (keyword, "Manufacturer") == 0) {
			manufacturer = g_strdup (value);
		} else if (g_strcmp0 (keyword, "ModelName") == 0) {
			model = g_strdup (value);
		} else if (g_strcmp0 (keyword, "ShortNickName") == 0) {
			title = g_strdup (value);
		} else if (g_strcmp0 (keyword, "1284DeviceID") == 0) {
			device_id = g_strdup (value);
		} else if (g_strcmp0 (keyword, "DefaultColorSpace") == 0) {
			if (g_strcmp0 (value, "RGB") == 0)
				colorspace = MCM_COLORSPACE_RGB;
			else if (g_strcmp0 (value, "CMYK") == 0)
				colorspace = MCM_COLORSPACE_CMYK;
			else if (g_strcmp0 (value, "Gray") == 0)
				colorspace = MCM_COLORSPACE_GRAY;
			else
				egg_warning ("colorspace not recognized: %s", value);
		} else if (g_strcmp0 (keyword, "cupsICCProfile") == 0) {
			/* FIXME: possibly map from http://localhost:port/profiles/dave.icc to ~/.icc/color/dave.icc */
			profile_filename = g_strdup (value);
			egg_warning ("remap %s?", profile_filename);
		}

//		egg_debug ("keyword: %s, value: %s, spec: %s", keyword, value, ppd_file->attrs[i]->spec);
	}

	/* convert device_id 'MFG:HP;MDL:deskjet d1300 series;DES:deskjet d1300 series;' to suitable id */
	id = g_strdup_printf ("cups_%s", device_id);
	mcm_utils_alphanum_lcase (id);

	g_object_set (device,
		      "kind", MCM_DEVICE_KIND_PRINTER,
		      "colorspace", colorspace,
		      "id", id,
// FIXME: find out from CUPS if the printer is connected
		      "connected", TRUE,
//		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", device_id,
		      NULL);
	if (profile_filename != NULL)
		mcm_device_set_default_profile_filename (device, profile_filename);
out:
	g_free (serial);
	g_free (profile_filename);
	g_free (manufacturer);
	g_free (model);
	g_free (id);
	g_free (device_id);
	g_free (title);
	if (ppd_file != NULL) {
		ppdClose (ppd_file);
		unlink (ppd_file_location);
	}
	return ret;
}

/**
 * mcm_device_cups_get_property:
 **/
static void
mcm_device_cups_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmDeviceCups *device_cups = MCM_DEVICE_CUPS (object);
	McmDeviceCupsPrivate *priv = device_cups->priv;

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
 * mcm_device_cups_set_property:
 **/
static void
mcm_device_cups_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmDeviceCups *device_cups = MCM_DEVICE_CUPS (object);
	McmDeviceCupsPrivate *priv = device_cups->priv;

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
 * mcm_device_cups_class_init:
 **/
static void
mcm_device_cups_class_init (McmDeviceCupsClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_device_cups_finalize;
	object_class->get_property = mcm_device_cups_get_property;
	object_class->set_property = mcm_device_cups_set_property;


	/**
	 * McmDeviceCups:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (McmDeviceCupsPrivate));
}

/**
 * mcm_device_cups_init:
 **/
static void
mcm_device_cups_init (McmDeviceCups *device_cups)
{
	device_cups->priv = MCM_DEVICE_CUPS_GET_PRIVATE (device_cups);
	device_cups->priv->native_device = NULL;
}

/**
 * mcm_device_cups_finalize:
 **/
static void
mcm_device_cups_finalize (GObject *object)
{
	McmDeviceCups *device_cups = MCM_DEVICE_CUPS (object);
	McmDeviceCupsPrivate *priv = device_cups->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (mcm_device_cups_parent_class)->finalize (object);
}

/**
 * mcm_device_cups_new:
 *
 * Return value: a new #McmDevice object.
 **/
McmDevice *
mcm_device_cups_new (void)
{
	McmDevice *device;
	device = g_object_new (MCM_TYPE_DEVICE_CUPS, NULL);
	return MCM_DEVICE (device);
}

