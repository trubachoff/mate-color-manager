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

#ifndef __MCM_PRINT_H
#define __MCM_PRINT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_PRINT		(mcm_print_get_type ())
#define MCM_PRINT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_PRINT, McmPrint))
#define MCM_PRINT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_PRINT, McmPrintClass))
#define MCM_IS_PRINT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_PRINT))
#define MCM_IS_PRINT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_PRINT))
#define MCM_PRINT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_PRINT, McmPrintClass))

typedef struct _McmPrintPrivate	McmPrintPrivate;
typedef struct _McmPrint	McmPrint;
typedef struct _McmPrintClass	McmPrintClass;

struct _McmPrint
{
	 GObject			 parent;
	 McmPrintPrivate		*priv;
};

struct _McmPrintClass
{
	GObjectClass	parent_class;
	void		(* status_changed)		(GtkPrintStatus status);
};


typedef GPtrArray	*(*McmPrintRenderCb)		(McmPrint		*print,
							 GtkPageSetup		*page_setup,
							 gpointer		 user_data,
							 GError			**error);

GType			 mcm_print_get_type		(void);
McmPrint		*mcm_print_new			(void);
gboolean		 mcm_print_with_render_callback	(McmPrint		*print,
							 GtkWindow		*window,
							 McmPrintRenderCb	 render_callback,
							 gpointer		 user_data,
							 GError			**error);

G_END_DECLS

#endif /* __MCM_PRINT_H */

