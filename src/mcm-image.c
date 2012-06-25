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
 * SECTION:mcm-image-manual
 * @short_description: routines to manually create a color profile.
 *
 * This object can create an ICC file manually.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <lcms.h>

#include "egg-debug.h"

#include "mcm-image.h"

static void     mcm_image_finalize	(GObject     *object);

#define MCM_IMAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_IMAGE, McmImagePrivate))

/**
 * McmImagePrivate:
 *
 * Private #McmImage data
 **/
struct _McmImagePrivate
{
	gboolean			 has_embedded_icc_profile;
	gboolean			 use_embedded_icc_profile;
	gchar				*output_icc_profile;
	gchar				*input_icc_profile;
	GdkPixbuf			*original_pixbuf;
};

enum {
	PROP_0,
	PROP_HAS_EMBEDDED_ICC_PROFILE,
	PROP_USE_EMBEDDED_ICC_PROFILE,
	PROP_OUTPUT_ICC_PROFILE,
	PROP_INPUT_ICC_PROFILE,
	PROP_LAST
};

G_DEFINE_TYPE (McmImage, mcm_image, GTK_TYPE_IMAGE)

/**
 * mcm_image_get_profile_in:
 **/
static cmsHPROFILE
mcm_image_get_profile_in (McmImage *image, const gchar *icc_profile_base64)
{
	cmsHPROFILE profile = NULL;
	guchar *icc_profile = NULL;
	gsize icc_profile_size;
	McmImagePrivate *priv = image->priv;

	/* ignore built-in */
	if (priv->use_embedded_icc_profile == FALSE) {
		egg_debug ("ignoring embedded ICC profile, assume sRGB");
		profile = cmsCreate_sRGBProfile ();
		goto out;
	}

	/* decode */
	icc_profile = g_base64_decode (icc_profile_base64, &icc_profile_size);
	if (icc_profile == NULL) {
		egg_warning ("failed to decode base64");
		goto out;
	}

	/* use embedded profile */
	profile = cmsOpenProfileFromMem (icc_profile, icc_profile_size);
out:
	g_free (icc_profile);
	return profile;
}

/**
 * mcm_image_get_profile_out:
 **/
static cmsHPROFILE
mcm_image_get_profile_out (McmImage *image)
{
	cmsHPROFILE profile = NULL;
	guchar *icc_profile = NULL;
	gsize icc_profile_size;
	McmImagePrivate *priv = image->priv;

	/* not set */
	if (priv->output_icc_profile == NULL) {
		egg_debug ("no output ICC profile, assume sRGB");
		profile = cmsCreate_sRGBProfile ();
		goto out;
	}

	/* decode */
	icc_profile = g_base64_decode (priv->output_icc_profile, &icc_profile_size);
	if (icc_profile == NULL) {
		egg_warning ("failed to decode base64");
		goto out;
	}

	/* use embedded profile */
	profile = cmsOpenProfileFromMem (icc_profile, icc_profile_size);
out:
	g_free (icc_profile);
	return profile;
}

/**
 * mcm_image_get_format:
 **/
static DWORD
mcm_image_get_format (McmImage *image)
{
	guint bits;
	guint has_alpha;
	DWORD format = 0;
	McmImagePrivate *priv = image->priv;

	/* get data */
	has_alpha = gdk_pixbuf_get_has_alpha (priv->original_pixbuf);
	bits = gdk_pixbuf_get_bits_per_sample (priv->original_pixbuf);

	/* no alpha channel */
	if (G_LIKELY (!has_alpha)) {
		if (bits == 8) {
			format = TYPE_RGB_8;
			goto out;
		}
		if (bits == 16) {
			format = TYPE_RGB_16;
			goto out;
		}
		goto out;
	}

	/* alpha channel */
	if (bits == 8) {
		format = TYPE_RGBA_8;
		goto out;
	}
	if (bits == 16) {
		format = TYPE_RGBA_16;
		goto out;
	}
out:
	return format;
}

/**
 * mcm_image_cms_convert_pixbuf:
 **/
