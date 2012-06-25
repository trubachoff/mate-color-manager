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

#ifndef __MCM_BRIGHTNESS_H
#define __MCM_BRIGHTNESS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MCM_TYPE_BRIGHTNESS		(mcm_brightness_get_type ())
#define MCM_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_BRIGHTNESS, McmBrightness))
#define MCM_BRIGHTNESS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_BRIGHTNESS, McmBrightnessClass))
#define MCM_IS_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_BRIGHTNESS))
#define MCM_IS_BRIGHTNESS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_BRIGHTNESS))
#define MCM_BRIGHTNESS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_BRIGHTNESS, McmBrightnessClass))

typedef struct _McmBrightnessPrivate	McmBrightnessPrivate;
typedef struct _McmBrightness		McmBrightness;
typedef struct _McmBrightnessClass	McmBrightnessClass;

struct _McmBrightness
{
	 GObject		 parent;
	 McmBrightnessPrivate	*priv;
};

struct _McmBrightnessClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

GType		 mcm_brightness_get_type		(void);
McmBrightness	*mcm_brightness_new			(void);
gboolean	 mcm_brightness_set_percentage		(McmBrightness		*brightness,
							 guint			 percentage,
							 GError			**error);
gboolean	 mcm_brightness_get_percentage		(McmBrightness		*brightness,
							 guint			*percentage,
							 GError			**error);

G_END_DECLS

#endif /* __MCM_BRIGHTNESS_H */

