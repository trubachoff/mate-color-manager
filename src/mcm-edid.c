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
 * SECTION:mcm-edid
 * @short_description: EDID parsing object
 *
 * This object parses EDID data blocks.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "mcm-edid.h"
#include "mcm-tables.h"

#include "egg-debug.h"

static void     mcm_edid_finalize	(GObject     *object);

#define MCM_EDID_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_EDID, McmEdidPrivate))

/**
 * McmEdidPrivate:
 *
 * Private #McmEdid data
 **/
struct _McmEdidPrivate
{
	gchar				*monitor_name;
	gchar				*vendor_name;
	gchar				*serial_number;
	gchar				*eisa_id;
	gchar				*pnp_id;
	guint				 width;
	guint				 height;
	gfloat				 gamma;
	McmTables			*tables;
};

enum {
	PROP_0,
	PROP_MONITOR_NAME,
	PROP_VENDOR_NAME,
	PROP_SERIAL_NUMBER,
	PROP_EISA_ID,
	PROP_GAMMA,
	PROP_PNP_ID,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_LAST
};

G_DEFINE_TYPE (McmEdid, mcm_edid, G_TYPE_OBJECT)

#define MCM_EDID_OFFSET_PNPID				0x08
#define MCM_EDID_OFFSET_SERIAL				0x0c
#define MCM_EDID_OFFSET_SIZE				0x15
#define MCM_EDID_OFFSET_GAMMA				0x17
#define MCM_EDID_OFFSET_DATA_BLOCKS			0x36
#define MCM_EDID_OFFSET_LAST_BLOCK			0x6c
#define MCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT		0x7e

#define MCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define MCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define MCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA		0xf9
#define MCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING		0xfe
#define MCM_DESCRIPTOR_COLOR_POINT			0xfb


/**
 * mcm_edid_get_monitor_name:
 **/
const gchar *
mcm_edid_get_monitor_name (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), NULL);
	return edid->priv->monitor_name;
}

/**
 * mcm_edid_get_vendor_name:
 **/
const gchar *
mcm_edid_get_vendor_name (McmEdid *edid)
{
	McmEdidPrivate *priv = edid->priv;
	g_return_val_if_fail (MCM_IS_EDID (edid), NULL);

	if (priv->vendor_name == NULL)
		priv->vendor_name = mcm_tables_get_pnp_id (priv->tables, priv->pnp_id, NULL);
	return priv->vendor_name;
}

/**
 * mcm_edid_get_serial_number:
 **/
const gchar *
mcm_edid_get_serial_number (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), NULL);
	return edid->priv->serial_number;
}

/**
 * mcm_edid_get_eisa_id:
 **/
const gchar *
mcm_edid_get_eisa_id (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), NULL);
	return edid->priv->eisa_id;
}

/**
 * mcm_edid_get_pnp_id:
 **/
const gchar *
mcm_edid_get_pnp_id (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), NULL);
	return edid->priv->pnp_id;
}

/**
 * mcm_edid_get_width:
 **/
guint
mcm_edid_get_width (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), 0);
	return edid->priv->width;
}

/**
 * mcm_edid_get_height:
 **/
guint
mcm_edid_get_height (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), 0);
	return edid->priv->height;
}

/**
 * mcm_edid_get_gamma:
 **/
gfloat
mcm_edid_get_gamma (McmEdid *edid)
{
	g_return_val_if_fail (MCM_IS_EDID (edid), 0.0f);
	return edid->priv->gamma;
}

/**
 * mcm_edid_reset:
 **/
void
mcm_edid_reset (McmEdid *edid)
{
	McmEdidPrivate *priv = edid->priv;

	g_return_if_fail (MCM_IS_EDID (edid));

	/* free old data */
	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->eisa_id);

	/* do not deallocate, just blank */
	priv->pnp_id[0] = '\0';

	/* set to default values */
	priv->monitor_name = NULL;
	priv->vendor_name = NULL;
	priv->serial_number = NULL;
	priv->eisa_id = NULL;
	priv->width = 0;
	priv->height = 0;
	priv->gamma = 0.0f;
}

/**
 * mcm_edid_get_bit:
 *
 * Originally Copyright Soren Sandmann <sandmann@redhat.com>
 **/
static gint
mcm_edid_get_bit (gint in, gint bit)
{
	return (in & (1 << bit)) >> bit;
}

/**
 * mcm_edid_get_bits:
 **/
static gint
mcm_edid_get_bits (gint in, gint begin, gint end)
{
	gint mask = (1 << (end - begin + 1)) - 1;

	return (in >> begin) & mask;
}

/**
 * mcm_edid_decode_fraction:
 **/
static gdouble
mcm_edid_decode_fraction (gint high, gint low)
{
	gdouble result = 0.0;
	gint i;

	high = (high << 2) | low;
	for (i = 0; i < 10; ++i)
		result += mcm_edid_get_bit (high, i) * pow (2, i - 10);
	return result;
}

