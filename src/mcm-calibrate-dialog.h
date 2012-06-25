/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __MCM_CALIBRATE_DIALOG_H
#define __MCM_CALIBRATE_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "mcm-device.h"

G_BEGIN_DECLS

#define MCM_TYPE_CALIBRATE_DIALOG		(mcm_calibrate_dialog_get_type ())
#define MCM_CALIBRATE_DIALOG(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), MCM_TYPE_CALIBRATE_DIALOG, McmCalibrateDialog))
#define MCM_CALIBRATE_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), MCM_TYPE_CALIBRATE_DIALOG, McmCalibrateDialogClass))
#define MCM_IS_CALIBRATE_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), MCM_TYPE_CALIBRATE_DIALOG))
#define MCM_IS_CALIBRATE_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), MCM_TYPE_CALIBRATE_DIALOG))
#define MCM_CALIBRATE_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), MCM_TYPE_CALIBRATE_DIALOG, McmCalibrateDialogClass))

typedef struct _McmCalibrateDialogPrivate	McmCalibrateDialogPrivate;
typedef struct _McmCalibrateDialog		McmCalibrateDialog;
typedef struct _McmCalibrateDialogClass		McmCalibrateDialogClass;

struct _McmCalibrateDialog
{
	 GObject				 parent;
	 McmCalibrateDialogPrivate		*priv;
};

struct _McmCalibrateDialogClass
{
	GObjectClass	parent_class;
	void		(* response)				(McmCalibrateDialog	*calibrate_dialog,
								 GtkResponseType		 response);
};

typedef enum {
	MCM_CALIBRATE_DIALOG_TAB_DISPLAY_TYPE,
	MCM_CALIBRATE_DIALOG_TAB_TARGET_TYPE,
	MCM_CALIBRATE_DIALOG_TAB_MANUAL,
	MCM_CALIBRATE_DIALOG_TAB_GENERIC,
	MCM_CALIBRATE_DIALOG_TAB_PRINT_MODE,
	MCM_CALIBRATE_DIALOG_TAB_PRECISION,
	MCM_CALIBRATE_DIALOG_TAB_LAST
} McmCalibrateDialogTab;

GType			 mcm_calibrate_dialog_get_type		(void);
McmCalibrateDialog	*mcm_calibrate_dialog_new		(void);

void			 mcm_calibrate_dialog_set_window	(McmCalibrateDialog	*calibrate_dialog,
								 GtkWindow		*window);
void			 mcm_calibrate_dialog_show		(McmCalibrateDialog	*calibrate_dialog,
								 McmCalibrateDialogTab	 tab,
								 const gchar		*title,
								 const gchar		*message);
void			 mcm_calibrate_dialog_set_move_window	(McmCalibrateDialog	*calibrate_dialog,
								 gboolean		 move_window);
void			 mcm_calibrate_dialog_set_show_expander	(McmCalibrateDialog	*calibrate_dialog,
								 gboolean		 visible);
void			 mcm_calibrate_dialog_set_show_button_ok (McmCalibrateDialog	*calibrate_dialog,
								 gboolean		 visible);
void			 mcm_calibrate_dialog_set_button_ok_id	(McmCalibrateDialog	*calibrate_dialog,
								 const gchar		*button_id);
void			 mcm_calibrate_dialog_set_image_filename (McmCalibrateDialog	*calibrate_dialog,
								 const gchar		*image_filename);
void			 mcm_calibrate_dialog_pop		(McmCalibrateDialog	*calibrate_dialog);
void			 mcm_calibrate_dialog_hide		(McmCalibrateDialog	*calibrate_dialog);
GtkResponseType		 mcm_calibrate_dialog_run		(McmCalibrateDialog	*calibrate_dialog);
GtkWindow		*mcm_calibrate_dialog_get_window	(McmCalibrateDialog	*calibrate_dialog);
void			 mcm_calibrate_dialog_pack_details	(McmCalibrateDialog	*calibrate_dialog,
								 GtkWidget		*widget);

G_END_DECLS

#endif /* __MCM_CALIBRATE_DIALOG_H */

