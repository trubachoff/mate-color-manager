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
#include <gtk/gtk.h>
#include <math.h>
#include <unique/unique.h>
#include <glib/gstdio.h>
#include <gudev/gudev.h>
#include <libmateui/mate-rr.h>
#include <locale.h>
#include <canberra-gtk.h>

#include "egg-debug.h"

#include "mcm-calibrate-argyll.h"
#include "mcm-cie-widget.h"
#include "mcm-client.h"
#include "mcm-colorimeter.h"
#include "mcm-device-xrandr.h"
#include "mcm-device-virtual.h"
#include "mcm-profile.h"
#include "mcm-profile-store.h"
#include "mcm-trc-widget.h"
#include "mcm-utils.h"
#include "mcm-xyz.h"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_devices = NULL;
static GtkListStore *list_store_profiles = NULL;
static McmDevice *current_device = NULL;
static McmProfileStore *profile_store = NULL;
static McmClient *mcm_client = NULL;
static McmColorimeter *colorimeter = NULL;
static gboolean setting_up_device = FALSE;
static GtkWidget *info_bar_loading = NULL;
static GtkWidget *info_bar_vcgt = NULL;
static GtkWidget *cie_widget = NULL;
static GtkWidget *trc_widget = NULL;
static GSettings *settings = NULL;

enum {
	MCM_DEVICES_COLUMN_ID,
	MCM_DEVICES_COLUMN_SORT,
	MCM_DEVICES_COLUMN_ICON,
	MCM_DEVICES_COLUMN_TITLE,
	MCM_DEVICES_COLUMN_LAST
};

enum {
	MCM_PROFILES_COLUMN_ID,
	MCM_PROFILES_COLUMN_SORT,
	MCM_PROFILES_COLUMN_ICON,
	MCM_PROFILES_COLUMN_TITLE,
	MCM_PROFILES_COLUMN_PROFILE,
	MCM_PROFILES_COLUMN_LAST
};

enum {
	MCM_PREFS_COMBO_COLUMN_TEXT,
	MCM_PREFS_COMBO_COLUMN_PROFILE,
	MCM_PREFS_COMBO_COLUMN_TYPE,
	MCM_PREFS_COMBO_COLUMN_SORTABLE,
	MCM_PREFS_COMBO_COLUMN_LAST
};

typedef enum {
	MCM_PREFS_ENTRY_TYPE_PROFILE,
	MCM_PREFS_ENTRY_TYPE_NONE,
	MCM_PREFS_ENTRY_TYPE_IMPORT,
	MCM_PREFS_ENTRY_TYPE_LAST
} McmPrefsEntryType;

static void mcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata);

/**
 * mcm_prefs_error_dialog:
 **/
