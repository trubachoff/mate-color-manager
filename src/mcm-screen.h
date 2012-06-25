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

#ifndef __MCM_SCREEN_H
#define __MCM_SCREEN_H

#include <glib-object.h>
#include <gio/gio.h>
#include <libmateui/mate-rr.h>

G_BEGIN_DECLS

#define MCM_TYPE_SCREEN		(mcm_screen_get_type ())
#define MCM_SCREEN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_SCREEN, McmScreen))
#define MCM_SCREEN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_SCREEN, McmScreenClass))
#define MCM_IS_SCREEN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_SCREEN))
#define MCM_IS_SCREEN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_SCREEN))
#define MCM_SCREEN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_SCREEN, McmScreenClass))
#define MCM_SCREEN_ERROR	(mcm_screen_error_quark ())
#define MCM_SCREEN_TYPE_ERROR	(mcm_screen_error_get_type ())

typedef struct _McmScreenPrivate	McmScreenPrivate;
typedef struct _McmScreen		McmScreen;
typedef struct _McmScreenClass		McmScreenClass;

/**
 * McmScreenError:
 * @MCM_SCREEN_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	MCM_SCREEN_ERROR_FAILED
} McmScreenError;

struct _McmScreen
{
	 GObject		 parent;
	 McmScreenPrivate	*priv;
};

struct _McmScreenClass
{
	GObjectClass	parent_class;

	/* signals */
	void		(* outputs_changed)		(McmScreen		*screen);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_screen_get_type		  	(void);
McmScreen	*mcm_screen_new				(void);

MateRROutput	*mcm_screen_get_output_by_name		(McmScreen		*screen,
							 const gchar		*output_name,
							 GError			**error);
MateRROutput	**mcm_screen_get_outputs		(McmScreen		*screen,
							 GError			**error);
G_END_DECLS

#endif /* __MCM_SCREEN_H */

