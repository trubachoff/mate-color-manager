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
 * SECTION:mcm-profile-lcms1
 * @short_description: A parser object that understands the ICC profile data format.
 *
 * This object is a simple parser for the ICC binary profile data. If only understands
 * a subset of the ICC profile, just enought to get some metadata and the LUT.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <math.h>
#include <lcms.h>

#include "egg-debug.h"

#include "mcm-profile-lcms1.h"
#include "mcm-utils.h"
#include "mcm-xyz.h"

static void     mcm_profile_lcms1_finalize	(GObject     *object);

#define MCM_PROFILE_LCMS1_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_PROFILE_LCMS1, McmProfileLcms1Private))

#define MCM_NUMTAGS			0x80
#define MCM_BODY			0x84

#define MCM_TAG_ID			0x00
#define MCM_TAG_OFFSET			0x04
#define MCM_TAG_SIZE			0x08
#define MCM_TAG_WIDTH			0x0c

#define icSigVideoCartGammaTableTag	0x76636774
#define icSigMachineLookUpTableTag	0x6d4c5554

#define MCM_MLUT_RED			0x000
#define MCM_MLUT_GREEN			0x200
#define MCM_MLUT_BLUE			0x400

#define MCM_DESC_RECORD_SIZE		0x08
#define MCM_DESC_RECORD_TEXT		0x0c
#define MCM_TEXT_RECORD_TEXT		0x08

#define MCM_VCGT_ID			0x00
#define MCM_VCGT_DUMMY			0x04
#define MCM_VCGT_GAMMA_TYPE		0x08
#define MCM_VCGT_GAMMA_DATA		0x0c

#define MCM_VCGT_FORMULA_GAMMA_RED	0x00
#define MCM_VCGT_FORMULA_MIN_RED	0x04
#define MCM_VCGT_FORMULA_MAX_RED	0x08
#define MCM_VCGT_FORMULA_GAMMA_GREEN	0x0c
#define MCM_VCGT_FORMULA_MIN_GREEN	0x10
#define MCM_VCGT_FORMULA_MAX_GREEN	0x14
#define MCM_VCGT_FORMULA_GAMMA_BLUE	0x18
#define MCM_VCGT_FORMULA_MIN_BLUE	0x1c
#define MCM_VCGT_FORMULA_MAX_BLUE	0x20

#define MCM_VCGT_TABLE_NUM_CHANNELS	0x00
#define MCM_VCGT_TABLE_NUM_ENTRIES	0x02
#define MCM_VCGT_TABLE_NUM_SIZE		0x04
#define MCM_VCGT_TABLE_NUM_DATA		0x06

/**
 * McmProfileLcms1Private:
 *
 * Private #McmProfileLcms1 data
 **/
struct _McmProfileLcms1Private
{
	gboolean			 loaded;
	gboolean			 has_mlut;
	gboolean			 has_vcgt_formula;
	gboolean			 has_vcgt_table;
	cmsHPROFILE			 lcms_profile;
	McmClutData			*vcgt_data;
	guint				 vcgt_data_size;
	McmClutData			*mlut_data;
	guint				 mlut_data_size;
	gboolean			 adobe_gamma_workaround;
};

G_DEFINE_TYPE (McmProfileLcms1, mcm_profile_lcms1, MCM_TYPE_PROFILE)

/**
 * mcm_parser_decode_32:
 **/
static guint
mcm_parser_decode_32 (const guint8 *data)
{
	guint retval;
	retval = (*(data + 0) << 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24);
	return GUINT32_FROM_BE (retval);
}

/**
 * mcm_parser_decode_16:
 **/
static guint
mcm_parser_decode_16 (const guint8 *data)
{
	guint retval;
	retval = (*(data + 0) << 0) + (*(data + 1) << 8);
	return GUINT16_FROM_BE (retval);
}

/**
 * mcm_parser_decode_8:
 **/
static guint
mcm_parser_decode_8 (const guint8 *data)
{
	guint retval;
	retval = (*data << 0);
	return GUINT16_FROM_BE (retval);
}

/**
 * mcm_parser_load_icc_mlut:
 **/
