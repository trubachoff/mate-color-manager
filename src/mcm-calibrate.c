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
 * SECTION:mcm-calibrate
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using CMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <tiff.h>
#include <tiffio.h>
#include <mateconf/mateconf-client.h>

#include "mcm-calibrate.h"
#include "mcm-dmi.h"
#include "mcm-device-xrandr.h"
#include "mcm-utils.h"
#include "mcm-brightness.h"
#include "mcm-colorimeter.h"
#include "mcm-calibrate-dialog.h"

#include "egg-debug.h"

static void     mcm_calibrate_finalize	(GObject     *object);

#define MCM_CALIBRATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_CALIBRATE, McmCalibratePrivate))

/**
 * McmCalibratePrivate:
 *
 * Private #McmCalibrate data
 **/
struct _McmCalibratePrivate
{
	McmDmi				*dmi;
	McmColorimeter			*colorimeter;
	McmCalibrateReferenceKind	 reference_kind;
	McmCalibrateDeviceKind		 calibrate_device_kind;
	McmCalibratePrintKind		 print_kind;
	McmCalibratePrecision		 precision;
	McmColorimeterKind		 colorimeter_kind;
	McmCalibrateDialog		*calibrate_dialog;
	McmDeviceKind			 device_kind;
	gchar				*output_name;
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*filename_result;
	gchar				*basename;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	gchar				*serial;
	gchar				*device;
	gchar				*working_path;
	MateConfClient			*mateconf_client;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_SERIAL,
	PROP_DEVICE,
	PROP_MANUFACTURER,
	PROP_REFERENCE_KIND,
	PROP_CALIBRATE_DEVICE_KIND,
	PROP_PRINT_KIND,
	PROP_DEVICE_KIND,
	PROP_COLORIMETER_KIND,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
	PROP_FILENAME_RESULT,
	PROP_WORKING_PATH,
	PROP_PRECISION,
	PROP_LAST
};

G_DEFINE_TYPE (McmCalibrate, mcm_calibrate, G_TYPE_OBJECT)

/**
 * mcm_calibrate_precision_from_string:
 **/
static McmCalibratePrecision
mcm_calibrate_precision_from_string (const gchar *string)
{
	if (g_strcmp0 (string, "short") == 0)
		return MCM_CALIBRATE_PRECISION_SHORT;
	if (g_strcmp0 (string, "normal") == 0)
		return MCM_CALIBRATE_PRECISION_NORMAL;
	if (g_strcmp0 (string, "long") == 0)
		return MCM_CALIBRATE_PRECISION_LONG;
	if (g_strcmp0 (string, "ask") == 0)
		return MCM_CALIBRATE_PRECISION_UNKNOWN;
	egg_warning ("failed to convert to precision: %s", string);
	return MCM_CALIBRATE_PRECISION_UNKNOWN;
}

/**
 * mcm_calibrate_get_model_fallback:
 **/
const gchar *
mcm_calibrate_get_model_fallback (McmCalibrate *calibrate)
{
	McmCalibratePrivate *priv = calibrate->priv;
	if (priv->model != NULL)
		return priv->model;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown model");
}

/**
 * mcm_calibrate_get_description_fallback:
 **/
const gchar *
mcm_calibrate_get_description_fallback (McmCalibrate *calibrate)
{
	McmCalibratePrivate *priv = calibrate->priv;
	if (priv->description != NULL)
		return priv->description;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown description");
}

/**
 * mcm_calibrate_get_manufacturer_fallback:
 **/
const gchar *
mcm_calibrate_get_manufacturer_fallback (McmCalibrate *calibrate)
{
	McmCalibratePrivate *priv = calibrate->priv;
	if (priv->manufacturer != NULL)
		return priv->manufacturer;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown manufacturer");
}

/**
 * mcm_calibrate_get_device_fallback:
 **/
const gchar *
mcm_calibrate_get_device_fallback (McmCalibrate *calibrate)
{
	McmCalibratePrivate *priv = calibrate->priv;
	if (priv->device != NULL)
		return priv->device;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown device");
}

/**
 * mcm_calibrate_get_time:
 **/
static gchar *
mcm_calibrate_get_time (void)
{
	gchar *text;
	time_t c_time;

	/* get the time now */
	time (&c_time);
	text = g_new0 (gchar, 255);

	/* format text */
	strftime (text, 254, "%H-%M-%S", localtime (&c_time));
	return text;
}

/**
 * mcm_calibrate_get_filename_result:
 **/
const gchar *
mcm_calibrate_get_filename_result (McmCalibrate *calibrate)
{
	return calibrate->priv->filename_result;
}

