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

#include "mcm-utils.h"
#include "mcm-client.h"
#include "mcm-device-xrandr.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	gboolean login = FALSE;
	guint retval = 0;
	GError *error = NULL;
	GOptionContext *context;
	GPtrArray *array = NULL;
	guint i;
	McmClient *client = NULL;
	McmDevice *device;

	const GOptionEntry options[] = {
		{ "login", 'l', 0, G_OPTION_ARG_NONE, &login,
		  /* TRANSLATORS: we use this mode at login as we're sure there are no previous settings to clear */
		  _("Do not attempt to clear previously applied settings"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("mate-color-manager apply program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get devices */
	client = mcm_client_new ();
	ret = mcm_client_coldplug (client, MCM_CLIENT_COLDPLUG_XRANDR, &error);
	if (!ret) {
		egg_warning ("failed to get devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set for each output */
	array = mcm_client_get_devices (client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* optimize for login to save a few hundred ms */
		mcm_device_xrandr_set_remove_atom (MCM_DEVICE_XRANDR (device), !login);

		/* set gamma for device */
		egg_debug ("setting profiles on device: %s", mcm_device_get_id (device));
		ret = mcm_device_apply (device, &error);
		if (!ret) {
			retval = 1;
			egg_warning ("failed to set gamma: %s", error->message);
			g_error_free (error);
			break;
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (client != NULL)
		g_object_unref (client);
	return retval;
}