static void
mcm_image_cms_convert_pixbuf (McmImage *image)
{
	const gchar *icc_profile_base64;
	gint i;
	DWORD format;
	gint width, height, rowstride;
	guchar *p_in;
	guchar *p_out;
	cmsHPROFILE profile_in;
	cmsHPROFILE profile_out;
	cmsHTRANSFORM transform;
	GdkPixbuf *pixbuf_cms;
	McmImagePrivate *priv = image->priv;

	/* not a pixbuf-backed image */
	if (priv->original_pixbuf == NULL) {
		egg_warning ("no pixbuf to convert");
		goto out;
	}

	/* CMYK not supported */
	if (gdk_pixbuf_get_colorspace (priv->original_pixbuf) != GDK_COLORSPACE_RGB) {
		egg_debug ("non-RGB not supported");
		goto out;
	}

	/* work out the LCMS format flags */
	format = mcm_image_get_format (image);
	if (format == 0) {
		egg_warning ("format not supported");
		goto out;
	}

	/* get profile from pixbuf */
	pixbuf_cms = gtk_image_get_pixbuf (GTK_IMAGE(image));
	icc_profile_base64 = gdk_pixbuf_get_option (pixbuf_cms, "icc-profile");

	/* set the boolean property */
	priv->use_embedded_icc_profile = (icc_profile_base64 != NULL);

	/* just exit, and have no color management done */
	if (icc_profile_base64 == NULL) {
		egg_debug ("not a color managed image");
		goto out;
	}

	/* get profiles */
	profile_in = mcm_image_get_profile_in (image, icc_profile_base64);
	profile_out = mcm_image_get_profile_out (image);

	/* create transform */
	transform = cmsCreateTransform (profile_in, format, profile_out, format, INTENT_PERCEPTUAL, 0);

	/* process each row */
	height = gdk_pixbuf_get_height (priv->original_pixbuf);
	width = gdk_pixbuf_get_width (priv->original_pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (priv->original_pixbuf);
	p_in = gdk_pixbuf_get_pixels (priv->original_pixbuf);
	p_out = gdk_pixbuf_get_pixels (pixbuf_cms);
	for (i=0; i<height; ++i) {
		cmsDoTransform (transform, p_in, p_out, width);
		p_in += rowstride;
		p_out += rowstride;
	}

	/* destroy lcms state */
	cmsDeleteTransform (transform);
	cmsCloseProfile (profile_in);
	cmsCloseProfile (profile_out);

	/* refresh widget */
	gtk_widget_set_visible (GTK_WIDGET(image), FALSE);
	gtk_widget_set_visible (GTK_WIDGET(image), TRUE);
out:
	return;
}

/**
 * mcm_image_notify_pixbuf_cb:
 **/
static void
mcm_image_notify_pixbuf_cb (GObject *object, GParamSpec *pspec, McmImage *image)
{
	GdkPixbuf *pixbuf;
	gpointer applied;
	McmImagePrivate *priv = image->priv;

	/* check */
	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE(image));
	if (pixbuf == NULL)
		goto out;
	applied = g_object_get_data (G_OBJECT(pixbuf), "cms-converted-pixbuf");
	if (applied != NULL) {
		egg_debug ("already copied and converted pixbuf, use mcm_image_cms_convert_pixbuf() instead");
		goto out;
	}

	/* unref existing */
	if (priv->original_pixbuf != NULL) {
		g_object_unref (priv->original_pixbuf);
		priv->original_pixbuf = NULL;
	}

	/* get new */
	if (pixbuf != NULL) {
		priv->original_pixbuf = gdk_pixbuf_copy (pixbuf);
		mcm_image_cms_convert_pixbuf (image);
		g_object_set_data (G_OBJECT(pixbuf), "cms-converted-pixbuf", (gpointer)"true");
	}
out:
	/* we do not own the pixbuf */
	return;
}

/**
 * mcm_image_get_property:
 **/
