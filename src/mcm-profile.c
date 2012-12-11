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

/**
 * SECTION:mcm-profile
 * @short_description: A parser object that understands the ICC profile data format.
 *
 * This object is a simple parser for the ICC binary profile data. If only understands
 * a subset of the ICC profile, just enought to get some metadata and the LUT.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "egg-debug.h"

#include "mcm-profile.h"
#include "mcm-utils.h"
#include "mcm-xyz.h"
#include "mcm-profile-lcms1.h"

static void     mcm_profile_finalize	(GObject     *object);

#define MCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_PROFILE, McmProfilePrivate))

/**
 * McmProfilePrivate:
 *
 * Private #McmProfile data
 **/
struct _McmProfilePrivate
{
	McmProfileKind		 kind;
	McmColorspace		 colorspace;
	guint			 size;
	gboolean		 has_vcgt;
	gboolean		 can_delete;
	gchar			*description;
	gchar			*filename;
	gchar			*copyright;
	gchar			*manufacturer;
	gchar			*model;
	gchar			*datetime;
	McmXyz			*white;
	McmXyz			*black;
	McmXyz			*red;
	McmXyz			*green;
	McmXyz			*blue;
	GFileMonitor		*monitor;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_DESCRIPTION,
	PROP_FILENAME,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_SIZE,
	PROP_HAS_VCGT,
	PROP_CAN_DELETE,
	PROP_WHITE,
	PROP_BLACK,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_LAST
};

G_DEFINE_TYPE (McmProfile, mcm_profile, G_TYPE_OBJECT)

static void mcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, McmProfile *profile);

/**
 * mcm_profile_get_description:
 **/
const gchar *
mcm_profile_get_description (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->description;
}

/**
 * mcm_profile_set_description:
 **/
void
mcm_profile_set_description (McmProfile *profile, const gchar *description)
{
	McmProfilePrivate *priv = profile->priv;
	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->description);
	priv->description = g_strdup (description);

	if (priv->description != NULL)
		mcm_utils_ensure_printable (priv->description);

	/* there's nothing sensible to display */
	if (priv->description == NULL || priv->description[0] == '\0') {
		g_free (priv->description);
		if (priv->filename != NULL) {
			priv->description = g_path_get_basename (priv->filename);
		} else {
			/* TRANSLATORS: this is where the ICC profile_lcms1 has no description */
			priv->description = g_strdup (_("Missing description"));
		}
	}
	g_object_notify (G_OBJECT (profile), "description");
}


/**
 * mcm_profile_get_filename:
 **/
const gchar *
mcm_profile_get_filename (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->filename;
}

/**
 * mcm_profile_has_colorspace_description:
 *
 * Return value: if the description mentions the profile colorspace explicity,
 * e.g. "Adobe RGB" for %MCM_COLORSPACE_RGB.
 **/
gboolean
mcm_profile_has_colorspace_description (McmProfile *profile)
{
	McmProfilePrivate *priv = profile->priv;
	g_return_val_if_fail (MCM_IS_PROFILE (profile), FALSE);

	/* for each profile type */
	if (priv->colorspace == MCM_COLORSPACE_RGB)
		return (g_strstr_len (priv->description, -1, "RGB") != NULL);
	if (priv->colorspace == MCM_COLORSPACE_CMYK)
		return (g_strstr_len (priv->description, -1, "CMYK") != NULL);

	/* nothing */
	return FALSE;
}

/**
 * mcm_profile_set_filename:
 **/
void
mcm_profile_set_filename (McmProfile *profile, const gchar *filename)
{
	McmProfilePrivate *priv = profile->priv;
	GFile *file;

	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->filename);
	priv->filename = g_strdup (filename);

	/* unref old instance */
	if (priv->monitor != NULL) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	/* setup watch on new profile */
	if (priv->filename != NULL) {
		file = g_file_new_for_path (priv->filename);
		priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
		if (priv->monitor != NULL)
			g_signal_connect (priv->monitor, "changed", G_CALLBACK(mcm_profile_file_monitor_changed_cb), profile);
		g_object_unref (file);
	}
	g_object_notify (G_OBJECT (profile), "filename");
}


/**
 * mcm_profile_get_copyright:
 **/
const gchar *
mcm_profile_get_copyright (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->copyright;
}

/**
 * mcm_profile_set_copyright:
 **/
