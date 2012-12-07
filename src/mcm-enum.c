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

#include "config.h"

#include <glib/gi18n.h>

#include "mcm-enum.h"

/**
 * mcm_intent_to_string:
 **/
const gchar *
mcm_intent_to_string (McmIntent intent)
{
	if (intent == MCM_INTENT_PERCEPTUAL)
		return "perceptual";
	if (intent == MCM_INTENT_RELATIVE_COLORMETRIC)
		return "relative-colormetric";
	if (intent == MCM_INTENT_SATURATION)
		return "saturation";
	if (intent == MCM_INTENT_ABSOLUTE_COLORMETRIC)
		return "absolute-colormetric";
	return "unknown";
}

/**
 * mcm_intent_from_string:
 **/
McmIntent
mcm_intent_from_string (const gchar *intent)
{
	if (g_strcmp0 (intent, "perceptual") == 0)
		return MCM_INTENT_PERCEPTUAL;
	if (g_strcmp0 (intent, "relative-colormetric") == 0)
		return MCM_INTENT_RELATIVE_COLORMETRIC;
	if (g_strcmp0 (intent, "saturation") == 0)
		return MCM_INTENT_SATURATION;
	if (g_strcmp0 (intent, "absolute-colormetric") == 0)
		return MCM_INTENT_ABSOLUTE_COLORMETRIC;
	return MCM_INTENT_UNKNOWN;
}

/**
 * mcm_profile_kind_to_string:
 **/
const gchar *
mcm_profile_kind_to_string (McmProfileKind kind)
{
	if (kind == MCM_PROFILE_KIND_INPUT_DEVICE)
		return "input-device";
	if (kind == MCM_PROFILE_KIND_DISPLAY_DEVICE)
		return "display-device";
	if (kind == MCM_PROFILE_KIND_OUTPUT_DEVICE)
		return "output-device";
	if (kind == MCM_PROFILE_KIND_DEVICELINK)
		return "devicelink";
	if (kind == MCM_PROFILE_KIND_COLORSPACE_CONVERSION)
		return "colorspace-conversion";
	if (kind == MCM_PROFILE_KIND_ABSTRACT)
		return "abstract";
	if (kind == MCM_PROFILE_KIND_NAMED_COLOR)
		return "named-color";
	return "unknown";
}

/**
 * mcm_colorspace_to_string:
 **/
const gchar *
mcm_colorspace_to_string (McmColorspace colorspace)
{
	if (colorspace == MCM_COLORSPACE_XYZ)
		return "xyz";
	if (colorspace == MCM_COLORSPACE_LAB)
		return "lab";
	if (colorspace == MCM_COLORSPACE_LUV)
		return "luv";
	if (colorspace == MCM_COLORSPACE_YCBCR)
		return "ycbcr";
	if (colorspace == MCM_COLORSPACE_YXY)
		return "yxy";
	if (colorspace == MCM_COLORSPACE_RGB)
		return "rgb";
	if (colorspace == MCM_COLORSPACE_GRAY)
		return "gray";
	if (colorspace == MCM_COLORSPACE_HSV)
		return "hsv";
	if (colorspace == MCM_COLORSPACE_CMYK)
		return "cmyk";
	if (colorspace == MCM_COLORSPACE_CMY)
		return "cmy";
	return "unknown";
}

/**
 * mcm_colorspace_from_string:
 **/
McmColorspace
mcm_colorspace_from_string (const gchar *colorspace)
{
	if (g_strcmp0 (colorspace, "xyz") == 0)
		return MCM_COLORSPACE_XYZ;
	if (g_strcmp0 (colorspace, "lab") == 0)
		return MCM_COLORSPACE_LAB;
	if (g_strcmp0 (colorspace, "luv") == 0)
		return MCM_COLORSPACE_LUV;
	if (g_strcmp0 (colorspace, "ycbcr") == 0)
		return MCM_COLORSPACE_YCBCR;
	if (g_strcmp0 (colorspace, "yxy") == 0)
		return MCM_COLORSPACE_YXY;
	if (g_strcmp0 (colorspace, "rgb") == 0)
		return MCM_COLORSPACE_RGB;
	if (g_strcmp0 (colorspace, "gray") == 0)
		return MCM_COLORSPACE_GRAY;
	if (g_strcmp0 (colorspace, "hsv") == 0)
		return MCM_COLORSPACE_HSV;
	if (g_strcmp0 (colorspace, "cmyk") == 0)
		return MCM_COLORSPACE_CMYK;
	if (g_strcmp0 (colorspace, "cmy") == 0)
		return MCM_COLORSPACE_CMY;
	return MCM_COLORSPACE_UNKNOWN;
}

