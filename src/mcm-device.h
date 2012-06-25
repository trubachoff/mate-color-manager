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

#ifndef __MCM_DEVICE_H
#define __MCM_DEVICE_H

#include <glib-object.h>

#include "mcm-enum.h"

G_BEGIN_DECLS

#define MCM_TYPE_DEVICE			(mcm_device_get_type ())
#define MCM_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_DEVICE, McmDevice))
#define MCM_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_DEVICE, McmDeviceClass))
#define MCM_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_DEVICE))
#define MCM_IS_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_DEVICE))
#define MCM_DEVICE_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_DEVICE, McmDeviceClass))

typedef struct _McmDevicePrivate	McmDevicePrivate;
typedef struct _McmDevice		McmDevice;
typedef struct _McmDeviceClass		McmDeviceClass;

struct _McmDevice
{
	 GObject			 parent;
	 McmDevicePrivate		*priv;
};

struct _McmDeviceClass
{
	GObjectClass	parent_class;
	void		 (*changed)			(McmDevice		*device);
	gboolean	 (*apply)			(McmDevice		*device,
							 GError			**error);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType			 mcm_device_get_type		  	(void);
gboolean		 mcm_device_load			(McmDevice	*device,
								 GError		**error);
gboolean		 mcm_device_save			(McmDevice	*device,
								 GError		**error);
gboolean		 mcm_device_apply			(McmDevice	*device,
								 GError		**error);

/* accessors */
McmDeviceKind		 mcm_device_get_kind			(McmDevice	*device);
void			 mcm_device_set_kind			(McmDevice	*device,
								 McmDeviceKind kind);
gboolean		 mcm_device_get_connected		(McmDevice	*device);
void			 mcm_device_set_connected		(McmDevice	*device,
								 gboolean	 connected);
gboolean		 mcm_device_get_virtual			(McmDevice	*device);
void			 mcm_device_set_virtual			(McmDevice	*device,
								 gboolean	 virtual);
gboolean		 mcm_device_get_saved			(McmDevice	*device);
void			 mcm_device_set_saved			(McmDevice	*device,
								 gboolean	 saved);
gfloat			 mcm_device_get_gamma			(McmDevice	*device);
void			 mcm_device_set_gamma			(McmDevice	*device,
								 gfloat		 gamma);
gfloat			 mcm_device_get_brightness		(McmDevice	*device);
void			 mcm_device_set_brightness		(McmDevice	*device,
								 gfloat		 brightness);
gfloat			 mcm_device_get_contrast		(McmDevice	*device);
void			 mcm_device_set_contrast		(McmDevice	*device,
								 gfloat		 contrast);
McmColorspace		 mcm_device_get_colorspace		(McmDevice	*device);
void			 mcm_device_set_colorspace		(McmDevice	*device,
								 McmColorspace colorspace);
const gchar		*mcm_device_get_id			(McmDevice	*device);
void			 mcm_device_set_id			(McmDevice	*device,
								 const gchar 	*id);
const gchar		*mcm_device_get_serial			(McmDevice	*device);
void			 mcm_device_set_serial			(McmDevice	*device,
								 const gchar 	*serial);
const gchar		*mcm_device_get_manufacturer		(McmDevice	*device);
void			 mcm_device_set_manufacturer		(McmDevice	*device,
								 const gchar 	*manufacturer);
const gchar		*mcm_device_get_model			(McmDevice	*device);
void			 mcm_device_set_model			(McmDevice	*device,
								 const gchar 	*model);
const gchar		*mcm_device_get_title			(McmDevice	*device);
void			 mcm_device_set_title			(McmDevice	*device,
								 const gchar 	*title);
const gchar		*mcm_device_get_profile_filename	(McmDevice	*device);
void			 mcm_device_set_profile_filename	(McmDevice	*device,
								 const gchar 	*profile_filename);
glong			 mcm_device_get_modified_time		(McmDevice	*device);

G_END_DECLS

#endif /* __MCM_DEVICE_H */

