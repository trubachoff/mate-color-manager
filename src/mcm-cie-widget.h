/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __MCM_CIE_WIDGET_H__
#define __MCM_CIE_WIDGET_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MCM_TYPE_CIE_WIDGET		(mcm_cie_widget_get_type ())
#define MCM_CIE_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), MCM_TYPE_CIE_WIDGET, McmCieWidget))
#define MCM_CIE_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), MCM_CIE_WIDGET, McmCieWidgetClass))
#define MCM_IS_CIE_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MCM_TYPE_CIE_WIDGET))
#define MCM_IS_CIE_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_CIE_WIDGET))
#define MCM_CIE_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), MCM_TYPE_CIE_WIDGET, McmCieWidgetClass))

typedef struct McmCieWidget		McmCieWidget;
typedef struct McmCieWidgetClass	McmCieWidgetClass;
typedef struct McmCieWidgetPrivate	McmCieWidgetPrivate;

struct McmCieWidget
{
	GtkDrawingArea		 parent;
	McmCieWidgetPrivate	*priv;
};

struct McmCieWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 mcm_cie_widget_get_type		(void);
GtkWidget	*mcm_cie_widget_new			(void);

G_END_DECLS

#endif
