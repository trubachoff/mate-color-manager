/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <math.h>
#include <glib/gstdio.h>

#include "mcm-brightness.h"
#include "mcm-calibrate.h"
#include "mcm-calibrate-dialog.h"
#include "mcm-calibrate-manual.h"
#include "mcm-cie-widget.h"
#include "mcm-client.h"
#include "mcm-clut.h"
#include "mcm-device.h"
#include "mcm-device-udev.h"
#include "mcm-device-xrandr.h"
#include "mcm-dmi.h"
#include "mcm-edid.h"
#include "mcm-exif.h"
#include "mcm-gamma-widget.h"
#include "mcm-image.h"
#include "mcm-print.h"
#include "mcm-profile.h"
#include "mcm-profile-store.h"
#include "mcm-profile-lcms1.h"
#include "mcm-tables.h"
#include "mcm-trc-widget.h"
#include "mcm-utils.h"
#include "mcm-xyz.h"
#include "mcm-profile.h"
#include "mcm-xyz.h"

/**
 * mcm_test_get_data_file:
 **/
static gchar *
mcm_test_get_data_file (const gchar *filename)
{
	gboolean ret;
	gchar *full;

	/* check to see if we are being run in the build root */
	full = g_build_filename ("..", "data", "tests", filename, NULL);
	ret = g_file_test (full, G_FILE_TEST_EXISTS);
	if (ret)
		return full;
	g_free (full);

	/* check to see if we are being run in make check */
	full = g_build_filename ("..", "..", "data", "tests", filename, NULL);
	ret = g_file_test (full, G_FILE_TEST_EXISTS);
	if (ret)
		return full;
	g_free (full);
	return NULL;
}

static void
mcm_test_assert_basename (const gchar *filename1, const gchar *filename2)
{
	gchar *basename1;
	gchar *basename2;
	basename1 = g_path_get_basename (filename1);
	basename2 = g_path_get_basename (filename2);
	g_assert_cmpstr (basename1, ==, basename2);
	g_free (basename1);
	g_free (basename2);
}

