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

#ifndef __MCM_XSERVER_H
#define __MCM_XSERVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_XSERVER		(mcm_xserver_get_type ())
#define MCM_XSERVER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_XSERVER, McmXserver))
#define MCM_XSERVER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_XSERVER, McmXserverClass))
#define MCM_IS_XSERVER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_XSERVER))
#define MCM_IS_XSERVER_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_XSERVER))
#define MCM_XSERVER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_XSERVER, McmXserverClass))

typedef struct _McmXserverPrivate	McmXserverPrivate;
typedef struct _McmXserver		McmXserver;
typedef struct _McmXserverClass	McmXserverClass;

struct _McmXserver
{
	 GObject		 parent;
	 McmXserverPrivate	*priv;
};

struct _McmXserverClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_xserver_get_type		  		(void);
McmXserver	*mcm_xserver_new				(void);

/* per screen */
gboolean	 mcm_xserver_get_root_window_profile_data	(McmXserver		*xserver,
								 guint8			**data,
								 gsize			*length,
								 GError			**error);
gboolean	 mcm_xserver_set_root_window_profile_data	(McmXserver		*xserver,
								 const guint8		*data,
								 gsize			 length,
								 GError			**error);
gboolean	 mcm_xserver_set_root_window_profile		(McmXserver		*xserver,
								 const gchar		*filename,
								 GError			**error);
gboolean	 mcm_xserver_remove_root_window_profile		(McmXserver		*xserver,
								 GError			**error);
gboolean	 mcm_xserver_set_protocol_version		(McmXserver		*xserver,
								 guint			 major,
								 guint			 minor,
								 GError			**error);
gboolean	 mcm_xserver_remove_protocol_version		(McmXserver		*xserver,
								 GError			**error);
gboolean	 mcm_xserver_get_protocol_version		(McmXserver		*xserver,
								 guint			*major,
								 guint			*minor,
								 GError			**error);

/* per output */
gboolean	 mcm_xserver_get_output_profile_data		(McmXserver		*xserver,
								 const gchar		*output_name,
								 guint8			**data,
								 gsize			*length,
								 GError			**error);
gboolean	 mcm_xserver_set_output_profile_data		(McmXserver		*xserver,
								 const gchar		*output_name,
								 const guint8		*data,
								 gsize			 length,
								 GError			**error);
gboolean	 mcm_xserver_set_output_profile			(McmXserver		*xserver,
								 const gchar		*output_name,
								 const gchar		*filename,
								 GError			**error);
gboolean	 mcm_xserver_remove_output_profile		(McmXserver		*xserver,
								 const gchar		*output_name,
								 GError			**error);

G_END_DECLS

#endif /* __MCM_XSERVER_H */

