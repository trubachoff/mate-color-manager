/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:mcm-exif
 * @short_description: EXIF metadata object
 *
 * This object represents a a file wih EXIF data.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <tiff.h>
#include <tiffio.h>
#include <libexif/exif-data.h>

#include "mcm-exif.h"

#include "egg-debug.h"

static void     mcm_exif_finalize	(GObject     *object);

#define MCM_EXIF_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_EXIF, McmExifPrivate))

/**
 * McmExifPrivate:
 *
 * Private #McmExif data
 **/
struct _McmExifPrivate
{
	gchar				*manufacturer;
	gchar				*model;
	gchar				*serial;
	McmDeviceKind			 device_kind;
};

enum {
	PROP_0,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_DEVICE_KIND,
	PROP_LAST
};

G_DEFINE_TYPE (McmExif, mcm_exif, G_TYPE_OBJECT)

/**
 * mcm_exif_parse_tiff:
 **/
static gboolean
mcm_exif_parse_tiff (McmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret = TRUE;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	const gchar *serial = NULL;
	const gchar *temp = NULL;
	McmDeviceKind device_kind = MCM_DEVICE_KIND_UNKNOWN;
	TIFF *tiff;
	McmExifPrivate *priv = exif->priv;

	/* open file */
	tiff = TIFFOpen (filename, "r");
	TIFFGetField (tiff,TIFFTAG_MAKE, &manufacturer);
	TIFFGetField (tiff,TIFFTAG_MODEL, &model);
	TIFFGetField (tiff,TIFFTAG_CAMERASERIALNUMBER, &serial);

	/* we failed to get data */
	if (manufacturer == NULL || model == NULL) {
		g_set_error (error,
			     MCM_EXIF_ERROR,
			     MCM_EXIF_ERROR_NO_DATA,
			     "failed to get EXIF data from TIFF");
		ret = FALSE;
		goto out;
	}

	/* these are all camera specific values */
	TIFFGetField (tiff,EXIFTAG_FNUMBER, &temp);
	if (temp != NULL)
		device_kind = MCM_DEVICE_KIND_CAMERA;
	TIFFGetField (tiff,TIFFTAG_LENSINFO, &temp);
	if (temp != NULL)
		device_kind = MCM_DEVICE_KIND_CAMERA;

	/* crappy fallback */
	if (g_str_has_prefix (manufacturer, "NIKON"))
		device_kind = MCM_DEVICE_KIND_CAMERA;

	/* free old versions */
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	/* create copies for ourselves */
	priv->manufacturer = g_strdup (manufacturer);
	priv->model = g_strdup (model);
	priv->serial = g_strdup (serial);
	priv->device_kind = device_kind;
out:
	TIFFClose (tiff);
	return ret;
}

/**
 * mcm_exif_parse_jpeg:
 **/
static gboolean
mcm_exif_parse_jpeg (McmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret = TRUE;
	McmExifPrivate *priv = exif->priv;
	ExifData *ed = NULL;
	ExifEntry *entry;
	McmDeviceKind device_kind = MCM_DEVICE_KIND_UNKNOWN;
	gchar make[1024] = { '\0' };
	gchar model[1024] = { '\0' };

	/* load EXIF file */
	ed = exif_data_new_from_file (filename);
	if (ed == NULL) {
		g_set_error (error,
			     MCM_EXIF_ERROR,
			     MCM_EXIF_ERROR_NO_DATA,
			     "File not readable or no EXIF data in file");
		ret = FALSE;
		goto out;
	}

	/* get make */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_MAKE);
	if (entry != NULL) {
		exif_entry_get_value (entry, make, sizeof (make));
		g_strchomp (make);
	}

	/* get model */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_MODEL);
	if (entry != NULL) {
		exif_entry_get_value (entry, model, sizeof (model));
		g_strchomp (model);
	}

	/* these are all camera specific values */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_FNUMBER);
	if (entry != NULL)
		device_kind = MCM_DEVICE_KIND_CAMERA;
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_SHUTTER_SPEED_VALUE);
	if (entry != NULL)
		device_kind = MCM_DEVICE_KIND_CAMERA;
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_FLASH);
	if (entry != NULL)
		device_kind = MCM_DEVICE_KIND_CAMERA;

	/* we failed to get data */
	if (make == NULL || model == NULL) {
		g_set_error (error,
			     MCM_EXIF_ERROR,
			     MCM_EXIF_ERROR_NO_DATA,
			     "failed to get EXIF data from TIFF");
		ret = FALSE;
		goto out;
	}

	/* free old versions */
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	/* create copies for ourselves */
	priv->manufacturer = g_strdup (make);
	priv->model = g_strdup (model);
	priv->serial = NULL;
	priv->device_kind = device_kind;