static void
mcm_prefs_error_dialog (const gchar *title, const gchar *message)
{
	GtkWindow *window;
	GtkWidget *dialog;

	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", title);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * mcm_prefs_close_cb:
 **/
static void
mcm_prefs_close_cb (GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
}

/**
 * mcm_prefs_set_default:
 **/
static gboolean
mcm_prefs_set_default (McmDevice *device)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	gchar *cmdline = NULL;
	const gchar *filename;
	const gchar *id;
	gchar *install_cmd = NULL;

	/* nothing set */
	id = mcm_device_get_id (device);
	filename = mcm_device_get_profile_filename (device);
	if (filename == NULL) {
		egg_debug ("no filename for %s", id);
		goto out;
	}

	/* run using PolicyKit */
	install_cmd = g_build_filename (SBINDIR, "mcm-install-system-wide", NULL);
	cmdline = g_strdup_printf ("pkexec %s --id %s \"%s\"", install_cmd, id, filename);
	egg_debug ("running: %s", cmdline);
	ret = g_spawn_command_line_sync (cmdline, NULL, NULL, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: could not save for all users */
		mcm_prefs_error_dialog (_("Failed to save defaults for all users"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (install_cmd);
	g_free (cmdline);
	return ret;
}

/**
 * mcm_prefs_combobox_add_profile:
 **/
static void
mcm_prefs_combobox_add_profile (GtkWidget *widget, McmProfile *profile, McmPrefsEntryType entry_type, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_tmp;
	const gchar *description;
	gchar *sortable;

	/* iter is optional */
	if (iter == NULL)
		iter = &iter_tmp;

	/* use description */
	if (entry_type == MCM_PREFS_ENTRY_TYPE_NONE) {
		/* TRANSLATORS: this is where no profile is selected */
		description = _("None");
		sortable = g_strdup ("1");
	} else if (entry_type == MCM_PREFS_ENTRY_TYPE_IMPORT) {
		/* TRANSLATORS: this is where the user can click and import a profile */
		description = _("Other profile…");
		sortable = g_strdup ("9");
	} else {
		description = mcm_profile_get_description (profile);
		sortable = g_strdup_printf ("5%s", description);
	}

	/* also add profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_list_store_append (GTK_LIST_STORE(model), iter);
	gtk_list_store_set (GTK_LIST_STORE(model), iter,
			    MCM_PREFS_COMBO_COLUMN_TEXT, description,
			    MCM_PREFS_COMBO_COLUMN_PROFILE, profile,
			    MCM_PREFS_COMBO_COLUMN_TYPE, entry_type,
			    MCM_PREFS_COMBO_COLUMN_SORTABLE, sortable,
			    -1);
	g_free (sortable);
}

/**
 * mcm_prefs_default_cb:
 **/
static void
mcm_prefs_default_cb (GtkWidget *widget, gpointer data)
{
	GPtrArray *array = NULL;
	McmDevice *device;
	McmDeviceKind kind;
	gboolean ret;
	guint i;

	/* set for each output */
	array = mcm_client_get_devices (mcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* not a xrandr panel */
		kind = mcm_device_get_kind (device);
		if (kind != MCM_DEVICE_KIND_DISPLAY)
			continue;

		/* set for this device */
		ret = mcm_prefs_set_default (device);
		if (!ret)
			break;
	}
	g_ptr_array_unref (array);
}

/**
 * mcm_prefs_help_cb:
 **/
static void
mcm_prefs_help_cb (GtkWidget *widget, gpointer data)
{
	mcm_mate_help ("preferences");
}

/**
 * mcm_prefs_delete_event_cb:
 **/
static gboolean
mcm_prefs_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	mcm_prefs_close_cb (widget, data);
	return FALSE;
}

/**
 * mcm_prefs_calibrate_display:
 **/
static gboolean
mcm_prefs_calibrate_display (McmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	gboolean ret_tmp;
	GError *error = NULL;
	GtkWindow *window;

	/* no device */
	if (current_device == NULL)
		goto out;

	/* set properties from the device */
	ret = mcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* run each task in order */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = mcm_calibrate_display (calibrate, window, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	/* need to set the gamma back to the default after calibration */
	error = NULL;
	ret_tmp = mcm_device_apply (current_device, &error);
	if (!ret_tmp) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * mcm_prefs_calibrate_device:
 **/
static gboolean
mcm_prefs_calibrate_device (McmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* set defaults from device */
	ret = mcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = mcm_calibrate_device (calibrate, window, &error);
	if (!ret) {
		if (error->code != MCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			mcm_prefs_error_dialog (_("Failed to calibrate device"), error->message);
		} else {
			egg_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * mcm_prefs_calibrate_printer:
 **/
static gboolean
mcm_prefs_calibrate_printer (McmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* set defaults from device */
	ret = mcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = mcm_calibrate_printer (calibrate, window, &error);
	if (!ret) {
		if (error->code != MCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			mcm_prefs_error_dialog (_("Failed to calibrate printer"), error->message);
		} else {
			egg_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * mcm_prefs_profile_kind_to_icon_name:
 **/
static const gchar *
mcm_prefs_profile_kind_to_icon_name (McmProfileKind kind)
{
	if (kind == MCM_PROFILE_KIND_DISPLAY_DEVICE)
		return "video-display";
	if (kind == MCM_PROFILE_KIND_INPUT_DEVICE)
		return "scanner";
	if (kind == MCM_PROFILE_KIND_OUTPUT_DEVICE)
		return "printer";
	if (kind == MCM_PROFILE_KIND_COLORSPACE_CONVERSION)
		return "view-refresh";
	return "image-missing";
}

/**
 * mcm_prefs_profile_get_sort_string:
 **/
static const gchar *
mcm_prefs_profile_get_sort_string (McmProfileKind kind)
{
	if (kind == MCM_PROFILE_KIND_DISPLAY_DEVICE)
		return "1";
	if (kind == MCM_PROFILE_KIND_INPUT_DEVICE)
		return "2";
	if (kind == MCM_PROFILE_KIND_OUTPUT_DEVICE)
		return "3";
	return "4";
}

/**
 * mcm_prefs_update_profile_list:
 **/
static void
mcm_prefs_update_profile_list (void)
{
	GtkTreeIter iter;
	const gchar *description;
	const gchar *icon_name;
	McmProfileKind profile_kind = MCM_PROFILE_KIND_UNKNOWN;
	McmProfile *profile;
	guint i;
	const gchar *filename = NULL;
	gchar *sort = NULL;
	GPtrArray *profile_array = NULL;

	egg_debug ("updating profile list");

	/* get new list */
	profile_array = mcm_profile_store_get_array (profile_store);

	/* clear existing list */
	gtk_list_store_clear (list_store_profiles);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		profile_kind = mcm_profile_get_kind (profile);
		icon_name = mcm_prefs_profile_kind_to_icon_name (profile_kind);
		gtk_list_store_append (list_store_profiles, &iter);
		description = mcm_profile_get_description (profile);
		sort = g_strdup_printf ("%s%s",
					mcm_prefs_profile_get_sort_string (profile_kind),
					description);
		filename = mcm_profile_get_filename (profile);
		egg_debug ("add %s to profiles list", filename);
		gtk_list_store_set (list_store_profiles, &iter,
				    MCM_PROFILES_COLUMN_ID, filename,
				    MCM_PROFILES_COLUMN_SORT, sort,
				    MCM_PROFILES_COLUMN_TITLE, description,
				    MCM_PROFILES_COLUMN_ICON, icon_name,
				    MCM_PROFILES_COLUMN_PROFILE, profile,
				    -1);

		g_free (sort);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * mcm_prefs_profile_delete_cb:
 **/
static void
mcm_prefs_profile_delete_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GtkResponseType response;
	GtkWindow *window;
	gint retval;
	const gchar *filename;
	McmProfile *profile;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Permanently delete profile?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  /* TRANSLATORS: dialog message */
						  _("Are you sure you want to remove this profile from your system permanently?"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	/* TRANSLATORS: button, delete a profile */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_YES);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    MCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* try to remove file */
	filename = mcm_profile_get_filename (profile);
	retval = g_unlink (filename);
	if (retval != 0)
		goto out;
out:
	return;
}

/**
 * mcm_prefs_file_chooser_get_icc_profile:
 **/
static GFile *
mcm_prefs_file_chooser_get_icc_profile (void)
{
	GtkWindow *window;
	GtkWidget *dialog;
	GFile *file = NULL;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Import"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.icc");
	gtk_file_filter_add_pattern (filter, "*.icm");
	gtk_file_filter_add_pattern (filter, "*.ICC");
	gtk_file_filter_add_pattern (filter, "*.ICM");

	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return file;
}

/**
 * mcm_prefs_profile_import_file:
 **/
static gboolean
mcm_prefs_profile_import_file (GFile *file)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GFile *destination = NULL;

	/* copy icc file to ~/.color/icc */
	destination = mcm_utils_get_profile_destination (file);
	ret = mcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		mcm_prefs_error_dialog (_("Failed to copy file"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_object_unref (destination);
	return ret;
}

/**
 * mcm_prefs_profile_import_cb:
 **/
static void
mcm_prefs_profile_import_cb (GtkWidget *widget, gpointer data)
{
	GFile *file;

	/* get new file */
	file = mcm_prefs_file_chooser_get_icc_profile ();
	if (file == NULL) {
		egg_warning ("failed to get filename");
		goto out;
	}

	/* import this */
	mcm_prefs_profile_import_file (file);
out:
	if (file != NULL)
		g_object_unref (file);
}

/**
 * mcm_prefs_drag_data_received_cb:
 **/
static void
mcm_prefs_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, gpointer user_data)
{
	const guchar *filename;
	gchar **filenames = NULL;
	GFile *file = NULL;
	guint i;
	gboolean ret;

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, _time);
		goto out;
	}

	/* import this */
	egg_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* check this is an ICC profile */
		egg_debug ("trying to import %s", filenames[i]);
		file = g_file_new_for_uri (filenames[i]);
		ret = mcm_utils_is_icc_profile (file);
		if (!ret) {
			egg_debug ("%s is not a ICC profile", filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}

		/* try to import it */
		ret = mcm_prefs_profile_import_file (file);
		if (!ret) {
			egg_debug ("%s did not import correctly", filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}
		g_object_unref (file);
		file = NULL;
	}

	gtk_drag_finish (context, TRUE, FALSE, _time);
out:
	if (file != NULL)
		g_object_unref (file);
	g_strfreev (filenames);
}

/**
 * mcm_prefs_ensure_argyllcms_installed:
 **/
static gboolean
mcm_prefs_ensure_argyllcms_installed (void)
{
	gboolean ret;
	GtkWindow *window;
	GtkWidget *dialog;
	GtkResponseType response;
	GString *string = NULL;

	/* find whether argyllcms is installed using a tool which should exist */
	ret = g_file_test ("/usr/bin/dispcal", G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

#ifndef MCM_USE_PACKAGEKIT
	egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
	goto out;
#endif

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Install missing calibration and profiling software?"));

	string = g_string_new ("");
	/* TRANSLATORS: dialog message saying the argyllcms is not installed */
	g_string_append_printf (string, "%s\n", _("Calibration and profiling software is not installed on this computer."));
	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s\n\n", _("These tools are required to build color profiles for devices."));
	/* TRANSLATORS: dialog message, asking if it's okay to install it */
	g_string_append_printf (string, "%s", _("Do you want them to be automatically installed?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	/* TRANSLATORS: button, install a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
	/* TRANSLATORS: button, skip installing a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* only install if the user wanted to */
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* do the install */
	ret = mcm_utils_install_package (MCM_PREFS_PACKAGE_NAME_ARGYLLCMS, window);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * mcm_prefs_calibrate_cb:
 **/
static void
mcm_prefs_calibrate_cb (GtkWidget *widget, gpointer data)
{
	McmCalibrate *calibrate = NULL;
	McmDeviceKind kind;
	gboolean ret;
	GError *error = NULL;
	const gchar *filename;
	guint i;
	const gchar *name;
	McmProfile *profile;
	GPtrArray *profile_array = NULL;
	GFile *file = NULL;
	GFile *dest = NULL;
	gchar *destination = NULL;

	/* ensure argyllcms is installed */
	ret = mcm_prefs_ensure_argyllcms_installed ();
	if (!ret)
		goto out;

	/* create new calibration object */
	calibrate = MCM_CALIBRATE(mcm_calibrate_argyll_new ());

	/* choose the correct kind of calibration */
	kind = mcm_device_get_kind (current_device);
	switch (kind) {
	case MCM_DEVICE_KIND_DISPLAY:
		ret = mcm_prefs_calibrate_display (calibrate);
		break;
	case MCM_DEVICE_KIND_SCANNER:
	case MCM_DEVICE_KIND_CAMERA:
		ret = mcm_prefs_calibrate_device (calibrate);
		break;
	case MCM_DEVICE_KIND_PRINTER:
		ret = mcm_prefs_calibrate_printer (calibrate);
		break;
	default:
		egg_warning ("calibration and/or profiling not supported for this device");
		goto out;
	}

	/* we failed to calibrate */
	if (!ret)
		goto out;

	/* failed to get profile */
	filename = mcm_calibrate_get_filename_result (calibrate);
	if (filename == NULL) {
		egg_warning ("failed to get filename from calibration");
		goto out;
	}

	/* copy the ICC file to the proper location */
	file = g_file_new_for_path (filename);
	dest = mcm_utils_get_profile_destination (file);
	ret = mcm_utils_mkdir_and_copy (file, dest, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get new list */
	profile_array = mcm_profile_store_get_array (profile_store);

	/* find an existing profile of this name */
	destination = g_file_get_path (dest);
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		name = mcm_profile_get_filename (profile);
		if (g_strcmp0 (name, destination) == 0) {
			egg_debug ("found existing profile: %s", destination);
			break;
		}
	}

	/* we didn't find an existing profile */
	if (i == profile_array->len) {
		egg_debug ("adding: %s", destination);

		/* set this default */
		mcm_device_set_profile_filename (current_device, destination);
		ret = mcm_device_save (current_device, &error);
		if (!ret) {
			egg_warning ("failed to save default: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* remove temporary file */
	g_unlink (filename);

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "complete",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("MATE Color Manager"),
			 /* TRANSLATORS: this is the sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Profiling completed"), NULL);
out:
	g_free (destination);
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	if (file != NULL)
		g_object_unref (file);
	if (dest != NULL)
		g_object_unref (dest);
}

/**
 * mcm_prefs_device_add_cb:
 **/
static void
mcm_prefs_device_add_cb (GtkWidget *widget, gpointer data)
{
	/* show ui */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_virtual"));
	gtk_widget_show (widget);

	/* clear entries */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_model"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_manufacturer"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
}

/**
 * mcm_prefs_button_virtual_add_cb:
 **/
static void
mcm_prefs_button_virtual_add_cb (GtkWidget *widget, gpointer data)
{
	McmDeviceKind device_kind;
	McmDevice *device;
	const gchar *model;
	const gchar *manufacturer;
	gboolean ret;
	GError *error = NULL;

	/* get device details */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_virtual_type"));
	device_kind = gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) + 2;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* create device */
	device = mcm_device_virtual_new	();
	ret = mcm_device_virtual_create_from_params (MCM_DEVICE_VIRTUAL (device), device_kind, model, manufacturer, MCM_COLORSPACE_RGB);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		mcm_prefs_error_dialog (_("Failed to create virtual device"), NULL);
		goto out;
	}

	/* save what we've got */
	ret = mcm_device_save (device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		mcm_prefs_error_dialog (_("Failed to save virtual device"), error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the device list */
	ret = mcm_client_add_virtual_device (mcm_client, device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		mcm_prefs_error_dialog (_("Failed to add virtual device"), error->message);
		g_error_free (error);
		goto out;
	}

out:
	/* we're done */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_virtual"));
	gtk_widget_hide (widget);
}

/**
 * mcm_prefs_button_virtual_cancel_cb:
 **/
static void
mcm_prefs_button_virtual_cancel_cb (GtkWidget *widget, gpointer data)
{
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_virtual"));
	gtk_widget_hide (widget);
}

/**
 * mcm_prefs_virtual_delete_event_cb:
 **/
static gboolean
mcm_prefs_virtual_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	mcm_prefs_button_virtual_cancel_cb (widget, data);
	return TRUE;
}

/**
 * mcm_prefs_delete_cb:
 **/
static void
mcm_prefs_delete_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	GError *error = NULL;

	/* try to delete device */
	ret = mcm_client_delete_device (mcm_client, current_device, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		mcm_prefs_error_dialog (_("Failed to delete file"), error->message);
		g_error_free (error);
	}
}

/**
 * mcm_prefs_reset_cb:
 **/
static void
mcm_prefs_reset_cb (GtkWidget *widget, gpointer data)
{
	setting_up_device = TRUE;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), 0.0f);
	setting_up_device = FALSE;
	/* we only want one save, not three */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
}

/**
 * mcm_prefs_message_received_cb
 **/
static UniqueResponse
mcm_prefs_message_received_cb (UniqueApp *app, UniqueCommand command, UniqueMessageData *message_data, guint time_ms, gpointer data)
{
	GtkWindow *window;
	if (command == UNIQUE_ACTIVATE) {
		window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_prefs"));
		gtk_window_present (window);
	}
	return UNIQUE_RESPONSE_OK;
}

/**
 * mcm_window_set_parent_xid:
 **/
static void
mcm_window_set_parent_xid (GtkWindow *window, guint32 xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_window_foreign_new_for_display (display, xid);
	our_window = gtk_widget_get_window (GTK_WIDGET (window));

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
}

/**
 * mcm_prefs_add_devices_columns:
 **/
static void
mcm_prefs_add_devices_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", MCM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 40,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", MCM_DEVICES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, MCM_DEVICES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_devices), MCM_DEVICES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * mcm_prefs_add_profiles_columns:
 **/
static void
mcm_prefs_add_profiles_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", MCM_PROFILES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 50,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", MCM_PROFILES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, MCM_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_profiles), MCM_PROFILES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * mcm_prefs_set_calibrate_button_sensitivity:
 **/
static void
mcm_prefs_set_calibrate_button_sensitivity (void)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	const gchar *tooltip;
	McmDeviceKind kind;
	gboolean connected;
	gboolean xrandr_fallback;

	/* TRANSLATORS: this is when the button is sensitive */
	tooltip = _("Create a color profile for the selected device");

	/* no device selected */
	if (current_device == NULL) {
		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot profile: No device is selected");
		goto out;
	}

	/* are we a display */
	kind = mcm_device_get_kind (current_device);
	if (kind == MCM_DEVICE_KIND_DISPLAY) {

		/* are we disconnected */
		connected = mcm_device_get_connected (current_device);
		if (!connected) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The device is not connected");
			goto out;
		}

		/* are we not XRandR 1.3 compat */
		xrandr_fallback = mcm_device_xrandr_get_fallback (MCM_DEVICE_XRANDR (current_device));
		if (xrandr_fallback) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The display driver does not support XRandR 1.3");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = mcm_colorimeter_get_present (colorimeter);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The measuring instrument is not plugged in");
			goto out;
		}
	} else if (kind == MCM_DEVICE_KIND_SCANNER ||
		   kind == MCM_DEVICE_KIND_CAMERA) {

		/* TODO: find out if we can scan using mate-scan */
		ret = TRUE;

	} else if (kind == MCM_DEVICE_KIND_PRINTER) {

		/* find whether we have hardware installed */
		ret = mcm_colorimeter_get_present (colorimeter);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot profile: The measuring instrument is not plugged in");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = mcm_colorimeter_supports_printer (colorimeter);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot profile: The measuring instrument does not support printer profiling");
			goto out;
		}

	} else {

		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot profile this type of device");
	}
out:
	/* control the tooltip and sensitivity of the button */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_widget_set_sensitive (widget, ret);
}

/**
 * mcm_prefs_is_profile_suitable_for_device:
 **/
static gboolean
mcm_prefs_is_profile_suitable_for_device (McmProfile *profile, McmDevice *device)
{
	McmProfileKind profile_kind_tmp;
	McmProfileKind profile_kind;
	McmColorspace profile_colorspace;
	McmColorspace device_colorspace;
	gboolean ret = FALSE;
	McmDeviceKind device_kind;

	/* not the right colorspace */
	device_colorspace = mcm_device_get_colorspace (device);
	profile_colorspace = mcm_profile_get_colorspace (profile);
	if (device_colorspace != profile_colorspace)
		goto out;

	/* not the correct kind */
	device_kind = mcm_device_get_kind (device);
	profile_kind_tmp = mcm_profile_get_kind (profile);
	profile_kind = mcm_utils_device_kind_to_profile_kind (device_kind);
	if (profile_kind_tmp != profile_kind)
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * mcm_prefs_add_profiles_suitable_for_devices:
 **/
static void
mcm_prefs_add_profiles_suitable_for_devices (GtkWidget *widget, const gchar *profile_filename)
{
	GtkTreeModel *model;
	guint i;
	const gchar *filename;
	gboolean ret;
	gboolean set_active = FALSE;
	McmProfile *profile;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;

	/* clear existing entries */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* add a 'None' entry */
	mcm_prefs_combobox_add_profile (widget, NULL, MCM_PREFS_ENTRY_TYPE_NONE, NULL);

	/* get new list */
	profile_array = mcm_profile_store_get_array (profile_store);

	/* add profiles of the right kind */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only add correct types */
		ret = mcm_prefs_is_profile_suitable_for_device (profile, current_device);
		if (ret) {
			/* add */
			mcm_prefs_combobox_add_profile (widget, profile, MCM_PREFS_ENTRY_TYPE_PROFILE, &iter);

			/* set active option */
			filename = mcm_profile_get_filename (profile);
			if (g_strcmp0 (filename, profile_filename) == 0) {
				//FIXME: does not work for sorted lists
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
				set_active = TRUE;
			}
		}
	}

	/* add a import entry */
	mcm_prefs_combobox_add_profile (widget, NULL, MCM_PREFS_ENTRY_TYPE_IMPORT, NULL);

	/* select 'None' if there was no match */
	if (!set_active) {
		if (profile_filename != NULL)
			egg_warning ("no match for %s", profile_filename);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * mcm_prefs_devices_treeview_clicked_cb:
 **/
static void
mcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar *profile_filename = NULL;
	GtkWidget *widget;
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	gboolean connected;
	gchar *id = NULL;
	gboolean ret;
	McmDeviceKind kind;
	const gchar *device_serial = NULL;
	const gchar *device_model = NULL;
	const gchar *device_manufacturer = NULL;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* get id */
	gtk_tree_model_get (model, &iter,
			    MCM_DEVICES_COLUMN_ID, &id,
			    -1);

	/* we have a new device */
	egg_debug ("selected device is: %s", id);
	if (current_device != NULL) {
		g_object_unref (current_device);
		current_device = NULL;
	}
	current_device = mcm_client_get_device_by_id (mcm_client, id);
	if (current_device == NULL)
		goto out;

	/* not a xrandr device */
	kind = mcm_device_get_kind (current_device);
	if (kind != MCM_DEVICE_KIND_DISPLAY) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		/* show more UI */
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, TRUE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, TRUE);
	}

	/* show broken devices */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_problems"));
	if (kind == MCM_DEVICE_KIND_DISPLAY) {
		ret = mcm_device_xrandr_get_fallback (MCM_DEVICE_XRANDR (current_device));
		if (ret) {
			/* TRANSLATORS: Some shitty binary drivers do not support per-head gamma controls.
			 * Whilst this does not matter if you only have one monitor attached, it means you
			 * can't color correct additional monitors or projectors. */
			gtk_label_set_label (GTK_LABEL (widget), _("Per-device settings not supported. Check your display driver."));
			gtk_widget_show (widget);
		} else {
			gtk_widget_hide (widget);
		}
	} else {
		gtk_widget_hide (widget);
	}

	/* set device labels */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_serial"));
	device_serial = mcm_device_get_serial (current_device);
	if (device_serial != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_serial"));
		gtk_label_set_label (GTK_LABEL (widget), device_serial);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_model"));
	device_model = mcm_device_get_model (current_device);
	if (device_model != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_model"));
		gtk_label_set_label (GTK_LABEL (widget), device_model);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_manufacturer"));
	device_manufacturer = mcm_device_get_manufacturer (current_device);
	if (device_manufacturer != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_manufacturer"));
		gtk_label_set_label (GTK_LABEL (widget), device_manufacturer);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_device_details"));
	gtk_widget_show (widget);

	/* set adjustments */
	setting_up_device = TRUE;
	localgamma = mcm_device_get_gamma (current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), localgamma);
	brightness = mcm_device_get_brightness (current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), brightness);
	contrast = mcm_device_get_contrast (current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), contrast);
	setting_up_device = FALSE;

	/* add profiles of the right kind */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	profile_filename = mcm_device_get_profile_filename (current_device);
	mcm_prefs_add_profiles_suitable_for_devices (widget, profile_filename);

	/* make sure selectable */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile"));
	gtk_widget_set_sensitive (widget, TRUE);

	/* can we delete this device? */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete"));
	connected = mcm_device_get_connected (current_device);
	gtk_widget_set_sensitive (widget, !connected);

	/* can this device calibrate */
	mcm_prefs_set_calibrate_button_sensitivity ();
out:
	g_free (id);
}

/**
 * mcm_prefs_profile_kind_to_string:
 **/
static gchar *
mcm_prefs_profile_kind_to_string (McmProfileKind kind)
{
	if (kind == MCM_PROFILE_KIND_INPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Input device");
	}
	if (kind == MCM_PROFILE_KIND_DISPLAY_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Display device");
	}
	if (kind == MCM_PROFILE_KIND_OUTPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Output device");
	}
	if (kind == MCM_PROFILE_KIND_DEVICELINK) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Devicelink");
	}
	if (kind == MCM_PROFILE_KIND_COLORSPACE_CONVERSION) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Colorspace conversion");
	}
	if (kind == MCM_PROFILE_KIND_ABSTRACT) {
		/* TRANSLATORS: this the ICC profile kind */
		return _("Abstract");
	}
	if (kind == MCM_PROFILE_KIND_NAMED_COLOR) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Named color");
	}
	/* TRANSLATORS: this the ICC profile type */
	return _("Unknown");
}

/**
 * mcm_prefs_profile_colorspace_to_string:
 **/
static gchar *
mcm_prefs_profile_colorspace_to_string (McmColorspace colorspace)
{
	if (colorspace == MCM_COLORSPACE_XYZ) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("XYZ");
	}
	if (colorspace == MCM_COLORSPACE_LAB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LAB");
	}
	if (colorspace == MCM_COLORSPACE_LUV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LUV");
	}
	if (colorspace == MCM_COLORSPACE_YCBCR) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("YCbCr");
	}
	if (colorspace == MCM_COLORSPACE_YXY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Yxy");
	}
	if (colorspace == MCM_COLORSPACE_RGB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("RGB");
	}
	if (colorspace == MCM_COLORSPACE_GRAY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Gray");
	}
	if (colorspace == MCM_COLORSPACE_HSV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("HSV");
	}
	if (colorspace == MCM_COLORSPACE_CMYK) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMYK");
	}
	if (colorspace == MCM_COLORSPACE_CMY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMY");
	}
	/* TRANSLATORS: this the ICC colorspace type */
	return _("Unknown");
}