/**
 * mcm_calibrate_get_working_path:
 **/
const gchar *
mcm_calibrate_get_working_path (McmCalibrate *calibrate)
{
	return calibrate->priv->working_path;
}

/**
 * mcm_calibrate_set_basename:
 **/
static void
mcm_calibrate_set_basename (McmCalibrate *calibrate)
{
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *timespec = NULL;
	GDate *date = NULL;
	GString *basename;

	/* get device properties */
	g_object_get (calibrate,
		      "serial", &serial,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	timespec = mcm_calibrate_get_time ();

	/* form basename */
	basename = g_string_new ("MCM");
	if (manufacturer != NULL)
		g_string_append_printf (basename, " - %s", manufacturer);
	if (model != NULL)
		g_string_append_printf (basename, " - %s", model);
	if (serial != NULL)
		g_string_append_printf (basename, " - %s", serial);
	g_string_append_printf (basename, " (%04i-%02i-%02i)", date->year, date->month, date->day);

	/* maybe configure in MateConf? */
	if (0)
		g_string_append_printf (basename, " [%s]", timespec);

	/* save this */
	g_object_set (calibrate, "basename", basename->str, NULL);

	g_date_free (date);
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (timespec);
	g_string_free (basename, TRUE);
}

/**
 * mcm_calibrate_set_from_device:
 **/
gboolean
mcm_calibrate_set_from_device (McmCalibrate *calibrate, McmDevice *device, GError **error)
{
	gboolean lcd_internal;
	gboolean ret = TRUE;
	const gchar *native_device = NULL;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	const gchar *description = NULL;
	const gchar *serial = NULL;
	McmDeviceKind kind;
	McmCalibratePrivate *priv = calibrate->priv;

	/* get the device */
	kind = mcm_device_get_kind (device);
	serial = mcm_device_get_serial (device);
	model = mcm_device_get_model (device);
	description = mcm_device_get_title (device);
	manufacturer = mcm_device_get_manufacturer (device);

	/* if we're a laptop, maybe use the dmi data instead */
	if (kind == MCM_DEVICE_KIND_DISPLAY) {
		native_device = mcm_device_xrandr_get_native_device (MCM_DEVICE_XRANDR (device));
		lcd_internal = mcm_utils_output_is_lcd_internal (native_device);
		if (lcd_internal) {
			model = mcm_dmi_get_name (priv->dmi);
			manufacturer = mcm_dmi_get_vendor (priv->dmi);
		}
	}

	/* set the proper values */
	g_object_set (calibrate,
		      "device-kind", kind,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "serial", serial,
		      NULL);

	/* get a filename based on calibration attributes we've just set */
	mcm_calibrate_set_basename (calibrate);

	/* display specific properties */
	if (kind == MCM_DEVICE_KIND_DISPLAY) {
		native_device = mcm_device_xrandr_get_native_device (MCM_DEVICE_XRANDR (device));
		if (native_device == NULL) {
			g_set_error (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_INTERNAL,
				     "failed to get output");
			ret = FALSE;
			goto out;
		}
		g_object_set (calibrate,
			      "output-name", native_device,
			      NULL);
	}

out:
	return ret;
}

/**
 * mcm_calibrate_set_from_exif:
 **/
gboolean
mcm_calibrate_set_from_exif (McmCalibrate *calibrate, const gchar *filename, GError **error)
{
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	gchar *description = NULL;
	const gchar *serial = NULL;
	TIFF *tiff;
	gboolean ret = TRUE;

	/* open file */
	tiff = TIFFOpen (filename, "r");
	TIFFGetField (tiff,TIFFTAG_MAKE, &manufacturer);
	TIFFGetField (tiff,TIFFTAG_MODEL, &model);
	TIFFGetField (tiff,TIFFTAG_CAMERASERIALNUMBER, &serial);

	/* we failed to get data */
	if (manufacturer == NULL || model == NULL) {
		g_set_error (error,
			     MCM_CALIBRATE_ERROR,
			     MCM_CALIBRATE_ERROR_NO_DATA,
			     "failed to get EXIF data from TIFF");
		ret = FALSE;
		goto out;
	}

	/* do the best we can */
	description = g_strdup_printf ("%s - %s", manufacturer, model);

	/* only set what we've got, don't nuke perfectly good device data */
	if (model != NULL)
		g_object_set (calibrate, "model", model, NULL);
	if (description != NULL)
		g_object_set (calibrate, "description", description, NULL);
	if (manufacturer != NULL)
		g_object_set (calibrate, "manufacturer", manufacturer, NULL);
	if (serial != NULL)
		g_object_set (calibrate, "serial", serial, NULL);

out:
	g_free (description);
	TIFFClose (tiff);
	return ret;
}

/**
 * mcm_calibrate_get_display_kind:
 **/
static gboolean
mcm_calibrate_get_display_kind (McmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = TRUE;
	const gchar *title;
	const gchar *message;
	GtkResponseType response;
	McmCalibratePrivate *priv = calibrate->priv;

	/* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
	title = _("Could not detect screen type");

	/* TRANSLATORS: dialog message */
	message = _("Please indicate if the screen you are trying to profile is an LCD, CRT or a projector.");

	/* show the ui */
	mcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
	mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_DISPLAY_TYPE, title, message);
	mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		mcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose crt or lcd");
		ret = FALSE;
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "device-kind", &priv->calibrate_device_kind, NULL);

	/* can this device support projectors? */
	if (priv->calibrate_device_kind == MCM_CALIBRATE_DEVICE_KIND_PROJECTOR &&
	    !mcm_colorimeter_supports_projector (priv->colorimeter)) {
		/* TRANSLATORS: title, the hardware calibration device does not support projectors */
		title = _("Could not calibrate and profile using this color measuring instrument");

		/* TRANSLATORS: dialog message */
		message = _("This color measuring instrument is not designed to support calibration and profiling projectors.");

		/* ask the user again */
		mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
		mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
		response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
		if (response != GTK_RESPONSE_OK) {
			mcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error_literal (error,
					     MCM_CALIBRATE_ERROR,
					     MCM_CALIBRATE_ERROR_NO_SUPPORT,
					     "hardware not capable of profiling a projector");
			ret = FALSE;
			goto out;
		}
	}