static gboolean
mcm_parser_load_icc_mlut (McmProfileLcms1 *profile_lcms1, const guint8 *data, guint size)
{
	gboolean ret = TRUE;
	guint i;
	McmClutData *mlut_data;

	/* just load in data into a fixed size LUT */
	profile_lcms1->priv->mlut_data = g_new0 (McmClutData, 256);
	mlut_data = profile_lcms1->priv->mlut_data;

	for (i=0; i<256; i++)
		mlut_data[i].red = mcm_parser_decode_16 (data + MCM_MLUT_RED + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].green = mcm_parser_decode_16 (data + MCM_MLUT_GREEN + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].blue = mcm_parser_decode_16 (data + MCM_MLUT_BLUE + i*2);

	/* save datatype */
	profile_lcms1->priv->has_mlut = TRUE;
	return ret;
}

/**
 * mcm_parser_load_icc_vcgt_formula:
 **/
static gboolean
mcm_parser_load_icc_vcgt_formula (McmProfileLcms1 *profile_lcms1, const guint8 *data, guint size)
{
	gboolean ret = FALSE;
	McmClutData *vcgt_data;

	/* just load in data into a temporary array */
	profile_lcms1->priv->vcgt_data = g_new0 (McmClutData, 4);
	vcgt_data = profile_lcms1->priv->vcgt_data;

	/* read in block of data */
	vcgt_data[0].red = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_GAMMA_RED);
	vcgt_data[0].green = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_GAMMA_GREEN);
	vcgt_data[0].blue = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_GAMMA_BLUE);

	vcgt_data[1].red = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MIN_RED);
	vcgt_data[1].green = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MIN_GREEN);
	vcgt_data[1].blue = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MIN_BLUE);

	vcgt_data[2].red = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MAX_RED);
	vcgt_data[2].green = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MAX_GREEN);
	vcgt_data[2].blue = mcm_parser_decode_32 (data + MCM_VCGT_FORMULA_MAX_BLUE);

	/* check if valid */
	if (vcgt_data[0].red / 65536.0 > 5.0 || vcgt_data[0].green / 65536.0 > 5.0 || vcgt_data[0].blue / 65536.0 > 5.0) {
		egg_warning ("Gamma values out of range: [R:%u G:%u B:%u]", vcgt_data[0].red, vcgt_data[0].green, vcgt_data[0].blue);
		goto out;
	}
	if (vcgt_data[1].red / 65536.0 >= 1.0 || vcgt_data[1].green / 65536.0 >= 1.0 || vcgt_data[1].blue / 65536.0 >= 1.0) {
		egg_warning ("Gamma min limit out of range: [R:%u G:%u B:%u]", vcgt_data[1].red, vcgt_data[1].green, vcgt_data[1].blue);
		goto out;
	}
	if (vcgt_data[2].red / 65536.0 > 1.0 || vcgt_data[2].green / 65536.0 > 1.0 || vcgt_data[2].blue / 65536.0 > 1.0) {
		egg_warning ("Gamma max limit out of range: [R:%u G:%u B:%u]", vcgt_data[2].red, vcgt_data[2].green, vcgt_data[2].blue);
		goto out;
	}

	/* save datatype */
	profile_lcms1->priv->has_vcgt_formula = TRUE;
	profile_lcms1->priv->vcgt_data_size = 3;
	ret = TRUE;
out:
	return ret;
}

/**
 * mcm_parser_load_icc_vcgt_table:
 **/
