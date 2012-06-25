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

#ifndef __MCM_CLIENT_H
#define __MCM_CLIENT_H

#include <glib-object.h>
#include <gdk/gdk.h>

#include "mcm-device.h"

G_BEGIN_DECLS

#define MCM_TYPE_CLIENT			(mcm_client_get_type ())
#define MCM_CLIENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_CLIENT, McmClient))
#define MCM_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_CLIENT, McmClientClass))
#define MCM_IS_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_CLIENT))
#define MCM_IS_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_CLIENT))
#define MCM_CLIENT_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_CLIENT, McmClientClass))

typedef struct _McmClientPrivate	McmClientPrivate;
typedef struct _McmClient		McmClient;
typedef struct _McmClientClass		McmClientClass;

struct _McmClient
{
	 GObject			 parent;
	 McmClientPrivate		*priv;
};

struct _McmClientClass
{
	GObjectClass	parent_class;
	void		(* added)				(McmDevice	*device);
	void		(* removed)				(McmDevice	*device);
	void		(* changed)				(McmDevice	*device);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

typedef enum {
	MCM_CLIENT_COLDPLUG_ALL		= 0x00,
	MCM_CLIENT_COLDPLUG_XRANDR	= 0x01,
	MCM_CLIENT_COLDPLUG_CUPS	= 0x02,
	MCM_CLIENT_COLDPLUG_SANE	= 0x04,
	MCM_CLIENT_COLDPLUG_UDEV	= 0x08,
	MCM_CLIENT_COLDPLUG_LAST,
} McmClientColdplug;

GType		 mcm_client_get_type		  		(void);
McmClient	*mcm_client_new					(void);

McmDevice	*mcm_client_get_device_by_id			(McmClient		*client,
								 const gchar		*id);
McmDevice	*mcm_client_get_device_by_window		(McmClient		*client,
								 GdkWindow		*window);
gboolean	 mcm_client_add_virtual_device			(McmClient		*client,
								 McmDevice		*device,
								 GError			**error);
gboolean	 mcm_client_delete_device			(McmClient		*client,
								 McmDevice		*device,
								 GError			**error);
gboolean	 mcm_client_add_connected			(McmClient		*client,
								 McmClientColdplug	 coldplug,
								 GError			**error);
gboolean	 mcm_client_add_saved				(McmClient		*client,
								 GError			**error);
GPtrArray	*mcm_client_get_devices				(McmClient		*client);
void		 mcm_client_set_use_threads			(McmClient		*client,
								 gboolean		 use_threads);
gboolean	 mcm_client_get_loading				(McmClient		*client);

G_END_DECLS

#endif /* __MCM_CLIENT_H */

