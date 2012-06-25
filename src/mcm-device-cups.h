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

#ifndef __MCM_DEVICE_CUPS_H
#define __MCM_DEVICE_CUPS_H

#include <glib-object.h>
#include <cups/cups.h>

#include "mcm-device.h"

G_BEGIN_DECLS

#define MCM_TYPE_DEVICE_CUPS		(mcm_device_cups_get_type ())
#define MCM_DEVICE_CUPS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_DEVICE_CUPS, McmDeviceCups))
#define MCM_IS_DEVICE_CUPS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_DEVICE_CUPS))

typedef struct _McmDeviceCupsPrivate	McmDeviceCupsPrivate;
typedef struct _McmDeviceCups		McmDeviceCups;
typedef struct _McmDeviceCupsClass	McmDeviceCupsClass;

struct _McmDeviceCups
{
	 McmDevice			 parent;
	 McmDeviceCupsPrivate		*priv;
};

struct _McmDeviceCupsClass
{
	McmDeviceClass	parent_class;
};

GType		 mcm_device_cups_get_type		  	(void);
McmDevice	*mcm_device_cups_new				(void);
gboolean	 mcm_device_cups_set_from_dest			(McmDevice	*device,
								 http_t		*http,
								 cups_dest_t	 dest,
								 GError		**error);

G_END_DECLS

#endif /* __MCM_DEVICE_CUPS_H */