out:
	return ret;
}

/**
 * mcm_calibrate_set_working_path:
 **/
static gboolean
mcm_calibrate_set_working_path (McmCalibrate *calibrate, GError **error)
{
	gboolean ret = FALSE;
	gchar *timespec = NULL;
	gchar *folder = NULL;
	McmCalibratePrivate *priv = calibrate->priv;

	/* remove old value */
	g_free (priv->working_path);

	/* use the basename and the timespec */
	timespec = mcm_calibrate_get_time ();
	folder = g_strjoin (" - ", priv->basename, timespec, NULL);
	priv->working_path = g_build_filename (g_get_user_config_dir (), "mate-color-manager", "calibration", folder, NULL);
	ret = mcm_utils_mkdir_with_parents (priv->working_path, error);
	g_free (timespec);
	g_free (folder);
	return ret;
}


/**
 * mcm_calibrate_get_precision:
 **/
static McmCalibratePrecision
mcm_calibrate_get_precision (McmCalibrate *calibrate, GError **error)
{
	McmCalibratePrecision precision = MCM_CALIBRATE_PRECISION_UNKNOWN;
	const gchar *title;
	GString *string;
	GtkResponseType response;
	McmCalibratePrivate *priv = calibrate->priv;

	string = g_string_new ("");

	/* TRANSLATORS: dialog title */
	title = _("Profile Precision");

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append (string, _("A higher precision profile provides higher accuracy in color matching but requires more time for reading the color patches."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s", _("For a typical workflow, a normal precision profile is sufficient."));

	/* printer specific options */
	if (priv->device_kind == MCM_DEVICE_KIND_PRINTER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "\n%s", _("The high precision profile also requires more paper and printer ink."));
	}

	/* push new messages into the UI */
	mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_PRECISION, title, string->str);
	mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		mcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose precision type and ask is specified in MateConf");
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "precision", &precision, NULL);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return precision;
}

/**
 * mcm_calibrate_display:
 **/
