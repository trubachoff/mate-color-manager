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

/**
 * SECTION:mcm-xserver
 * @short_description: Object to interact with the XServer
 *
 * This object talks to the currently running X Server.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "mcm-xserver.h"

#include "egg-debug.h"

static void     mcm_xserver_finalize	(GObject     *object);

#define MCM_XSERVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_XSERVER, McmXserverPrivate))

/**
 * McmXserverPrivate:
 *
 * Private #McmXserver data
 **/
struct _McmXserverPrivate
{
	gchar				*display_name;
	GdkDisplay			*display_gdk;
	GdkWindow			*window_gdk;
	Display				*display;
	Window				 window;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

static gpointer mcm_xserver_object = NULL;

G_DEFINE_TYPE (McmXserver, mcm_xserver, G_TYPE_OBJECT)

/**
 * mcm_xserver_get_root_window_profile_data:
 *
 * @xserver: a valid %McmXserver instance
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Gets the ICC profile data from the XServer.
 **/
gboolean
mcm_xserver_get_root_window_profile_data (McmXserver *xserver, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gchar *data_tmp = NULL;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XGetWindowProperty (priv->display, priv->window, atom, 0, G_MAXLONG, False, XA_CARDINAL,
				 &type, &format, &nitems, &bytes_after, (void*) &data_tmp);
	gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "%s atom has not been set", atom_name);
		goto out;
	}

	/* allocate the data using Glib, rather than asking the user to use XFree */
	*data = g_new0 (guint8, nitems);
	memcpy (*data, data_tmp, nitems);

	/* copy the length */
	if (length != NULL)
		*length = nitems;

	/* success */
	ret = TRUE;
out:
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * mcm_xserver_set_root_window_profile:
 * @xserver: a valid %McmXserver instance
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 **/
gboolean
mcm_xserver_set_root_window_profile (McmXserver *xserver, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("setting root window ICC profile atom from %s", filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = mcm_xserver_set_root_window_profile_data (xserver, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * mcm_xserver_set_root_window_profile_data:
 * @xserver: a valid %McmXserver instance
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 **/
gboolean
mcm_xserver_set_root_window_profile_data (McmXserver *xserver, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	Atom atom = None;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XChangeProperty (priv->display, priv->window, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, length);
	gdk_error_trap_pop ();

	/* for some reason this fails with BadRequest, but actually sets the value */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * mcm_xserver_set_protocol_version:
 * @xserver: a valid %McmXserver instance
 * @major: the major version
 * @minor: the minor version
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC Profiles in X supported version to the XServer.
 **/
gboolean
mcm_xserver_set_protocol_version (McmXserver *xserver, guint major, guint minor, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	Atom atom = None;
	guint data;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE_IN_X_VERSION";
	data = major * 100 + minor * 1;

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XChangeProperty (priv->display, priv->window, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) &data, 1);
	gdk_error_trap_pop ();

	/* for some reason this fails with BadRequest, but actually sets the value */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * mcm_xserver_remove_protocol_version:
 * @xserver: a valid %McmXserver instance
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Removes the ICC profile version data from the XServer.
 **/
gboolean
mcm_xserver_remove_protocol_version (McmXserver *xserver, GError **error)
{
	const gchar *atom_name;
	Atom atom = None;
	gint rc;
	gboolean ret = TRUE;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);

	egg_debug ("removing root window ICC profile atom");

	/* get the atom name */
	atom_name = "_ICC_PROFILE_IN_X_VERSION";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XDeleteProperty(priv->display, priv->window, atom);
	gdk_error_trap_pop ();

	/* this fails with BadRequest if the atom was not set */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to delete %s root window atom with rc %i", atom_name, rc);
		goto out;
	}
out:
	return ret;
}

/**
 * mcm_xserver_get_protocol_version:
 *
 * @xserver: a valid %McmXserver instance
 * @major: the major version
 * @minor: the minor version
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Gets the ICC profile data from the XServer.
 **/
gboolean
mcm_xserver_get_protocol_version (McmXserver *xserver, guint *major, guint *minor, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gchar *data_tmp;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (major != NULL, FALSE);
	g_return_val_if_fail (minor != NULL, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE_IN_X_VERSION";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XGetWindowProperty (priv->display, priv->window, atom, 0, G_MAXLONG, False, XA_CARDINAL,
				 &type, &format, &nitems, &bytes_after, (unsigned char **) &data_tmp);
	gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "%s atom has not been set", atom_name);
		goto out;
	}

	/* set total */
	*major = (guint) data_tmp[0] / 100;
	*minor = (guint) data_tmp[0] % 100;

	/* success */
	ret = TRUE;
out:
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * mcm_xserver_remove_root_window_profile:
 * @xserver: a valid %McmXserver instance
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Removes the ICC profile data from the XServer.
 **/
gboolean
mcm_xserver_remove_root_window_profile (McmXserver *xserver, GError **error)
{
	const gchar *atom_name;
	Atom atom = None;
	gint rc;
	gboolean ret = TRUE;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);

	egg_debug ("removing root window ICC profile atom");

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XDeleteProperty(priv->display, priv->window, atom);
	gdk_error_trap_pop ();

	/* this fails with BadRequest if the atom was not set */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to delete %s root window atom with rc %i", atom_name, rc);
		goto out;
	}
