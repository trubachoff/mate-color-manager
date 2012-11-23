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
 * SECTION:mcm-colorimeter
 * @short_description: Colorimeter device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gudev/gudev.h>
#include <gtk/gtk.h>

#include "mcm-colorimeter.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_colorimeter_finalize	(GObject     *object);

#define MCM_COLORIMETER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_COLORIMETER, McmColorimeterPrivate))

/**
 * McmColorimeterPrivate:
 *
 * Private #McmColorimeter data
 **/
struct _McmColorimeterPrivate
{
	gboolean			 present;
	gboolean			 supports_display;
	gboolean			 supports_projector;
	gboolean			 supports_printer;
	gboolean 			 supports_spot;
	gchar				*vendor;
	gchar				*model;
	GUdevClient			*client;
	McmColorimeterKind		 colorimeter_kind;
	gboolean			 shown_warning;
};

enum {
	PROP_0,
	PROP_PRESENT,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_COLORIMETER_KIND,
	PROP_SUPPORTS_DISPLAY,
	PROP_SUPPORTS_PROJECTOR,
	PROP_SUPPORTS_PRINTER,
	PROP_SUPPORTS_SPOT,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer mcm_colorimeter_object = NULL;

G_DEFINE_TYPE (McmColorimeter, mcm_colorimeter, G_TYPE_OBJECT)

/**
 * mcm_colorimeter_get_model:
 **/
const gchar *
mcm_colorimeter_get_model (McmColorimeter *colorimeter)
{
	return colorimeter->priv->model;
}

/**
 * mcm_colorimeter_get_vendor:
 **/
const gchar *
mcm_colorimeter_get_vendor (McmColorimeter *colorimeter)
{
	return colorimeter->priv->vendor;
}

/**
 * mcm_colorimeter_get_present:
 **/
gboolean
mcm_colorimeter_get_present (McmColorimeter *colorimeter)
{
	return colorimeter->priv->present;
}

/**
 * mcm_colorimeter_supports_display:
 **/
gboolean
mcm_colorimeter_supports_display (McmColorimeter *colorimeter)
{
	return colorimeter->priv->supports_display;
}

/**
 * mcm_colorimeter_supports_projector:
 **/
gboolean
mcm_colorimeter_supports_projector (McmColorimeter *colorimeter)
{
	return colorimeter->priv->supports_projector;
}

/**
 * mcm_colorimeter_supports_printer:
 **/
gboolean
mcm_colorimeter_supports_printer (McmColorimeter *colorimeter)
{
	return colorimeter->priv->supports_printer;
}

/**
 * mcm_colorimeter_supports_spot:
 **/
gboolean
mcm_colorimeter_supports_spot (McmColorimeter *colorimeter)
{
	return colorimeter->priv->supports_spot;
}

/**
 * mcm_colorimeter_get_kind:
 **/
McmColorimeterKind
mcm_colorimeter_get_kind (McmColorimeter *colorimeter)
{
	return colorimeter->priv->colorimeter_kind;
}

/**
 * mcm_colorimeter_get_property:
 **/
static void
mcm_colorimeter_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmColorimeter *colorimeter = MCM_COLORIMETER (object);
	McmColorimeterPrivate *priv = colorimeter->priv;

	switch (prop_id) {
	case PROP_PRESENT:
		g_value_set_boolean (value, priv->present);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_COLORIMETER_KIND:
		g_value_set_uint (value, priv->colorimeter_kind);
		break;
	case PROP_SUPPORTS_DISPLAY:
		g_value_set_uint (value, priv->supports_display);
		break;
	case PROP_SUPPORTS_PROJECTOR:
		g_value_set_uint (value, priv->supports_projector);
		break;
	case PROP_SUPPORTS_PRINTER:
		g_value_set_uint (value, priv->supports_printer);
		break;
	case PROP_SUPPORTS_SPOT:
		g_value_set_uint (value, priv->supports_spot);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_colorimeter_set_property:
 **/
static void
mcm_colorimeter_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_colorimeter_class_init:
 **/
static void
mcm_colorimeter_class_init (McmColorimeterClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_colorimeter_finalize;
	object_class->get_property = mcm_colorimeter_get_property;
	object_class->set_property = mcm_colorimeter_set_property;

	/**
	 * McmColorimeter:present:
	 */
	pspec = g_param_spec_boolean ("present", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRESENT, pspec);

	/**
	 * McmColorimeter:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	/**
	 * McmColorimeter:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * McmColorimeter:colorimeter-kind:
	 */
	pspec = g_param_spec_uint ("colorimeter-kind", NULL, NULL,
				   0, G_MAXUINT, MCM_COLORIMETER_KIND_UNKNOWN,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COLORIMETER_KIND, pspec);

	/**
	 * McmColorimeter:supports-display:
	 */
	pspec = g_param_spec_boolean ("supports-display", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_DISPLAY, pspec);

	/**
	 * McmColorimeter:supports-projector:
	 */
	pspec = g_param_spec_boolean ("supports-projector", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_PROJECTOR, pspec);


	/**
	 * McmColorimeter:supports-printer:
	 */
	pspec = g_param_spec_boolean ("supports-printer", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_PRINTER, pspec);

	/**
	* McmColorimeter:supports-spot:
	*/
	pspec = g_param_spec_boolean ("supports-spot", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_SPOT, pspec);

	/**
	 * McmColorimeter::changed:
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmColorimeterClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (McmColorimeterPrivate));
}

/**
 * mcm_colorimeter_kind_to_string:
 **/
const gchar *
mcm_colorimeter_kind_to_string (McmColorimeterKind colorimeter_kind)
{
	if (colorimeter_kind == MCM_COLORIMETER_KIND_HUEY)
		return "huey";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_COLOR_MUNKI)
		return "color-munki";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_SPYDER)
		return "spyder";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_DTP20)
		return "dtp20";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_DTP22)
		return "dtp22";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_DTP41)
		return "dtp41";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_DTP51)
		return "dtp51";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_SPECTRO_SCAN)
		return "spectro-scan";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_I1_PRO)
		return "i1-pro";
	if (colorimeter_kind == MCM_COLORIMETER_KIND_COLORIMTRE_HCFR)
		return "colorimtre-hcfr";
	return "unknown";
}

