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
#include "egg-test.h"
#include <egg-debug.h>

/* prototypes */
void mcm_print_test (EggTest *test);
void mcm_edid_test (EggTest *test);
void mcm_image_test (EggTest *test);
void mcm_tables_test (EggTest *test);
void mcm_utils_test (EggTest *test);
void mcm_device_test (EggTest *test);
void mcm_profile_test (EggTest *test);
void mcm_brightness_test (EggTest *test);
void mcm_clut_test (EggTest *test);
void mcm_dmi_test (EggTest *test);
void mcm_xyz_test (EggTest *test);
void mcm_cie_widget_test (EggTest *test);
void mcm_trc_widget_test (EggTest *test);
void mcm_calibrate_test (EggTest *test);
void mcm_calibrate_manual_test (EggTest *test);

int
main (int argc, char **argv)
{
	EggTest *test;

	if (! g_thread_supported ())
		g_thread_init (NULL);
	gtk_init (&argc, &argv);
	test = egg_test_init ();
	egg_debug_init (&argc, &argv);

	/* components */
	mcm_calibrate_test (test);
	mcm_edid_test (test);
	mcm_tables_test (test);
	mcm_utils_test (test);
	mcm_device_test (test);
	mcm_profile_test (test);
	mcm_brightness_test (test);
	mcm_clut_test (test);
	mcm_dmi_test (test);
	mcm_xyz_test (test);
	mcm_trc_widget_test (test);
	mcm_cie_widget_test (test);
	mcm_gamma_widget_test (test);
	mcm_image_test (test);
	mcm_print_test (test);
//	mcm_calibrate_manual_test (test);

	return (egg_test_finish (test));
}

