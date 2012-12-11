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
//#include <math.h>
#include <tiff.h>
#include <tiffio.h>

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
};

enum {
	PROP_0,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_LAST
};

G_DEFINE_TYPE (McmExif, mcm_exif, G_TYPE_OBJECT)

/**
 * mcm_exif_parse:
 **/
gboolean
mcm_exif_parse (McmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret = TRUE;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	const gchar *serial = NULL;
	TIFF *tiff;
	McmExifPrivate *priv = exif->priv;

	g_return_val_if_fail (MCM_IS_EXIF (exif), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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

	/* create copies for ourselves */
	priv->manufacturer = g_strdup (manufacturer);
	priv->model = g_strdup (model);
	priv->serial = g_strdup (serial);
out:
	TIFFClose (tiff);
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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

#if 0
/**
 * mcm_exif_set_property:
 **/
static void
mcm_exif_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}
#endif

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
//	object_class->set_property = mcm_exif_set_property;

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