/**
 * mcm_colorimeter_kind_from_string:
 **/
McmColorimeterKind
mcm_colorimeter_kind_from_string (const gchar *colorimeter_kind)
{
	if (g_strcmp0 (colorimeter_kind, "huey") == 0)
		return MCM_COLORIMETER_KIND_HUEY;
	if (g_strcmp0 (colorimeter_kind, "color-munki") == 0)
		return MCM_COLORIMETER_KIND_COLOR_MUNKI;
	if (g_strcmp0 (colorimeter_kind, "spyder") == 0)
		return MCM_COLORIMETER_KIND_SPYDER;
	if (g_strcmp0 (colorimeter_kind, "dtp20") == 0)
		return MCM_COLORIMETER_KIND_DTP20;
	if (g_strcmp0 (colorimeter_kind, "dtp22") == 0)
		return MCM_COLORIMETER_KIND_DTP22;
	if (g_strcmp0 (colorimeter_kind, "dtp41") == 0)
		return MCM_COLORIMETER_KIND_DTP41;
	if (g_strcmp0 (colorimeter_kind, "dtp51") == 0)
		return MCM_COLORIMETER_KIND_DTP51;
	if (g_strcmp0 (colorimeter_kind, "spectro-scan") == 0)
		return MCM_COLORIMETER_KIND_SPECTRO_SCAN;
	if (g_strcmp0 (colorimeter_kind, "i1-pro") == 0)
		return MCM_COLORIMETER_KIND_I1_PRO;
	if (g_strcmp0 (colorimeter_kind, "colorimtre-hcfr") == 0)
		return MCM_COLORIMETER_KIND_COLORIMTRE_HCFR;
	return MCM_COLORIMETER_KIND_UNKNOWN;
}

/**
 * mcm_colorimeter_device_add:
 **/
