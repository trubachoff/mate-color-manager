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

#ifndef __MCM_DMI_H
#define __MCM_DMI_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_DMI		(mcm_dmi_get_type ())
#define MCM_DMI(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_DMI, McmDmi))
#define MCM_DMI_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_DMI, McmDmiClass))
#define MCM_IS_DMI(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_DMI))
#define MCM_IS_DMI_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_DMI))
#define MCM_DMI_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_DMI, McmDmiClass))

typedef struct _McmDmiPrivate	McmDmiPrivate;
typedef struct _McmDmi		McmDmi;
typedef struct _McmDmiClass	McmDmiClass;

struct _McmDmi
{
	 GObject		 parent;
	 McmDmiPrivate		*priv;
};

struct _McmDmiClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_dmi_get_type		  	(void);
McmDmi		*mcm_dmi_new				(void);
const gchar	*mcm_dmi_get_name			(McmDmi		*dmi);
const gchar	*mcm_dmi_get_version			(McmDmi		*dmi);
const gchar	*mcm_dmi_get_vendor			(McmDmi		*dmi);

G_END_DECLS

#endif /* __MCM_DMI_H */