/**
 * mcm_prefs_profiles_treeview_clicked_cb:
 **/
static void
mcm_prefs_profiles_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	McmProfile *profile;
	McmClut *clut = NULL;
	McmXyz *white;
	McmXyz *red;
	McmXyz *green;
	McmXyz *blue;
	const gchar *profile_copyright;
	const gchar *profile_manufacturer;
	const gchar *profile_model ;
	const gchar *profile_datetime;
	gchar *temp;
	const gchar *filename;
	gchar *basename = NULL;
	gchar *size_text = NULL;
	McmProfileKind profile_kind;
	McmColorspace profile_colorspace;
	const gchar *profile_kind_text;
	const gchar *profile_colorspace_text;
	gboolean ret;
	gboolean has_vcgt;
	guint size = 0;
	guint filesize;
	gfloat x;
	gboolean show_section = FALSE;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    MCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* get the new details from the profile */
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);

	/* check we have enough data for the CIE widget */
	x = mcm_xyz_get_x (red);
	if (x > 0.001) {
		g_object_set (cie_widget,
			      "white", white,
			      "red", red,
			      "green", green,
			      "blue", blue,
			      NULL);
		gtk_widget_show (cie_widget);
		show_section = TRUE;
	} else {
		gtk_widget_hide (cie_widget);
	}

	/* get curve data */
	clut = mcm_profile_generate_curve (profile, 256);

	/* only show if there is useful information */
	if (clut != NULL)
		size = mcm_clut_get_size (clut);
	if (size > 0) {
		g_object_set (trc_widget,
			      "clut", clut,
			      NULL);
		gtk_widget_show (trc_widget);
		show_section = TRUE;
	} else {
		gtk_widget_hide (trc_widget);
	}

	/* set kind */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_type"));
	profile_kind = mcm_profile_get_kind (profile);
	if (profile_kind == MCM_PROFILE_KIND_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_type"));
		profile_kind_text = mcm_prefs_profile_kind_to_string (profile_kind);
		gtk_label_set_label (GTK_LABEL (widget), profile_kind_text);
	}

	/* set colorspace */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_colorspace"));
	profile_colorspace = mcm_profile_get_colorspace (profile);
	if (profile_colorspace == MCM_COLORSPACE_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_colorspace"));
		profile_colorspace_text = mcm_prefs_profile_colorspace_to_string (profile_colorspace);
		gtk_label_set_label (GTK_LABEL (widget), profile_colorspace_text);
	}

	/* set vcgt */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_vcgt"));
	gtk_widget_set_visible (widget, (profile_kind == MCM_PROFILE_KIND_DISPLAY_DEVICE));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_vcgt"));
	has_vcgt = mcm_profile_get_has_vcgt (profile);
	if (has_vcgt) {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("Yes"));
	} else {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("No"));
	}

	/* set basename */
	filename = mcm_profile_get_filename (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_filename"));
	basename = g_path_get_basename (filename);
	gtk_label_set_label (GTK_LABEL (widget), basename);

	/* set size */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_size"));
	filesize = mcm_profile_get_size (profile);
	if (filesize == 0) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_size"));
		size_text = g_format_size (filesize);
		gtk_label_set_label (GTK_LABEL (widget), size_text);
	}

	/* set new copyright */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_copyright"));
	profile_copyright = mcm_profile_get_copyright (profile);
	if (profile_copyright == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_copyright"));
		temp = mcm_utils_linkify (profile_copyright);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new manufacturer */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_profile_manufacturer"));
	profile_manufacturer = mcm_profile_get_manufacturer (profile);
	if (profile_manufacturer == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_manufacturer"));
		temp = mcm_utils_linkify (profile_manufacturer);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new model */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_profile_model"));
	profile_model = mcm_profile_get_model (profile);
	if (profile_model == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_model"));
		gtk_label_set_label (GTK_LABEL(widget), profile_model);
	}

	/* set new datetime */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_datetime"));
	profile_datetime = mcm_profile_get_datetime (profile);
	if (profile_datetime == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_datetime"));
		gtk_label_set_label (GTK_LABEL(widget), profile_datetime);
	}

	/* set delete sensitivity */
	ret = mcm_profile_get_can_delete (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_delete"));
	gtk_widget_set_sensitive (widget, ret);

	/* should we show the pane at all */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_profile_graphs"));
	gtk_widget_set_visible (widget, show_section);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, TRUE);

	if (clut != NULL)
		g_object_unref (clut);
	g_object_unref (white);
	g_object_unref (red);
	g_object_unref (green);
	g_object_unref (blue);
	g_free (size_text);
	g_free (basename);
}

/**
 * mcm_device_kind_to_string:
 **/
static const gchar *
mcm_prefs_device_kind_to_string (McmDeviceKind kind)
{
	if (kind == MCM_DEVICE_KIND_DISPLAY)
		return "1";
	if (kind == MCM_DEVICE_KIND_SCANNER)
		return "2";
	if (kind == MCM_DEVICE_KIND_CAMERA)
		return "3";
	if (kind == MCM_DEVICE_KIND_PRINTER)
		return "4";
	return "5";
}

/**
 * mcm_prefs_add_device_xrandr:
 **/
static void
mcm_prefs_add_device_xrandr (McmDevice *device)
{
	GtkTreeIter iter;
	const gchar *title_tmp;
	gchar *title = NULL;
	gchar *sort = NULL;
	const gchar *id;
	gboolean ret;
	gboolean connected;
	GError *error = NULL;

	/* sanity check */
	if (!MCM_IS_DEVICE_XRANDR (device)) {
		egg_warning ("not a xrandr device");
		goto out;
	}

	/* italic for non-connected devices */
	connected = mcm_device_get_connected (device);
	title_tmp = mcm_device_get_title (device);
	if (connected) {
		/* set the gamma on the device */
		ret = mcm_device_apply (device, &error);
		if (!ret) {
			egg_warning ("failed to apply profile: %s", error->message);
			g_error_free (error);
		}

		/* use a different title if we have crap xorg drivers */
		if (ret) {
			title = g_strdup (title_tmp);
		} else {
			/* TRANSLATORS: this is where an output is not settable, but we are showing it in the UI */
			title = g_strdup_printf ("%s\n(%s)", title_tmp, _("No hardware support"));
		}
	} else {
		/* TRANSLATORS: this is where the device has been setup but is not connected */
		title = g_strdup_printf ("%s\n<i>[%s]</i>", title_tmp, _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				mcm_prefs_device_kind_to_string (MCM_DEVICE_KIND_DISPLAY),
				title);

	/* add to list */
	id = mcm_device_get_id (device);
	egg_debug ("add %s to device list", id);
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    MCM_DEVICES_COLUMN_ID, id,
			    MCM_DEVICES_COLUMN_SORT, sort,
			    MCM_DEVICES_COLUMN_TITLE, title,
			    MCM_DEVICES_COLUMN_ICON, "video-display", -1);
out:
	g_free (sort);
	g_free (title);
}

/**
 * mcm_prefs_set_combo_simple_text:
 **/
static void
mcm_prefs_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (4, G_TYPE_STRING, MCM_TYPE_PROFILE, G_TYPE_UINT, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), MCM_PREFS_COMBO_COLUMN_SORTABLE, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 60,
		      NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", MCM_PREFS_COMBO_COLUMN_TEXT,
					NULL);
}

