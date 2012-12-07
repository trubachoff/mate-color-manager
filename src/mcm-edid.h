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

#ifndef __MCM_EDID_H
#define __MCM_EDID_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_EDID		(mcm_edid_get_type ())
#define MCM_EDID(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_EDID, McmEdid))
#define MCM_EDID_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_EDID, McmEdidClass))
#define MCM_IS_EDID(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_EDID))
#define MCM_IS_EDID_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_EDID))
#define MCM_EDID_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_EDID, McmEdidClass))

typedef struct _McmEdidPrivate	McmEdidPrivate;
typedef struct _McmEdid		McmEdid;
typedef struct _McmEdidClass	McmEdidClass;

struct _McmEdid
{
	 GObject		 parent;
	 McmEdidPrivate		*priv;
};

struct _McmEdidClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_edid_get_type		  	(void);
McmEdid		*mcm_edid_new				(void);
void		 mcm_edid_reset				(McmEdid		*edid);
gboolean	 mcm_edid_parse				(McmEdid		*edid,
							 const guint8		*data,
							 GError			**error);
const gchar	*mcm_edid_get_monitor_name		(McmEdid		*edid);
const gchar	*mcm_edid_get_vendor_name		(McmEdid		*edid);
const gchar	*mcm_edid_get_serial_number		(McmEdid		*edid);
const gchar	*mcm_edid_get_ascii_string		(McmEdid		*edid);
const gchar	*mcm_edid_get_pnp_id			(McmEdid		*edid);
guint		 mcm_edid_get_width			(McmEdid		*edid);
guint		 mcm_edid_get_height			(McmEdid		*edid);
gfloat		 mcm_edid_get_gamma			(McmEdid		*edid);

G_END_DECLS

#endif /* __MCM_EDID_H */