static gboolean
mcm_parser_load_icc_vcgt_table (McmProfileLcms1 *profile_lcms1, const guint8 *data, guint size)
{
	gboolean ret = TRUE;
	guint num_channels = 0;
	guint num_entries = 0;
	guint entry_size = 0;
	guint i;
	McmClutData *vcgt_data;

	num_channels = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_CHANNELS);
	num_entries = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_ENTRIES);
	entry_size = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_SIZE);

	/* work-around for AdobeGamma-ProfileLcms1s (taken from xcalib) */
	if (profile_lcms1->priv->adobe_gamma_workaround) {
		egg_debug ("Working around AdobeGamma profile_lcms1");
		entry_size = 2;
		num_entries = 256;
		num_channels = 3;
	}

	/* only able to parse RGB data */
	if (num_channels != 3) {
		egg_warning ("cannot parse non RGB entries");
		ret = FALSE;
		goto out;
	}

	/* bigger than will fit in 16 bits? */
	if (entry_size > 2) {
		egg_warning ("cannot parse large entries");
		ret = FALSE;
		goto out;
	}

	/* allocate ramp, plus one entry for extrapolation */
	profile_lcms1->priv->vcgt_data = g_new0 (McmClutData, num_entries + 1);
	vcgt_data = profile_lcms1->priv->vcgt_data;

	if (entry_size == 1) {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = mcm_parser_decode_8 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = mcm_parser_decode_8 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 1) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = mcm_parser_decode_8 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + i);
	} else {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = mcm_parser_decode_16 (data + MCM_VCGT_TABLE_NUM_DATA + (num_entries * 4) + (i*2));
	}

	/* save datatype */
	profile_lcms1->priv->has_vcgt_table = TRUE;
	profile_lcms1->priv->vcgt_data_size = num_entries;
out:
	return ret;
}

/**
 * mcm_parser_load_icc_vcgt:
 **/
static gboolean
mcm_parser_load_icc_vcgt (McmProfileLcms1 *profile_lcms1, const guint8 *data, guint size)
{
	gboolean ret = FALSE;
	guint tag_id;
	guint gamma_type;

	/* check we have a VCGT block */
	tag_id = mcm_parser_decode_32 (data);
	if (tag_id != icSigVideoCartGammaTableTag) {
		egg_warning ("invalid content of table vcgt, starting with %x", tag_id);
		goto out;
	}

	/* check what type of gamma encoding we have */
	gamma_type = mcm_parser_decode_32 (data + MCM_VCGT_GAMMA_TYPE);
	if (gamma_type == 0) {
		ret = mcm_parser_load_icc_vcgt_table (profile_lcms1, data + MCM_VCGT_GAMMA_DATA, size);
		goto out;
	}
	if (gamma_type == 1) {
		ret = mcm_parser_load_icc_vcgt_formula (profile_lcms1, data + MCM_VCGT_GAMMA_DATA, size);
		goto out;
	}

	/* we didn't understand the encoding */
	egg_warning ("gamma type encoding not recognized");
out:
	return ret;
}

/**
 * mcm_profile_lcms1_utf16be_to_locale:
 *
 * Convert ICC encoded UTF-16BE into a string the user can understand
 **/
