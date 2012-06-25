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

#ifndef __MCM_UTILS_H
#define __MCM_UTILS_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "mcm-device.h"
#include "mcm-profile.h"
#include "mcm-enum.h"

#define MCM_STOCK_ICON				"mate-color-manager"
#define MCM_PROFILE_PATH			"/.color/icc"

#define MCM_SETTINGS_DIR			"/apps/mate-color-manager"
#define MCM_SETTINGS_DEFAULT_GAMMA		"/apps/mate-color-manager/default_gamma"
#define MCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION	"/apps/mate-color-manager/global_display_correction"
#define MCM_SETTINGS_SET_ICC_PROFILE_ATOM	"/apps/mate-color-manager/set_icc_profile_atom"
#define MCM_SETTINGS_RENDERING_INTENT_DISPLAY	"/apps/mate-color-manager/rendering_intent_display"
#define MCM_SETTINGS_RENDERING_INTENT_SOFTPROOF	"/apps/mate-color-manager/rendering_intent_softproof"
#define MCM_SETTINGS_COLORSPACE_RGB		"/apps/mate-color-manager/colorspace_rgb"
#define MCM_SETTINGS_COLORSPACE_CMYK		"/apps/mate-color-manager/colorspace_cmyk"
#define MCM_SETTINGS_USE_PROFILES_FROM_VOLUMES	"/apps/mate-color-manager/use_profiles_from_volumes"
#define MCM_SETTINGS_CALIBRATION_LENGTH		"/apps/mate-color-manager/calibration_length"
#define MCM_SETTINGS_SHOW_FINE_TUNING		"/apps/mate-color-manager/show_fine_tuning"

#define MCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD	"/apps/mate-color-manager/recalibrate_printer_threshold"
#define MCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD	"/apps/mate-color-manager/recalibrate_display_threshold"

/* DISTROS: you will have to patch if you have changed the name of these packages */
#define MCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS	"shared-color-targets"
#define MCM_PREFS_PACKAGE_NAME_ARGYLLCMS		"argyllcms"

gboolean	 mcm_utils_mkdir_for_filename		(const gchar		*filename,
							 GError			**error);
gboolean	 mcm_utils_mkdir_with_parents		(const gchar		*filename,
							 GError			**error);
gboolean	 mcm_utils_mkdir_and_copy		(GFile			*source,
							 GFile			*destination,
							 GError			**error);
GFile		*mcm_utils_get_profile_destination	(GFile			*file);
gchar		**mcm_utils_ptr_array_to_strv		(GPtrArray		*array);
gboolean	 mcm_mate_help				(const gchar		*link_id);
gboolean	 mcm_utils_output_is_lcd_internal	(const gchar		*output_name);
gboolean	 mcm_utils_output_is_lcd		(const gchar		*output_name);
void		 mcm_utils_alphanum_lcase		(gchar			*string);
void		 mcm_utils_ensure_sensible_filename	(gchar			*string);
gchar		*mcm_utils_get_default_config_location	(void);
McmProfileKind	 mcm_utils_device_kind_to_profile_kind	(McmDeviceKind		 kind);
gchar		*mcm_utils_format_date_time		(const struct tm	*created);
gboolean	 mcm_utils_install_package		(const gchar		*package_name,
							 GtkWindow		*window);
void		 mcm_utils_ensure_printable		(gchar			*text);
gboolean	 mcm_utils_is_icc_profile		(GFile			*file);
gchar		*mcm_utils_linkify			(const gchar		*text);
const gchar	*mcm_intent_to_localized_text	(McmIntent	 intent);

#endif /* __MCM_UTILS_H */

