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

#include "mcm-device-udev.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_device_udev_finalize	(GObject     *object);

#define MCM_DEVICE_UDEV_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DEVICE_UDEV, McmDeviceUdevPrivate))

/**
 * McmDeviceUdevPrivate:
 *
 * Private #McmDeviceUdev data
 **/
struct _McmDeviceUdevPrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (McmDeviceUdev, mcm_device_udev, MCM_TYPE_DEVICE)

/**
 * mcm_device_udev_get_id_for_udev_device:
 **/
static gchar *
mcm_device_udev_get_id_for_udev_device (GUdevDevice *udev_device)
{
	gchar *id;

	/* get id */
	id = g_strdup_printf ("sysfs_%s_%s",
			      g_udev_device_get_property (udev_device, "ID_VENDOR"),
			      g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* replace unsafe chars */
	mcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * mcm_device_udev_set_from_device:
 **/
gboolean
mcm_device_udev_set_from_device (McmDevice *device, GUdevDevice *udev_device, GError **error)
{
	gchar *id = NULL;
	gchar *title;
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	McmDeviceKind kind;
	const gchar *value;
	const gchar *sysfs_path;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "MCM_TYPE");
	kind = mcm_device_kind_from_string (value);

	/* add new device */
	id = mcm_device_udev_get_id_for_udev_device (udev_device);
	title = g_strdup_printf ("%s - %s",
				g_udev_device_get_property (udev_device, "ID_VENDOR"),
				g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* turn space delimiters into spaces */
	g_strdelimit (title, "_", ' ');

	/* get sysfs path */
	sysfs_path = g_udev_device_get_sysfs_path (udev_device);

	/* create device */
	manufacturer = g_strdup (g_udev_device_get_property (udev_device, "ID_VENDOR"));
	model = g_strdup (g_udev_device_get_property (udev_device, "ID_MODEL"));
	serial = g_strdup (g_udev_device_get_property (udev_device, "ID_SERIAL"));

	/* get rid of underscores */
	if (manufacturer != NULL) {
		g_strdelimit (manufacturer, "_", ' ');
		g_strchomp (manufacturer);
	}
	if (model != NULL) {
		g_strdelimit (model, "_", ' ');
		g_strchomp (model);
	}
	if (serial != NULL) {
		g_strdelimit (serial, "_", ' ');
		g_strchomp (serial);
	}

	g_object_set (device,
		      "kind", kind,
		      "colorspace", MCM_COLORSPACE_RGB,
		      "id", id,
		      "connected", TRUE,
		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", sysfs_path,
		      NULL);

	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * mcm_device_udev_get_property:
 **/
static void
mcm_device_udev_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmDeviceUdev *device_udev = MCM_DEVICE_UDEV (object);
	McmDeviceUdevPrivate *priv = device_udev->priv;

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
 * mcm_device_udev_set_property:
 **/
static void
mcm_device_udev_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmDeviceUdev *device_udev = MCM_DEVICE_UDEV (object);
	McmDeviceUdevPrivate *priv = device_udev->priv;

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
 * mcm_device_udev_class_init:
 **/
static void
mcm_device_udev_class_init (McmDeviceUdevClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_device_udev_finalize;
	object_class->get_property = mcm_device_udev_get_property;
	object_class->set_property = mcm_device_udev_set_property;


	/**
	 * McmDeviceUdev:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (McmDeviceUdevPrivate));
}

/**
 * mcm_device_udev_init:
 **/
static void
mcm_device_udev_init (McmDeviceUdev *device_udev)
{
	device_udev->priv = MCM_DEVICE_UDEV_GET_PRIVATE (device_udev);
	device_udev->priv->native_device = NULL;
}

/**
 * mcm_device_udev_finalize:
 **/
static void
mcm_device_udev_finalize (GObject *object)
{
	McmDeviceUdev *device_udev = MCM_DEVICE_UDEV (object);
	McmDeviceUdevPrivate *priv = device_udev->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (mcm_device_udev_parent_class)->finalize (object);
}

/**
 * mcm_device_udev_new:
 *
 * Return value: a new #McmDevice object.
 **/
McmDevice *
mcm_device_udev_new (void)
{
	McmDevice *device;
	device = g_object_new (MCM_TYPE_DEVICE_UDEV, NULL);
	return MCM_DEVICE (device);
}

