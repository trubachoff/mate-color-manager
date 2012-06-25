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
 * SECTION:mcm-brightness
 * @short_description: Object to allow setting the brightness using mate-power-manager
 *
 * This object sets the laptop panel brightness using mate-power-manager.
 */

#include "config.h"

#include <glib-object.h>
#include <dbus/dbus-glib.h>

#include "mcm-brightness.h"

#include "egg-debug.h"

static void     mcm_brightness_finalize	(GObject     *object);

#define MCM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_BRIGHTNESS, McmBrightnessPrivate))

/**
 * McmBrightnessPrivate:
 *
 * Private #McmBrightness data
 **/
struct _McmBrightnessPrivate
{
	guint				 percentage;
	DBusGProxy			*proxy;
	DBusGConnection			*connection;
};

enum {
	PROP_0,
	PROP_PERCENTAGE,
	PROP_LAST
};

G_DEFINE_TYPE (McmBrightness, mcm_brightness, G_TYPE_OBJECT)

#define	GPM_DBUS_SERVICE		"org.mate.PowerManager"
#define	GPM_DBUS_INTERFACE_BACKLIGHT	"org.mate.PowerManager.Backlight"
#define	GPM_DBUS_PATH_BACKLIGHT		"/org/mate/PowerManager/Backlight"

/**
 * mcm_brightness_set_percentage:
 **/
gboolean
mcm_brightness_set_percentage (McmBrightness *brightness, guint percentage, GError **error)
{
	McmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;

	g_return_val_if_fail (MCM_IS_BRIGHTNESS (brightness), FALSE);
	g_return_val_if_fail (percentage <= 100, FALSE);

	/* are we connected */
	if (priv->proxy == NULL) {
		g_set_error_literal (error, 1, 0, "not connected to mate-power-manager");
		goto out;
	}

	/* set the brightness */
	ret = dbus_g_proxy_call (priv->proxy, "SetBrightness", error,
				 G_TYPE_UINT, percentage,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * mcm_brightness_get_percentage:
 **/
gboolean
mcm_brightness_get_percentage (McmBrightness *brightness, guint *percentage, GError **error)
{
	McmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;

	g_return_val_if_fail (MCM_IS_BRIGHTNESS (brightness), FALSE);

	/* are we connected */
	if (priv->proxy == NULL) {
		g_set_error_literal (error, 1, 0, "not connected to mate-power-manager");
		goto out;
	}

	/* get the brightness */
	ret = dbus_g_proxy_call (priv->proxy, "GetBrightness", error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &priv->percentage,
				 G_TYPE_INVALID);
	if (!ret)
		goto out;

	/* copy if set */
	if (percentage != NULL)
		*percentage = priv->percentage;
out:
	return ret;
}

/**
 * mcm_brightness_get_property:
 **/
static void
mcm_brightness_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmBrightness *brightness = MCM_BRIGHTNESS (object);
	McmBrightnessPrivate *priv = brightness->priv;

	switch (prop_id) {
	case PROP_PERCENTAGE:
		g_value_set_uint (value, priv->percentage);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_brightness_set_property:
 **/
static void
mcm_brightness_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_brightness_class_init:
 **/
static void
mcm_brightness_class_init (McmBrightnessClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_brightness_finalize;
	object_class->get_property = mcm_brightness_get_property;
	object_class->set_property = mcm_brightness_set_property;

	/**
	 * McmBrightness:percentage:
	 */
	pspec = g_param_spec_uint ("percentage", NULL, NULL,
				   0, 100, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PERCENTAGE, pspec);

	g_type_class_add_private (klass, sizeof (McmBrightnessPrivate));
}

/**
 * mcm_brightness_init:
 **/
static void
mcm_brightness_init (McmBrightness *brightness)
{
	GError *error = NULL;

	brightness->priv = MCM_BRIGHTNESS_GET_PRIVATE (brightness);
	brightness->priv->percentage = 0;

	/* get a session connection */
	brightness->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (brightness->priv->connection == NULL) {
		egg_warning ("Could not connect to DBUS daemon: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get a proxy to mate-power-manager */
	brightness->priv->proxy = dbus_g_proxy_new_for_name_owner (brightness->priv->connection,
								   GPM_DBUS_SERVICE, GPM_DBUS_PATH_BACKLIGHT, GPM_DBUS_INTERFACE_BACKLIGHT, &error);
	if (brightness->priv->proxy == NULL) {
		egg_warning ("Cannot connect, maybe mate-power-manager is not running: %s\n", error->message);
		g_error_free (error);
	}
}

/**
 * mcm_brightness_finalize:
 **/
static void
mcm_brightness_finalize (GObject *object)
{
	McmBrightness *brightness = MCM_BRIGHTNESS (object);
	McmBrightnessPrivate *priv = brightness->priv;

	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

	G_OBJECT_CLASS (mcm_brightness_parent_class)->finalize (object);
}

/**
 * mcm_brightness_new:
 *
 * Return value: a new McmBrightness object.
 **/
McmBrightness *
mcm_brightness_new (void)
{
	McmBrightness *brightness;
	brightness = g_object_new (MCM_TYPE_BRIGHTNESS, NULL);
	return MCM_BRIGHTNESS (brightness);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
mcm_brightness_test (EggTest *test)
{
	McmBrightness *brightness;
	gboolean ret;
	GError *error = NULL;
	guint orig_percentage;
	guint percentage;

	if (!egg_test_start (test, "McmBrightness"))
		return;

	/************************************************************/
	egg_test_title (test, "get a brightness object");
	brightness = mcm_brightness_new ();
	egg_test_assert (test, brightness != NULL);

	/************************************************************/
	egg_test_title (test, "get original brightness");
	ret = mcm_brightness_get_percentage (brightness, &orig_percentage, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get brightness: %s", error->message);

	/************************************************************/
	egg_test_title (test, "set the new brightness");
	ret = mcm_brightness_set_percentage (brightness, 10, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to set brightness: %s", error->message);

	/************************************************************/
	egg_test_title (test, "get the new brightness");
	ret = mcm_brightness_get_percentage (brightness, &percentage, &error);
	if (!ret)
		egg_test_failed (test, "failed to get brightness: %s", error->message);
	else if (percentage < 5 || percentage > 15)
		egg_test_failed (test, "percentage was not set: %i", percentage);
	else
		egg_test_success (test, NULL);

	/************************************************************/
	egg_test_title (test, "set back original brightness");
	ret = mcm_brightness_set_percentage (brightness, orig_percentage, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to set brightness: %s", error->message);

	g_object_unref (brightness);

	egg_test_end (test);
}
#endif

