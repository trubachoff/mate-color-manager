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

#ifndef MCM_CELL_RENDERER_PROFILE_H
#define MCM_CELL_RENDERER_PROFILE_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "mcm-profile.h"

#define MCM_TYPE_CELL_RENDERER_PROFILE		(mcm_cell_renderer_profile_get_type())
#define MCM_CELL_RENDERER_PROFILE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), MCM_TYPE_CELL_RENDERER_PROFILE, McmCellRendererProfile))
#define MCM_CELL_RENDERER_PROFILE_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), MCM_TYPE_CELL_RENDERER_PROFILE, McmCellRendererProfileClass))
#define MCM_IS_CELL_RENDERER_PROFILE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), MCM_TYPE_CELL_RENDERER_PROFILE))
#define MCM_IS_CELL_RENDERER_PROFILE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), MCM_TYPE_CELL_RENDERER_PROFILE))
#define MCM_CELL_RENDERER_PROFILE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), MCM_TYPE_CELL_RENDERER_PROFILE, McmCellRendererProfileClass))

G_BEGIN_DECLS

typedef struct _McmCellRendererProfile		McmCellRendererProfile;
typedef struct _McmCellRendererProfileClass	McmCellRendererProfileClass;

struct _McmCellRendererProfile
{
	GtkCellRendererText	 parent;
	McmProfile		*profile;
	gboolean		 is_default;
	gchar			*markup;
};

struct _McmCellRendererProfileClass
{
	GtkCellRendererTextClass parent_class;
};

GType		 mcm_cell_renderer_profile_get_type	(void);
GtkCellRenderer	*mcm_cell_renderer_profile_new		(void);

G_END_DECLS

#endif /* MCM_CELL_RENDERER_PROFILE_H */