/**
 * mcm_prefs_profile_combo_changed_cb:
 **/
static void
mcm_prefs_profile_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	const gchar *profile_old;
	GFile *file = NULL;
	GFile *dest = NULL;
	gboolean ret;
	GError *error = NULL;
	McmProfile *profile = NULL;
	McmProfile *profile_tmp = NULL;
	gboolean changed;
	McmDeviceKind kind;
	GtkTreeIter iter;
	GtkTreeModel *model;
	McmPrefsEntryType entry_type;
	gboolean has_vcgt;
	gchar *filename = NULL;

	/* no devices */
	if (current_device == NULL)
		return;

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;

	/* get entry */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    MCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    MCM_PREFS_COMBO_COLUMN_TYPE, &entry_type,
			    -1);

	/* import */
	if (entry_type == MCM_PREFS_ENTRY_TYPE_IMPORT) {
		file = mcm_prefs_file_chooser_get_icc_profile ();
		if (file == NULL) {
			egg_warning ("failed to get ICC file");
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			goto out;
		}

		/* check the file is suitable */
		profile_tmp = mcm_profile_default_new ();
		filename = g_file_get_path (file);
		ret = mcm_profile_parse (profile_tmp, file, &error);
		if (!ret) {
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			egg_warning ("failed to parse ICC file: %s", error->message);
			g_error_free (error);
			goto out;
		}
		ret = mcm_prefs_is_profile_suitable_for_device (profile_tmp, current_device);
		if (!ret) {
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			/* TRANSLATORS: the profile was of the wrong sort for this device */
			mcm_prefs_error_dialog (_("Could not import profile"), _("The profile was of the wrong type for this device"));
			goto out;
		}

		/* actually set this as the default */
		ret = mcm_prefs_profile_import_file (file);
		if (!ret) {
			gchar *uri;
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			uri = g_file_get_uri (file);
			egg_debug ("%s did not import correctly", uri);
			g_free (uri);
			goto out;
		}

		/* now use the new profile as the device default */
		dest = mcm_utils_get_profile_destination (file);
		filename = g_file_get_path (dest);
	}

	/* get the device kind */
	kind = mcm_device_get_kind (current_device);

	/* get profile filename */
	if (entry_type == MCM_PREFS_ENTRY_TYPE_PROFILE) {

		/* show a warning if the profile is crap */
		filename = g_strdup (mcm_profile_get_filename (profile));
		has_vcgt = mcm_profile_get_has_vcgt (profile);
		if (kind == MCM_DEVICE_KIND_DISPLAY && !has_vcgt && filename != NULL) {
			gtk_widget_show (info_bar_vcgt);
		} else {
			gtk_widget_hide (info_bar_vcgt);
		}
	} else {
		gtk_widget_hide (info_bar_vcgt);
	}

	/* see if it's changed */
	profile_old = mcm_device_get_profile_filename (current_device);
	egg_debug ("old: %s, new:%s", profile_old, filename);
	changed = ((g_strcmp0 (profile_old, filename) != 0));

	/* set new profile */
	if (changed) {

		/* save new profile */
		mcm_device_set_profile_filename (current_device, filename);
		ret = mcm_device_save (current_device, &error);
		if (!ret) {
			egg_warning ("failed to save config: %s", error->message);
			g_error_free (error);
			goto out;
		}

		/* set the profile */
		ret = mcm_device_apply (current_device, &error);
		if (!ret) {
			egg_warning ("failed to apply profile: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (dest != NULL)
		g_object_unref (dest);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
}

/**
 * mcm_prefs_slider_changed_cb:
 **/
static void
mcm_prefs_slider_changed_cb (GtkRange *range, gpointer *user_data)
{
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;

	/* we're just setting up the device, not moving the slider */
	if (setting_up_device)
		return;

	/* get values */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	localgamma = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	brightness = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	contrast = gtk_range_get_value (GTK_RANGE (widget));

	mcm_device_set_gamma (current_device, localgamma);
	mcm_device_set_brightness (current_device, brightness * 100.0f);
	mcm_device_set_contrast (current_device, contrast * 100.0f);

	/* save new profile */
	ret = mcm_device_save (current_device, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* actually set the new profile */
	ret = mcm_device_apply (current_device, &error);
	if (!ret) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * mcm_prefs_colorimeter_changed_cb:
 **/
static void
mcm_prefs_colorimeter_changed_cb (McmColorimeter *_colorimeter, gpointer user_data)
{
	gboolean present;
	const gchar *event_id;
	const gchar *message;

	present = mcm_colorimeter_get_present (_colorimeter);

	if (present) {
		/* TRANSLATORS: this is a sound description */
		message = _("Device added");
		event_id = "device-added";
	} else {
		/* TRANSLATORS: this is a sound description */
		message = _("Device removed");
		event_id = "device-removed";
	}

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, event_id,
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("MATE Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, message, NULL);

	mcm_prefs_set_calibrate_button_sensitivity ();
}

/**
 * mcm_prefs_device_kind_to_icon_name:
 **/
static const gchar *
mcm_prefs_device_kind_to_icon_name (McmDeviceKind kind)
{
	if (kind == MCM_DEVICE_KIND_DISPLAY)
		return "video-display";
	if (kind == MCM_DEVICE_KIND_SCANNER)
		return "scanner";
	if (kind == MCM_DEVICE_KIND_PRINTER)
		return "printer";
	if (kind == MCM_DEVICE_KIND_CAMERA)
		return "camera-photo";
	return "image-missing";
}

/**
 * mcm_prefs_add_device_kind:
 **/
static void
mcm_prefs_add_device_kind (McmDevice *device)
{
	GtkTreeIter iter;
	const gchar *title;
	GString *string;
	const gchar *id;
	gchar *sort = NULL;
	McmDeviceKind kind;
	const gchar *icon_name;
	gboolean connected;
	gboolean virtual;

	/* get icon */
	kind = mcm_device_get_kind (device);
	icon_name = mcm_prefs_device_kind_to_icon_name (kind);

	/* create a title for the device */
	title = mcm_device_get_title (device);
	string = g_string_new (title);

	/* italic for non-connected devices */
	connected = mcm_device_get_connected (device);
	virtual = mcm_device_get_virtual (device);
	if (!connected && !virtual) {
		/* TRANSLATORS: this is where the device has been setup but is not connected */
		g_string_append_printf (string, "\n<i>[%s]</i>", _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				mcm_prefs_device_kind_to_string (kind),
				string->str);

	/* add to list */
	id = mcm_device_get_id (device);
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    MCM_DEVICES_COLUMN_ID, id,
			    MCM_DEVICES_COLUMN_SORT, sort,
			    MCM_DEVICES_COLUMN_TITLE, string->str,
			    MCM_DEVICES_COLUMN_ICON, icon_name, -1);
	g_free (sort);
	g_string_free (string, TRUE);
}


/**
 * mcm_prefs_remove_device:
 **/
static void
mcm_prefs_remove_device (McmDevice *mcm_device)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *id;
	gchar *id_tmp;
	gboolean ret;

	/* remove */
	id = mcm_device_get_id (mcm_device);
	egg_debug ("removing: %s (connected: %i)", id,
		   mcm_device_get_connected (mcm_device));

	/* get first element */
	model = GTK_TREE_MODEL (list_store_devices);
	ret = gtk_tree_model_get_iter_first (model, &iter);
	if (!ret)
		return;

	/* get the other elements */
	do {
		gtk_tree_model_get (model, &iter,
				    MCM_DEVICES_COLUMN_ID, &id_tmp,
				    -1);
		if (g_strcmp0 (id_tmp, id) == 0) {
			gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
			g_free (id_tmp);
			break;
		}
		g_free (id_tmp);
	} while (gtk_tree_model_iter_next (model, &iter));
}

/**
 * mcm_prefs_added_idle_cb:
 **/
static gboolean
mcm_prefs_added_idle_cb (McmDevice *device)
{
	McmDeviceKind kind;
	egg_debug ("added: %s (connected: %i)",
		   mcm_device_get_id (device),
		   mcm_device_get_connected (device));

	/* remove the saved device if it's already there */
	mcm_prefs_remove_device (device);

	/* add the device */
	kind = mcm_device_get_kind (device);
	if (kind == MCM_DEVICE_KIND_DISPLAY)
		mcm_prefs_add_device_xrandr (device);
	else
		mcm_prefs_add_device_kind (device);

	/* unref the instance */
	g_object_unref (device);
	return FALSE;
}

/**
 * mcm_prefs_added_cb:
 **/
static void
mcm_prefs_added_cb (McmClient *mcm_client_, McmDevice *mcm_device, gpointer user_data)
{
	g_idle_add ((GSourceFunc) mcm_prefs_added_idle_cb, g_object_ref (mcm_device));
}

/**
 * mcm_prefs_changed_cb:
 **/
static void
mcm_prefs_changed_cb (McmClient *mcm_client_, McmDevice *mcm_device, gpointer user_data)
{
	g_idle_add ((GSourceFunc) mcm_prefs_added_idle_cb, g_object_ref (mcm_device));
}

/**
 * mcm_prefs_removed_cb:
 **/
static void
mcm_prefs_removed_cb (McmClient *mcm_client_, McmDevice *mcm_device, gpointer user_data)
{
	gboolean connected;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gboolean ret;

	/* remove from the UI */
	mcm_prefs_remove_device (mcm_device);

	/* ensure this device is re-added if it's been saved */
	connected = mcm_device_get_connected (mcm_device);
	if (connected)
		mcm_client_add_saved (mcm_client, NULL);

	/* select the first device */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store_devices), &iter);
	if (!ret)
		return;

	/* click it */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &iter);
}

/**
 * mcm_prefs_startup_phase2_idle_cb:
 **/
static gboolean
mcm_prefs_startup_phase2_idle_cb (gpointer user_data)
{
	GtkWidget *widget;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	/* update list of profiles */
	mcm_prefs_update_profile_list ();

	/* select a profile to display */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * mcm_prefs_colorspace_to_localised_string:
 **/
static const gchar *
mcm_prefs_colorspace_to_localised_string (McmColorspace colorspace)
{
	if (colorspace == MCM_COLORSPACE_RGB) {
		/* TRANSLATORS: this is the colorspace, e.g. red, green, blue */
		return _("RGB");
	}
	if (colorspace == MCM_COLORSPACE_CMYK) {
		/* TRANSLATORS: this is the colorspace, e.g. cyan, magenta, yellow, black */
		return _("CMYK");
	}
	return NULL;
}

/**
 * mcm_prefs_setup_space_combobox:
 **/
static void
mcm_prefs_setup_space_combobox (GtkWidget *widget, McmColorspace colorspace, const gchar *profile_filename)
{
	McmProfile *profile;
	guint i;
	const gchar *filename;
	const gchar *description;
	McmColorspace colorspace_tmp;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gchar *text = NULL;
	const gchar *search = "RGB";
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;

	/* search is a way to reduce to number of profiles */
	if (colorspace == MCM_COLORSPACE_CMYK)
		search = "CMYK";

	/* get new list */
	profile_array = mcm_profile_store_get_array (profile_store);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only for correct kind */
		description = mcm_profile_get_description (profile);
		has_vcgt = mcm_profile_get_has_vcgt (profile);
		colorspace_tmp = mcm_profile_get_colorspace (profile);
		if (!has_vcgt &&
		    colorspace == colorspace_tmp &&
		    (colorspace == MCM_COLORSPACE_CMYK ||
		     g_strstr_len (description, -1, search) != NULL)) {
			mcm_prefs_combobox_add_profile (widget, profile, MCM_PREFS_ENTRY_TYPE_PROFILE, &iter);

			/* set active option */
			filename = mcm_profile_get_filename (profile);
			if (g_strcmp0 (filename, profile_filename) == 0)
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			has_profile = TRUE;
		}
	}
	if (!has_profile) {
		/* TRANSLATORS: this is when there are no profiles that can be used; the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					mcm_prefs_colorspace_to_localised_string (colorspace));
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	g_free (text);
}

/**
 * mcm_prefs_space_combo_changed_cb:
 **/
static void
mcm_prefs_space_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	GtkTreeIter iter;
	const gchar *filename;
	GtkTreeModel *model;
	McmProfile *profile = NULL;
	const gchar *key = MCM_SETTINGS_COLORSPACE_RGB;

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;

	/* get profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    MCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);
	if (profile == NULL)
		goto out;

	if (data != NULL)
		key = MCM_SETTINGS_COLORSPACE_CMYK;

	filename = mcm_profile_get_filename (profile);
	egg_debug ("changed working space %s", filename);
	g_settings_set_string (settings, key, filename);
out:
	if (profile != NULL)
		g_object_unref (profile);
}


/**
 * mcm_prefs_renderer_combo_changed_cb:
 **/
static void
mcm_prefs_renderer_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gint active;
	const gchar *key = MCM_SETTINGS_RENDERING_INTENT_DISPLAY;
	const gchar *value;

	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;

	if (data != NULL)
		key = MCM_SETTINGS_RENDERING_INTENT_SOFTPROOF;

	/* save to GSettings */
	value = mcm_intent_to_string (active+1);
	egg_debug ("changed rendering intent to %s", value);
	g_settings_set_string (settings, key, value);
}

/**
 * mcm_prefs_setup_rendering_combobox:
 **/
static void
mcm_prefs_setup_rendering_combobox (GtkWidget *widget, const gchar *intent)
{
	guint i;
	gboolean ret = FALSE;
	const gchar *text;

	for (i=1; i<MCM_INTENT_LAST; i++) {
		text = mcm_intent_to_localized_text (i);
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), text);
		text = mcm_intent_to_string (i);
		if (g_strcmp0 (text, intent) == 0) {
			ret = TRUE;
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i-1);
		}
	}
	/* nothing matches, just set the first option */
	if (!ret)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}

