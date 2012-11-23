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

#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <unique/unique.h>

#include "egg-debug.h"

#include "mcm-colorimeter.h"
#include "mcm-utils.h"

static GtkBuilder *builder = NULL;
static GtkWidget *info_bar_hardware = NULL;
static GtkWidget *info_bar_hardware_label = NULL;

/**
 * mcm_picker_set_pixbuf_color:
 **/
static void
mcm_picker_set_pixbuf_color (GdkPixbuf *pixbuf, gchar red, gchar green, gchar blue)
{
	gint x, y;
	gint width, height, rowstride, n_channels;
	guchar *pixels, *p;

	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	/* set to all the correct colors */
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			p = pixels + y * rowstride + x * n_channels;
			p[0] = red;
			p[1] = green;
			p[2] = blue;
//			p[3] = alpha;
		}
	}
}

/**
 * mcm_picker_measure_cb:
 **/
static void
mcm_picker_measure_cb (GtkWidget *widget, gpointer data)
{
	GtkImage *image;
	GtkLabel *label;
	GdkPixbuf *pixbuf;
	image = GTK_IMAGE (gtk_builder_get_object (builder, "image_preview"));
	pixbuf = gtk_image_get_pixbuf (image);
	if (pixbuf == NULL) {
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 200, 200);
		gtk_image_set_from_pixbuf (image, pixbuf);
	}

	mcm_picker_set_pixbuf_color (pixbuf, 254, 128, 0);

	/* set LAB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_lab"));
	gtk_label_set_label (label, "1, 2, 3");

	/* set XYZ */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_xyz"));
	gtk_label_set_label (label, "1, 2, 3");

	/* set RGB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_rgb"));
	gtk_label_set_label (label, "1, 2, 3");

	/* set XYZ */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);

}

/**
 * mcm_picker_close_cb:
 **/
static void
mcm_picker_close_cb (GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
}

/**
 * mcm_picker_help_cb:
 **/
static void
mcm_picker_help_cb (GtkWidget *widget, gpointer data)
{
	mcm_mate_help ("picker");
}

/**
 * mcm_picker_delete_event_cb:
 **/
static gboolean
mcm_picker_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	mcm_picker_close_cb (widget, data);
	return FALSE;
}

/**
 * mcm_picker_colorimeter_setup_ui:
 **/
static void
mcm_picker_colorimeter_setup_ui (McmColorimeter *colorimeter)
{
	gboolean present;
	gboolean supports_spot;
	gboolean ret;
	GtkWidget *widget;

	present = mcm_colorimeter_get_present (colorimeter);
	supports_spot = mcm_colorimeter_supports_spot (colorimeter);
	ret = (present && supports_spot);

	/* change the label */
	if (present && !supports_spot) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label), _("The attached colorimeter is not capable of reading a spot color."));
	} else if (!present) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label), _("No colorimeter is attached."));
	}

	/* hide some stuff */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	gtk_widget_set_sensitive (widget, ret);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_widget_set_sensitive (widget, ret);
	gtk_widget_set_visible (info_bar_hardware, !ret);
}

/**
 * mcm_picker_colorimeter_changed_cb:
 **/
static void
mcm_picker_colorimeter_changed_cb (McmColorimeter *colorimeter, gpointer user_data)
{
	mcm_picker_colorimeter_setup_ui (colorimeter);
}

/**
 * mcm_picker_message_received_cb
 **/
static UniqueResponse
mcm_picker_message_received_cb (UniqueApp *app, UniqueCommand command, UniqueMessageData *message_data, guint time_ms, gpointer data)
{
	GtkWindow *window;
	if (command == UNIQUE_ACTIVATE) {
		window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_picker"));
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
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	guint retval = 0;
	GError *error = NULL;
//	gboolean ret = FALSE;
	GMainLoop *loop;
	GtkWidget *main_window;
	GtkWidget *widget;
	UniqueApp *unique_app;
	guint xid = 0;
	McmColorimeter *colorimeter = NULL;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	/* setup translations */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup type system */
	g_type_init ();

	context = g_option_context_new (NULL);
	/* TRANSLATORS: tool that is used to pick colors */
	g_option_context_set_summary (context, _("MATE Color Manager Color Picker"));
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* block in a loop */
	loop = g_main_loop_new (NULL, FALSE);


	/* are we already activated? */
	unique_app = unique_app_new ("org.mate.ColorManager.Picker", NULL);
	if (unique_app_is_running (unique_app)) {
		egg_debug ("You have another instance running. This program will now close");
		unique_app_send_message (unique_app, UNIQUE_ACTIVATE, NULL);
		goto out;
	}
	g_signal_connect (unique_app, "message-received",
			  G_CALLBACK (mcm_picker_message_received_cb), NULL);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, MCM_DATA "/mcm-picker.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_picker"));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), MCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (mcm_picker_delete_event_cb), loop);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_picker_close_cb), loop);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_picker_help_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (mcm_picker_measure_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_widget_set_size_request (widget, 200, 200);

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   MCM_DATA G_DIR_SEPARATOR_S "icons");

	/* use the color device */
	colorimeter = mcm_colorimeter_new ();
	g_signal_connect (colorimeter, "changed", G_CALLBACK (mcm_picker_colorimeter_changed_cb), NULL);

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		mcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* use an info bar if there is no device, or the wrong device */
	info_bar_hardware = gtk_info_bar_new ();
	info_bar_hardware_label = gtk_label_new (NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_hardware), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_hardware));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_hardware_label);
	gtk_widget_show (info_bar_hardware_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox1"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_hardware, FALSE, FALSE, 0);

	/* disable some ui if no hardware */
	mcm_picker_colorimeter_setup_ui (colorimeter);

	/* wait */
	gtk_widget_show (main_window);
	g_main_loop_run (loop);

out:
	g_object_unref (unique_app);
	if (colorimeter != NULL)
		g_object_unref (colorimeter);
	if (builder != NULL)
		g_object_unref (builder);
	g_main_loop_unref (loop);
	return retval;
}

