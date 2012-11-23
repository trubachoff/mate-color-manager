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

#ifndef __MCM_CALIBRATE_H
#define __MCM_CALIBRATE_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "mcm-device.h"

G_BEGIN_DECLS

#define MCM_TYPE_CALIBRATE		(mcm_calibrate_get_type ())
#define MCM_CALIBRATE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_CALIBRATE, McmCalibrate))
#define MCM_CALIBRATE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_CALIBRATE, McmCalibrateClass))
#define MCM_IS_CALIBRATE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_CALIBRATE))
#define MCM_IS_CALIBRATE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_CALIBRATE))
#define MCM_CALIBRATE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_CALIBRATE, McmCalibrateClass))

typedef struct _McmCalibratePrivate	McmCalibratePrivate;
typedef struct _McmCalibrate		McmCalibrate;
typedef struct _McmCalibrateClass	McmCalibrateClass;

struct _McmCalibrate
{
	 GObject			 parent;
	 McmCalibratePrivate		*priv;
};

struct _McmCalibrateClass
{
	GObjectClass	parent_class;
	/* vtable */
	gboolean	 (*calibrate_display)		(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*calibrate_device)		(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*calibrate_printer)		(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*calibrate_spotread)		(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
	/* padding for future expansion */
	void (*_mcm_reserved1) (void);
	void (*_mcm_reserved2) (void);
	void (*_mcm_reserved3) (void);
	void (*_mcm_reserved4) (void);
	void (*_mcm_reserved5) (void);
};

typedef enum
{
	MCM_CALIBRATE_ERROR_USER_ABORT,
	MCM_CALIBRATE_ERROR_NO_SUPPORT,
	MCM_CALIBRATE_ERROR_NO_DATA,
	MCM_CALIBRATE_ERROR_INTERNAL
} McmCalibrateError;

/* dummy */
#define MCM_CALIBRATE_ERROR	1

typedef enum {
	MCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3,
	MCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003,
	MCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER,
	MCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC,
	MCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG,
	MCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR,
	MCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4,
	MCM_CALIBRATE_REFERENCE_KIND_IT8,
	MCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO,
	MCM_CALIBRATE_REFERENCE_KIND_QPCARD_201,
	MCM_CALIBRATE_REFERENCE_KIND_UNKNOWN
} McmCalibrateReferenceKind;

typedef enum {
	MCM_CALIBRATE_DEVICE_KIND_CRT,
	MCM_CALIBRATE_DEVICE_KIND_LCD,
	MCM_CALIBRATE_DEVICE_KIND_PROJECTOR,
	MCM_CALIBRATE_DEVICE_KIND_UNKNOWN
} McmCalibrateDeviceKind;

typedef enum {
	MCM_CALIBRATE_PRINT_KIND_LOCAL,
	MCM_CALIBRATE_PRINT_KIND_GENERATE,
	MCM_CALIBRATE_PRINT_KIND_ANALYZE,
	MCM_CALIBRATE_PRINT_KIND_UNKNOWN
} McmCalibratePrintKind;

typedef enum {
	MCM_CALIBRATE_PRECISION_SHORT,
	MCM_CALIBRATE_PRECISION_NORMAL,
	MCM_CALIBRATE_PRECISION_LONG,
	MCM_CALIBRATE_PRECISION_UNKNOWN
} McmCalibratePrecision;

GType		 mcm_calibrate_get_type			(void);
McmCalibrate	*mcm_calibrate_new			(void);
gboolean	 mcm_calibrate_display			(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
gboolean	 mcm_calibrate_device			(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
gboolean	 mcm_calibrate_printer			(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
gboolean	 mcm_calibrate_spotread			(McmCalibrate	*calibrate,
							 GtkWindow	*window,
							 GError		**error);
gboolean	 mcm_calibrate_set_from_device		(McmCalibrate	*calibrate,
							 McmDevice	*device,
							 GError		**error);
gboolean	 mcm_calibrate_set_from_exif		(McmCalibrate	*calibrate,
							 const gchar	*filename,
							 GError		**error);
const gchar	*mcm_calibrate_get_model_fallback	(McmCalibrate	*calibrate);
const gchar	*mcm_calibrate_get_description_fallback	(McmCalibrate	*calibrate);
const gchar	*mcm_calibrate_get_manufacturer_fallback (McmCalibrate	*calibrate);
const gchar	*mcm_calibrate_get_device_fallback	(McmCalibrate	*calibrate);
const gchar	*mcm_calibrate_get_filename_result	(McmCalibrate	*calibrate);
const gchar	*mcm_calibrate_get_working_path		(McmCalibrate	*calibrate);

G_END_DECLS

#endif /* __MCM_CALIBRATE_H */

