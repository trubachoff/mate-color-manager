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

#ifndef __MCM_ENUM_H
#define __MCM_ENUM_H

#include <glib/gi18n.h>

typedef enum {
	MCM_INTENT_UNKNOWN,
	MCM_INTENT_PERCEPTUAL,
	MCM_INTENT_RELATIVE_COLORMETRIC,
	MCM_INTENT_SATURATION,
	MCM_INTENT_ABSOLUTE_COLORMETRIC,
	MCM_INTENT_LAST
} McmIntent;

typedef enum {
	MCM_PROFILE_KIND_UNKNOWN,
	MCM_PROFILE_KIND_INPUT_DEVICE,
	MCM_PROFILE_KIND_DISPLAY_DEVICE,
	MCM_PROFILE_KIND_OUTPUT_DEVICE,
	MCM_PROFILE_KIND_DEVICELINK,
	MCM_PROFILE_KIND_COLORSPACE_CONVERSION,
	MCM_PROFILE_KIND_ABSTRACT,
	MCM_PROFILE_KIND_NAMED_COLOR,
	MCM_PROFILE_KIND_LAST
} McmProfileKind;

typedef enum {
	MCM_COLORSPACE_UNKNOWN,
	MCM_COLORSPACE_XYZ,
	MCM_COLORSPACE_LAB,
	MCM_COLORSPACE_LUV,
	MCM_COLORSPACE_YCBCR,
	MCM_COLORSPACE_YXY,
	MCM_COLORSPACE_RGB,
	MCM_COLORSPACE_GRAY,
	MCM_COLORSPACE_HSV,
	MCM_COLORSPACE_CMYK,
	MCM_COLORSPACE_CMY,
	MCM_COLORSPACE_LAST
} McmColorspace;

typedef enum {
	MCM_DEVICE_KIND_UNKNOWN,
	MCM_DEVICE_KIND_DISPLAY,
	MCM_DEVICE_KIND_SCANNER,
	MCM_DEVICE_KIND_PRINTER,
	MCM_DEVICE_KIND_CAMERA,
	MCM_DEVICE_KIND_LAST
} McmDeviceKind;

McmIntent	 mcm_intent_from_string			(const gchar		*intent);
const gchar	*mcm_intent_to_string			(McmIntent		 intent);
const gchar	*mcm_profile_kind_to_string		(McmProfileKind		 profile_kind);
const gchar	*mcm_colorspace_to_string		(McmColorspace		 colorspace);
const gchar	*mcm_colorspace_to_localised_string	(McmColorspace	 	 colorspace);
McmColorspace	 mcm_colorspace_from_string		(const gchar		*colorspace);
McmDeviceKind	 mcm_device_kind_from_string		(const gchar		*kind);
const gchar	*mcm_device_kind_to_string		(McmDeviceKind		 kind);

#endif /* __MCM_ENUM_H */

