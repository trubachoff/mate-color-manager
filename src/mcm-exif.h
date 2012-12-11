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

#ifndef __MCM_EXIF_H
#define __MCM_EXIF_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mcm-enum.h"

G_BEGIN_DECLS

#define MCM_TYPE_EXIF		(mcm_exif_get_type ())
#define MCM_EXIF(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_EXIF, McmExif))
#define MCM_EXIF_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_EXIF, McmExifClass))
#define MCM_IS_EXIF(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_EXIF))
#define MCM_IS_EXIF_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_EXIF))
#define MCM_EXIF_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_EXIF, McmExifClass))

typedef struct _McmExifPrivate	McmExifPrivate;
typedef struct _McmExif		McmExif;
typedef struct _McmExifClass	McmExifClass;

struct _McmExif
{
	 GObject		 parent;
	 McmExifPrivate		*priv;
};

struct _McmExifClass
{
	GObjectClass	parent_class;
};

typedef enum
{
	MCM_EXIF_ERROR_NO_DATA,
	MCM_EXIF_ERROR_NO_SUPPORT,
	MCM_EXIF_ERROR_INTERNAL
} McmExifError;

/* dummy */
#define MCM_EXIF_ERROR	1

GType		 mcm_exif_get_type		  	(void);
McmExif		*mcm_exif_new				(void);

const gchar	*mcm_exif_get_manufacturer		(McmExif	*exif);
const gchar	*mcm_exif_get_model			(McmExif	*exif);
const gchar	*mcm_exif_get_serial			(McmExif	*exif);
McmDeviceKind	 mcm_exif_get_device_kind		(McmExif	*exif);
gboolean	 mcm_exif_parse				(McmExif	*exif,
							 GFile		*file,
							 GError		**error);

G_END_DECLS

#endif /* __MCM_EXIF_H */