static void
mcm_image_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmImage *image = MCM_IMAGE (object);
	McmImagePrivate *priv = image->priv;

	switch (prop_id) {
	case PROP_HAS_EMBEDDED_ICC_PROFILE:
		g_value_set_boolean (value, priv->has_embedded_icc_profile);
		break;
	case PROP_USE_EMBEDDED_ICC_PROFILE:
		g_value_set_boolean (value, priv->use_embedded_icc_profile);
		break;
	case PROP_OUTPUT_ICC_PROFILE:
		g_value_set_string (value, priv->output_icc_profile);
		break;
	case PROP_INPUT_ICC_PROFILE:
		g_value_set_string (value, priv->input_icc_profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_image_set_property:
 **/
static void
mcm_image_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmImage *image = MCM_IMAGE (object);
	McmImagePrivate *priv = image->priv;

	switch (prop_id) {
	case PROP_USE_EMBEDDED_ICC_PROFILE:
		priv->use_embedded_icc_profile = g_value_get_boolean (value);
		mcm_image_cms_convert_pixbuf (image);
		break;
	case PROP_OUTPUT_ICC_PROFILE:
		g_free (priv->output_icc_profile);
		priv->output_icc_profile = g_strdup (g_value_get_string (value));
		mcm_image_cms_convert_pixbuf (image);
		break;
	case PROP_INPUT_ICC_PROFILE:
		g_free (priv->input_icc_profile);
		priv->input_icc_profile = g_strdup (g_value_get_string (value));
		mcm_image_cms_convert_pixbuf (image);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_image_class_init:
 **/
static void
mcm_image_class_init (McmImageClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_image_finalize;
	object_class->get_property = mcm_image_get_property;
	object_class->set_property = mcm_image_set_property;

	/**
	 * McmImage:has-embedded-icc-profile:
	 */
	pspec = g_param_spec_boolean ("has-embedded-icc-profile", NULL, NULL,
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HAS_EMBEDDED_ICC_PROFILE, pspec);

	/**
	 * McmImage:use-embedded-icc-profile:
	 */
	pspec = g_param_spec_boolean ("use-embedded-icc-profile", NULL, NULL,
				      TRUE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_EMBEDDED_ICC_PROFILE, pspec);

	/**
	 * McmImage:output-icc-profile:
	 */
	pspec = g_param_spec_string ("output-icc-profile", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_ICC_PROFILE, pspec);

	/**
	 * McmImage:input-icc-profile:
	 */
	pspec = g_param_spec_string ("input-icc-profile", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_INPUT_ICC_PROFILE, pspec);

	g_type_class_add_private (klass, sizeof (McmImagePrivate));
}

/**
 * mcm_image_init:
 **/
static void
mcm_image_init (McmImage *image)
{
	McmImagePrivate *priv;

	priv = image->priv = MCM_IMAGE_GET_PRIVATE (image);

	priv->has_embedded_icc_profile = FALSE;
	priv->use_embedded_icc_profile = TRUE;
	priv->original_pixbuf = NULL;

	/* only convert pixbuf if the size changes */
	g_signal_connect (GTK_WIDGET(image), "notify::pixbuf",
			  G_CALLBACK(mcm_image_notify_pixbuf_cb), image);
}

/**
 * mcm_image_finalize:
 **/
static void
mcm_image_finalize (GObject *object)
{
	McmImage *image = MCM_IMAGE (object);
	McmImagePrivate *priv = image->priv;

	if (priv->original_pixbuf != NULL)
		g_object_unref (priv->original_pixbuf);
	g_free (priv->output_icc_profile);
	g_free (priv->input_icc_profile);

	G_OBJECT_CLASS (mcm_image_parent_class)->finalize (object);
}

/**
 * mcm_image_new:
 *
 * Return value: a new McmImage object.
 **/
McmImage *
mcm_image_new (void)
{
	McmImage *image;
	image = g_object_new (MCM_TYPE_IMAGE, NULL);
	return MCM_IMAGE (image);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

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
	filename = egg_test_get_data_file ("ibm-t61.icc");

	/* get contents */
	ret = g_file_get_contents (filename, &contents, &length, &error);
	if (!ret) {
		egg_warning ("failed to get contents: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* encode */
	profile_base64 = g_base64_encode (contents, length);
out:
	g_free (contents);
	g_free (filename);
	return profile_base64;
}

void
mcm_image_test (EggTest *test)
{
	McmImage *image;
	GtkWidget *image_test;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gboolean ret;
	GError *error = NULL;
	gint response;
	gchar *filename_widget;
	gchar *filename_test;
	gchar *profile_base64;

	if (!egg_test_start (test, "McmImage"))
		return;

	/************************************************************/
	egg_test_title (test, "get a clever image object");
	image = mcm_image_new ();
	egg_test_assert (test, image != NULL);

	/************************************************************/
	filename_widget = egg_test_get_data_file ("image-widget.png");
	gtk_image_set_from_file (GTK_IMAGE(image), filename_widget);

	/************************************************************/
	egg_test_title (test, "get filename of image file");
	filename_test = egg_test_get_data_file ("image-widget-good.png");
	egg_test_assert (test, (filename_test != NULL));

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

	/************************************************************/
	egg_test_title (test, "converted as expected?");
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	egg_test_assert (test, (response == GTK_RESPONSE_YES));
	g_free (filename_test);

	/************************************************************/
	filename_test = egg_test_get_data_file ("image-widget-nonembed.png");
	gtk_image_set_from_file (GTK_IMAGE(image_test), filename_test);
	g_object_set (image,
		      "use-embedded-icc-profile", FALSE,
		      NULL);

	/************************************************************/
	egg_test_title (test, "converted as expected?");
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	egg_test_assert (test, (response == GTK_RESPONSE_YES));
	g_free (filename_test);

	/************************************************************/
	egg_test_title (test, "get dummy display profile");
	profile_base64 = mcm_image_test_get_ibmt61_profile ();
	egg_test_assert (test, (profile_base64 != NULL));

	/************************************************************/
	filename_test = egg_test_get_data_file ("image-widget-output.png");
	gtk_image_set_from_file (GTK_IMAGE(image_test), filename_test);
	g_object_set (image,
		      "use-embedded-icc-profile", TRUE,
		      "output-icc-profile", profile_base64,
		      NULL);

	/************************************************************/
	egg_test_title (test, "converted as expected?");
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	egg_test_assert (test, (response == GTK_RESPONSE_YES));
	g_free (filename_test);

	gtk_widget_destroy (dialog);

	g_free (profile_base64);
	g_free (filename_widget);

	egg_test_end (test);
}
#endif

