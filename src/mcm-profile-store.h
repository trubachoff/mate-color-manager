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

#ifndef __MCM_PROFILE_STORE_H
#define __MCM_PROFILE_STORE_H

#include <glib-object.h>

#include "mcm-profile.h"

G_BEGIN_DECLS

#define MCM_TYPE_PROFILE_STORE		(mcm_profile_store_get_type ())
#define MCM_PROFILE_STORE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_PROFILE_STORE, McmProfileStore))
#define MCM_PROFILE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_PROFILE_STORE, McmProfileStoreClass))
#define MCM_IS_PROFILE_STORE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_PROFILE_STORE))
#define MCM_IS_PROFILE_STORE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_PROFILE_STORE))
#define MCM_PROFILE_STORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_PROFILE_STORE, McmProfileStoreClass))

typedef struct _McmProfileStorePrivate	McmProfileStorePrivate;
typedef struct _McmProfileStore		McmProfileStore;
typedef struct _McmProfileStoreClass	McmProfileStoreClass;

struct _McmProfileStore
{
	 GObject			 parent;
	 McmProfileStorePrivate		*priv;
};

struct _McmProfileStoreClass
{
	GObjectClass	parent_class;
	void		(* added)			(McmProfile		*profile);
	void		(* removed)			(McmProfile		*profile);
	void		(* changed)			(void);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_profile_store_get_type		(void);
McmProfileStore	*mcm_profile_store_new			(void);

McmProfile	*mcm_profile_store_get_by_filename	(McmProfileStore	*profile_store,
							 const gchar		*filename);
McmProfile	*mcm_profile_store_get_by_checksum	(McmProfileStore	*profile_store,
							 const gchar		*checksum);
GPtrArray	*mcm_profile_store_get_array		(McmProfileStore	*profile_store);
gboolean	 mcm_profile_store_search_default	(McmProfileStore	*profile_store);
gboolean	 mcm_profile_store_search_by_path	(McmProfileStore	*profile_store,
							 const gchar		*path);

G_END_DECLS

#endif /* __MCM_PROFILE_STORE_H */