void
mcm_profile_set_copyright (McmProfile *profile, const gchar *copyright)
{
	McmProfilePrivate *priv = profile->priv;

	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->copyright);
	priv->copyright = g_strdup (copyright);
	if (priv->copyright != NULL)
		mcm_utils_ensure_printable (priv->copyright);
	g_object_notify (G_OBJECT (profile), "copyright");
}


/**
 * mcm_profile_get_model:
 **/
const gchar *
mcm_profile_get_model (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->model;
}

/**
 * mcm_profile_set_model:
 **/
void
mcm_profile_set_model (McmProfile *profile, const gchar *model)
{
	McmProfilePrivate *priv = profile->priv;

	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->model);
	priv->model = g_strdup (model);
	if (priv->model != NULL)
		mcm_utils_ensure_printable (priv->model);
	g_object_notify (G_OBJECT (profile), "model");
}

/**
 * mcm_profile_get_manufacturer:
 **/
const gchar *
mcm_profile_get_manufacturer (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->manufacturer;
}

/**
 * mcm_profile_set_manufacturer:
 **/
void
mcm_profile_set_manufacturer (McmProfile *profile, const gchar *manufacturer)
{
	McmProfilePrivate *priv = profile->priv;

	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->manufacturer);
	priv->manufacturer = g_strdup (manufacturer);
	if (priv->manufacturer != NULL)
		mcm_utils_ensure_printable (priv->manufacturer);
	g_object_notify (G_OBJECT (profile), "manufacturer");
}


/**
 * mcm_profile_get_datetime:
 **/
const gchar *
mcm_profile_get_datetime (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), NULL);
	return profile->priv->datetime;
}

/**
 * mcm_profile_set_datetime:
 **/
void
mcm_profile_set_datetime (McmProfile *profile, const gchar *datetime)
{
	McmProfilePrivate *priv = profile->priv;

	g_return_if_fail (MCM_IS_PROFILE (profile));

	g_free (priv->datetime);
	priv->datetime = g_strdup (datetime);
	g_object_notify (G_OBJECT (profile), "datetime");
}


/**
 * mcm_profile_get_size:
 **/
guint
mcm_profile_get_size (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), 0);
	return profile->priv->size;
}

/**
 * mcm_profile_set_size:
 **/
void
mcm_profile_set_size (McmProfile *profile, guint size)
{
	g_return_if_fail (MCM_IS_PROFILE (profile));
	profile->priv->size = size;
	g_object_notify (G_OBJECT (profile), "size");
}


/**
 * mcm_profile_get_kind:
 **/
McmProfileKind
mcm_profile_get_kind (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), MCM_PROFILE_KIND_UNKNOWN);
	return profile->priv->kind;
}

/**
 * mcm_profile_set_kind:
 **/
void
mcm_profile_set_kind (McmProfile *profile, McmProfileKind kind)
{
	g_return_if_fail (MCM_IS_PROFILE (profile));
	profile->priv->kind = kind;
	g_object_notify (G_OBJECT (profile), "kind");
}


/**
 * mcm_profile_get_colorspace:
 **/
McmColorspace
mcm_profile_get_colorspace (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), MCM_COLORSPACE_UNKNOWN);
	return profile->priv->colorspace;
}

/**
 * mcm_profile_set_colorspace:
 **/
void
mcm_profile_set_colorspace (McmProfile *profile, McmColorspace colorspace)
{
	g_return_if_fail (MCM_IS_PROFILE (profile));
	profile->priv->colorspace = colorspace;
	g_object_notify (G_OBJECT (profile), "colorspace");
}


/**
 * mcm_profile_get_has_vcgt:
 **/
gboolean
mcm_profile_get_has_vcgt (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), FALSE);
	return profile->priv->has_vcgt;
}

/**
 * mcm_profile_set_has_vcgt:
 **/
void
mcm_profile_set_has_vcgt (McmProfile *profile, gboolean has_vcgt)
{
	g_return_if_fail (MCM_IS_PROFILE (profile));
	profile->priv->has_vcgt = has_vcgt;
	g_object_notify (G_OBJECT (profile), "has_vcgt");
}

/**
 * mcm_profile_get_can_delete:
 **/
gboolean
mcm_profile_get_can_delete (McmProfile *profile)
{
	g_return_val_if_fail (MCM_IS_PROFILE (profile), FALSE);
	return profile->priv->can_delete;
}

/**
 * mcm_profile_parse_data:
 **/