/**
 * mcm_edid_parse_string:
 *
 * This is always 12 bytes, but we can't guarantee it's null terminated
 * or not junk.
 *
 * Return value: the sane text, or %NULL, use g_free() to unref.
 **/
static gchar *
mcm_edid_parse_string (const guint8 *data)
{
	gchar *text;

	/* copy 12 bytes */
	text = g_strndup ((const gchar *) data, 12);

	/* remove insane newline chars */
	g_strdelimit (text, "\n\r", '\0');

	/* remove spaces */
	g_strchomp (text);

	/* nothing left? */
	if (text[0] == '\0') {
		g_free (text);
		text = NULL;
	}
	return text;
}

/**
 * mcm_edid_parse:
 **/
gboolean
mcm_edid_parse (McmEdid *edid, const guint8 *data, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	McmEdidPrivate *priv = edid->priv;
	guint32 serial;
	guint extension_blocks;
	gdouble x, y;
	gchar *tmp;

	g_return_val_if_fail (MCM_IS_EDID (edid), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* check header */
	if (data[0] != 0x00 || data[1] != 0xff) {
		g_set_error_literal (error, 1, 0, "failed to parse header");
		ret = FALSE;
		goto out;
	}

	/* free old data */
	mcm_edid_reset (edid);

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	priv->pnp_id[0] = 'A' + ((data[MCM_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
	priv->pnp_id[1] = 'A' + ((data[MCM_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[MCM_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
	priv->pnp_id[2] = 'A' + (data[MCM_EDID_OFFSET_PNPID+1] & 0x1f) - 1;
	egg_debug ("PNPID: %s", priv->pnp_id);

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	serial = (guint32) data[MCM_EDID_OFFSET_SERIAL+0];
	serial += (guint32) data[MCM_EDID_OFFSET_SERIAL+1] * 0x100;
	serial += (guint32) data[MCM_EDID_OFFSET_SERIAL+2] * 0x10000;
	serial += (guint32) data[MCM_EDID_OFFSET_SERIAL+3] * 0x1000000;
	if (serial > 0) {
		priv->serial_number = g_strdup_printf ("%" G_GUINT32_FORMAT, serial);
		egg_debug ("Serial: %s", priv->serial_number);
	}

	/* get the size */
	priv->width = data[MCM_EDID_OFFSET_SIZE+0];
	priv->height = data[MCM_EDID_OFFSET_SIZE+1];

	/* we don't care about aspect */
	if (priv->width == 0 || priv->height == 0) {
		priv->width = 0;
		priv->height = 0;
	}

	/* get gamma */
	if (data[MCM_EDID_OFFSET_GAMMA] == 0xff) {
		priv->gamma = 1.0f;
		egg_debug ("gamma is stored in an extension block");
	} else {
		priv->gamma = ((gfloat) data[MCM_EDID_OFFSET_GAMMA] / 100) + 1;
		egg_debug ("gamma is reported as %f", priv->gamma);
	}

	/* get color red */
	x = mcm_edid_decode_fraction (data[0x1b], mcm_edid_get_bits (data[0x19], 6, 7));
	y = mcm_edid_decode_fraction (data[0x1c], mcm_edid_get_bits (data[0x19], 5, 4));
	egg_debug ("red x=%f,y=%f", x, y);

	/* get color green */
	x = mcm_edid_decode_fraction (data[0x1d], mcm_edid_get_bits (data[0x19], 2, 3));
	y = mcm_edid_decode_fraction (data[0x1e], mcm_edid_get_bits (data[0x19], 0, 1));
	egg_debug ("green x=%f,y=%f", x, y);

	/* get color blue */
	x = mcm_edid_decode_fraction (data[0x1f], mcm_edid_get_bits (data[0x1a], 6, 7));
	y = mcm_edid_decode_fraction (data[0x20], mcm_edid_get_bits (data[0x1a], 4, 5));
	egg_debug ("blue x=%f,y=%f", x, y);

	/* get color white */
	x = mcm_edid_decode_fraction (data[0x21], mcm_edid_get_bits (data[0x1a], 2, 3));
	y = mcm_edid_decode_fraction (data[0x22], mcm_edid_get_bits (data[0x1a], 0, 1));
	egg_debug ("white x=%f,y=%f", x, y);

	/* parse EDID data */
	for (i=MCM_EDID_OFFSET_DATA_BLOCKS; i <= MCM_EDID_OFFSET_LAST_BLOCK; i+=18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == MCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			tmp = mcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->monitor_name);
				priv->monitor_name = tmp;
			}
		} else if (data[i+3] == MCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			tmp = mcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->serial_number);
				priv->serial_number = tmp;
			}
		} else if (data[i+3] == MCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
			egg_warning ("failing to parse color management data");
		} else if (data[i+3] == MCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			tmp = mcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->eisa_id);
				priv->eisa_id = tmp;
			}
		} else if (data[i+3] == MCM_DESCRIPTOR_COLOR_POINT) {
			if (data[i+3+9] != 0xff) {
				egg_debug ("extended EDID block(1) which contains a better gamma value");
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
				egg_debug ("gamma is overridden as %f", priv->gamma);
			}
			if (data[i+3+14] != 0xff) {
				egg_debug ("extended EDID block(2) which contains a better gamma value");
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
				egg_debug ("gamma is overridden as %f", priv->gamma);
			}
		}
	}

	/* extension blocks */
	extension_blocks = data[MCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT];
	if (extension_blocks > 0)
		egg_warning ("%i extension blocks to parse", extension_blocks);

	/* print what we've got */
	egg_debug ("monitor name: %s", priv->monitor_name);
	egg_debug ("serial number: %s", priv->serial_number);
	egg_debug ("ascii string: %s", priv->eisa_id);
out:
	return ret;
}

/**
 * mcm_edid_get_property:
 **/
static void
mcm_edid_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmEdid *edid = MCM_EDID (object);
	McmEdidPrivate *priv = edid->priv;

	switch (prop_id) {
	case PROP_MONITOR_NAME:
		g_value_set_string (value, priv->monitor_name);
		break;
	case PROP_VENDOR_NAME:
		g_value_set_string (value, mcm_edid_get_vendor_name (edid));
		break;
	case PROP_SERIAL_NUMBER:
		g_value_set_string (value, priv->serial_number);
		break;
	case PROP_EISA_ID:
		g_value_set_string (value, priv->eisa_id);
		break;
	case PROP_GAMMA:
		g_value_set_float (value, priv->gamma);
		break;
	case PROP_PNP_ID:
		g_value_set_string (value, priv->pnp_id);
		break;
	case PROP_WIDTH:
		g_value_set_uint (value, priv->width);
		break;
	case PROP_HEIGHT:
		g_value_set_uint (value, priv->height);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_edid_set_property:
 **/
static void
mcm_edid_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_edid_class_init:
 **/
static void
mcm_edid_class_init (McmEdidClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_edid_finalize;
	object_class->get_property = mcm_edid_get_property;
	object_class->set_property = mcm_edid_set_property;

	/**
	 * McmEdid:monitor-name:
	 */
	pspec = g_param_spec_string ("monitor-name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MONITOR_NAME, pspec);

	/**
	 * McmEdid:vendor-name:
	 */
	pspec = g_param_spec_string ("vendor-name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR_NAME, pspec);

	/**
	 * McmEdid:serial-number:
	 */
	pspec = g_param_spec_string ("serial-number", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SERIAL_NUMBER, pspec);

	/**
	 * McmEdid:eisa-id:
	 */
	pspec = g_param_spec_string ("eisa-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_EISA_ID, pspec);

	/**
	 * McmEdid:gamma:
	 */
	pspec = g_param_spec_float ("gamma", NULL, NULL,
				    1.0f, 5.0f, 1.0f,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_GAMMA, pspec);

	/**
	 * McmEdid:pnp-id:
	 */
	pspec = g_param_spec_string ("pnp-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PNP_ID, pspec);

	/**
	 * McmEdid:width:
	 */
	pspec = g_param_spec_uint ("width", "in cm", NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_WIDTH, pspec);

	/**
	 * McmEdid:height:
	 */
	pspec = g_param_spec_uint ("height", "in cm", NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HEIGHT, pspec);

	g_type_class_add_private (klass, sizeof (McmEdidPrivate));
}

/**
 * mcm_edid_init:
 **/
static void
mcm_edid_init (McmEdid *edid)
{
	edid->priv = MCM_EDID_GET_PRIVATE (edid);
	edid->priv->monitor_name = NULL;
	edid->priv->vendor_name = NULL;
	edid->priv->serial_number = NULL;
	edid->priv->eisa_id = NULL;
	edid->priv->tables = mcm_tables_new ();
	edid->priv->pnp_id = g_new0 (gchar, 4);
}

/**
 * mcm_edid_finalize:
 **/
static void
mcm_edid_finalize (GObject *object)
{
	McmEdid *edid = MCM_EDID (object);
	McmEdidPrivate *priv = edid->priv;

	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->eisa_id);
	g_free (priv->pnp_id);
	g_object_unref (priv->tables);

	G_OBJECT_CLASS (mcm_edid_parent_class)->finalize (object);
}

/**
 * mcm_edid_new:
 *
 * Return value: a new McmEdid object.
 **/
McmEdid *
mcm_edid_new (void)
{
	McmEdid *edid;
	edid = g_object_new (MCM_TYPE_EDID, NULL);
	return MCM_EDID (edid);
}