static void
mcm_test_brightness_func (void)
{
	McmBrightness *brightness;
	gboolean ret;
	GError *error = NULL;
	guint orig_percentage;
	guint percentage;

	brightness = mcm_brightness_new ();
	g_assert (brightness != NULL);

	ret = mcm_brightness_get_percentage (brightness, &orig_percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = mcm_brightness_set_percentage (brightness, 10, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = mcm_brightness_get_percentage (brightness, &percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (percentage, >, 5);
	g_assert_cmpint (percentage, <, 15);

	ret = mcm_brightness_set_percentage (brightness, orig_percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (brightness);
}

static void
mcm_test_calibrate_func (void)
{
	McmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;

	calibrate = mcm_calibrate_new ();
	g_assert (calibrate != NULL);

	/* calibrate display manually */
	filename = mcm_test_get_data_file ("test.tif");
	ret = mcm_calibrate_set_from_exif (MCM_CALIBRATE(calibrate), filename, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (mcm_calibrate_get_model_fallback (calibrate), ==, "NIKON D60");
	g_assert_cmpstr (mcm_calibrate_get_manufacturer_fallback (calibrate), ==, "NIKON CORPORATION");

	g_object_unref (calibrate);
	g_free (filename);
}

static void
mcm_test_calibrate_dialog_func (void)
{
	McmCalibrateDialog *calibrate_dialog;
	calibrate_dialog = mcm_calibrate_dialog_new ();
	g_assert (calibrate_dialog != NULL);
	g_object_unref (calibrate_dialog);
}

static void
mcm_test_calibrate_manual_func (void)
{
	McmCalibrateManual *calibrate;
	gboolean ret;
	GError *error = NULL;

	calibrate = mcm_calibrate_manual_new ();
	g_assert (calibrate != NULL);

	/* set to avoid a critical warning */
	g_object_set (calibrate,
		      "output-name", "lvds1",
		      NULL);

	/* calibrate display manually */
	ret = mcm_calibrate_display (MCM_CALIBRATE(calibrate), NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (calibrate);
}

static void
mcm_test_cie_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	McmProfile *profile;
	McmXyz *white;
	McmXyz *red;
	McmXyz *green;
	McmXyz *blue;
	gint response;
	gchar *filename_profile;
	gchar *filename_image;
	GFile *file = NULL;

	widget = mcm_cie_widget_new ();
	g_assert (widget != NULL);

	filename_image = mcm_test_get_data_file ("cie-widget.png");
	g_assert ((filename_image != NULL));

	filename_profile = mcm_test_get_data_file ("bluish.icc");
	g_assert ((filename_profile != NULL));

	profile = mcm_profile_default_new ();
	file = g_file_new_for_path (filename_profile);
	mcm_profile_parse (profile, file, NULL);
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);
	g_object_unref (file);

	g_object_set (widget,
		      "red", red,
		      "green", green,
		      "blue", blue,
		      "white", white,
		      NULL);

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does CIE widget match\nthe picture below?");
	image = gtk_image_new_from_file (filename_image);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_object_unref (profile);
	g_object_unref (white);
	g_object_unref (red);
	g_object_unref (green);
	g_object_unref (blue);
	g_free (filename_profile);
	g_free (filename_image);
}

static void
mcm_test_clut_func (void)
{
	McmClut *clut;
	GPtrArray *array;
	const McmClutData *data;

	clut = mcm_clut_new ();
	g_assert (clut != NULL);

	/* set some initial properties */
	g_object_set (clut,
		      "size", 3,
		      "contrast", 100.0f,
		      "brightness", 0.0f,
		      NULL);

	array = mcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 0);
	g_assert_cmpint (data->green, ==, 0);
	g_assert_cmpint (data->blue, ==, 0);

	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 32767);
	g_assert_cmpint (data->green, ==, 32767);
	g_assert_cmpint (data->blue, ==, 32767);

	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 65535);
	g_assert_cmpint (data->green, ==, 65535);
	g_assert_cmpint (data->blue, ==, 65535);

	g_ptr_array_unref (array);

	/* set some initial properties */
	g_object_set (clut,
		      "contrast", 99.0f,
		      "brightness", 0.0f,
		      NULL);

	array = mcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 0);
	g_assert_cmpint (data->green, ==, 0);
	g_assert_cmpint (data->blue, ==, 0);
	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 32439);
	g_assert_cmpint (data->green, ==, 32439);
	g_assert_cmpint (data->blue, ==, 32439);
	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 64879);
	g_assert_cmpint (data->green, ==, 64879);
	g_assert_cmpint (data->blue, ==, 64879);

	g_ptr_array_unref (array);

	/* set some initial properties */
	g_object_set (clut,
		      "contrast", 100.0f,
		      "brightness", 1.0f,
		      NULL);

	array = mcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 655);
	g_assert_cmpint (data->green, ==, 655);
	g_assert_cmpint (data->blue, ==, 655);
	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 33094);
	g_assert_cmpint (data->green, ==, 33094);
	g_assert_cmpint (data->blue, ==, 33094);
	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 65535);
	g_assert_cmpint (data->green, ==, 65535);
	g_assert_cmpint (data->blue, ==, 65535);

	g_ptr_array_unref (array);

	g_object_unref (clut);
}

static guint _changes = 0;
static GMainLoop *_loop = NULL;

static void
mcm_device_test_changed_cb (McmDevice *device)
{
	_changes++;
	g_main_loop_quit (_loop);
}

