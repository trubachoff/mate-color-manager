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
 * SECTION:mcm-xyz
 * @short_description: XYZ color object
 *
 * This object represents a XYZ color block.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "mcm-xyz.h"

#include "egg-debug.h"

#define MCM_XYZ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_XYZ, McmXyzPrivate))

/**
 * McmXyzPrivate:
 *
 * Private #McmXyz data
 **/
struct _McmXyzPrivate
{
	gdouble				 cie_x;
	gdouble				 cie_y;
	gdouble				 cie_z;
};

enum {
	PROP_0,
	PROP_CIE_X,
	PROP_CIE_Y,
	PROP_CIE_Z,
	PROP_LAST
};

G_DEFINE_TYPE (McmXyz, mcm_xyz, G_TYPE_OBJECT)

/**
 * mcm_xyz_print:
 **/
void
mcm_xyz_print (McmXyz *xyz)
{
	McmXyzPrivate *priv = xyz->priv;
	g_print ("xyz: %f,%f,%f\n", priv->cie_x, priv->cie_y, priv->cie_z);
}

/**
 * mcm_xyz_clear:
 **/
void
mcm_xyz_clear (McmXyz *xyz)
{
	McmXyzPrivate *priv = xyz->priv;
	priv->cie_x = 0.0f;
	priv->cie_y = 0.0f;
	priv->cie_z = 0.0f;
}

/**
 * mcm_xyz_get_x:
 **/
gdouble
mcm_xyz_get_x (McmXyz *xyz)
{
	McmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_x / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * mcm_xyz_get_y:
 **/
gdouble
mcm_xyz_get_y (McmXyz *xyz)
{
	McmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_y / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * mcm_xyz_get_z:
 **/
gdouble
mcm_xyz_get_z (McmXyz *xyz)
{
	McmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_z / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * mcm_xyz_get_property:
 **/
static void
mcm_xyz_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmXyz *xyz = MCM_XYZ (object);
	McmXyzPrivate *priv = xyz->priv;

	switch (prop_id) {
	case PROP_CIE_X:
		g_value_set_double (value, priv->cie_x);
		break;
	case PROP_CIE_Y:
		g_value_set_double (value, priv->cie_y);
		break;
	case PROP_CIE_Z:
		g_value_set_double (value, priv->cie_z);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_xyz_set_property:
 **/
static void
mcm_xyz_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmXyz *xyz = MCM_XYZ (object);
	McmXyzPrivate *priv = xyz->priv;

	switch (prop_id) {
	case PROP_CIE_X:
		priv->cie_x = g_value_get_double (value);
		egg_debug ("CIE x now %f", priv->cie_x);
		break;
	case PROP_CIE_Y:
		priv->cie_y = g_value_get_double (value);
		egg_debug ("CIE y now %f", priv->cie_y);
		break;
	case PROP_CIE_Z:
		priv->cie_z = g_value_get_double (value);
		egg_debug ("CIE z now %f", priv->cie_z);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_xyz_class_init:
 **/
static void
mcm_xyz_class_init (McmXyzClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = mcm_xyz_get_property;
	object_class->set_property = mcm_xyz_set_property;

	/**
	 * McmXyz:cie-x:
	 */
	pspec = g_param_spec_double ("cie-x", NULL, NULL,
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0f,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_X, pspec);

	/**
	 * McmXyz:cie-y:
	 */
	pspec = g_param_spec_double ("cie-y", NULL, NULL,
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0f,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_Y, pspec);

	/**
	 * McmXyz:cie-z:
	 */
	pspec = g_param_spec_double ("cie-z", NULL, NULL,
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0f,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_Z, pspec);

	g_type_class_add_private (klass, sizeof (McmXyzPrivate));
}

/**
 * mcm_xyz_init:
 **/
static void
mcm_xyz_init (McmXyz *xyz)
{
	xyz->priv = MCM_XYZ_GET_PRIVATE (xyz);
}

/**
 * mcm_xyz_new:
 *
 * Return value: a new McmXyz object.
 **/
McmXyz *
mcm_xyz_new (void)
{
	McmXyz *xyz;
	xyz = g_object_new (MCM_TYPE_XYZ, NULL);
	return MCM_XYZ (xyz);
}

