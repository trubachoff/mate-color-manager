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
 * SECTION:mcm-dmi
 * @short_description: DMI parsing object
 *
 * This object parses DMI data blocks.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "mcm-dmi.h"
#include "mcm-tables.h"

#include "egg-debug.h"

static void     mcm_dmi_finalize	(GObject     *object);

#define MCM_DMI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DMI, McmDmiPrivate))

/**
 * McmDmiPrivate:
 *
 * Private #McmDmi data
 **/
struct _McmDmiPrivate
{
	gchar				*name;
	gchar				*version;
	gchar				*vendor;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_VERSION,
	PROP_VENDOR,
	PROP_LAST
};

static gpointer mcm_dmi_object = NULL;

G_DEFINE_TYPE (McmDmi, mcm_dmi, G_TYPE_OBJECT)

/**
 * mcm_dmi_get_data:
 **/
static gchar *
mcm_dmi_get_data (const gchar *filename)
{
	gboolean ret;
	GError *error = NULL;
	gchar *data = NULL;

	/* get the contents */
	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get contents of %s: %s", filename, error->message);
		g_error_free (error);
	}

	/* process the random chars and trailing spaces */
	if (data != NULL) {
		g_strdelimit (data, "\t_", ' ');
		g_strdelimit (data, "\n\r", '\0');
		g_strchomp (data);
	}

	/* don't return an empty string */
	if (data != NULL && data[0] == '\0') {
		g_free (data);
		data = NULL;
	}

	return data;
}


/**
 * mcm_dmi_get_name:
 **/
const gchar *
mcm_dmi_get_name (McmDmi *dmi)
{
	McmDmiPrivate *priv = dmi->priv;
	g_return_val_if_fail (MCM_IS_DMI (dmi), NULL);

	if (priv->name == NULL)
		priv->name = mcm_dmi_get_data ("/sys/class/dmi/id/product_name");
	if (priv->name == NULL)
		priv->name = mcm_dmi_get_data ("/sys/class/dmi/id/board_name");
	return priv->name;
}

/**
 * mcm_dmi_get_version:
 **/
const gchar *
mcm_dmi_get_version (McmDmi *dmi)
{
	McmDmiPrivate *priv = dmi->priv;
	g_return_val_if_fail (MCM_IS_DMI (dmi), NULL);

	if (priv->version == NULL)
		priv->version = mcm_dmi_get_data ("/sys/class/dmi/id/product_version");
	if (priv->version == NULL)
		priv->version = mcm_dmi_get_data ("/sys/class/dmi/id/chassis_version");
	if (priv->version == NULL)
		priv->version = mcm_dmi_get_data ("/sys/class/dmi/id/board_version");
	return priv->version;
}

/**
 * mcm_dmi_get_vendor:
 **/
const gchar *
mcm_dmi_get_vendor (McmDmi *dmi)
{
	McmDmiPrivate *priv = dmi->priv;
	g_return_val_if_fail (MCM_IS_DMI (dmi), NULL);

	if (priv->vendor == NULL)
		priv->vendor = mcm_dmi_get_data ("/sys/class/dmi/id/sys_vendor");
	if (priv->vendor == NULL)
		priv->vendor = mcm_dmi_get_data ("/sys/class/dmi/id/chassis_vendor");
	if (priv->vendor == NULL)
		priv->vendor = mcm_dmi_get_data ("/sys/class/dmi/id/board_vendor");
	return priv->vendor;
}

/**
 * mcm_dmi_get_property:
 **/
static void
mcm_dmi_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmDmi *dmi = MCM_DMI (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, mcm_dmi_get_name (dmi));
		break;
	case PROP_VERSION:
		g_value_set_string (value, mcm_dmi_get_version (dmi));
		break;
	case PROP_VENDOR:
		g_value_set_string (value, mcm_dmi_get_vendor (dmi));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_dmi_set_property:
 **/
static void
mcm_dmi_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_dmi_class_init:
 **/
static void
mcm_dmi_class_init (McmDmiClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_dmi_finalize;
	object_class->get_property = mcm_dmi_get_property;
	object_class->set_property = mcm_dmi_set_property;

	/**
	 * McmDmi:name:
	 */
	pspec = g_param_spec_string ("name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_NAME, pspec);

	/**
	 * McmDmi:version:
	 */
	pspec = g_param_spec_string ("version", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * McmDmi:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	g_type_class_add_private (klass, sizeof (McmDmiPrivate));
}

/**
 * mcm_dmi_init:
 **/
static void
mcm_dmi_init (McmDmi *dmi)
{
	dmi->priv = MCM_DMI_GET_PRIVATE (dmi);
	dmi->priv->name = NULL;
	dmi->priv->version = NULL;
	dmi->priv->vendor = NULL;
}

/**
 * mcm_dmi_finalize:
 **/
static void
mcm_dmi_finalize (GObject *object)
{
	McmDmi *dmi = MCM_DMI (object);
	McmDmiPrivate *priv = dmi->priv;

	g_free (priv->name);
	g_free (priv->version);
	g_free (priv->vendor);

	G_OBJECT_CLASS (mcm_dmi_parent_class)->finalize (object);
}

/**
 * mcm_dmi_new:
 *
 * Return value: a new McmDmi object.
 **/
McmDmi *
mcm_dmi_new (void)
{
	if (mcm_dmi_object != NULL) {
		g_object_ref (mcm_dmi_object);
	} else {
		mcm_dmi_object = g_object_new (MCM_TYPE_DMI, NULL);
		g_object_add_weak_pointer (mcm_dmi_object, &mcm_dmi_object);
	}
	return MCM_DMI (mcm_dmi_object);
}

