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
#include <locale.h>

#include "egg-debug.h"

#include "mcm-profile.h"
#include "mcm-utils.h"
#include "mcm-xyz.h"
#include "mcm-cie-widget.h"

/**
 * mcm_import_add_cie_widget:
 **/
static void
mcm_import_add_cie_widget (GtkDialog *dialog, GtkWidget *cie_widget)
{
	GtkWidget *aspect;
	GtkWidget *vbox;

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	aspect = gtk_aspect_frame_new (NULL, 0.0f, 0.0f, 1.0f, FALSE);
	gtk_frame_set_shadow_type (GTK_FRAME(aspect), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER(aspect), cie_widget);
	gtk_box_pack_end (GTK_BOX(vbox), aspect, TRUE, TRUE, 12);
	gtk_widget_show (cie_widget);
	gtk_widget_show (aspect);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	const gchar *copyright;
	const gchar *description;
	GFile *destination = NULL;
	GFile *file = NULL;
	gchar **files = NULL;
	guint retval = 1;
	McmProfile *profile = NULL;
	McmColorspace colorspace;
	GError *error = NULL;
	GOptionContext *context;
	GString *string = NULL;
	GtkWidget *dialog;
	GtkResponseType response;
	GtkWidget *cie_widget = NULL;
	McmXyz *white = NULL;
	McmXyz *red = NULL;
	McmXyz *green = NULL;
	McmXyz *blue = NULL;

	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of catalogs to install */
		  _("ICC profile to install"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("mate-color-manager import program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* nothing sent */
	if (files == NULL) {
		/* TRANSLATORS: nothing was specified on the command line */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("No filename specified"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* load profile */
	profile = mcm_profile_default_new ();
	file = g_file_new_for_path (files[0]);
	ret = mcm_profile_parse (profile, file, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to open ICC profile"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
		/* TRANSLATORS: parsing error */
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Failed to parse file: %s"), error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* get data */
	description = mcm_profile_get_description (profile);
	copyright = mcm_profile_get_copyright (profile);
	colorspace = mcm_profile_get_colorspace (profile);
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);

	/* use CIE widget */
	cie_widget = mcm_cie_widget_new ();
	gtk_widget_set_size_request (cie_widget, 200, 200);
	g_object_set (cie_widget,
		      "use-grid", FALSE,
		      "use-whitepoint", FALSE,
		      "white", white,
		      "red", red,
		      "green", green,
		      "blue", blue,
		      NULL);

	/* check file does't already exist */
	destination = mcm_utils_get_profile_destination (file);
	ret = g_file_query_exists (destination, NULL);
	if (ret) {
		/* TRANSLATORS: color profile already been installed */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("ICC profile already installed"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s\n%s", description, copyright);

		/* add cie widget */
		mcm_import_add_cie_widget (GTK_DIALOG(dialog), cie_widget);

		gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* create message */
	string = g_string_new ("");
	if (description != NULL) {
		/* TRANSLATORS: message text */
		g_string_append_printf (string, _("Import ICC color profile %s?"), description);
	} else {
		/* TRANSLATORS: message text */
		g_string_append (string, _("Import ICC color profile?"));
	}

	/* add copyright */
	if (copyright != NULL)
		g_string_append_printf (string, "\n%s", copyright);

	/* ask confirmation */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CLOSE, "%s", _("Import ICC profile"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	/* TRANSLATORS: button text */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_OK);

	/* add cie widget */
	mcm_import_add_cie_widget (GTK_DIALOG(dialog), cie_widget);

	/* only show the cie widget if we have RGB data */
	if (colorspace != MCM_COLORSPACE_RGB)
		gtk_widget_hide (cie_widget);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* not the correct response */
	if (response != GTK_RESPONSE_OK)
		goto out;

	/* copy icc file to users profile path */
	ret = mcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to copy file"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* open up the preferences */
	ret = g_spawn_command_line_async (BINDIR "/mcm-prefs", &error);
	if (!ret) {
		egg_warning ("failed to spawn preferences: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (white != NULL)
		g_object_unref (white);
	if (red != NULL)
		g_object_unref (red);
	if (green != NULL)
		g_object_unref (green);
	if (blue != NULL)
		g_object_unref (blue);
	if (string != NULL)
		g_string_free (string, TRUE);
	if (profile != NULL)
		g_object_unref (profile);
	if (destination != NULL)
		g_object_unref (destination);
	g_strfreev (files);
	return retval;
}