static gboolean
mcm_colorimeter_device_add (McmColorimeter *colorimeter, GUdevDevice *device)
{
	gboolean ret;
	GtkWidget *dialog;
	const gchar *kind_str;
	McmColorimeterPrivate *priv = colorimeter->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "MCM_COLORIMETER");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("adding color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = TRUE;

	/* vendor */
	g_free (priv->vendor);
	priv->vendor = g_strdup (g_udev_device_get_property (device, "ID_VENDOR_FROM_DATABASE"));
	if (priv->vendor == NULL)
		priv->vendor = g_strdup (g_udev_device_get_property (device, "ID_VENDOR"));
	if (priv->vendor == NULL)
		priv->vendor = g_strdup (g_udev_device_get_sysfs_attr (device, "manufacturer"));

	/* model */
	g_free (priv->model);
	priv->model = g_strdup (g_udev_device_get_property (device, "ID_MODEL_FROM_DATABASE"));
	if (priv->model == NULL)
		priv->model = g_strdup (g_udev_device_get_property (device, "ID_MODEL"));
	if (priv->model == NULL)
		priv->model = g_strdup (g_udev_device_get_sysfs_attr (device, "product"));

	/* device support */
	priv->supports_display = g_udev_device_get_property_as_boolean (device, "MCM_TYPE_DISPLAY");
	priv->supports_projector = g_udev_device_get_property_as_boolean (device, "MCM_TYPE_PROJECTOR");
	priv->supports_printer = g_udev_device_get_property_as_boolean (device, "MCM_TYPE_PRINTER");
	priv->supports_spot = g_udev_device_get_property_as_boolean (device, "MCM_TYPE_SPOT");

	/* try to get type */
	kind_str = g_udev_device_get_property (device, "MCM_KIND");
	priv->colorimeter_kind = mcm_colorimeter_kind_from_string (kind_str);
	if (priv->colorimeter_kind == MCM_COLORIMETER_KIND_UNKNOWN) {
		egg_warning ("Failed to recognize color device: %s", priv->model);

		/* show dialog, in order to help the project */
		if (!priv->shown_warning) {
			dialog = gtk_message_dialog_new (NULL,
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_INFO,
							 GTK_BUTTONS_OK,
							 /* TRANSLATORS: this is when the device is not recognized */
							 _("Measuring instrument not recognized"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								  "Could not recognize attached measuring instrument '%s'. "
								  "It should work okay, but if you want to help the project, "
								  "please visit %s and supply the required information.",
								  priv->model, "http://live.gnome.org/MateColorManager/Help");
			gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			priv->shown_warning = TRUE;
		}
		priv->colorimeter_kind = MCM_COLORIMETER_KIND_UNKNOWN;
	}

	/* signal the addition */
	egg_debug ("emit: changed");
	g_signal_emit (colorimeter, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * mcm_colorimeter_device_remove:
 **/
static gboolean
mcm_colorimeter_device_remove (McmColorimeter *colorimeter, GUdevDevice *device)
{
	gboolean ret;
	McmColorimeterPrivate *priv = colorimeter->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "MCM_COLORIMETER");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("removing color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = FALSE;
	priv->supports_display = FALSE;
	priv->supports_projector = FALSE;
	priv->supports_printer = FALSE;
	priv->supports_spot = FALSE;

	/* vendor */
	g_free (priv->vendor);
	priv->vendor = NULL;

	/* model */
	g_free (priv->model);
	priv->model = NULL;

	/* signal the removal */
	egg_debug ("emit: changed");
	g_signal_emit (colorimeter, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * mcm_colorimeter_coldplug:
 **/
static gboolean
mcm_colorimeter_coldplug (McmColorimeter *colorimeter)
{
	GList *devices;
	GList *l;
	gboolean ret = FALSE;
	McmColorimeterPrivate *priv = colorimeter->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		ret = mcm_colorimeter_device_add (colorimeter, l->data);
		if (ret) {
			egg_debug ("found color management device");
			break;
		}
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return ret;
}

/**
 * mcm_colorimeter_uevent_cb:
 **/
static void
mcm_colorimeter_uevent_cb (GUdevClient *client, const gchar *action, GUdevDevice *device, McmColorimeter *colorimeter)
{
	egg_debug ("uevent %s", action);
	if (g_strcmp0 (action, "add") == 0) {
		mcm_colorimeter_device_add (colorimeter, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		mcm_colorimeter_device_remove (colorimeter, device);
	}
}

/**
 * mcm_colorimeter_init:
 **/
static void
mcm_colorimeter_init (McmColorimeter *colorimeter)
{
	const gchar *subsystems[] = {"usb", NULL};

	colorimeter->priv = MCM_COLORIMETER_GET_PRIVATE (colorimeter);
	colorimeter->priv->vendor = NULL;
	colorimeter->priv->model = NULL;
	colorimeter->priv->shown_warning = FALSE;
	colorimeter->priv->colorimeter_kind = MCM_COLORIMETER_KIND_UNKNOWN;

	/* use GUdev to find the calibration device */
	colorimeter->priv->client = g_udev_client_new (subsystems);
	g_signal_connect (colorimeter->priv->client, "uevent",
			  G_CALLBACK (mcm_colorimeter_uevent_cb), colorimeter);

	/* coldplug */
	mcm_colorimeter_coldplug (colorimeter);
}

/**
 * mcm_colorimeter_finalize:
 **/
static void
mcm_colorimeter_finalize (GObject *object)
{
	McmColorimeter *colorimeter = MCM_COLORIMETER (object);
	McmColorimeterPrivate *priv = colorimeter->priv;

	g_object_unref (priv->client);
	g_free (priv->vendor);
	g_free (priv->model);

	G_OBJECT_CLASS (mcm_colorimeter_parent_class)->finalize (object);
}

/**
 * mcm_colorimeter_new:
 *
 * Return value: a new McmColorimeter object.
 **/
McmColorimeter *
mcm_colorimeter_new (void)
{
	if (mcm_colorimeter_object != NULL) {
		g_object_ref (mcm_colorimeter_object);
	} else {
		mcm_colorimeter_object = g_object_new (MCM_TYPE_COLORIMETER, NULL);
		g_object_add_weak_pointer (mcm_colorimeter_object, &mcm_colorimeter_object);
	}
	return MCM_COLORIMETER (mcm_colorimeter_object);
}

