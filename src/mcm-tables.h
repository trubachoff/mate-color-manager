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

#ifndef __MCM_TABLES_H
#define __MCM_TABLES_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_TABLES			(mcm_tables_get_type ())
#define MCM_TABLES(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_TABLES, McmTables))
#define MCM_TABLES_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_TABLES, McmTablesClass))
#define MCM_IS_TABLES(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_TABLES))
#define MCM_IS_TABLES_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_TABLES))
#define MCM_TABLES_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_TABLES, McmTablesClass))

typedef struct _McmTablesPrivate	McmTablesPrivate;
typedef struct _McmTables		McmTables;
typedef struct _McmTablesClass		McmTablesClass;

struct _McmTables
{
	 GObject			 parent;
	 McmTablesPrivate		*priv;
};

struct _McmTablesClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_tables_get_type		  	(void);
McmTables	*mcm_tables_new				(void);
gchar		*mcm_tables_get_pnp_id			(McmTables		*tables,
							 const gchar		*pnp_id,
							 GError			**error);

G_END_DECLS

#endif /* __MCM_TABLES_H */

