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

#ifndef __MCM_CLUT_H
#define __MCM_CLUT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_CLUT		(mcm_clut_get_type ())
#define MCM_CLUT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_CLUT, McmClut))
#define MCM_CLUT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_CLUT, McmClutClass))
#define MCM_IS_CLUT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_CLUT))
#define MCM_IS_CLUT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_CLUT))
#define MCM_CLUT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_CLUT, McmClutClass))

typedef struct _McmClutPrivate	McmClutPrivate;
typedef struct _McmClut		McmClut;
typedef struct _McmClutClass	McmClutClass;

struct _McmClut
{
	 GObject		 parent;
	 McmClutPrivate		*priv;
};

struct _McmClutClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

typedef struct {
	guint32		 red;
	guint32		 green;
	guint32		 blue;
} McmClutData;

GType		 mcm_clut_get_type		  	(void);
McmClut		*mcm_clut_new				(void);
GPtrArray	*mcm_clut_get_array			(McmClut		*clut);
gboolean	 mcm_clut_set_source_array		(McmClut		*clut,
							 GPtrArray		*array);
gboolean	 mcm_clut_reset				(McmClut		*clut);
void		 mcm_clut_print				(McmClut		*clut);
guint		 mcm_clut_get_size			(McmClut		*clut);

G_END_DECLS

#endif /* __MCM_CLUT_H */