static gchar *
mcm_profile_lcms1_utf16be_to_locale (const guint8 *text, guint size)
{
	gsize items_written;
	gchar *text_utf8 = NULL;
	gchar *text_locale = NULL;
	GError *error = NULL;

	/* convert from ICC text encoding to UTF-8 */
	text_utf8 = g_convert ((const gchar*)text, size, "UTF-8", "UTF-16BE", NULL, &items_written, &error);
	if (text_utf8 == NULL) {
		egg_warning ("failed to convert to UTF-8: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* convert from UTF-8 to the users locale*/
	text_locale = g_locale_from_utf8 (text_utf8, items_written, NULL, NULL, &error);
	if (text_locale == NULL) {
		egg_warning ("failed to convert to locale: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (text_utf8);
	return text_locale;
}

/**
 * mcm_profile_lcms1_parse_multi_localized_unicode:
 **/
static gchar *
mcm_profile_lcms1_parse_multi_localized_unicode (McmProfileLcms1 *profile_lcms1, const guint8 *data, guint size)
{
	guint i;
	gchar *text = NULL;
	guint record_size;
	guint names_size;
	guint len;
	guint offset_name;
	guint32 type;

	/* get type */
	type = mcm_parser_decode_32 (data);

	/* check we are not a localized tag */
	if (type == icSigTextDescriptionType) {
		record_size = mcm_parser_decode_32 (data + MCM_DESC_RECORD_SIZE);
		text = g_strndup ((const gchar*)&data[MCM_DESC_RECORD_TEXT], record_size);
		goto out;
	}

	/* check we are not a localized tag */
	if (type == icSigTextType) {
		text = g_strdup ((const gchar*)&data[MCM_TEXT_RECORD_TEXT]);
		goto out;
	}

	/* check we are not a localized tag */
	if (type == icSigMultiLocalizedUnicodeType) {
		names_size = mcm_parser_decode_32 (data + 8);
		if (names_size != 1) {
			/* there is more than one language encoded */
			egg_warning ("more than one item of data in MLUC (names size: %i), using first one", names_size);
		}
		len = mcm_parser_decode_32 (data + 20);
		offset_name = mcm_parser_decode_32 (data + 24);
		text = mcm_profile_lcms1_utf16be_to_locale (data + offset_name, len);
		goto out;
	}

	/* an unrecognized tag */
	for (i=0x0; i<0x1c; i++) {
		egg_warning ("unrecognized text tag");
		if (data[i] >= 'A' && data[i] <= 'z')
			egg_debug ("%i\t%c (%i)", i, data[i], data[i]);
		else
			egg_debug ("%i\t  (%i)", i, data[i]);
	}
out:
	return text;
}

/**
 * mcm_profile_lcms1_save:
 **/
static gboolean
mcm_profile_lcms1_save (McmProfile *profile, const gchar *filename, GError **error)
{
	McmProfileLcms1 *profile_lcms1 = MCM_PROFILE_LCMS1 (profile);
	McmProfileLcms1Private *priv = profile_lcms1->priv;
	_cmsSaveProfile (priv->lcms_profile, filename);
	return TRUE;
}

/**
 * mcm_profile_lcms1_parse_data:
 **/
static gboolean
mcm_profile_lcms1_parse_data (McmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	guint num_tags;
	guint i;
	guint tag_id;
	guint offset;
	guint tag_size;
	guint tag_offset;
	icProfileClassSignature profile_class;
	icColorSpaceSignature color_space;
	McmColorspace colorspace;
	McmProfileKind profile_kind;
	cmsCIEXYZ cie_xyz;
	cmsCIEXYZTRIPLE cie_illum;
	struct tm created;
	cmsHPROFILE xyz_profile_lcms1;
	cmsHTRANSFORM transform;
	gchar *text;
	McmXyz *xyz;
	McmProfileLcms1 *profile_lcms1 = MCM_PROFILE_LCMS1 (profile);
	McmProfileLcms1Private *priv = profile_lcms1->priv;

	g_return_val_if_fail (MCM_IS_PROFILE_LCMS1 (profile_lcms1), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (priv->loaded == FALSE, FALSE);

	priv->loaded = TRUE;

	/* load profile into lcms */
	priv->lcms_profile = cmsOpenProfileFromMem ((LPVOID)data, length);
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "failed to load: not an ICC profile");
		goto out;
	}

	/* get white point */
	ret = cmsTakeMediaWhitePoint (&cie_xyz, priv->lcms_profile);
	if (ret) {
		xyz = mcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_xyz.X,
			      "cie-y", cie_xyz.Y,
			      "cie-z", cie_xyz.Z,
			      NULL);
		g_object_set (profile,
			      "white", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_warning ("failed to get white point");
	}

	/* get black point */
	ret = cmsTakeMediaBlackPoint (&cie_xyz, priv->lcms_profile);
	if (ret) {
		xyz = mcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_xyz.X,
			      "cie-y", cie_xyz.Y,
			      "cie-z", cie_xyz.Z,
			      NULL);
		g_object_set (profile,
			      "black", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_warning ("failed to get black point");
	}

	/* get the profile kind */
	profile_class = cmsGetDeviceClass (priv->lcms_profile);
	switch (profile_class) {
	case icSigInputClass:
		profile_kind = MCM_PROFILE_KIND_INPUT_DEVICE;
		break;
	case icSigDisplayClass:
		profile_kind = MCM_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case icSigOutputClass:
		profile_kind = MCM_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case icSigLinkClass:
		profile_kind = MCM_PROFILE_KIND_DEVICELINK;
		break;
	case icSigColorSpaceClass:
		profile_kind = MCM_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case icSigAbstractClass:
		profile_kind = MCM_PROFILE_KIND_ABSTRACT;
		break;
	case icSigNamedColorClass:
		profile_kind = MCM_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		profile_kind = MCM_PROFILE_KIND_UNKNOWN;
	}
	mcm_profile_set_kind (profile, profile_kind);

	/* get colorspace */
	color_space = cmsGetColorSpace (priv->lcms_profile);
	switch (color_space) {
	case icSigXYZData:
		colorspace = MCM_COLORSPACE_XYZ;
		break;
	case icSigLabData:
		colorspace = MCM_COLORSPACE_LAB;
		break;
	case icSigLuvData:
		colorspace = MCM_COLORSPACE_LUV;
		break;
	case icSigYCbCrData:
		colorspace = MCM_COLORSPACE_YCBCR;
		break;
	case icSigYxyData:
		colorspace = MCM_COLORSPACE_YXY;
		break;
	case icSigRgbData:
		colorspace = MCM_COLORSPACE_RGB;
		break;
	case icSigGrayData:
		colorspace = MCM_COLORSPACE_GRAY;
		break;
	case icSigHsvData:
		colorspace = MCM_COLORSPACE_HSV;
		break;
	case icSigCmykData:
		colorspace = MCM_COLORSPACE_CMYK;
		break;
	case icSigCmyData:
		colorspace = MCM_COLORSPACE_CMY;
		break;
	default:
		colorspace = MCM_COLORSPACE_UNKNOWN;
	}
	mcm_profile_set_colorspace (profile, colorspace);

	/* get primary illuminants */
	ret = cmsTakeColorants (&cie_illum, priv->lcms_profile);

	/* geting the illuminants failed, try running it through the profile_lcms1 */
	if (!ret && color_space == icSigRgbData) {
		gdouble rgb_values[3];

		/* create a transform from profile_lcms1 to XYZ */
		xyz_profile_lcms1 = cmsCreateXYZProfile ();
		transform = cmsCreateTransform (priv->lcms_profile, TYPE_RGB_DBL, xyz_profile_lcms1, TYPE_XYZ_DBL, INTENT_PERCEPTUAL, 0);
		if (transform != NULL) {

			/* red */
			rgb_values[0] = 1.0;
			rgb_values[1] = 0.0;
			rgb_values[2] = 0.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Red, 1);

			/* green */
			rgb_values[0] = 0.0;
			rgb_values[1] = 1.0;
			rgb_values[2] = 0.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Green, 1);

			/* blue */
			rgb_values[0] = 0.0;
			rgb_values[1] = 0.0;
			rgb_values[2] = 1.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Blue, 1);

			/* we're done */
			cmsDeleteTransform (transform);
			ret = TRUE;
		}

		/* no more need for the output profile_lcms1 */
		cmsCloseProfile (xyz_profile_lcms1);
	}

	/* we've got valid values */
	if (ret) {
		/* red */
		xyz = mcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Red.X,
			      "cie-y", cie_illum.Red.Y,
			      "cie-z", cie_illum.Red.Z,
			      NULL);
		g_object_set (profile,
			      "red", xyz,
			      NULL);
		g_object_unref (xyz);

		/* green */
		xyz = mcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Green.X,
			      "cie-y", cie_illum.Green.Y,
			      "cie-z", cie_illum.Green.Z,
			      NULL);
		g_object_set (profile,
			      "green", xyz,
			      NULL);
		g_object_unref (xyz);

		/* blue */
		xyz = mcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Blue.X,
			      "cie-y", cie_illum.Blue.Y,
			      "cie-z", cie_illum.Blue.Z,
			      NULL);
		g_object_set (profile,
			      "blue", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_debug ("failed to get luminance values");
	}

	/* get the profile_lcms1 created time and date */
	ret = cmsTakeCreationDateTime (&created, priv->lcms_profile);
	if (ret) {
		text = mcm_utils_format_date_time (&created);
		mcm_profile_set_datetime (profile, text);
		g_free (text);
	}

	/* get the number of tags in the file */
	num_tags = mcm_parser_decode_32 (data + MCM_NUMTAGS);

	for (i=0; i<num_tags; i++) {
		offset = MCM_TAG_WIDTH * i;
		tag_id = mcm_parser_decode_32 (data + MCM_BODY + offset + MCM_TAG_ID);
		tag_offset = mcm_parser_decode_32 (data + MCM_BODY + offset + MCM_TAG_OFFSET);
		tag_size = mcm_parser_decode_32 (data + MCM_BODY + offset + MCM_TAG_SIZE);

		/* print tag */
//		egg_debug ("tag %x is present at 0x%x with size %u", tag_id, tag_offset, tag_size);

		if (tag_id == icSigProfileDescriptionTag) {
			text = mcm_profile_lcms1_parse_multi_localized_unicode (profile_lcms1, data + tag_offset, tag_size);
			mcm_profile_set_description (profile, text);
			g_free (text);
		}
		if (tag_id == icSigCopyrightTag) {
			text = mcm_profile_lcms1_parse_multi_localized_unicode (profile_lcms1, data + tag_offset, tag_size);
			mcm_profile_set_copyright (profile, text);
			g_free (text);
		}
		if (tag_id == icSigDeviceMfgDescTag) {
			text = mcm_profile_lcms1_parse_multi_localized_unicode (profile_lcms1, data + tag_offset, tag_size);
			mcm_profile_set_manufacturer (profile, text);
			g_free (text);
		}
		if (tag_id == icSigDeviceModelDescTag) {
			text = mcm_profile_lcms1_parse_multi_localized_unicode (profile_lcms1, data + tag_offset, tag_size);
			mcm_profile_set_model (profile, text);
			g_free (text);
		}
		if (tag_id == icSigMachineLookUpTableTag) {
			ret = mcm_parser_load_icc_mlut (profile_lcms1, data + tag_offset, tag_size);
			if (!ret) {
				g_set_error_literal (error, 1, 0, "failed to load mlut");
				goto out;
			}
		}
		if (tag_id == icSigVideoCartGammaTableTag) {
			if (tag_size == 1584)
				priv->adobe_gamma_workaround = TRUE;
			ret = mcm_parser_load_icc_vcgt (profile_lcms1, data + tag_offset, tag_size);
			if (!ret) {
				g_set_error_literal (error, 1, 0, "failed to load vcgt");
				goto out;
			}
		}
	}

	/* success */
	ret = TRUE;

	/* set properties */
	mcm_profile_set_has_vcgt (profile, priv->has_vcgt_formula || priv->has_vcgt_table);
