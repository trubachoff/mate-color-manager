/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __MCM_DBUS_H
#define __MCM_DBUS_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define MCM_TYPE_DBUS		(mcm_dbus_get_type ())
#define MCM_DBUS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_DBUS, McmDbus))
#define MCM_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_DBUS, McmDbusClass))
#define PK_IS_DBUS(o)	 	(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_DBUS))
#define PK_IS_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_DBUS))
#define MCM_DBUS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_DBUS, McmDbusClass))
#define MCM_DBUS_ERROR		(mcm_dbus_error_quark ())
#define MCM_DBUS_TYPE_ERROR	(mcm_dbus_error_get_type ())

typedef struct McmDbusPrivate McmDbusPrivate;

typedef struct
{
	 GObject		 parent;
	 McmDbusPrivate	*priv;
} McmDbus;

typedef struct
{
	GObjectClass	parent_class;
} McmDbusClass;

typedef enum
{
	MCM_DBUS_ERROR_FAILED,
	MCM_DBUS_ERROR_INTERNAL_ERROR,
	MCM_DBUS_ERROR_LAST
} McmDbusError;

GQuark		 mcm_dbus_error_quark			(void);
GType		 mcm_dbus_error_get_type		(void);
GType		 mcm_dbus_get_type			(void);
McmDbus		*mcm_dbus_new				(void);

guint		 mcm_dbus_get_idle_time			(McmDbus	*dbus);

/* org.mate.ColorManager */
void		 mcm_dbus_get_profiles_for_device	(McmDbus	*dbus,
							 const gchar	*device_id,
							 const gchar	*options,
							 DBusGMethodInvocation *context);
void		 mcm_dbus_get_profiles_for_type		(McmDbus	*dbus,
							 const gchar	*type,
							 const gchar	*options,
							 DBusGMethodInvocation *context);
void		 mcm_dbus_get_profile_for_window	(McmDbus	*dbus,
							 guint		 xid,
							 DBusGMethodInvocation *context);
void		 mcm_dbus_get_devices			(McmDbus	*dbus,
							 DBusGMethodInvocation *context);

G_END_DECLS

#endif /* __MCM_DBUS_H */