gboolean
mcm_calibrate_display (McmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	McmCalibrateClass *klass = MCM_CALIBRATE_GET_CLASS (calibrate);
	gboolean ret = TRUE;
	const gchar *hardware_device;
	gboolean ret_tmp;
	GString *string = NULL;
	McmBrightness *brightness = NULL;
	guint percentage = G_MAXUINT;
	GtkResponseType response;
	GError *error_tmp = NULL;
	gchar *precision = NULL;
	McmCalibratePrivate *priv = calibrate->priv;

	/* coldplug source */
	if (priv->output_name == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_INTERNAL,
				     "no output name set");
		goto out;
	}

	/* set the per-profile filename */
	ret = mcm_calibrate_set_working_path (calibrate, error);
	if (!ret)
		goto out;

	/* coldplug source */
	if (klass->calibrate_display == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* get calibration device model */
	hardware_device = mcm_colorimeter_get_model (priv->colorimeter);

	/* get device, harder */
	if (hardware_device == NULL) {
		/* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated */
		hardware_device = g_strdup (_("Custom"));
	}

	/* set display specific properties */
	g_object_set (calibrate,
		      "device", hardware_device,
		      NULL);

	/* this wasn't previously set */
	if (priv->calibrate_device_kind == MCM_CALIBRATE_DEVICE_KIND_UNKNOWN) {
		ret = mcm_calibrate_get_display_kind (calibrate, window, error);
		if (!ret)
			goto out;
	}

	/* get default precision */
	precision = mateconf_client_get_string (priv->mateconf_client, MCM_SETTINGS_CALIBRATION_LENGTH, NULL);
	priv->precision = mcm_calibrate_precision_from_string (precision);
	if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
		priv->precision = mcm_calibrate_get_precision (calibrate, error);
		if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
			ret = FALSE;
			goto out;
		}
	}

	/* show a warning for external monitors */
	ret = mcm_utils_output_is_lcd_internal (priv->output_name);
	if (!ret) {
		string = g_string_new ("");

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Before calibrating the display, it is recommended to configure your display with the following settings to get optimal results."));

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n\n", _("You may want to consult the owner's manual for your display on how to achieve these settings."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Reset your display to the factory defaults."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Disable dynamic contrast if your display has this feature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s", _("Configure your display with custom color settings and ensure the RGB channels are set to the same values."));

		/* TRANSLATORS: dialog message, addition to bullet item */
		g_string_append_printf (string, " %s\n", _("If custom color is not available then use a 6500K color temperature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Adjust the display brightness to a comfortable level for prolonged viewing."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "\n%s\n", _("For best results, the display should have been powered for at least 15 minutes before starting the calibration."));

		/* TRANSLATORS: window title */
		mcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
		mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_GENERIC, _("Display setup"), string->str);
		mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
		response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
		if (response != GTK_RESPONSE_OK) {
			mcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error_literal (error,
					     MCM_CALIBRATE_ERROR,
					     MCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not follow calibration steps");
			ret = FALSE;
			goto out;
		}
	}

	/* create new helper objects */
	brightness = mcm_brightness_new ();

	/* if we are an internal LCD, we need to set the brightness to maximum */
	ret = mcm_utils_output_is_lcd_internal (priv->output_name);
	if (ret) {
		/* get the old brightness so we can restore state */
		ret = mcm_brightness_get_percentage (brightness, &percentage, &error_tmp);
		if (!ret) {
			egg_warning ("failed to get brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}

		/* set the new brightness */
		ret = mcm_brightness_set_percentage (brightness, 100, &error_tmp);
		if (!ret) {
			egg_warning ("failed to set brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}
	}

	/* proxy */
	ret = klass->calibrate_display (calibrate, window, error);
out:
	/* restore brightness */
	if (percentage != G_MAXUINT) {
		/* set the new brightness */
		ret_tmp = mcm_brightness_set_percentage (brightness, percentage, &error_tmp);
		if (!ret_tmp) {
			egg_warning ("failed to restore brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error = NULL;
		}
	}

	if (brightness != NULL)
		g_object_unref (brightness);
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (precision);
	return ret;
}


/**
 * mcm_calibrate_device_get_reference_image:
 **/
static gchar *
mcm_calibrate_device_get_reference_image (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog. A calibration target image is the
	 * aquired image of the calibration target, e.g. an image file that looks
	 * a bit like this: http://www.colorreference.de/targets/target.jpg */
	dialog = gtk_file_chooser_dialog_new (_("Select calibration target image"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "image/tiff");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported images files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * mcm_calibrate_device_get_reference_data:
 **/
static gchar *
mcm_calibrate_device_get_reference_data (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select CIE reference values file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/x-it87");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_filter_add_pattern (filter, "*.TXT");
	gtk_file_filter_add_pattern (filter, "*.it8");
	gtk_file_filter_add_pattern (filter, "*.IT8");

	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("CIE values"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * mcm_calibrate_get_device_for_it8_file:
 **/
static gchar *
mcm_calibrate_get_device_for_it8_file (const gchar *filename)
{
	gchar *contents = NULL;
	gchar **lines = NULL;
	gchar *device = NULL;
	gboolean ret;
	GError *error = NULL;
	guint i;

	/* get contents */
	ret = g_file_get_contents (filename, &contents, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get contents: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* split */
	lines = g_strsplit (contents, "\n", 15);
	for (i=0; lines[i] != NULL; i++) {
		if (!g_str_has_prefix (lines[i], "ORIGINATOR"))
			continue;

		/* copy, without the header or double quotes */
		device = g_strdup (lines[i]+12);
		g_strdelimit (device, "\"", '\0');
		break;
	}
out:
	g_free (contents);
	g_strfreev (lines);
	return device;
}

/**
 * mcm_calibrate_file_chooser_get_working_path:
 **/
static gchar *
mcm_calibrate_file_chooser_get_working_path (McmCalibrate *calibrate, GtkWindow *window)
{
	GtkWidget *dialog;
	gchar *current_folder;
	gchar *working_path = NULL;

	/* start in the correct place */
	current_folder = g_build_filename (g_get_user_config_dir (), "mate-color-manager", "calibration", NULL);

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Open"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), current_folder);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		working_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	g_free (current_folder);
	return working_path;
}

/**
 * mcm_calibrate_printer:
 **/
gboolean
mcm_calibrate_printer (McmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	const gchar *title;
	const gchar *message;
	gchar *ptr;
	GtkWindow *window_tmp;
	GtkResponseType response;
	gchar *precision = NULL;
	McmCalibrateClass *klass = MCM_CALIBRATE_GET_CLASS (calibrate);
	McmCalibratePrivate *priv = calibrate->priv;

	/* TRANSLATORS: title, you can profile all at once, or in steps */
	title = _("Please choose a profiling mode");

	/* TRANSLATORS: dialog message. Test patches are pages of colored squares
	 * that are printed with a printer, and then read in with a calibration
	 * device to create a profile */
	message = _("Please indicate if you want to profile a local printer, generate some test patches, or profile using existing test patches.");

	/* push new messages into the UI */
	mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_PRINT_MODE, title, message);
	mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		mcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose print mode");
		ret = FALSE;
		goto out;
	}

	/* get default precision */
	precision = mateconf_client_get_string (priv->mateconf_client, MCM_SETTINGS_CALIBRATION_LENGTH, NULL);
	priv->precision = mcm_calibrate_precision_from_string (precision);
	if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
		priv->precision = mcm_calibrate_get_precision (calibrate, error);
		if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
			ret = FALSE;
			goto out;
		}
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "print-kind", &priv->print_kind, NULL);

	if (priv->print_kind != MCM_CALIBRATE_PRINT_KIND_ANALYZE) {
		/* set the per-profile filename */
		ret = mcm_calibrate_set_working_path (calibrate, error);
		if (!ret)
			goto out;
	} else {

		/* remove previously set value (if any) */
		g_free (priv->working_path);
		priv->working_path = NULL;

		/* get from the user */
		window_tmp = mcm_calibrate_dialog_get_window (priv->calibrate_dialog);
		priv->working_path = mcm_calibrate_file_chooser_get_working_path (calibrate, window_tmp);
		if (priv->working_path == NULL) {
			g_set_error_literal (error,
					     MCM_CALIBRATE_ERROR,
					     MCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not choose folder");
			ret = FALSE;
			goto out;
		}

		/* reprogram the basename */
		g_free (priv->basename);
		priv->basename = g_path_get_basename (priv->working_path);

		/* remove the timespec */
		ptr = g_strrstr (priv->basename, " - ");
		if (ptr != NULL)
			ptr[0] = '\0';
	}

	/* coldplug source */
	if (klass->calibrate_printer == NULL) {
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_printer (calibrate, window, error);
out:
	g_free (precision);
	return ret;
}

/**
 * mcm_calibrate_device:
 **/
gboolean
mcm_calibrate_device (McmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gboolean has_shared_targets;
	gchar *reference_image = NULL;
	gchar *reference_data = NULL;
	gchar *device = NULL;
	const gchar *directory;
	GString *string;
	GtkResponseType response;
	GtkWindow *window_tmp;
	gchar *precision = NULL;
#ifdef MCM_USE_PACKAGEKIT
	GtkWidget *dialog;
#endif
	const gchar *title;
	McmCalibratePrivate *priv = calibrate->priv;
	McmCalibrateClass *klass = MCM_CALIBRATE_GET_CLASS (calibrate);

	string = g_string_new ("");

	/* install shared-color-targets package */
	has_shared_targets = g_file_test ("/usr/share/shared-color-targets", G_FILE_TEST_IS_DIR);
	if (!has_shared_targets) {
#ifdef MCM_USE_PACKAGEKIT
		/* ask the user to confirm */
		dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
						 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
						 _("Install missing files?"));

		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s ", _("Common color target files are not installed on this computer."));
		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s\n\n", _("Color target files are needed to convert the image to a color profile."));
		/* TRANSLATORS: dialog message, asking if it's okay to install them */
		g_string_append_printf (string, "%s\n\n", _("Do you want them to be installed?"));
		/* TRANSLATORS: dialog message, if the user has the target file on a CDROM then there's no need for this package */
		g_string_append_printf (string, "%s", _("If you already have the correct file, you can skip this step."));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
		gtk_window_set_icon_name (GTK_WINDOW (dialog), MCM_STOCK_ICON);
		/* TRANSLATORS: button, skip installing a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
		/* TRANSLATORS: button, install a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		/* only install if the user wanted to */
		if (response == GTK_RESPONSE_YES)
			has_shared_targets = mcm_utils_install_package (MCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS, window);
#else
		egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
#endif
	}

	/* TRANSLATORS: this is the window title for when the user selects the calibration target.
	 * A calibration target is an accuratly printed grid of colors, for instance:
	 * the IT 8.7 targets available here: http://www.targets.coloraid.de/ */
	title = _("Please select a calibration target");
	g_string_set_size (string, 0);

	/* TRANSLATORS: dialog message, preface. A calibration target looks like
	 * this: http://www.colorreference.de/targets/target.jpg */
	g_string_append_printf (string, "%s\n", _("Before profiling the device, you have to manually capture an image of a calibration target and save it as a TIFF image file."));

	/* scanner specific options */
	if (priv->device_kind == MCM_DEVICE_KIND_SCANNER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the contrast and brightness are not changed and color correction profiles are not applied."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "%s\n", _("The device sensor should have been cleaned prior to scanning and the output file resolution should be at least 200dpi."));
	}

	/* camera specific options */
	if (priv->device_kind == MCM_DEVICE_KIND_CAMERA) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the white-balance has not been modified by the camera and that the lens is clean."));
	}

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "\n%s\n", _("For best results, the reference target should also be less than two years old."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s\n", _("Please select the calibration target type which corresponds to your reference file."));

	/* push new messages into the UI */
	mcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
	mcm_calibrate_dialog_show (priv->calibrate_dialog, MCM_CALIBRATE_DIALOG_TAB_TARGET_TYPE, title, string->str);
	mcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	mcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = mcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		mcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose calibration target type");
		ret = FALSE;
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "reference-kind", &priv->reference_kind, NULL);

	/* get default precision */
	precision = mateconf_client_get_string (priv->mateconf_client, MCM_SETTINGS_CALIBRATION_LENGTH, NULL);
	priv->precision = mcm_calibrate_precision_from_string (precision);
	if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
		priv->precision = mcm_calibrate_get_precision (calibrate, error);
		if (priv->precision == MCM_CALIBRATE_PRECISION_UNKNOWN) {
			ret = FALSE;
			goto out;
		}
	}

	/* get scanned image */
	directory = g_get_home_dir ();
	window_tmp = mcm_calibrate_dialog_get_window (priv->calibrate_dialog);
	reference_image = mcm_calibrate_device_get_reference_image (directory, window_tmp);
	if (reference_image == NULL) {
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get reference image");
		ret = FALSE;
		goto out;
	}

	/* use the exif data if there is any present */
	ret = mcm_calibrate_set_from_exif (calibrate, reference_image, NULL);
	if (!ret)
		egg_debug ("no EXIF data, so using device attributes");

	/* get reference data */
	directory = has_shared_targets ? "/usr/share/color/targets" : "/media";
	reference_data = mcm_calibrate_device_get_reference_data (directory, window_tmp);
	if (reference_data == NULL) {
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get reference target");
		ret = FALSE;
		goto out;
	}

	/* use the ORIGINATOR in the it8 file */
	device = mcm_calibrate_get_device_for_it8_file (reference_data);
	if (device == NULL)
		device = g_strdup ("IT8.7");

	/* set the calibration parameters */
	g_object_set (calibrate,
		      "filename-source", reference_image,
		      "filename-reference", reference_data,
		      "device", device,
		      NULL);

	/* set the per-profile filename */
	ret = mcm_calibrate_set_working_path (calibrate, error);
	if (!ret)
		goto out;

	/* coldplug source */
	if (klass->calibrate_device == NULL) {
		g_set_error_literal (error,
				     MCM_CALIBRATE_ERROR,
				     MCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_device (calibrate, window, error);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (precision);
	g_free (device);
	g_free (reference_image);
	g_free (reference_data);
	return ret;
}

/**
 * mcm_calibrate_get_property:
 **/
static void
mcm_calibrate_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmCalibrate *calibrate = MCM_CALIBRATE (object);
	McmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_REFERENCE_KIND:
		g_value_set_uint (value, priv->reference_kind);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	case PROP_PRINT_KIND:
		g_value_set_uint (value, priv->print_kind);
		break;
	case PROP_CALIBRATE_DEVICE_KIND:
		g_value_set_uint (value, priv->calibrate_device_kind);
		break;
	case PROP_COLORIMETER_KIND:
		g_value_set_uint (value, priv->colorimeter_kind);
		break;
	case PROP_OUTPUT_NAME:
		g_value_set_string (value, priv->output_name);
		break;
	case PROP_FILENAME_SOURCE:
		g_value_set_string (value, priv->filename_source);
		break;
	case PROP_FILENAME_REFERENCE:
		g_value_set_string (value, priv->filename_reference);
		break;
	case PROP_FILENAME_RESULT:
		g_value_set_string (value, priv->filename_result);
		break;
	case PROP_BASENAME:
		g_value_set_string (value, priv->basename);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_WORKING_PATH:
		g_value_set_string (value, priv->working_path);
		break;
	case PROP_PRECISION:
		g_value_set_uint (value, priv->precision);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_calibrate_guess_kind:
 **/
static void
mcm_calibrate_guess_kind (McmCalibrate *calibrate)
{
	gboolean ret;
	McmCalibratePrivate *priv = calibrate->priv;

	/* guess based on the output name */
	ret = mcm_utils_output_is_lcd_internal (priv->output_name);
	if (ret)
		priv->calibrate_device_kind = MCM_CALIBRATE_DEVICE_KIND_LCD;
}

/**
 * mcm_prefs_colorimeter_changed_cb:
 **/
static void
mcm_prefs_colorimeter_changed_cb (McmColorimeter *_colorimeter, McmCalibrate *calibrate)
{
	calibrate->priv->colorimeter_kind = mcm_colorimeter_get_kind (_colorimeter);
	g_object_notify (G_OBJECT (calibrate), "colorimeter-kind");
}

/**
 * mcm_calibrate_set_property:
 **/
static void
mcm_calibrate_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmCalibrate *calibrate = MCM_CALIBRATE (object);
	McmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_OUTPUT_NAME:
		g_free (priv->output_name);
		priv->output_name = g_strdup (g_value_get_string (value));
		mcm_calibrate_guess_kind (calibrate);
		break;
	case PROP_FILENAME_SOURCE:
		g_free (priv->filename_source);
		priv->filename_source = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_REFERENCE:
		g_free (priv->filename_reference);
		priv->filename_reference = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_RESULT:
		g_free (priv->filename_result);
		priv->filename_result = g_strdup (g_value_get_string (value));
		break;
	case PROP_BASENAME:
		g_free (priv->basename);
		priv->basename = g_strdup (g_value_get_string (value));
		mcm_utils_ensure_sensible_filename (priv->basename);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
		break;
	case PROP_SERIAL:
		g_free (priv->serial);
		priv->serial = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_strdup (g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE_KIND:
		priv->device_kind = g_value_get_uint (value);
		break;
	case PROP_WORKING_PATH:
		g_free (priv->working_path);
		priv->working_path = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_calibrate_class_init:
 **/
static void
mcm_calibrate_class_init (McmCalibrateClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_calibrate_finalize;
	object_class->get_property = mcm_calibrate_get_property;
	object_class->set_property = mcm_calibrate_set_property;

	/**
	 * McmCalibrate:reference-kind:
	 */
	pspec = g_param_spec_uint ("reference-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	/**
	 * McmCalibrate:calibrate-device-kind:
	 */
	pspec = g_param_spec_uint ("calibrate-device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CALIBRATE_DEVICE_KIND, pspec);

	/**
	 * McmCalibrate:print-kind:
	 */
	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRINT_KIND, pspec);

	/**
	 * McmCalibrate:device-kind:
	 */
	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	/**
	 * McmCalibrate:colorimeter-kind:
	 */
	pspec = g_param_spec_uint ("colorimeter-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COLORIMETER_KIND, pspec);

	/**
	 * McmCalibrate:output-name:
	 */
	pspec = g_param_spec_string ("output-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_NAME, pspec);

	/**
	 * McmCalibrate:filename-source:
	 */
	pspec = g_param_spec_string ("filename-source", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_SOURCE, pspec);

	/**
	 * McmCalibrate:filename-reference:
	 */
	pspec = g_param_spec_string ("filename-reference", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_REFERENCE, pspec);

	/**
	 * McmCalibrate:filename-result:
	 */
	pspec = g_param_spec_string ("filename-result", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_RESULT, pspec);

	/**
	 * McmCalibrate:basename:
	 */
	pspec = g_param_spec_string ("basename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BASENAME, pspec);

	/**
	 * McmCalibrate:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * McmCalibrate:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * McmCalibrate:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * McmCalibrate:device:
	 */
	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	/**
	 * McmCalibrate:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * McmCalibrate:working-path:
	 */
	pspec = g_param_spec_string ("working-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WORKING_PATH, pspec);

	/**
	 * McmCalibrate:precision:
	 */
	pspec = g_param_spec_uint ("precision", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRECISION, pspec);

	g_type_class_add_private (klass, sizeof (McmCalibratePrivate));
}

/**
 * mcm_calibrate_init:
 **/
static void
mcm_calibrate_init (McmCalibrate *calibrate)
{
	calibrate->priv = MCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->filename_source = NULL;
	calibrate->priv->filename_reference = NULL;
	calibrate->priv->filename_result = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->manufacturer = NULL;
	calibrate->priv->model = NULL;
	calibrate->priv->description = NULL;
	calibrate->priv->device = NULL;
	calibrate->priv->serial = NULL;
	calibrate->priv->working_path = NULL;
	calibrate->priv->calibrate_device_kind = MCM_CALIBRATE_DEVICE_KIND_UNKNOWN;
	calibrate->priv->print_kind = MCM_CALIBRATE_PRINT_KIND_UNKNOWN;
	calibrate->priv->reference_kind = MCM_CALIBRATE_REFERENCE_KIND_UNKNOWN;
	calibrate->priv->precision = MCM_CALIBRATE_PRECISION_UNKNOWN;
	calibrate->priv->colorimeter = mcm_colorimeter_new ();
	calibrate->priv->dmi = mcm_dmi_new ();
	calibrate->priv->calibrate_dialog = mcm_calibrate_dialog_new ();

	// FIXME: this has to be per-run specific
	calibrate->priv->working_path = g_strdup ("/tmp");

	/* use MateConf to get defaults */
	calibrate->priv->mateconf_client = mateconf_client_get_default ();

	/* coldplug, and watch for changes */
	calibrate->priv->colorimeter_kind = mcm_colorimeter_get_kind (calibrate->priv->colorimeter);
	g_signal_connect (calibrate->priv->colorimeter, "changed", G_CALLBACK (mcm_prefs_colorimeter_changed_cb), calibrate);
}

/**
 * mcm_calibrate_finalize:
 **/
static void
mcm_calibrate_finalize (GObject *object)
{
	McmCalibrate *calibrate = MCM_CALIBRATE (object);
	McmCalibratePrivate *priv = calibrate->priv;

	g_free (priv->filename_source);
	g_free (priv->filename_reference);
	g_free (priv->filename_result);
	g_free (priv->output_name);
	g_free (priv->basename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->description);
	g_free (priv->device);
	g_free (priv->serial);
	g_free (priv->working_path);
	g_signal_handlers_disconnect_by_func (calibrate->priv->colorimeter, G_CALLBACK (mcm_prefs_colorimeter_changed_cb), calibrate);
	g_object_unref (priv->colorimeter);
	g_object_unref (priv->dmi);
	g_object_unref (priv->calibrate_dialog);
	g_object_unref (priv->mateconf_client);

	G_OBJECT_CLASS (mcm_calibrate_parent_class)->finalize (object);
}

/**
 * mcm_calibrate_new:
 *
 * Return value: a new McmCalibrate object.
 **/
McmCalibrate *
mcm_calibrate_new (void)
{
	McmCalibrate *calibrate;
	calibrate = g_object_new (MCM_TYPE_CALIBRATE, NULL);
	return MCM_CALIBRATE (calibrate);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
mcm_calibrate_test (EggTest *test)
{
	McmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;

	if (!egg_test_start (test, "McmCalibrate"))
		return;

	/************************************************************/
	egg_test_title (test, "get a calibrate object");
	calibrate = mcm_calibrate_new ();
	egg_test_assert (test, calibrate != NULL);

	/************************************************************/
	egg_test_title (test, "calibrate display manually");
	filename = egg_test_get_data_file ("test.tif");
	ret = mcm_calibrate_set_from_exif (MCM_CALIBRATE(calibrate), filename, &error);
	if (!ret)
		egg_test_failed (test, "error: %s", error->message);
	else if (g_strcmp0 (mcm_calibrate_get_model_fallback (calibrate), "NIKON D60") != 0)
		egg_test_failed (test, "got model: %s", mcm_calibrate_get_model_fallback (calibrate));
	else if (g_strcmp0 (mcm_calibrate_get_manufacturer_fallback (calibrate), "NIKON CORPORATION") != 0)
		egg_test_failed (test, "got manufacturer: %s", mcm_calibrate_get_manufacturer_fallback (calibrate));
	else
		egg_test_success (test, NULL);

	g_object_unref (calibrate);
	g_free (filename);

	egg_test_end (test);
}
#endif

