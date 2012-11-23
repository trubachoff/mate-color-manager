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

#ifndef __MCM_COLORIMETER_H
#define __MCM_COLORIMETER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_COLORIMETER		(mcm_colorimeter_get_type ())
#define MCM_COLORIMETER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_COLORIMETER, McmColorimeter))
#define MCM_COLORIMETER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_COLORIMETER, McmColorimeterClass))
#define MCM_IS_COLORIMETER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_COLORIMETER))
#define MCM_IS_COLORIMETER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_COLORIMETER))
#define MCM_COLORIMETER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_COLORIMETER, McmColorimeterClass))

typedef struct _McmColorimeterPrivate	McmColorimeterPrivate;
typedef struct _McmColorimeter		McmColorimeter;
typedef struct _McmColorimeterClass	McmColorimeterClass;

struct _McmColorimeter
{
	 GObject			 parent;
	 McmColorimeterPrivate		*priv;
};

struct _McmColorimeterClass
{
	GObjectClass	parent_class;
	void		(* changed)			(void);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

typedef enum {
	MCM_COLORIMETER_KIND_HUEY,
	MCM_COLORIMETER_KIND_COLOR_MUNKI,
	MCM_COLORIMETER_KIND_SPYDER,
	MCM_COLORIMETER_KIND_DTP20,
	MCM_COLORIMETER_KIND_DTP22,
	MCM_COLORIMETER_KIND_DTP41,
	MCM_COLORIMETER_KIND_DTP51,
	MCM_COLORIMETER_KIND_SPECTRO_SCAN,
	MCM_COLORIMETER_KIND_I1_PRO,
	MCM_COLORIMETER_KIND_COLORIMTRE_HCFR,
	MCM_COLORIMETER_KIND_UNKNOWN
} McmColorimeterKind;

GType			 mcm_colorimeter_get_type		(void);
McmColorimeter		*mcm_colorimeter_new			(void);

/* accessors */
const gchar		*mcm_colorimeter_get_model		(McmColorimeter		*colorimeter);
const gchar		*mcm_colorimeter_get_vendor		(McmColorimeter		*colorimeter);
gboolean		 mcm_colorimeter_get_present		(McmColorimeter		*colorimeter);
McmColorimeterKind	 mcm_colorimeter_get_kind		(McmColorimeter		*colorimeter);
gboolean		 mcm_colorimeter_supports_display	(McmColorimeter 	*colorimeter);
gboolean		 mcm_colorimeter_supports_projector	(McmColorimeter 	*colorimeter);
gboolean		 mcm_colorimeter_supports_printer	(McmColorimeter		*colorimeter);
gboolean 		 mcm_colorimeter_supports_spot		(McmColorimeter		*colorimeter);
const gchar		*mcm_colorimeter_kind_to_string		(McmColorimeterKind	 colorimeter_kind);
McmColorimeterKind	 mcm_colorimeter_kind_from_string	(const gchar		*colorimeter_kind);

G_END_DECLS

#endif /* __MCM_COLORIMETER_H */