out:
	return ret;
}

/**
 * mcm_profile_lcms1_generate_vcgt:
 *
 * Free with g_object_unref();
 **/
static McmClut *
mcm_profile_lcms1_generate_vcgt (McmProfile *profile, guint size)
{
	guint i;
	guint ratio;
	McmClutData *tmp;
	McmClutData *vcgt_data;
	McmClutData *mlut_data;
	gfloat gamma_red, min_red, max_red;
	gfloat gamma_green, min_green, max_green;
	gfloat gamma_blue, min_blue, max_blue;
	guint num_entries;
	McmClut *clut = NULL;
	GPtrArray *array = NULL;
	gfloat inverse_ratio;
	guint idx;
	gfloat frac;
	McmProfileLcms1 *profile_lcms1 = MCM_PROFILE_LCMS1 (profile);

	g_return_val_if_fail (MCM_IS_PROFILE_LCMS1 (profile_lcms1), NULL);
	g_return_val_if_fail (size != 0, FALSE);

	/* reduce dereferences */
	vcgt_data = profile_lcms1->priv->vcgt_data;
	mlut_data = profile_lcms1->priv->mlut_data;

	if (profile_lcms1->priv->has_vcgt_table) {

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile_lcms1->priv->vcgt_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				/* add a point */
				tmp = g_new0 (McmClutData, 1);
				tmp->red = vcgt_data[ratio*i].red;
				tmp->green = vcgt_data[ratio*i].green;
				tmp->blue = vcgt_data[ratio*i].blue;
				g_ptr_array_add (array, tmp);
			}
			goto out;
		}

		/* LUT is bigger than number of entries, so interpolate */
		inverse_ratio = (gfloat) num_entries / size;
		vcgt_data[num_entries].red = 0xffff;
		vcgt_data[num_entries].green = 0xffff;
		vcgt_data[num_entries].blue = 0xffff;

		/* interpolate */
		for (i=0; i<size; i++) {
			idx = floor(i*inverse_ratio);
			frac = (i*inverse_ratio) - idx;
			tmp = g_new0 (McmClutData, 1);
			tmp->red = vcgt_data[idx].red * (1.0f-frac) + vcgt_data[idx + 1].red * frac;
			tmp->green = vcgt_data[idx].green * (1.0f-frac) + vcgt_data[idx + 1].green * frac;
			tmp->blue = vcgt_data[idx].blue * (1.0f-frac) + vcgt_data[idx + 1].blue * frac;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile_lcms1->priv->has_vcgt_formula) {

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

		gamma_red = (gfloat) vcgt_data[0].red / 65536.0;
		gamma_green = (gfloat) vcgt_data[0].green / 65536.0;
		gamma_blue = (gfloat) vcgt_data[0].blue / 65536.0;
		min_red = (gfloat) vcgt_data[1].red / 65536.0;
		min_green = (gfloat) vcgt_data[1].green / 65536.0;
		min_blue = (gfloat) vcgt_data[1].blue / 65536.0;
		max_red = (gfloat) vcgt_data[2].red / 65536.0;
		max_green = (gfloat) vcgt_data[2].green / 65536.0;
		max_blue = (gfloat) vcgt_data[2].blue / 65536.0;

		/* create mapping of desired size */
		for (i=0; i<size; i++) {
			/* add a point */
			tmp = g_new0 (McmClutData, 1);
			tmp->red = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_red) * (max_red - min_red) + min_red);
			tmp->green = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_green) * (max_green - min_green) + min_green);
			tmp->blue = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_blue) * (max_blue - min_blue) + min_blue);
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile_lcms1->priv->has_mlut) {

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

		/* roughly interpolate table */
		ratio = (guint) (256 / (size));
		for (i=0; i<size; i++) {
			/* add a point */
			tmp = g_new0 (McmClutData, 1);
			tmp->red = mlut_data[ratio*i].red;
			tmp->green = mlut_data[ratio*i].green;
			tmp->blue = mlut_data[ratio*i].blue;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	/* bugger */
	egg_debug ("no LUT to generate");
out:
	if (array != NULL) {
		/* create new output array */
		clut = mcm_clut_new ();
		mcm_clut_set_source_array (clut, array);
		g_ptr_array_unref (array);
	}
	return clut;
}

/**
 * mcm_profile_lcms1_generate_curve:
 *
 * Free with g_object_unref();
 **/
static McmClut *
mcm_profile_lcms1_generate_curve (McmProfile *profile, guint size)
{
	McmClut *clut = NULL;
	gdouble *values_in = NULL;
	gdouble *values_out = NULL;
	guint i;
	McmClutData *data;
	GPtrArray *array = NULL;
	gfloat divamount;
	gfloat divadd;
	guint component_width;
	cmsHPROFILE srgb_profile_lcms1 = NULL;
	cmsHTRANSFORM transform = NULL;
	guint type;
	McmColorspace colorspace;
	McmProfileLcms1 *profile_lcms1 = MCM_PROFILE_LCMS1 (profile);
	McmProfileLcms1Private *priv = profile_lcms1->priv;

	/* run through the profile */
	colorspace = mcm_profile_get_colorspace (profile);
	if (colorspace == MCM_COLORSPACE_RGB) {

		/* RGB */
		component_width = 3;
		type = TYPE_RGB_DBL;

		/* create input array */
		values_in = g_new0 (gdouble, size * 3 * component_width);
		divamount = 1.0f / (gfloat) (size - 1);
		for (i=0; i<size; i++) {
			divadd = divamount * (gfloat) i;

			/* red component */
			values_in[(i * 3 * component_width)+0] = divadd;
			values_in[(i * 3 * component_width)+1] = 0.0f;
			values_in[(i * 3 * component_width)+2] = 0.0f;

			/* green component */
			values_in[(i * 3 * component_width)+3] = 0.0f;
			values_in[(i * 3 * component_width)+4] = divadd;
			values_in[(i * 3 * component_width)+5] = 0.0f;

			/* blue component */
			values_in[(i * 3 * component_width)+6] = 0.0f;
			values_in[(i * 3 * component_width)+7] = 0.0f;
			values_in[(i * 3 * component_width)+8] = divadd;
		}
	}

	/* do each transform */
	if (values_in != NULL) {
		/* create output array */
		values_out = g_new0 (gdouble, size * 3 * component_width);

		/* create a transform from profile_lcms1 to sRGB */
		srgb_profile_lcms1 = cmsCreate_sRGBProfile ();
		transform = cmsCreateTransform (priv->lcms_profile, type, srgb_profile_lcms1, TYPE_RGB_DBL, INTENT_PERCEPTUAL, 0);
		if (transform == NULL)
			goto out;

		/* do transform */
		cmsDoTransform (transform, values_in, values_out, size * 3);

		/* create output array */
		array = g_ptr_array_new_with_free_func (g_free);

		for (i=0; i<size; i++) {
			data = g_new0 (McmClutData, 1);

			data->red = values_out[(i * 3 * component_width)+0] * (gfloat) 0xffff;
			data->green = values_out[(i * 3 * component_width)+4] * (gfloat) 0xffff;
			data->blue = values_out[(i * 3 * component_width)+8] * (gfloat) 0xffff;
			g_ptr_array_add (array, data);
		}
		clut = mcm_clut_new ();
		mcm_clut_set_source_array (clut, array);
	}

out:
	g_free (values_in);
	g_free (values_out);
	if (array != NULL)
		g_ptr_array_unref (array);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (srgb_profile_lcms1 != NULL)
		cmsCloseProfile (srgb_profile_lcms1);
	return clut;
}

/**
 * mcm_profile_lcms1_lcms_error_cb:
 **/
static int
mcm_profile_lcms1_lcms_error_cb (int ErrorCode, const char *ErrorText)
{
	egg_warning ("LCMS error %i: %s", ErrorCode, ErrorText);
	return LCMS_ERRC_WARNING;
}

/**
 * mcm_profile_lcms1_class_init:
 **/
static void
mcm_profile_lcms1_class_init (McmProfileLcms1Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	McmProfileClass *parent_class = MCM_PROFILE_CLASS (klass);
	object_class->finalize = mcm_profile_lcms1_finalize;

	parent_class->parse_data = mcm_profile_lcms1_parse_data;
	parent_class->save = mcm_profile_lcms1_save;
	parent_class->generate_vcgt = mcm_profile_lcms1_generate_vcgt;
	parent_class->generate_curve = mcm_profile_lcms1_generate_curve;

	g_type_class_add_private (klass, sizeof (McmProfileLcms1Private));
}

/**
 * mcm_profile_lcms1_init:
 **/
static void
mcm_profile_lcms1_init (McmProfileLcms1 *profile_lcms1)
{
	profile_lcms1->priv = MCM_PROFILE_LCMS1_GET_PRIVATE (profile_lcms1);
	profile_lcms1->priv->vcgt_data = NULL;
	profile_lcms1->priv->mlut_data = NULL;
	profile_lcms1->priv->adobe_gamma_workaround = FALSE;

	/* setup LCMS */
	cmsSetErrorHandler (mcm_profile_lcms1_lcms_error_cb);
	cmsErrorAction (LCMS_ERROR_SHOW);
	cmsSetLanguage ("en", "US");
}

/**
 * mcm_profile_lcms1_finalize:
 **/
static void
mcm_profile_lcms1_finalize (GObject *object)
{
	McmProfileLcms1 *profile_lcms1 = MCM_PROFILE_LCMS1 (object);
	McmProfileLcms1Private *priv = profile_lcms1->priv;

	if (priv->lcms_profile != NULL)
		cmsCloseProfile (priv->lcms_profile);

	g_free (priv->vcgt_data);
	g_free (priv->mlut_data);

	G_OBJECT_CLASS (mcm_profile_lcms1_parent_class)->finalize (object);
}

/**
 * mcm_profile_lcms1_new:
 *
 * Return value: a new McmProfileLcms1 object.
 **/
McmProfileLcms1 *
mcm_profile_lcms1_new (void)
{
	McmProfileLcms1 *profile_lcms1;
	profile_lcms1 = g_object_new (MCM_TYPE_PROFILE_LCMS1, NULL);
	return MCM_PROFILE_LCMS1 (profile_lcms1);
}