/**
 * mcm_prefs_startup_phase1_idle_cb:
 **/
static gboolean
mcm_prefs_startup_phase1_idle_cb (gpointer user_data)
{
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;
	gchar *colorspace_rgb;
	gchar *colorspace_cmyk;
	gchar *intent_display;
	gchar *intent_softproof;

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_rgb"));
	colorspace_rgb = g_settings_get_string (settings, MCM_SETTINGS_COLORSPACE_RGB);
	mcm_prefs_set_combo_simple_text (widget);
	mcm_prefs_setup_space_combobox (widget, MCM_COLORSPACE_RGB, colorspace_rgb);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (mcm_prefs_space_combo_changed_cb), NULL);

	/* setup CMYK combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_cmyk"));
	colorspace_cmyk = g_settings_get_string (settings, MCM_SETTINGS_COLORSPACE_CMYK);
	mcm_prefs_set_combo_simple_text (widget);
	mcm_prefs_setup_space_combobox (widget, MCM_COLORSPACE_CMYK, colorspace_cmyk);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (mcm_prefs_space_combo_changed_cb), (gpointer) "cmyk");

	/* setup rendering lists */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_display"));
	mcm_prefs_set_combo_simple_text (widget);
	intent_display = g_settings_get_string (settings, MCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	mcm_prefs_setup_rendering_combobox (widget, intent_display);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (mcm_prefs_renderer_combo_changed_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_softproof"));
	mcm_prefs_set_combo_simple_text (widget);
	intent_softproof = g_settings_get_string (settings, MCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	mcm_prefs_setup_rendering_combobox (widget, intent_softproof);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (mcm_prefs_renderer_combo_changed_cb), (gpointer) "softproof");

	/* coldplug plugged in devices */
	ret = mcm_client_add_connected (mcm_client, MCM_CLIENT_COLDPLUG_ALL, &error);
	if (!ret) {
		egg_warning ("failed to add connected devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* coldplug saved devices */
	ret = mcm_client_add_saved (mcm_client, &error);
	if (!ret) {
		egg_warning ("failed to add saved devices: %s", error->message);
		g_clear_error (&error);
		/* do not fail */
	}

	/* set calibrate button sensitivity */
	mcm_prefs_set_calibrate_button_sensitivity ();

	/* start phase 2 of the startup */
	g_idle_add ((GSourceFunc) mcm_prefs_startup_phase2_idle_cb, NULL);

out:
	g_free (intent_display);
	g_free (intent_softproof);
	g_free (colorspace_rgb);
	g_free (colorspace_cmyk);
	return FALSE;
}

/**
 * mcm_prefs_reset_devices_idle_cb:
 **/
static gboolean
mcm_prefs_reset_devices_idle_cb (gpointer user_data)
{
	GPtrArray *array = NULL;
	McmDevice *device;
	GError *error = NULL;
	gboolean ret;
	guint i;

	/* set for each output */
	array = mcm_client_get_devices (mcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* set gamma for device */
		ret = mcm_device_apply (device, &error);
		if (!ret) {
			egg_warning ("failed to set profile: %s", error->message);
			g_error_free (error);
			break;
		}
	}
	g_ptr_array_unref (array);
	return FALSE;
}

/**
 * mcm_prefs_checkbutton_changed_cb:
 **/
static void
mcm_prefs_checkbutton_changed_cb (GtkWidget *widget, gpointer user_data)
{
	/* set the new setting */
	g_idle_add ((GSourceFunc) mcm_prefs_reset_devices_idle_cb, NULL);
}

/**
 * mcm_prefs_setup_drag_and_drop:
 **/
static void
mcm_prefs_setup_drag_and_drop (GtkWidget *widget)
{
	GtkTargetEntry entry;

	/* setup a dummy entry */
	entry.target = g_strdup ("text/plain");
	entry.flags = GTK_TARGET_OTHER_APP;
	entry.info = 0;

	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, &entry, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_free (entry.target);
}

/**
 * mcm_prefs_profile_store_changed_cb:
 **/
static void
mcm_prefs_profile_store_changed_cb (McmProfileStore *_profile_store, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;

	/* clear and update the profile list */
	mcm_prefs_update_profile_list ();

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (selection == NULL)
		return;
	g_signal_emit_by_name (selection, "changed", NULL);
}

/**
 * mcm_prefs_select_first_device_idle_cb:
 **/
static gboolean
mcm_prefs_select_first_device_idle_cb (gpointer data)
{
	GtkTreePath *path;
	GtkWidget *widget;

	/* set the cursor on the first device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * mcm_prefs_client_notify_loading_cb:
 **/
static void
mcm_prefs_client_notify_loading_cb (McmClient *client, GParamSpec *pspec, gpointer data)
{
	gboolean loading;

	/*if loading show the bar */
	loading = mcm_client_get_loading (client);
	if (loading) {
		gtk_widget_show (info_bar_loading);
		return;
	}

	/* otherwise clear the loading widget */
	gtk_widget_hide (info_bar_loading);

	/* idle callback */
	g_idle_add (mcm_prefs_select_first_device_idle_cb, NULL);
}

/**
 * mcm_prefs_info_bar_response_cb:
 **/
static void
mcm_prefs_info_bar_response_cb (GtkDialog *dialog, GtkResponseType response, gpointer user_data)
{
	if (response == GTK_RESPONSE_HELP) {
		/* open the help file in the right place */
		mcm_mate_help ("faq-missing-vcgt");
	}
}

/**
 * mcm_device_kind_to_localised_string:
 **/
static const gchar *
mcm_device_kind_to_localised_string (McmDeviceKind device_kind)
{
	if (device_kind == MCM_DEVICE_KIND_DISPLAY) {
		/* TRANSLATORS: device type */
		return _("Display");
	}
	if (device_kind == MCM_DEVICE_KIND_SCANNER) {
		/* TRANSLATORS: device type */
		return _("Scanner");
	}
	if (device_kind == MCM_DEVICE_KIND_PRINTER) {
		/* TRANSLATORS: device type */
		return _("Printer");
	}
	if (device_kind == MCM_DEVICE_KIND_CAMERA) {
		/* TRANSLATORS: device type */
		return _("Camera");
	}
	return NULL;
}

/**
 * mcm_prefs_setup_virtual_combobox:
 **/
static void
mcm_prefs_setup_virtual_combobox (GtkWidget *widget)
{
	guint i;
	const gchar *text;

	for (i=MCM_DEVICE_KIND_SCANNER; i<MCM_DEVICE_KIND_LAST; i++) {
		text = mcm_device_kind_to_localised_string (i);
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), MCM_DEVICE_KIND_PRINTER - 2);
}

/**
 * gpk_update_viewer_notify_network_state_cb:
 **/
static void
mcm_prefs_button_virtual_entry_changed_cb (GtkEntry *entry, GParamSpec *pspec, gpointer user_data)
{
	const gchar *model;
	const gchar *manufacturer;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* only set the add button sensitive if both sections have text */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_virtual_add"));
	gtk_widget_set_sensitive (widget, (model != NULL && model[0] != '\0' && manufacturer != NULL && manufacturer[0] != '\0'));
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 0;
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	UniqueApp *unique_app;
	guint xid = 0;
	GError *error = NULL;
	GMainLoop *loop;
	GtkTreeSelection *selection;
	GtkWidget *info_bar_loading_label;
	GtkWidget *info_bar_vcgt_label;
	GtkSizeGroup *size_group = NULL;
	GtkSizeGroup *size_group2 = NULL;
	GdkScreen *screen;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("mate-color-manager prefs program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* block in a loop */
	loop = g_main_loop_new (NULL, FALSE);

	/* are we already activated? */
	unique_app = unique_app_new ("org.mate.ColorManager.Prefs", NULL);
	if (unique_app_is_running (unique_app)) {
		egg_debug ("You have another instance running. This program will now close");
		unique_app_send_message (unique_app, UNIQUE_ACTIVATE, NULL);
		goto out;
	}
	g_signal_connect (unique_app, "message-received",
			  G_CALLBACK (mcm_prefs_message_received_cb), NULL);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, MCM_DATA "/mcm-prefs.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   MCM_DATA G_DIR_SEPARATOR_S "icons");

	/* maintain a list of profiles */
	profile_store = mcm_profile_store_new ();
	g_signal_connect (profile_store, "changed", G_CALLBACK(mcm_prefs_profile_store_changed_cb), NULL);

	/* create list stores */
	list_store_devices = gtk_list_store_new (MCM_DEVICES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);
	list_store_profiles = gtk_list_store_new (MCM_PROFILES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						  G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	/* create device tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (mcm_prefs_devices_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	mcm_prefs_add_devices_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create profile tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_profiles));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (mcm_prefs_profiles_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	mcm_prefs_add_profiles_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_prefs"));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), MCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (mcm_prefs_delete_event_cb), loop);
	g_signal_connect (main_window, "drag-data-received",
			  G_CALLBACK (mcm_prefs_drag_data_received_cb), loop);
	mcm_prefs_setup_drag_and_drop (GTK_WIDGET(main_window));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_close_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_default_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_help_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_reset_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_delete_cb), NULL);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_device_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_device_add_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_calibrate_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_profile_delete_cb), NULL);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_import"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_profile_import_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
	gtk_widget_set_sensitive (widget, FALSE);

	/* hidden until a profile is selected */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_profile_graphs"));
	gtk_widget_set_visible (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, FALSE);

	/* hide widgets by default */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_device_details"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_manufacturer"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_model"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_serial"));
	gtk_widget_hide (widget);

	/* set up virtual device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_virtual"));
	gtk_window_set_icon_name (GTK_WINDOW (widget), MCM_STOCK_ICON);
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (main_window));
	gtk_window_set_modal (GTK_WINDOW (widget), TRUE);
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (mcm_prefs_virtual_delete_event_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_virtual_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_button_virtual_add_cb), NULL);
	gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_virtual_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_button_virtual_cancel_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_virtual_type"));
	mcm_prefs_set_combo_simple_text (widget);
	mcm_prefs_setup_virtual_combobox (widget);

	/* disable the add button if nothing in either box */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_model"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (mcm_prefs_button_virtual_entry_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "entry_virtual_manufacturer"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (mcm_prefs_button_virtual_entry_changed_cb), NULL);

	/* setup icc profiles list */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	mcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (mcm_prefs_profile_combo_changed_cb), NULL);

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 5.0f);
	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 1.8f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 2.2f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_range (GTK_RANGE (widget), 0.0f, 0.9f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 0.0f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 1.0f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");

	/* set alignment for left */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox5"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox10"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox6"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox21"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox22"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox23"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox30"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox32"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox34"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox36"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox39"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox48"));
	gtk_size_group_add_widget (size_group, widget);

	/* set alignment for right */
	size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox24"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox25"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox26"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox11"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox12"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox18"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox31"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox33"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox35"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox37"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox40"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox49"));
	gtk_size_group_add_widget (size_group2, widget);

	/* use a device client array */
	mcm_client = mcm_client_new ();
	mcm_client_set_use_threads (mcm_client, TRUE);
	g_signal_connect (mcm_client, "added", G_CALLBACK (mcm_prefs_added_cb), NULL);
	g_signal_connect (mcm_client, "removed", G_CALLBACK (mcm_prefs_removed_cb), NULL);
	g_signal_connect (mcm_client, "changed", G_CALLBACK (mcm_prefs_changed_cb), NULL);
	g_signal_connect (mcm_client, "notify::loading",
			  G_CALLBACK (mcm_prefs_client_notify_loading_cb), NULL);

	/* use the color device */
	colorimeter = mcm_colorimeter_new ();
	g_signal_connect (colorimeter, "changed", G_CALLBACK (mcm_prefs_colorimeter_changed_cb), NULL);

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		mcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* use cie widget */
	cie_widget = mcm_cie_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_cie_widget"));
	gtk_box_pack_start (GTK_BOX(widget), cie_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), cie_widget, 0);

	/* use trc widget */
	trc_widget = mcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_trc_widget"));
	gtk_box_pack_start (GTK_BOX(widget), trc_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), trc_widget, 0);

	/* do we set a default size to make the window larger? */
	screen = gdk_screen_get_default ();
	if (gdk_screen_get_width (screen) < 1024 ||
	    gdk_screen_get_height (screen) < 768) {
		gtk_widget_set_size_request (cie_widget, 50, 50);
		gtk_widget_set_size_request (trc_widget, 50, 50);
	} else {
		gtk_widget_set_size_request (cie_widget, 200, 200);
		gtk_widget_set_size_request (trc_widget, 200, 200);
	}

	/* use infobar */
	info_bar_loading = gtk_info_bar_new ();
	info_bar_vcgt = gtk_info_bar_new ();
	g_signal_connect (info_bar_vcgt, "response",
			  G_CALLBACK (mcm_prefs_info_bar_response_cb), NULL);

	/* TRANSLATORS: button for more details about the vcgt failure */
	gtk_info_bar_add_button (GTK_INFO_BAR(info_bar_vcgt), _("More Information"), GTK_RESPONSE_HELP);

	/* TRANSLATORS: this is displayed while the devices are being probed */
	info_bar_loading_label = gtk_label_new (_("Loading list of devices…"));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_loading), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_loading));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_loading_label);
	gtk_widget_show (info_bar_loading_label);

	/* TRANSLATORS: this is displayed when the profile is crap */
	info_bar_vcgt_label = gtk_label_new (_("This profile does not have the information required for whole-screen color correction."));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_vcgt), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_vcgt));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_vcgt_label);
	gtk_widget_show (info_bar_vcgt_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_devices"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_loading, FALSE, FALSE, 0);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_sections"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_vcgt, FALSE, FALSE, 0);

	/* show main UI */
	gtk_window_set_default_size (GTK_WINDOW(main_window), 1000, 450);
	gtk_widget_show (main_window);

	/* connect up sliders */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (mcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (mcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (mcm_prefs_slider_changed_cb), NULL);

	/* setup defaults */
	settings = g_settings_new (MCM_SETTINGS_SCHEMA);

	/* connect up global widget */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_display"));
	g_settings_bind (settings,
			 MCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_checkbutton_changed_cb), NULL);

	/* connect up atom widget */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_profile"));
	g_settings_bind (settings,
			 MCM_SETTINGS_SET_ICC_PROFILE_ATOM,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_prefs_checkbutton_changed_cb), NULL);

	/* do we show the fine tuning box */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
	g_settings_bind (settings,
			 MCM_SETTINGS_SHOW_FINE_TUNING,
			 widget, "visible",
			 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* do all this after the window has been set up */
	g_idle_add (mcm_prefs_startup_phase1_idle_cb, NULL);

	/* wait */
	g_main_loop_run (loop);

out:
	g_object_unref (unique_app);
	g_main_loop_unref (loop);
	if (size_group != NULL)
		g_object_unref (size_group);
	if (size_group2 != NULL)
		g_object_unref (size_group2);
	if (current_device != NULL)
		g_object_unref (current_device);
	if (colorimeter != NULL)
		g_object_unref (colorimeter);
	if (settings != NULL)
		g_object_unref (settings);
	if (builder != NULL)
		g_object_unref (builder);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (mcm_client != NULL)
		g_object_unref (mcm_client);
	return retval;
}

