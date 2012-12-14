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

#include "config.h"

#include <glib-object.h>

#include "mcm-device-virtual.h"
#include "mcm-enum.h"
#include "mcm-utils.h"

#include "egg-debug.h"

#define MCM_DEVICE_VIRTUAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_DEVICE_VIRTUAL, McmDeviceVirtualPrivate))

G_DEFINE_TYPE (McmDeviceVirtual, mcm_device_virtual, MCM_TYPE_DEVICE)

/**
 * mcm_device_virtual_create_from_params:
 **/
gboolean
mcm_device_virtual_create_from_params (McmDeviceVirtual *device_virtual,
				       McmDeviceKind	 device_kind,
				       const gchar      *model,
				       const gchar      *manufacturer,
				       const gchar      *serial,
				       McmColorspace	 colorspace)
{
	gchar *id;
	gchar *title;

	/* make some stuff up */
	title = g_strdup_printf ("%s - %s", manufacturer, model);
	id = g_strdup_printf ("%s_%s", manufacturer, model);
	mcm_utils_alphanum_lcase (id);

	/* create the device */
	egg_debug ("adding %s '%s'", id, title);
	g_object_set (device_virtual,
		      "connected", FALSE,
		      "kind", device_kind,
		      "id", id,
		      "manufacturer", manufacturer,
		      "model", model,
		      "serial", serial,
		      "title", title,
		      "colorspace", colorspace,
		      "virtual", TRUE,
		      NULL);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * mcm_device_virtual_class_init:
 **/
static void
mcm_device_virtual_class_init (McmDeviceVirtualClass *klass)
{
}

/**
 * mcm_device_virtual_init:
 **/
static void
mcm_device_virtual_init (McmDeviceVirtual *device_virtual)
{
}

/**
 * mcm_device_virtual_new:
 *
 * Return value: a new #McmDevice object.
 **/
McmDevice *
mcm_device_virtual_new (void)
{
	McmDevice *device;
	device = g_object_new (MCM_TYPE_DEVICE_VIRTUAL, NULL);
	return MCM_DEVICE (device);
}

