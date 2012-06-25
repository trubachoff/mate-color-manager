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

/**
 * SECTION:mcm-screen
 * @short_description: For querying data about PackageKit
 *
 * A GObject to use for accessing PackageKit asynchronously.
 */

#include "config.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "egg-debug.h"

#include "mcm-screen.h"

static void     mcm_screen_finalize	(GObject     *object);

#define MCM_SCREEN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_SCREEN, McmScreenPrivate))

#define MCM_SCREEN_DBUS_METHOD_TIMEOUT		1500 /* ms */

/**
 * McmScreenPrivate:
 *
 * Private #McmScreen data
 **/
struct _McmScreenPrivate
{
	MateRRScreen			*rr_screen;
};

enum {
	SIGNAL_OUTPUTS_CHANGED,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer mcm_screen_object = NULL;

G_DEFINE_TYPE (McmScreen, mcm_screen, G_TYPE_OBJECT)

/**
 * mcm_screen_randr_event_cb:
 **/
static void
mcm_screen_randr_event_cb (MateRRScreen *rr_screen, McmScreen *screen)
{
	egg_debug ("emit outputs-changed");
	g_signal_emit (screen, signals[SIGNAL_OUTPUTS_CHANGED], 0);
}

/**
 * mcm_screen_ensure_instance:
 **/
static gboolean
mcm_screen_ensure_instance (McmScreen *screen, GError **error)
{
	gboolean ret = TRUE;
	McmScreenPrivate *priv = screen->priv;

	/* already got */
	if (priv->rr_screen != NULL)
		goto out;

	/* get screen (this is slow) */
	priv->rr_screen = mate_rr_screen_new (gdk_screen_get_default (), (MateRRScreenChanged) mcm_screen_randr_event_cb, screen, error);
	if (priv->rr_screen == NULL) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * mcm_screen_get_output_by_name:
 **/
MateRROutput *
mcm_screen_get_output_by_name (McmScreen *screen, const gchar *output_name, GError **error)
{
	gboolean ret;
	MateRROutput *output = NULL;
	McmScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (MCM_IS_SCREEN (screen), NULL);
	g_return_val_if_fail (output_name != NULL, NULL);

	/* get instance */
	ret = mcm_screen_ensure_instance (screen, error);
	if (!ret)
		goto out;

	/* get output */
	output = mate_rr_screen_get_output_by_name (priv->rr_screen, output_name);
	if (output == NULL) {
		g_set_error (error, 1, 0, "no output for name: %s", output_name);
		goto out;
	}
out:
	return output;
}

/**
 * mcm_screen_get_outputs:
 **/
MateRROutput **
mcm_screen_get_outputs (McmScreen *screen, GError **error)
{
	gboolean ret;
	MateRROutput **outputs = NULL;
	McmScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (MCM_IS_SCREEN (screen), NULL);

	/* get instance */
	ret = mcm_screen_ensure_instance (screen, error);
	if (!ret)
		goto out;

	/* get output */
	outputs = mate_rr_screen_list_outputs (priv->rr_screen);
	if (outputs == NULL) {
		g_set_error (error, 1, 0, "no outputs for screen");
		goto out;
	}
out:
	return outputs;
}

/**
 * mcm_screen_class_init:
 **/
static void
mcm_screen_class_init (McmScreenClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_screen_finalize;

	/**
	 * McmScreen::outputs-changed:
	 **/
	signals[SIGNAL_OUTPUTS_CHANGED] =
		g_signal_new ("outputs-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmScreenClass, outputs_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (McmScreenPrivate));
}

/**
 * mcm_screen_init:
 **/
static void
mcm_screen_init (McmScreen *screen)
{
	screen->priv = MCM_SCREEN_GET_PRIVATE (screen);
}

/**
 * mcm_screen_finalize:
 **/
static void
mcm_screen_finalize (GObject *object)
{
	McmScreen *screen = MCM_SCREEN (object);
	McmScreenPrivate *priv = screen->priv;

	if (priv->rr_screen != NULL)
		mate_rr_screen_destroy (priv->rr_screen);

	G_OBJECT_CLASS (mcm_screen_parent_class)->finalize (object);
}

/**
 * mcm_screen_new:
 *
 * Return value: a new McmScreen object.
 **/
McmScreen *
mcm_screen_new (void)
{
	if (mcm_screen_object != NULL) {
		g_object_ref (mcm_screen_object);
	} else {
		mcm_screen_object = g_object_new (MCM_TYPE_SCREEN, NULL);
		g_object_add_weak_pointer (mcm_screen_object, &mcm_screen_object);
	}
	return MCM_SCREEN (mcm_screen_object);
}