gboolean
mcm_profile_parse_data (McmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	McmProfilePrivate *priv = profile->priv;
	McmProfileClass *klass = MCM_PROFILE_GET_CLASS (profile);

	/* save the length */
	priv->size = length;

	/* do we have support */
	if (klass->parse_data == NULL) {
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* proxy */
	ret = klass->parse_data (profile, data, length, error);
out:
	return ret;
}

/**
 * mcm_profile_parse:
 **/
gboolean
mcm_profile_parse (McmProfile *profile, GFile *file, GError **error)
{
	gchar *data = NULL;
	gboolean ret = FALSE;
	gsize length;
	gchar *filename = NULL;
	GError *error_local = NULL;
	GFileInfo *info;

	g_return_val_if_fail (MCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	/* find out if the user could delete this profile */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
				  G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL)
		goto out;
	profile->priv->can_delete = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

	/* load files */
	ret = g_file_load_contents (file, NULL, &data, &length, NULL, &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "failed to load profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = mcm_profile_parse_data (profile, (const guint8*)data, length, error);
	if (!ret)
		goto out;

	/* save */
	filename = g_file_get_path (file);
	mcm_profile_set_filename (profile, filename);
out:
	if (info != NULL)
		g_object_unref (info);
	g_free (filename);
	g_free (data);
	return ret;
}

/**
 * mcm_profile_save:
 **/
gboolean
mcm_profile_save (McmProfile *profile, const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	McmProfilePrivate *priv = profile->priv;
	McmProfileClass *klass = MCM_PROFILE_GET_CLASS (profile);

	/* not loaded */
	if (priv->size == 0) {
		g_set_error_literal (error, 1, 0, "not loaded");
		goto out;
	}

	/* do we have support */
	if (klass->save == NULL) {
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* proxy */
	ret = klass->save (profile, filename, error);
out:
	return ret;
}

/**
 * mcm_profile_generate_vcgt:
 *
 * Free with g_object_unref();
 **/
McmClut *
mcm_profile_generate_vcgt (McmProfile *profile, guint size)
{
	McmClut *clut = NULL;
	McmProfileClass *klass = MCM_PROFILE_GET_CLASS (profile);

	/* do we have support */
	if (klass->generate_vcgt == NULL)
		goto out;

	/* proxy */
	clut = klass->generate_vcgt (profile, size);
out:
	return clut;
}

/**
 * mcm_profile_generate_curve:
 *
 * Free with g_object_unref();
 **/
McmClut *
mcm_profile_generate_curve (McmProfile *profile, guint size)
{
	McmClut *clut = NULL;
	McmProfileClass *klass = MCM_PROFILE_GET_CLASS (profile);

	/* do we have support */
	if (klass->generate_curve == NULL)
		goto out;

	/* proxy */
	clut = klass->generate_curve (profile, size);
out:
	return clut;
}

/**
 * mcm_profile_file_monitor_changed_cb:
 **/
static void
mcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, McmProfile *profile)
{
	McmProfilePrivate *priv = profile->priv;

	/* ony care about deleted events */
	if (event_type != G_FILE_MONITOR_EVENT_DELETED)
		goto out;

	/* just rescan everything */
	egg_debug ("%s deleted, clearing filename", priv->filename);
	mcm_profile_set_filename (profile, NULL);
out:
	return;
}

/**
 * mcm_profile_get_property:
 **/
static void
mcm_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmProfile *profile = MCM_PROFILE (object);
	McmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		g_value_set_string (value, priv->copyright);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DATETIME:
		g_value_set_string (value, priv->datetime);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, priv->has_vcgt);
		break;
	case PROP_CAN_DELETE:
		g_value_set_boolean (value, priv->can_delete);
		break;
	case PROP_WHITE:
		g_value_set_object (value, priv->white);
		break;
	case PROP_BLACK:
		g_value_set_object (value, priv->black);
		break;
	case PROP_RED:
		g_value_set_object (value, priv->red);
		break;
	case PROP_GREEN:
		g_value_set_object (value, priv->green);
		break;
	case PROP_BLUE:
		g_value_set_object (value, priv->blue);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_profile_set_property:
 **/
static void
mcm_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmProfile *profile = MCM_PROFILE (object);
	McmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		mcm_profile_set_copyright (profile, g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		mcm_profile_set_manufacturer (profile, g_value_get_string (value));
		break;
	case PROP_MODEL:
		mcm_profile_set_model (profile, g_value_get_string (value));
		break;
	case PROP_DATETIME:
		mcm_profile_set_datetime (profile, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		mcm_profile_set_description (profile, g_value_get_string (value));
		break;
	case PROP_FILENAME:
		mcm_profile_set_filename (profile, g_value_get_string (value));
		break;
	case PROP_KIND:
		mcm_profile_set_kind (profile, g_value_get_uint (value));
		break;
	case PROP_COLORSPACE:
		mcm_profile_set_colorspace (profile, g_value_get_uint (value));
		break;
	case PROP_SIZE:
		mcm_profile_set_size (profile, g_value_get_uint (value));
		break;
	case PROP_HAS_VCGT:
		mcm_profile_set_has_vcgt (profile, g_value_get_boolean (value));
		break;
	case PROP_WHITE:
		priv->white = g_value_dup_object (value);
		break;
	case PROP_BLACK:
		priv->black = g_value_dup_object (value);
		break;
	case PROP_RED:
		priv->red = g_value_dup_object (value);
		break;
	case PROP_GREEN:
		priv->green = g_value_dup_object (value);
		break;
	case PROP_BLUE:
		priv->blue = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_profile_class_init:
 **/
static void
mcm_profile_class_init (McmProfileClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_profile_finalize;
	object_class->get_property = mcm_profile_get_property;
	object_class->set_property = mcm_profile_set_property;

	/**
	 * McmProfile:copyright:
	 */
	pspec = g_param_spec_string ("copyright", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	/**
	 * McmProfile:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * McmProfile:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * McmProfile:datetime:
	 */
	pspec = g_param_spec_string ("datetime", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DATETIME, pspec);

	/**
	 * McmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * McmProfile:filename:
	 */
	pspec = g_param_spec_string ("filename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME, pspec);

	/**
	 * McmProfile:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * McmProfile:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * McmProfile:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	/**
	 * McmProfile:has-vcgt:
	 */
	pspec = g_param_spec_boolean ("has-vcgt", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HAS_VCGT, pspec);

	/**
	 * McmProfile:can-delete:
	 */
	pspec = g_param_spec_boolean ("can-delete", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CAN_DELETE, pspec);

	/**
	 * McmProfile:white:
	 */
	pspec = g_param_spec_object ("white", NULL, NULL,
				     MCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * McmProfile:black:
	 */
	pspec = g_param_spec_object ("black", NULL, NULL,
				     MCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLACK, pspec);

	/**
	 * McmProfile:red:
	 */
	pspec = g_param_spec_object ("red", NULL, NULL,
				     MCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * McmProfile:green:
	 */
	pspec = g_param_spec_object ("green", NULL, NULL,
				     MCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * McmProfile:blue:
	 */
	pspec = g_param_spec_object ("blue", NULL, NULL,
				     MCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

	g_type_class_add_private (klass, sizeof (McmProfilePrivate));
}

/**
 * mcm_profile_init:
 **/
static void
mcm_profile_init (McmProfile *profile)
{
	profile->priv = MCM_PROFILE_GET_PRIVATE (profile);
	profile->priv->can_delete = FALSE;
	profile->priv->monitor = NULL;
	profile->priv->kind = MCM_PROFILE_KIND_UNKNOWN;
	profile->priv->colorspace = MCM_COLORSPACE_UNKNOWN;
	profile->priv->white = mcm_xyz_new ();
	profile->priv->black = mcm_xyz_new ();
	profile->priv->red = mcm_xyz_new ();
	profile->priv->green = mcm_xyz_new ();
	profile->priv->blue = mcm_xyz_new ();
}

/**
 * mcm_profile_finalize:
 **/
static void
mcm_profile_finalize (GObject *object)
{
	McmProfile *profile = MCM_PROFILE (object);
	McmProfilePrivate *priv = profile->priv;

	g_free (priv->copyright);
	g_free (priv->description);
	g_free (priv->filename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->datetime);
	g_object_unref (priv->white);
	g_object_unref (priv->black);
	g_object_unref (priv->red);
	g_object_unref (priv->green);
	g_object_unref (priv->blue);
	if (priv->monitor != NULL)
		g_object_unref (priv->monitor);

	G_OBJECT_CLASS (mcm_profile_parent_class)->finalize (object);
}

/**
 * mcm_profile_new:
 *
 * Return value: a new McmProfile object.
 **/
McmProfile *
mcm_profile_new (void)
{
	McmProfile *profile;
	profile = g_object_new (MCM_TYPE_PROFILE, NULL);
	return MCM_PROFILE (profile);
}

/**
 * mcm_profile_default_new:
 *
 * Return value: a new McmProfile object.
 **/
McmProfile *
mcm_profile_default_new (void)
{
	McmProfile *profile = NULL;
	profile = MCM_PROFILE (mcm_profile_lcms1_new ());
	return profile;
}