static void
mcm_test_device_func (void)
{
	McmDevice *device;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;
	McmProfile *profile;
	const gchar *profile_filename;
	GPtrArray *profiles;
	gchar *data;
	gchar **split;
	gchar *contents;
	gchar *icc_filename1;
	gchar *icc_filename2;

	/* get test files */
	icc_filename1 = mcm_test_get_data_file ("bluish.icc");
	icc_filename2 = mcm_test_get_data_file ("AdobeGammaTest.icm");

	device = mcm_device_udev_new ();
	g_assert (device != NULL);

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (mcm_device_test_changed_cb), NULL);

	g_assert_cmpint (_changes, ==, 0);

	g_assert (mcm_device_kind_from_string ("scanner") == MCM_DEVICE_KIND_SCANNER);
	g_assert (mcm_device_kind_from_string ("xxx") == MCM_DEVICE_KIND_UNKNOWN);

	g_assert_cmpstr (mcm_device_kind_to_string (MCM_DEVICE_KIND_SCANNER), ==, "scanner");
	g_assert_cmpstr (mcm_device_kind_to_string (MCM_DEVICE_KIND_UNKNOWN), ==, "unknown");

	/* set some properties */
	g_object_set (device,
		      "kind", MCM_DEVICE_KIND_SCANNER,
		      "id", "sysfs_dummy_device",
		      "connected", FALSE,
		      "virtual", FALSE,
		      "serial", "0123456789",
		      "colorspace", MCM_COLORSPACE_RGB,
		      NULL);

	_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (_loop);
	/* TODO: time out of loop */

	g_assert_cmpint (_changes, ==, 1);

	mcm_device_set_connected (device, TRUE);
	g_main_loop_run (_loop);
	g_assert_cmpint (_changes, ==, 2);

	g_assert_cmpstr (mcm_device_get_id (device), ==, "sysfs_dummy_device");

	/* ensure the file is nuked */
	filename = mcm_utils_get_default_config_location ();
	g_unlink (filename);

	/* missing file */
	ret = mcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (mcm_device_get_default_profile_filename (device), ==, NULL);

	/* empty file that exists */
	g_file_set_contents (filename, "", -1, NULL);

	/* load from empty file */
	ret = mcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set default file */
	contents = g_strdup_printf ("[sysfs_dummy_device]\n"
				    "title=Canon - CanoScan\n"
				    "type=scanner\n"
				    "profile=%s;%s\n",
				    icc_filename1, icc_filename2);
	g_file_set_contents (filename, contents, -1, NULL);

	ret = mcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_main_loop_run (_loop);
	/* TODO: time out of loop */

	g_assert_cmpint (_changes, ==, 3);

	/* get some properties */
	profile_filename = mcm_device_get_default_profile_filename (device);
	mcm_test_assert_basename (profile_filename, icc_filename1);
	profiles = mcm_device_get_profiles (device);
	g_assert_cmpint (profiles->len, ==, 2);

	profile = g_ptr_array_index (profiles, 0);
	g_assert (profile != NULL);
	mcm_test_assert_basename (mcm_profile_get_filename (profile), icc_filename1);

	profile = g_ptr_array_index (profiles, 1);
	g_assert (profile != NULL);
	mcm_test_assert_basename (mcm_profile_get_filename (profile), icc_filename2);

	/* set some properties */
	mcm_device_set_default_profile_filename (device, icc_filename1);

	/* ensure the file is nuked, again */
	g_unlink (filename);

	/* save to empty file */
	ret = mcm_device_save (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = g_file_get_contents (filename, &data, NULL, NULL);
	g_assert (ret);

	split = g_strsplit (data, "\n", -1);
	g_assert_cmpstr (split[1], ==, "[sysfs_dummy_device]");
	g_assert_cmpstr (split[5], ==, "serial=0123456789");
	g_assert_cmpstr (split[6], ==, "type=scanner");
	g_assert_cmpstr (split[7], ==, "colorspace=rgb");
	g_free (data);
	g_strfreev (split);

	/* ensure the file is nuked, in case we are running in distcheck */
	g_unlink (filename);
	g_main_loop_unref (_loop);
	g_free (filename);
	g_free (icc_filename1);
	g_free (icc_filename2);
	g_free (contents);

	g_object_unref (device);
}

static void
mcm_test_dmi_func (void)
{
	McmDmi *dmi;

	dmi = mcm_dmi_new ();
	g_assert (dmi != NULL);
	g_assert (mcm_dmi_get_name (dmi) != NULL);
	g_assert (mcm_dmi_get_version (dmi) != NULL);
	g_assert (mcm_dmi_get_vendor (dmi) != NULL);
	g_object_unref (dmi);
}

typedef struct {
	const gchar *monitor_name;
	const gchar *vendor_name;
	const gchar *serial_number;
	const gchar *eisa_id;
	const gchar *pnp_id;
	guint width;
	guint height;
	gfloat gamma;
} McmEdidTestData;