out:
	return ret;
}

/**
 * mcm_xserver_get_output_profile_data:
 *
 * @xserver: a valid %McmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Gets the ICC profile data from the specified output.
 **/
gboolean
mcm_xserver_get_output_profile_data (McmXserver *xserver, const gchar *output_name, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gchar *data_tmp = NULL;
	gint format;
	gint rc = -1;
	gulong bytes_after;
	gulong nitems = 0;
	Atom atom = None;
	Atom type;
	gint i;
	XRROutputInfo *output;
	XRRScreenResources *resources = NULL;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	resources = XRRGetScreenResources (priv->display, priv->window);
	for (i = 0; i < resources->noutput; i++) {
		output = XRRGetOutputInfo (priv->display, resources, resources->outputs[i]);
		if (g_strcmp0 (output->name, output_name) == 0) {
			rc = XRRGetOutputProperty (priv->display, resources->outputs[i],
						   atom, 0, ~0, False, False, AnyPropertyType, &type, &format, &nitems, &bytes_after, (unsigned char **) &data_tmp);
			egg_debug ("found %s, got %i bytes", output_name, (guint) nitems);
		}
		XRRFreeOutputInfo (output);
	}
	gdk_error_trap_pop ();

	/* we failed to match any outputs */
	if (rc == -1) {
		g_set_error (error, 1, 0, "failed to match adaptor %s", output_name);
		goto out;
	}

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "%s atom has not been set", atom_name);
		goto out;
	}

	/* allocate the data using Glib, rather than asking the user to use XFree */
	*data = g_new0 (guint8, nitems);
	memcpy (*data, data_tmp, nitems);

	/* copy the length */
	if (length != NULL)
		*length = nitems;

	/* success */
	ret = TRUE;
out:
	if (resources != NULL)
		XRRFreeScreenResources (resources);
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * mcm_xserver_set_output_profile:
 * @xserver: a valid %McmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the specified output.
 **/