out:
	if (ed != NULL)
		exif_data_unref (ed);
	return ret;
}

/**
 * mcm_exif_parse:
 **/
gboolean
mcm_exif_parse (McmExif *exif, GFile *file, GError **error)
{
	gboolean ret = FALSE;
	gchar *filename = NULL;
	GFileInfo *info = NULL;
	const gchar *content_type;

	g_return_val_if_fail (MCM_IS_EXIF (exif), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* check exists */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL)
		goto out;

	/* get EXIF data in different ways depending on content type */
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "image/tiff") == 0) {
		filename = g_file_get_path (file);
		ret = mcm_exif_parse_tiff (exif, filename, error);
		goto out;
	}
	if (g_strcmp0 (content_type, "image/jpeg") == 0) {
		filename = g_file_get_path (file);
		ret = mcm_exif_parse_jpeg (exif, filename, error);
		goto out;
	}

	/* no support */
	g_set_error (error,
		     MCM_EXIF_ERROR,
		     MCM_EXIF_ERROR_NO_SUPPORT,
		     "no support for %s content type", content_type);
out:
	g_free (filename);
	if (info != NULL)
		g_object_unref (info);
	return ret;
}

/**
 * mcm_exif_get_manufacturer:
 **/
const gchar *
mcm_exif_get_manufacturer (McmExif *exif)
{
	g_return_val_if_fail (MCM_IS_EXIF (exif), NULL);
	return exif->priv->manufacturer;
}

/**
 * mcm_exif_get_model:
 **/
const gchar *
mcm_exif_get_model (McmExif *exif)
{
	g_return_val_if_fail (MCM_IS_EXIF (exif), NULL);
	return exif->priv->model;
}

/**
 * mcm_exif_get_serial:
 **/
const gchar *
mcm_exif_get_serial (McmExif *exif)
{
	g_return_val_if_fail (MCM_IS_EXIF (exif), NULL);
	return exif->priv->serial;
}

/**
 * mcm_exif_get_device_kind:
 **/
McmDeviceKind
mcm_exif_get_device_kind (McmExif *exif)
{
	g_return_val_if_fail (MCM_IS_EXIF (exif), MCM_DEVICE_KIND_UNKNOWN);
	return exif->priv->device_kind;
}

/**
 * mcm_exif_get_property:
 **/
static void
mcm_exif_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmExif *exif = MCM_EXIF (object);
	McmExifPrivate *priv = exif->priv;

	switch (prop_id) {
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_exif_class_init:
 **/
static void
mcm_exif_class_init (McmExifClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_exif_finalize;
	object_class->get_property = mcm_exif_get_property;

	/**
	 * McmExif:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * McmExif:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * McmExif:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * McmExif:device-kind:
	 */
	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, MCM_DEVICE_KIND_UNKNOWN,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	g_type_class_add_private (klass, sizeof (McmExifPrivate));
}

/**
 * mcm_exif_init:
 **/
static void
mcm_exif_init (McmExif *exif)
{
	exif->priv = MCM_EXIF_GET_PRIVATE (exif);
	exif->priv->manufacturer = NULL;
	exif->priv->model = NULL;
	exif->priv->serial = NULL;
	exif->priv->device_kind = MCM_DEVICE_KIND_CAMERA;
}

/**
 * mcm_exif_finalize:
 **/
static void
mcm_exif_finalize (GObject *object)
{
	McmExif *exif = MCM_EXIF (object);
	McmExifPrivate *priv = exif->priv;

	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	G_OBJECT_CLASS (mcm_exif_parent_class)->finalize (object);
}

/**
 * mcm_exif_new:
 *
 * Return value: a new McmExif object.
 **/
McmExif *
mcm_exif_new (void)
{
	McmExif *exif;
	exif = g_object_new (MCM_TYPE_EXIF, NULL);
	return MCM_EXIF (exif);
}