static void
mcm_test_edid_test_parse_edid_file (McmEdid *edid, const gchar *datafile, McmEdidTestData *test_data)
{
	gchar *filename;
	gchar *data;
	gfloat mygamma;
	gboolean ret;
	GError *error = NULL;

	filename = mcm_test_get_data_file (datafile);
	ret = g_file_get_contents (filename, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = mcm_edid_parse (edid, (const guint8 *) data, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (mcm_edid_get_monitor_name (edid), ==, test_data->monitor_name);

	g_assert_cmpstr (mcm_edid_get_vendor_name (edid), ==, test_data->vendor_name);

	g_assert_cmpstr (mcm_edid_get_serial_number (edid), ==, test_data->serial_number);

	g_assert_cmpstr (mcm_edid_get_eisa_id (edid), ==, test_data->eisa_id);

	g_assert_cmpstr (mcm_edid_get_pnp_id (edid), ==, test_data->pnp_id);

	g_assert_cmpint (mcm_edid_get_height (edid), ==, test_data->height);

	g_assert_cmpint (mcm_edid_get_width (edid), ==, test_data->width);

	mygamma = mcm_edid_get_gamma (edid);
	g_assert_cmpfloat (mygamma, >=, test_data->gamma - 0.01);
	g_assert_cmpfloat (mygamma, <, test_data->gamma + 0.01);

	g_free (filename);
	g_free (data);
}

static void
mcm_test_edid_func (void)
{
	McmEdid *edid;
	McmEdidTestData test_data;

	edid = mcm_edid_new ();
	g_assert (edid != NULL);

	/* LG 21" LCD panel */
	test_data.monitor_name = "L225W";
	test_data.vendor_name = "Goldstar Company Ltd";
	test_data.serial_number = "34398";
	test_data.eisa_id = NULL;
	test_data.pnp_id = "GSM";
	test_data.height = 30;
	test_data.width = 47;
	test_data.gamma = 2.2f;
	mcm_test_edid_test_parse_edid_file (edid, "LG-L225W-External.bin", &test_data);

	/* Lenovo T61 Intel Panel */
	test_data.monitor_name = NULL;
	test_data.vendor_name = "IBM France";
	test_data.serial_number = NULL;
	test_data.eisa_id = "LTN154P2-L05";
	test_data.pnp_id = "IBM";
	test_data.height = 21;
	test_data.width = 33;
	test_data.gamma = 2.2f;
	mcm_test_edid_test_parse_edid_file (edid, "Lenovo-T61-Internal.bin", &test_data);

	g_object_unref (edid);
}

static void
mcm_test_exif_func (void)
{
	McmExif *exif;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;
	GFile *file;

	exif = mcm_exif_new ();
	g_assert (exif != NULL);

	/* TIFF */
	filename = mcm_test_get_data_file ("test.tif");
	file = g_file_new_for_path (filename);
	ret = mcm_exif_parse (exif, file, &error);
	g_free (filename);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (mcm_exif_get_model (exif), ==, "NIKON D60");
	g_assert_cmpstr (mcm_exif_get_manufacturer (exif), ==, "NIKON CORPORATION");
	g_assert_cmpstr (mcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (mcm_exif_get_device_kind (exif), ==, MCM_DEVICE_KIND_CAMERA);

	/* JPG */
	filename = mcm_test_get_data_file ("test.jpg");
	file = g_file_new_for_path (filename);
	ret = mcm_exif_parse (exif, file, &error);
	g_free (filename);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (mcm_exif_get_model (exif), ==, "NIKON D60");
	g_assert_cmpstr (mcm_exif_get_manufacturer (exif), ==, "NIKON CORPORATION");
	g_assert_cmpstr (mcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (mcm_exif_get_device_kind (exif), ==, MCM_DEVICE_KIND_CAMERA);

	/* RAW */
	filename = mcm_test_get_data_file ("test.kdc");
	file = g_file_new_for_path (filename);
	ret = mcm_exif_parse (exif, file, &error);
	g_free (filename);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (mcm_exif_get_model (exif), ==, "Eastman Kodak Company");
	g_assert_cmpstr (mcm_exif_get_manufacturer (exif), ==, "Kodak Digital Science DC50 Zoom Camera");
	g_assert_cmpstr (mcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (mcm_exif_get_device_kind (exif), ==, MCM_DEVICE_KIND_CAMERA);

	/* PNG */
	filename = mcm_test_get_data_file ("test.png");
	file = g_file_new_for_path (filename);
	ret = mcm_exif_parse (exif, file, &error);
	g_free (filename);
	g_object_unref (file);
	g_assert_error (error, MCM_EXIF_ERROR, MCM_EXIF_ERROR_NO_SUPPORT);
	g_assert (!ret);

	g_object_unref (exif);
}

static void
mcm_test_gamma_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gint response;
	gchar *filename_image;

	widget = mcm_gamma_widget_new ();
	g_assert (widget != NULL);

	g_object_set (widget,
		      "color-light", 0.5f,
		      "color-dark", 0.0f,
		      "color-red", 0.25f,
		      "color-green", 0.25f,
		      "color-blue", 0.25f,
		      NULL);

	filename_image = mcm_test_get_data_file ("gamma-widget.png");
	g_assert ((filename_image != NULL));

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does GAMMA widget match\nthe picture below?");
	image = gtk_image_new_from_file (filename_image);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_free (filename_image);
}

static gchar *
mcm_image_test_get_ibmt61_profile ()
{
	gchar *filename;
	gchar *profile_base64 = NULL;
	gchar *contents = NULL;
	gboolean ret;
	gsize length;
	GError *error = NULL;

	/* get test file */
	filename = mcm_test_get_data_file ("ibm-t61.icc");

	/* get contents */
	ret = g_file_get_contents (filename, &contents, &length, &error);
	if (!ret) {
		g_warning ("failed to get contents: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* encode */
	profile_base64 = g_base64_encode ((const guchar *)contents, length);
out:
	g_free (contents);
	g_free (filename);
	return profile_base64;
}

static void
mcm_test_image_func (void)
{
	McmImage *image;
	GtkWidget *image_test;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gint response;
	gchar *filename_widget;
	gchar *filename_test;
	gchar *profile_base64;

	image = mcm_image_new ();
	g_assert (image != NULL);

	filename_widget = mcm_test_get_data_file ("image-widget.png");
	gtk_image_set_from_file (GTK_IMAGE(image), filename_widget);

	filename_test = mcm_test_get_data_file ("image-widget-good.png");
	g_assert ((filename_test != NULL));

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does color-corrected image match\nthe picture below?");
	image_test = gtk_image_new_from_file (filename_test);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), GTK_WIDGET(image), TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image_test, TRUE, TRUE, 12);
	gtk_widget_set_size_request (GTK_WIDGET(image), 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (GTK_WIDGET(image));
	gtk_widget_show (image_test);

	g_object_set (image,
		      "use-embedded-icc-profile", TRUE,
		      "output-icc-profile", NULL,
		      NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));
	g_free (filename_test);

	filename_test = mcm_test_get_data_file ("image-widget-nonembed.png");
	gtk_image_set_from_file (GTK_IMAGE(image_test), filename_test);
	g_object_set (image,
		      "use-embedded-icc-profile", FALSE,
		      NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));
	g_free (filename_test);

	profile_base64 = mcm_image_test_get_ibmt61_profile ();
	g_assert ((profile_base64 != NULL));

	filename_test = mcm_test_get_data_file ("image-widget-output.png");
	gtk_image_set_from_file (GTK_IMAGE(image_test), filename_test);
	g_object_set (image,
		      "use-embedded-icc-profile", TRUE,
		      "output-icc-profile", profile_base64,
		      NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));
	g_free (filename_test);

	gtk_widget_destroy (dialog);

	g_free (profile_base64);
	g_free (filename_widget);
}

static GPtrArray *
mcm_print_test_render_cb (McmPrint *print,  GtkPageSetup *page_setup, gpointer user_data, GError **error)
{
	GPtrArray *filenames;
	filenames = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (filenames, mcm_test_get_data_file ("image-widget-nonembed.png"));
	g_ptr_array_add (filenames, mcm_test_get_data_file ("image-widget-good.png"));
	return filenames;
}

static void
mcm_test_print_func (void)
{
	gboolean ret;
	GError *error = NULL;
	McmPrint *print;

	print = mcm_print_new ();

	ret = mcm_print_with_render_callback (print, NULL, mcm_print_test_render_cb, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (print);
}

typedef struct {
	const gchar *copyright;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *datetime;
	const gchar *description;
	const gchar *checksum;
	McmProfileKind kind;
	McmColorspace colorspace;
	gfloat luminance;
	gboolean has_vcgt;
} McmProfileTestData;

static void
mcm_test_profile_test_parse_file (const gchar *datafile, McmProfileTestData *test_data)
{
	gchar *filename = NULL;
	gboolean ret;
	GError *error = NULL;
	McmProfile *profile_lcms1;
	McmXyz *xyz;
	gfloat luminance;
	GFile *file;

	profile_lcms1 = MCM_PROFILE(mcm_profile_lcms1_new ());
	g_assert (profile_lcms1 != NULL);

	filename = mcm_test_get_data_file (datafile);
	g_assert ((filename != NULL));

	file = g_file_new_for_path (filename);
	ret = mcm_profile_parse (profile_lcms1, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	g_assert_cmpstr (mcm_profile_get_copyright (profile_lcms1), ==, test_data->copyright);
	g_assert_cmpstr (mcm_profile_get_manufacturer (profile_lcms1), ==, test_data->manufacturer);
	g_assert_cmpstr (mcm_profile_get_model (profile_lcms1), ==, test_data->model);
	g_assert_cmpstr (mcm_profile_get_datetime (profile_lcms1), ==, test_data->datetime);
	g_assert_cmpstr (mcm_profile_get_description (profile_lcms1), ==, test_data->description);
	g_assert_cmpstr (mcm_profile_get_checksum (profile_lcms1), ==, test_data->checksum);
	g_assert_cmpint (mcm_profile_get_kind (profile_lcms1), ==, test_data->kind);
	g_assert_cmpint (mcm_profile_get_colorspace (profile_lcms1), ==, test_data->colorspace);
	g_assert_cmpint (mcm_profile_get_has_vcgt (profile_lcms1), ==, test_data->has_vcgt);

	g_object_get (profile_lcms1,
		      "red", &xyz,
		      NULL);
	luminance = mcm_xyz_get_x (xyz);
	g_assert_cmpfloat (fabs (luminance - test_data->luminance), <, 0.001);

	g_object_unref (xyz);
	g_object_unref (profile_lcms1);
	g_free (filename);
}

static void
mcm_test_profile_func (void)
{
	McmProfileTestData test_data;

	/* bluish test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "Blueish Test";
	test_data.kind = MCM_PROFILE_KIND_DISPLAY_DEVICE;
	test_data.colorspace = MCM_COLORSPACE_RGB;
	test_data.luminance = 0.648454;
	test_data.datetime = "February  9 1998, 06:49:00 AM";
	test_data.checksum = "8e2aed5dac6f8b5d8da75610a65b7f27";
	test_data.has_vcgt = TRUE;
	mcm_test_profile_test_parse_file ("bluish.icc", &test_data);

	/* Adobe test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company Modified using Adobe Gamma";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "ADOBEGAMMA-Test";
	test_data.kind = MCM_PROFILE_KIND_DISPLAY_DEVICE;
	test_data.colorspace = MCM_COLORSPACE_RGB;
	test_data.luminance = 0.648446;
	test_data.datetime = "August 16 2005, 09:49:54 PM";
	test_data.checksum = "bd847723f676e2b846daaf6759330624";
	test_data.has_vcgt = TRUE;
	mcm_test_profile_test_parse_file ("AdobeGammaTest.icm", &test_data);
}

static void
mcm_test_profile_store_func (void)
{
	McmProfileStore *store;
	GPtrArray *array;
	McmProfile *profile;
	gboolean ret;
	gchar *filename;

	store = mcm_profile_store_new ();
	g_assert (store != NULL);

	/* add test files */
	filename = mcm_test_get_data_file (".");
	ret = mcm_profile_store_search_by_path (store, filename);
	g_assert (ret);
	g_free (filename);

	/* profile does not exist */
	profile = mcm_profile_store_get_by_filename (store, "xxxxxxxxx");
	g_assert (profile == NULL);

	/* profile does exist */
	profile = mcm_profile_store_get_by_checksum (store, "8e2aed5dac6f8b5d8da75610a65b7f27");
	g_assert (profile != NULL);
	g_assert_cmpstr (mcm_profile_get_checksum (profile), ==, "8e2aed5dac6f8b5d8da75610a65b7f27");
	g_object_unref (profile);

	/* get array of profiles */
	array = mcm_profile_store_get_array (store);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 3);
	g_ptr_array_unref (array);

	g_object_unref (store);
}

static void
mcm_test_tables_func (void)
{
	McmTables *tables;
	GError *error = NULL;
	gchar *vendor;

	tables = mcm_tables_new ();
	g_assert (tables != NULL);

	vendor = mcm_tables_get_pnp_id (tables, "IBM", &error);
	g_assert_no_error (error);
	g_assert (vendor != NULL);
	g_assert_cmpstr (vendor, ==, "IBM France");
	g_free (vendor);

	vendor = mcm_tables_get_pnp_id (tables, "MIL", &error);
	g_assert_no_error (error);
	g_assert (vendor != NULL);
	g_assert_cmpstr (vendor, ==, "Marconi Instruments Ltd");
	g_free (vendor);

	vendor = mcm_tables_get_pnp_id (tables, "XXX", &error);
	g_assert_error (error, 1, 0);
	g_assert_cmpstr (vendor, ==, NULL);
	g_free (vendor);

	g_object_unref (tables);
}

static void
mcm_test_trc_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	McmClut *clut;
	McmProfile *profile;
	gint response;
	gchar *filename_profile;
	gchar *filename_image;
	GFile *file;

	widget = mcm_trc_widget_new ();
	g_assert (widget != NULL);

	filename_image = mcm_test_get_data_file ("trc-widget.png");
	g_assert ((filename_image != NULL));

	filename_profile = mcm_test_get_data_file ("AdobeGammaTest.icm");
	g_assert ((filename_profile != NULL));

	profile = mcm_profile_default_new ();
	file = g_file_new_for_path (filename_profile);
	mcm_profile_parse (profile, file, NULL);
	clut = mcm_profile_generate_vcgt (profile, 256);
	g_object_set (widget,
		      "clut", clut,
		      NULL);
	g_object_unref (file);

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does TRC widget match\nthe picture below?");
	image = gtk_image_new_from_file (filename_image);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_object_unref (clut);
	g_object_unref (profile);
	g_free (filename_profile);
	g_free (filename_image);
}

static void
mcm_test_utils_func (void)
{
	gboolean ret;
	gchar *text;
	gchar *filename;
	GFile *file;
	GFile *dest;

	text = mcm_utils_linkify ("http://www.dave.org is text http://www.hughsie.com that needs to be linked to http://www.bbc.co.uk really");
	g_assert_cmpstr (text, ==, "<a href=\"http://www.dave.org\">http://www.dave.org</a> is text "
			     "<a href=\"http://www.hughsie.com\">http://www.hughsie.com</a> that needs to be linked to "
			     "<a href=\"http://www.bbc.co.uk\">http://www.bbc.co.uk</a> really");
	g_free (text);

	file = g_file_new_for_path ("dave.icc");
	dest = mcm_utils_get_profile_destination (file);
	filename = g_file_get_path (dest);
	g_assert (g_str_has_suffix (filename, "/.local/share/icc/dave.icc"));
	g_free (filename);
	g_object_unref (file);
	g_object_unref (dest);

	filename = mcm_test_get_data_file ("bluish.icc");
	file = g_file_new_for_path (filename);
	ret = mcm_utils_is_icc_profile (file);
	g_assert (ret);
	g_object_unref (file);
	g_free (filename);

	ret = mcm_utils_output_is_lcd_internal ("LVDS1");
	g_assert (ret);

	ret = mcm_utils_output_is_lcd_internal ("DVI1");
	g_assert (!ret);

	ret = mcm_utils_output_is_lcd ("LVDS1");
	g_assert (ret);

	ret = mcm_utils_output_is_lcd ("DVI1");
	g_assert (ret);

	filename = g_strdup ("Hello\n\rWorld!");
	mcm_utils_alphanum_lcase (filename);
	g_assert_cmpstr (filename, ==, "hello__world_");
	g_free (filename);

	filename = g_strdup ("Hel lo\n\rWo-(r)ld!");
	mcm_utils_ensure_sensible_filename (filename);
	g_assert_cmpstr (filename, ==, "Hel lo__Wo-(r)ld_");
	g_free (filename);

	text = g_strdup ("1\r34 67_90");
	mcm_utils_ensure_printable (text);
	g_assert_cmpstr (text, ==, "134 67 90");
	g_free (text);

	/* get default config location (when in make check) */
	g_setenv ("MCM_TEST", "1", TRUE);
	filename = mcm_utils_get_default_config_location ();
	g_assert_cmpstr (filename, ==, "/tmp/device-profiles.conf");
	g_free (filename);

	g_assert (mcm_utils_device_kind_to_profile_kind (MCM_DEVICE_KIND_SCANNER) == MCM_PROFILE_KIND_INPUT_DEVICE);

	g_assert (mcm_utils_device_kind_to_profile_kind (MCM_DEVICE_KIND_UNKNOWN) == MCM_PROFILE_KIND_UNKNOWN);
}

static void
mcm_test_xyz_func (void)
{
	McmXyz *xyz;
	gdouble value;

	xyz = mcm_xyz_new ();
	g_assert (xyz != NULL);

	/* nothing set */
	value = mcm_xyz_get_x (xyz);
	g_assert_cmpfloat (fabs (value - 0.0f), <, 0.001f);

	/* set dummy values */
	g_object_set (xyz,
		      "cie-x", 0.125,
		      "cie-y", 0.25,
		      "cie-z", 0.5,
		      NULL);

	value = mcm_xyz_get_x (xyz);
	g_assert_cmpfloat (fabs (value - 0.142857143f), <, 0.001f);

	value = mcm_xyz_get_y (xyz);
	g_assert_cmpfloat (fabs (value - 0.285714286f), <, 0.001f);

	value = mcm_xyz_get_z (xyz);
	g_assert_cmpfloat (fabs (value - 0.571428571f), <, 0.001f);

	g_object_unref (xyz);
}

static void
mcm_test_client_func (void)
{
	McmClient *client;
	GError *error = NULL;
	gboolean ret;
	GPtrArray *array;
	McmDevice *device;
	gchar *contents;
	gchar *filename;
	gchar *icc_filename;
	gchar *data = NULL;

	/* get test file */
	icc_filename = mcm_test_get_data_file ("bluish.icc");

	client = mcm_client_new ();
	g_assert (client != NULL);

	/* ensure file is gone */
	g_setenv ("MCM_TEST", "1", TRUE);
	filename = mcm_utils_get_default_config_location ();
	g_unlink (filename);

	/* ensure we don't fail with no config file */
	ret = mcm_client_coldplug (client, MCM_CLIENT_COLDPLUG_SAVED, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = mcm_client_get_devices (client);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* ensure we get one device */
	contents = g_strdup_printf ("[xrandr_goldstar]\n"
				    "profile=%s\n"
				    "title=Goldstar\n"
				    "type=display\n"
				    "colorspace=rgb\n", icc_filename);
	ret = g_file_set_contents (filename, contents, -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = mcm_client_coldplug (client, MCM_CLIENT_COLDPLUG_SAVED, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = mcm_client_get_devices (client);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	device = g_ptr_array_index (array, 0);
	g_assert_cmpstr (mcm_device_get_id (device), ==, "xrandr_goldstar");
	g_assert_cmpstr (mcm_device_get_title (device), ==, "Goldstar");
	mcm_test_assert_basename (mcm_device_get_default_profile_filename (device), icc_filename);
	g_assert (mcm_device_get_saved (device));
	g_assert (!mcm_device_get_connected (device));
	g_assert (MCM_IS_DEVICE_XRANDR (device));
	g_ptr_array_unref (array);

	device = mcm_device_udev_new ();
	mcm_device_set_id (device, "xrandr_goldstar");
	mcm_device_set_title (device, "Slightly different");
	mcm_device_set_connected (device, TRUE);
	ret = mcm_client_add_device (client, device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure we merge saved properties into current devices */
	array = mcm_client_get_devices (client);
	g_assert_cmpint (array->len, ==, 1);
	device = g_ptr_array_index (array, 0);
	g_assert_cmpstr (mcm_device_get_id (device), ==, "xrandr_goldstar");
	g_assert_cmpstr (mcm_device_get_title (device), ==, "Slightly different");
	mcm_test_assert_basename (mcm_device_get_default_profile_filename (device), icc_filename);
	g_assert (mcm_device_get_saved (device));
	g_assert (mcm_device_get_connected (device));
	g_assert (MCM_IS_DEVICE_UDEV (device));
	g_ptr_array_unref (array);

	/* delete */
	mcm_device_set_connected (device, FALSE);
	ret = mcm_client_delete_device (client, device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = mcm_client_get_devices (client);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	ret = g_file_get_contents (filename, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (data, ==, "");

	g_object_unref (client);
	g_object_unref (device);
	g_unlink (filename);
	g_free (contents);
	g_free (filename);
	g_free (icc_filename);
	g_free (data);
}

int
main (int argc, char **argv)
{

	if (! g_thread_supported ())
		g_thread_init (NULL);
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/color/client", mcm_test_client_func);
	g_test_add_func ("/color/dmi", mcm_test_dmi_func);
	g_test_add_func ("/color/calibrate", mcm_test_calibrate_func);
	g_test_add_func ("/color/edid", mcm_test_edid_func);
	g_test_add_func ("/color/exif", mcm_test_exif_func);
	g_test_add_func ("/color/tables", mcm_test_tables_func);
	g_test_add_func ("/color/utils", mcm_test_utils_func);
	g_test_add_func ("/color/device", mcm_test_device_func);
	g_test_add_func ("/color/profile", mcm_test_profile_func);
	g_test_add_func ("/color/profile_store", mcm_test_profile_store_func);
	g_test_add_func ("/color/clut", mcm_test_clut_func);
	g_test_add_func ("/color/xyz", mcm_test_xyz_func);
	g_test_add_func ("/color/calibrate_dialog", mcm_test_calibrate_dialog_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/color/brightness", mcm_test_brightness_func);
		g_test_add_func ("/color/trc", mcm_test_trc_widget_func);
		g_test_add_func ("/color/cie", mcm_test_cie_widget_func);
		g_test_add_func ("/color/gamma_widget", mcm_test_gamma_widget_func);
		g_test_add_func ("/color/image", mcm_test_image_func);
	}
	if (g_test_slow ()) {
		g_test_add_func ("/color/print", mcm_test_print_func);
		g_test_add_func ("/color/calibrate_manual", mcm_test_calibrate_manual_func);
	}

	return g_test_run ();
}

