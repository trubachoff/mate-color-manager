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

#ifndef __MCM_PROFILE_H
#define __MCM_PROFILE_H

#include <glib-object.h>
#include <gio/gio.h>

#include "mcm-clut.h"
#include "mcm-enum.h"

G_BEGIN_DECLS

#define MCM_TYPE_PROFILE		(mcm_profile_get_type ())
#define MCM_PROFILE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_PROFILE, McmProfile))
#define MCM_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_PROFILE, McmProfileClass))
#define MCM_IS_PROFILE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_PROFILE))
#define MCM_IS_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_PROFILE))
#define MCM_PROFILE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_PROFILE, McmProfileClass))

typedef struct _McmProfilePrivate	McmProfilePrivate;
typedef struct _McmProfile		McmProfile;
typedef struct _McmProfileClass		McmProfileClass;

struct _McmProfile
{
	 GObject		 parent;
	 McmProfilePrivate	*priv;
};

struct _McmProfileClass
{
	GObjectClass	 parent_class;
	gboolean	 (*parse_data)		(McmProfile	*profile,
						 const guint8	*data,
						 gsize		 length,
						 GError		**error);
	gboolean	 (*save)		(McmProfile	*profile,
						 const gchar	*filename,
						 GError		**error);
	McmClut		*(*generate_vcgt)	(McmProfile	*profile,
						 guint		 size);
	McmClut		*(*generate_curve)	(McmProfile	*profile,
						 guint		 size);

	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_profile_get_type		  	(void);
McmProfile	*mcm_profile_new			(void);
McmProfile	*mcm_profile_default_new		(void);
gboolean	 mcm_profile_parse			(McmProfile	*profile,
							 GFile		*file,
							 GError		**error);
gboolean	 mcm_profile_parse_data			(McmProfile	*profile,
							 const guint8	*data,
							 gsize		 length,
							 GError		**error);
gboolean	 mcm_profile_save			(McmProfile	*profile,
							 const gchar	*filename,
							 GError		**error);
const gchar	*mcm_profile_get_checksum		(McmProfile	*profile);
gboolean	 mcm_profile_get_can_delete		(McmProfile	*profile);
McmClut		*mcm_profile_generate_vcgt		(McmProfile	*profile,
							 guint		 size);
McmClut		*mcm_profile_generate_curve		(McmProfile	*profile,
							 guint		 size);
const gchar	*mcm_profile_get_description		(McmProfile	*profile);
void		 mcm_profile_set_description		(McmProfile	*profile,
							 const gchar 	*description);
const gchar	*mcm_profile_get_filename		(McmProfile	*profile);
void		 mcm_profile_set_filename		(McmProfile	*profile,
							 const gchar 	*filename);
const gchar	*mcm_profile_get_copyright		(McmProfile	*profile);
void		 mcm_profile_set_copyright		(McmProfile	*profile,
							 const gchar 	*copyright);
const gchar	*mcm_profile_get_manufacturer		(McmProfile	*profile);
void		 mcm_profile_set_manufacturer		(McmProfile	*profile,
							 const gchar 	*manufacturer);
const gchar	*mcm_profile_get_model			(McmProfile	*profile);
void		 mcm_profile_set_model			(McmProfile	*profile,
							 const gchar 	*model);
const gchar	*mcm_profile_get_datetime		(McmProfile	*profile);
void		 mcm_profile_set_datetime		(McmProfile	*profile,
							 const gchar 	*datetime);
guint		 mcm_profile_get_size			(McmProfile	*profile);
void		 mcm_profile_set_size			(McmProfile	*profile,
							 guint		 size);
McmProfileKind	 mcm_profile_get_kind			(McmProfile	*profile);
void		 mcm_profile_set_kind			(McmProfile	*profile,
							 McmProfileKind	 kind);
McmColorspace	 mcm_profile_get_colorspace		(McmProfile	*profile);
void		 mcm_profile_set_colorspace		(McmProfile	*profile,
							 McmColorspace	 colorspace);
gboolean	 mcm_profile_get_has_vcgt		(McmProfile	*profile);
void		 mcm_profile_set_has_vcgt		(McmProfile	*profile,
							 gboolean	 has_vcgt);
gboolean	 mcm_profile_has_colorspace_description	(McmProfile	*profile);

G_END_DECLS

#endif /* __MCM_PROFILE_H */