gboolean
mcm_xserver_set_output_profile (McmXserver *xserver, const gchar *output_name, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("setting output '%s' ICC profile atom from %s", output_name, filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = mcm_xserver_set_output_profile_data (xserver, output_name, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * mcm_xserver_set_output_profile_data:
 * @xserver: a valid %McmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the specified output.
 **/
gboolean
mcm_xserver_set_output_profile_data (McmXserver *xserver, const gchar *output_name, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	gint i;
	Atom atom = None;
	XRROutputInfo *output;
	XRRScreenResources *resources = NULL;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	resources = XRRGetScreenResources (priv->display, priv->window);
	for (i = 0; i < resources->noutput; ++i) {
		output = XRRGetOutputInfo (priv->display, resources, resources->outputs[i]);
		if (g_strcmp0 (output->name, output_name) == 0) {
			egg_debug ("found %s, setting %i bytes", output_name, (guint) length);
			XRRChangeOutputProperty (priv->display, resources->outputs[i], atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, (gint)length);
		}
		XRRFreeOutputInfo (output);
	}
	rc = gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set output %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (resources != NULL)
		XRRFreeScreenResources (resources);
	return ret;
}

/**
 * mcm_xserver_remove_output_profile:
 * @xserver: a valid %McmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the specified output.
 **/
gboolean
mcm_xserver_remove_output_profile (McmXserver *xserver, const gchar *output_name, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	gint i;
	Atom atom = None;
	XRROutputInfo *output;
	XRRScreenResources *resources = NULL;
	McmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (MCM_IS_XSERVER (xserver), FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	resources = XRRGetScreenResources (priv->display, priv->window);
	for (i = 0; i < resources->noutput; ++i) {
		output = XRRGetOutputInfo (priv->display, resources, resources->outputs[i]);
		if (g_strcmp0 (output->name, output_name) == 0) {
			egg_debug ("found %s, removing atom", output_name);
			XRRDeleteOutputProperty (priv->display, resources->outputs[i], atom);
		}
		XRRFreeOutputInfo (output);
	}
	rc = gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to remove output %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (resources != NULL)
		XRRFreeScreenResources (resources);
	return ret;
}

/**
 * mcm_xserver_get_property:
 **/
static void
mcm_xserver_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmXserver *xserver = MCM_XSERVER (object);
	McmXserverPrivate *priv = xserver->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_value_set_string (value, priv->display_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_xserver_set_property:
 **/
static void
mcm_xserver_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmXserver *xserver = MCM_XSERVER (object);
	McmXserverPrivate *priv = xserver->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_free (priv->display_name);
		priv->display_name = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_xserver_class_init:
 **/
static void
mcm_xserver_class_init (McmXserverClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_xserver_finalize;
	object_class->get_property = mcm_xserver_get_property;
	object_class->set_property = mcm_xserver_set_property;

	/**
	 * McmXserver:display-name:
	 */
	pspec = g_param_spec_string ("display-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

	g_type_class_add_private (klass, sizeof (McmXserverPrivate));
}

/**
 * mcm_xserver_init:
 **/
static void
mcm_xserver_init (McmXserver *xserver)
{
	xserver->priv = MCM_XSERVER_GET_PRIVATE (xserver);
	xserver->priv->display_name = NULL;

	/* get defaults for single screen */
	xserver->priv->display_gdk = gdk_display_get_default ();
	xserver->priv->window_gdk = gdk_get_default_root_window ();
	xserver->priv->display = GDK_DISPLAY_XDISPLAY (xserver->priv->display_gdk);
	xserver->priv->window = GDK_WINDOW_XID (xserver->priv->window_gdk);
}

/**
 * mcm_xserver_finalize:
 **/
static void
mcm_xserver_finalize (GObject *object)
{
	McmXserver *xserver = MCM_XSERVER (object);
	McmXserverPrivate *priv = xserver->priv;

	g_free (priv->display_name);

	G_OBJECT_CLASS (mcm_xserver_parent_class)->finalize (object);
}

/**
 * mcm_xserver_new:
 *
 * Return value: a new McmXserver object.
 **/
McmXserver *
mcm_xserver_new (void)
{
	if (mcm_xserver_object != NULL) {
		g_object_ref (mcm_xserver_object);
	} else {
		mcm_xserver_object = g_object_new (MCM_TYPE_XSERVER, NULL);
		g_object_add_weak_pointer (mcm_xserver_object, &mcm_xserver_object);
	}
	return MCM_XSERVER (mcm_xserver_object);
}

