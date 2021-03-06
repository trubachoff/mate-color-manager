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
 * SECTION:mcm-client
 * @short_description: Client object to hold an array of devices.
 *
 * This object holds an array of %McmDevices, and watches both udev and xorg
 * for changes. If a device is added, removed or changed then a signal is fired.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <gudev/gudev.h>
#include <libmateui/mate-rr.h>
#include <cups/cups.h>

#ifdef HAVE_SANE
 #include <sane/sane.h>
#endif

#include "mcm-client.h"
#include "mcm-device-xrandr.h"
#include "mcm-device-udev.h"
#include "mcm-device-cups.h"
#ifdef HAVE_SANE
 #include "mcm-device-sane.h"
#endif
#include "mcm-device-virtual.h"
#include "mcm-screen.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_client_finalize	(GObject     *object);

#define MCM_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_CLIENT, McmClientPrivate))

static void mcm_client_xrandr_add (McmClient *client, MateRROutput *output);
#ifdef HAVE_SANE
static gboolean mcm_client_coldplug_devices_sane (McmClient *client, GError **error);
static gpointer mcm_client_coldplug_devices_sane_thrd (McmClient *client);
#endif
/**
 * McmClientPrivate:
 *
 * Private #McmClient data
 **/
struct _McmClientPrivate
{
	gchar				*display_name;
	GPtrArray			*array;
	GUdevClient			*gudev_client;
	GSettings			*settings;
	McmScreen			*screen;
	http_t				*http;
	gboolean			 loading;
	guint				 loading_refcount;
	gboolean			 use_threads;
	gboolean			 init_cups;
	gboolean			 init_sane;
	guint				 refresh_id;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LOADING,
	PROP_USE_THREADS,
	PROP_LAST
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer mcm_client_object = NULL;

G_DEFINE_TYPE (McmClient, mcm_client, G_TYPE_OBJECT)

#define	MCM_CLIENT_SANE_REMOVED_TIMEOUT		200	/* ms */

/**
 * mcm_client_set_use_threads:
 **/
void
mcm_client_set_use_threads (McmClient *client, gboolean use_threads)
{
	client->priv->use_threads = use_threads;
	g_object_notify (G_OBJECT (client), "use-threads");
}

/**
 * mcm_client_set_loading:
 **/
static void
mcm_client_set_loading (McmClient *client, gboolean ret)
{
	client->priv->loading = ret;
	g_object_notify (G_OBJECT (client), "loading");
	egg_debug ("loading: %i", ret);
}

/**
 * mcm_client_done_loading:
 **/
static void
mcm_client_done_loading (McmClient *client)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	/* decrement refcount, with a lock */
	g_static_mutex_lock (&mutex);
	client->priv->loading_refcount--;
	if (client->priv->loading_refcount == 0)
		mcm_client_set_loading (client, FALSE);
	g_static_mutex_unlock (&mutex);
}

/**
 * mcm_client_add_loading:
 **/
static void
mcm_client_add_loading (McmClient *client)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	/* decrement refcount, with a lock */
	g_static_mutex_lock (&mutex);
	client->priv->loading_refcount++;
	if (client->priv->loading_refcount > 0)
		mcm_client_set_loading (client, TRUE);
	g_static_mutex_unlock (&mutex);
}

/**
 * mcm_client_get_devices:
 *
 * @client: a valid %McmClient instance
 *
 * Gets the device list.
 *
 * Return value: an array, free with g_ptr_array_unref()
 **/
GPtrArray *
mcm_client_get_devices (McmClient *client)
{
	g_return_val_if_fail (MCM_IS_CLIENT (client), NULL);
	return g_ptr_array_ref (client->priv->array);
}

/**
 * mcm_client_get_loading:
 *
 * @client: a valid %McmClient instance
 *
 * Gets the loading status.
 *
 * Return value: %TRUE if the object is still loading devices
 **/
gboolean
mcm_client_get_loading (McmClient *client)
{
	g_return_val_if_fail (MCM_IS_CLIENT (client), FALSE);
	return client->priv->loading;
}

/**
 * mcm_client_get_device_by_id:
 *
 * @client: a valid %McmClient instance
 * @id: the device identifer
 *
 * Gets a device.
 *
 * Return value: a valid %McmDevice or %NULL. Free with g_object_unref()
 **/
McmDevice *
mcm_client_get_device_by_id (McmClient *client, const gchar *id)
{
	guint i;
	McmDevice *device = NULL;
	McmDevice *device_tmp;
	const gchar *id_tmp;
	McmClientPrivate *priv = client->priv;

	g_return_val_if_fail (MCM_IS_CLIENT (client), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* find device */
	for (i=0; i<priv->array->len; i++) {
		device_tmp = g_ptr_array_index (priv->array, i);
		id_tmp = mcm_device_get_id (device_tmp);
		if (g_strcmp0 (id, id_tmp) == 0) {
			device = g_object_ref (device_tmp);
			break;
		}
	}

	return device;
}

/**
 * mcm_client_device_changed_cb:
 **/
static void
mcm_client_device_changed_cb (McmDevice *device, McmClient *client)
{
	/* emit a signal */
	egg_debug ("emit changed: %s", mcm_device_get_id (device));
	g_signal_emit (client, signals[SIGNAL_CHANGED], 0, device);
}

/**
 * mcm_client_get_id_for_udev_device:
 **/
static gchar *
mcm_client_get_id_for_udev_device (GUdevDevice *udev_device)
{
	gchar *id;

	/* get id */
	id = g_strdup_printf ("sysfs_%s_%s",
			      g_udev_device_get_property (udev_device, "ID_VENDOR"),
			      g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* replace unsafe chars */
	mcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * mcm_client_remove_device_internal:
 **/
static gboolean
mcm_client_remove_device_internal (McmClient *client, McmDevice *device, gboolean emit_signal, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;

	g_return_val_if_fail (MCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (MCM_IS_DEVICE (device), FALSE);

	/* check device is not connected */
	device_id = mcm_device_get_id (device);
	if (mcm_device_get_connected (device)) {
		g_set_error (error, 1, 0, "device %s is still connected", device_id);
		goto out;
	}

	/* try to remove from array */
	ret = g_ptr_array_remove (client->priv->array, device);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "not found in device array");
		goto out;
	}

	/* ensure signal handlers are disconnected */
	g_signal_handlers_disconnect_by_func (device, G_CALLBACK (mcm_client_device_changed_cb), client);

	/* emit a signal */
	if (emit_signal) {
		egg_debug ("emit removed: %s", device_id);
		g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
	}
out:
	return ret;
}

/**
 * mcm_client_gudev_remove:
 **/
static gboolean
mcm_client_gudev_remove (McmClient *client, GUdevDevice *udev_device)
{
	McmDevice *device;
	GError *error = NULL;
	gchar *id;
	gboolean ret = FALSE;

	/* generate id */
	id = mcm_client_get_id_for_udev_device (udev_device);

	/* do we have a device that matches */
	device = mcm_client_get_device_by_id (client, id);
	if (device == NULL) {
		egg_debug ("no match for %s", id);
		goto out;
	}

	/* set disconnected */
	mcm_device_set_connected (device, FALSE);

	/* remove device as it never had a profile assigned */
	if (!mcm_device_get_saved (device)) {
		ret = mcm_client_remove_device_internal (client, device, TRUE, &error);
		if (!ret) {
			egg_warning ("failed to remove: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* success */
	ret = TRUE;
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (id);
	return ret;
}

/**
 * mcm_client_gudev_add:
 **/
static gboolean
mcm_client_gudev_add (McmClient *client, GUdevDevice *udev_device)
{
	McmDevice *device = NULL;
	gboolean ret = FALSE;
	const gchar *value;
	GError *error = NULL;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "MCM_DEVICE");
	if (value == NULL)
		goto out;

	/* create new device */
	device = mcm_device_udev_new ();
	ret = mcm_device_udev_set_from_device (device, udev_device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add device */
	ret = mcm_client_add_device (client, device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

#ifdef HAVE_SANE
/**
 * mcm_client_sane_refresh_cb:
 **/
static gboolean
mcm_client_sane_refresh_cb (McmClient *client)
{
	gboolean ret;
	GError *error = NULL;
	GThread *thread;

	/* inform UI if we are loading devices still */
	client->priv->loading_refcount = 1;
	mcm_client_set_loading (client, TRUE);

	/* rescan */
	egg_debug ("rescanning sane");
	if (client->priv->use_threads) {
		thread = g_thread_create ((GThreadFunc) mcm_client_coldplug_devices_sane_thrd, client, FALSE, &error);
		if (thread == NULL) {
			egg_debug ("failed to rescan sane devices: %s", error->message);
			g_error_free (error);
			goto out;
		}
	} else {
		ret = mcm_client_coldplug_devices_sane (client, &error);
		if (!ret) {
			egg_debug ("failed to rescan sane devices: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}
out:
	return FALSE;
}
#endif

/**
 * mcm_client_uevent_cb:
 **/
static void
mcm_client_uevent_cb (GUdevClient *gudev_client, const gchar *action, GUdevDevice *udev_device, McmClient *client)
{
	gboolean ret;
#ifdef HAVE_SANE
	const gchar *value;
	McmDevice *device_tmp;
	guint i;
	gboolean enable;
	McmClientPrivate *priv = client->priv;
#endif

	if (g_strcmp0 (action, "remove") == 0) {
		ret = mcm_client_gudev_remove (client, udev_device);
		if (ret)
			egg_debug ("removed %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef HAVE_SANE
		/* we need to remove scanner devices */
		value = g_udev_device_get_property (udev_device, "MCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0) {

			/* are we ignoring scanners */
			enable = g_settings_get_boolean (client->priv->settings, MCM_SETTINGS_ENABLE_SANE);
			if (!enable)
				return;

			/* set all scanners as disconnected */
			for (i=0; i<priv->array->len; i++) {
				device_tmp = g_ptr_array_index (priv->array, i);
				if (mcm_device_get_kind (device_tmp) == MCM_DEVICE_KIND_SCANNER)
					mcm_device_set_connected (device_tmp, FALSE);
			}

			/* find any others that might still be connected */
			if (priv->refresh_id != 0)
				g_source_remove (priv->refresh_id);
			priv->refresh_id = g_timeout_add (MCM_CLIENT_SANE_REMOVED_TIMEOUT,
							  (GSourceFunc) mcm_client_sane_refresh_cb,
							  client);
#if GLIB_CHECK_VERSION(2,25,8)
			g_source_set_name_by_id (priv->refresh_id, "[McmClient] refresh due to device remove");
#endif
		}
#endif

	} else if (g_strcmp0 (action, "add") == 0) {
		ret = mcm_client_gudev_add (client, udev_device);
		if (ret)
			egg_debug ("added %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef HAVE_SANE
		/* we need to rescan scanner devices */
		value = g_udev_device_get_property (udev_device, "MCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0) {

			/* are we ignoring scanners */
			enable = g_settings_get_boolean (client->priv->settings, MCM_SETTINGS_ENABLE_SANE);
			if (!enable)
				return;

			if (priv->refresh_id != 0)
				g_source_remove (priv->refresh_id);
			priv->refresh_id = g_timeout_add (MCM_CLIENT_SANE_REMOVED_TIMEOUT,
							  (GSourceFunc) mcm_client_sane_refresh_cb,
							  client);
#if GLIB_CHECK_VERSION(2,25,8)
			g_source_set_name_by_id (priv->refresh_id, "[McmClient] refresh due to device add");
#endif
		}
#endif
	}
}

/**
 * mcm_client_coldplug_devices_udev:
 **/
static gboolean
mcm_client_coldplug_devices_udev (McmClient *client, GError **error)
{
	GList *devices;
	GList *l;
	GUdevDevice *udev_device;
	McmClientPrivate *priv = client->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->gudev_client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		mcm_client_gudev_add (client, udev_device);
	}

	/* get all video4linux devices */
	devices = g_udev_client_query_by_subsystem (priv->gudev_client, "video4linux");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		mcm_client_gudev_add (client, udev_device);
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);

	/* inform the UI */
	mcm_client_done_loading (client);

	return TRUE;
}

/**
 * mcm_client_coldplug_devices_udev_thrd:
 **/
static gpointer
mcm_client_coldplug_devices_udev_thrd (McmClient *client)
{
	mcm_client_coldplug_devices_udev (client, NULL);
	return NULL;
}

/**
 * mcm_client_get_device_by_window_covered:
 **/
static gfloat
mcm_client_get_device_by_window_covered (gint x, gint y, gint width, gint height,
				         gint window_x, gint window_y, gint window_width, gint window_height)
{
	gfloat covered = 0.0f;
	gint overlap_x;
	gint overlap_y;

	/* to the right of the window */
	if (window_x > x + width)
		goto out;
	if (window_y > y + height)
		goto out;

	/* to the left of the window */
	if (window_x + window_width < x)
		goto out;
	if (window_y + window_height < y)
		goto out;

	/* get the overlaps */
	overlap_x = MIN((window_x + window_width - x), width) - MAX(window_x - x, 0);
	overlap_y = MIN((window_y + window_height - y), height) - MAX(window_y - y, 0);

	/* not in this window */
	if (overlap_x <= 0)
		goto out;
	if (overlap_y <= 0)
		goto out;

	/* get the coverage */
	covered = (gfloat) (overlap_x * overlap_y) / (gfloat) (window_width * window_height);
	egg_debug ("overlap_x=%i,overlap_y=%i,covered=%f", overlap_x, overlap_y, covered);
out:
	return covered;
}

/**
 * mcm_client_get_device_by_window:
 **/
McmDevice *
mcm_client_get_device_by_window (McmClient *client, GdkWindow *window)
{
	gfloat covered;
	gfloat covered_max = 0.0f;
	gint window_width, window_height;
	gint window_x, window_y;
	gint x, y;
	guint i;
	guint len = 0;
	guint width, height;
	MateRRMode *mode;
	MateRROutput *output_best = NULL;
	MateRROutput **outputs;
	McmDevice *device = NULL;

	/* get the window parameters, in root co-ordinates */
	gdk_window_get_origin (window, &window_x, &window_y);
	gdk_drawable_get_size (GDK_DRAWABLE(window), &window_width, &window_height);

	/* get list of updates */
	outputs = mcm_screen_get_outputs (client->priv->screen, NULL);
	if (outputs == NULL)
		goto out;

	/* find length */
	for (i=0; outputs[i] != NULL; i++)
		len++;

	/* go through each option */
	for (i=0; i<len; i++) {

		/* not interesting */
		if (!mate_rr_output_is_connected (outputs[i]))
			continue;

		/* get details about the output */
		mate_rr_output_get_position (outputs[i], &x, &y);
		mode = mate_rr_output_get_current_mode (outputs[i]);
		width = mate_rr_mode_get_width (mode);
		height = mate_rr_mode_get_height (mode);
		egg_debug ("%s: %ix%i -> %ix%i (%ix%i -> %ix%i)", mate_rr_output_get_name (outputs[i]),
			   x, y, x+width, y+height,
			   window_x, window_y, window_x+window_width, window_y+window_height);

		/* get the fraction of how much the window is covered */
		covered = mcm_client_get_device_by_window_covered (x, y, width, height,
								   window_x, window_y, window_width, window_height);

		/* keep a running total of which one is best */
		if (covered > 0.01f && covered > covered_max) {
			output_best = outputs[i];

			/* optimize */
			if (covered > 0.99) {
				egg_debug ("all in one window, no need to search other windows");
				goto out;
			}

			/* keep looking */
			covered_max = covered;
			egg_debug ("personal best of %f for %s", covered, mate_rr_output_get_name (output_best));
		}
	}
out:
	/* if we found an output, get the device */
	if (output_best != NULL) {
		McmDevice *device_tmp;
		device_tmp = mcm_device_xrandr_new ();
		mcm_device_xrandr_set_from_output (device_tmp, output_best, NULL);
		device = mcm_client_get_device_by_id (client, mcm_device_get_id (device_tmp));
		g_object_unref (device_tmp);
	}
	return device;
}

/**
 * mcm_client_xrandr_add:
 **/
static void
mcm_client_xrandr_add (McmClient *client, MateRROutput *output)
{
	gboolean ret;
	GError *error = NULL;
	McmDevice *device = NULL;

	/* if nothing connected then ignore */
	ret = mate_rr_output_is_connected (output);
	if (!ret) {
		egg_debug ("%s is not connected", mate_rr_output_get_name (output));
		goto out;
	}

	/* create new device */
	device = mcm_device_xrandr_new ();
	ret = mcm_device_xrandr_set_from_output (device, output, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add device */
	ret = mcm_client_add_device (client, device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
}

/**
 * mcm_client_coldplug_devices_xrandr:
 **/
static gboolean
mcm_client_coldplug_devices_xrandr (McmClient *client, GError **error)
{
	MateRROutput **outputs;
	guint i;
	McmClientPrivate *priv = client->priv;

	outputs = mcm_screen_get_outputs (priv->screen, error);
	if (outputs == NULL)
		return FALSE;
	for (i=0; outputs[i] != NULL; i++)
		mcm_client_xrandr_add (client, outputs[i]);

	/* inform the UI */
	mcm_client_done_loading (client);

	return TRUE;
}

/**
 * mcm_client_cups_add:
 **/
static void
mcm_client_cups_add (McmClient *client, cups_dest_t dest)
{
	gboolean ret;
	GError *error = NULL;
	McmDevice *device = NULL;
	McmClientPrivate *priv = client->priv;

	/* create new device */
	device = mcm_device_cups_new ();
	ret = mcm_device_cups_set_from_dest (device, priv->http, dest, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add device */
	ret = mcm_client_add_device (client, device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
}

/**
 * mcm_client_coldplug_devices_cups:
 **/
static gboolean
mcm_client_coldplug_devices_cups (McmClient *client, GError **error)
{
	gint num_dests;
	cups_dest_t *dests;
	gint i;
	McmClientPrivate *priv = client->priv;

	/* initialize */
	if (!client->priv->init_cups) {
		httpInitialize();
		/* should be okay for localhost */
		client->priv->http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
		client->priv->init_cups = TRUE;
	}

	num_dests = cupsGetDests2 (priv->http, &dests);
	egg_debug ("got %i printers", num_dests);

	/* get printers on the local server */
	for (i = 0; i < num_dests; i++)
		mcm_client_cups_add (client, dests[i]);
	cupsFreeDests (num_dests, dests);

	/* inform the UI */
	mcm_client_done_loading (client);
	return TRUE;
}

/**
 * mcm_client_coldplug_devices_cups_thrd:
 **/
static gpointer
mcm_client_coldplug_devices_cups_thrd (McmClient *client)
{
	mcm_client_coldplug_devices_cups (client, NULL);
	return NULL;
}

#ifdef HAVE_SANE
/**
 * mcm_client_sane_add:
 **/
static void
mcm_client_sane_add (McmClient *client, const SANE_Device *sane_device)
{
	gboolean ret;
	GError *error = NULL;
	McmDevice *device = NULL;

	/* create new device */
	device = mcm_device_sane_new ();
	ret = mcm_device_sane_set_from_device (device, sane_device, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add device */
	ret = mcm_client_add_device (client, device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
}

/**
 * mcm_client_coldplug_devices_sane:
 **/
static gboolean
mcm_client_coldplug_devices_sane (McmClient *client, GError **error)
{
	gint i;
	gboolean ret = TRUE;
	SANE_Status status;
	const SANE_Device **device_list;

	/* force sane to drop it's cache of devices -- yes, it is that crap */
	if (client->priv->init_sane) {
		sane_exit ();
		client->priv->init_sane = FALSE;
	}
	status = sane_init (NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to init SANE: %s", sane_strstatus (status));
		goto out;
	}
	client->priv->init_sane = TRUE;

	/* get scanners on the local server */
	status = sane_get_devices (&device_list, FALSE);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to get devices from SANE: %s", sane_strstatus (status));
		goto out;
	}

	/* nothing */
	if (device_list == NULL || device_list[0] == NULL) {
		egg_debug ("no devices to add");
		goto out;
	}

	/* add them */
	for (i=0; device_list[i] != NULL; i++)
		mcm_client_sane_add (client, device_list[i]);
out:
	/* inform the UI */
	mcm_client_done_loading (client);
	return ret;
}

/**
 * mcm_client_coldplug_devices_sane_thrd:
 **/
static gpointer
mcm_client_coldplug_devices_sane_thrd (McmClient *client)
{
	mcm_client_coldplug_devices_sane (client, NULL);
	return NULL;
}
#endif

/**
 * mcm_client_add_unconnected_device:
 **/
static void
mcm_client_add_unconnected_device (McmClient *client, GKeyFile *keyfile, const gchar *id)
{
	gchar *title;
	gchar *kind_text = NULL;
	gchar *colorspace_text = NULL;
	McmColorspace colorspace;
	McmDeviceKind kind;
	McmDevice *device = NULL;
	gboolean ret;
	gboolean virtual;
	GError *error = NULL;

	/* add new device */
	title = g_key_file_get_string (keyfile, id, "title", NULL);
	if (title == NULL)
		goto out;
	virtual = g_key_file_get_boolean (keyfile, id, "virtual", NULL);
	kind_text = g_key_file_get_string (keyfile, id, "type", NULL);
	kind = mcm_device_kind_from_string (kind_text);
	if (kind == MCM_DEVICE_KIND_UNKNOWN)
		goto out;

	/* get colorspace */
	colorspace_text = g_key_file_get_string (keyfile, id, "colorspace", NULL);
	if (colorspace_text == NULL) {
		egg_warning ("legacy device %s, falling back to RGB", id);
		colorspace = MCM_COLORSPACE_RGB;
	} else {
		colorspace = mcm_colorspace_from_string (colorspace_text);
	}

	/* create device of specified type */
	if (virtual) {
		device = mcm_device_virtual_new ();
	} else if (kind == MCM_DEVICE_KIND_DISPLAY) {
		device = mcm_device_xrandr_new ();
	} else if (kind == MCM_DEVICE_KIND_PRINTER) {
		device = mcm_device_cups_new ();
	} else if (kind == MCM_DEVICE_KIND_CAMERA) {
		/* FIXME: use GPhoto? */
		device = mcm_device_udev_new ();
#ifdef HAVE_SANE
	} else if (kind == MCM_DEVICE_KIND_SCANNER) {
		device = mcm_device_sane_new ();
#endif
	} else {
		egg_warning ("device kind internal error");
		goto out;
	}

	/* create device */
	g_object_set (device,
		      "kind", kind,
		      "id", id,
		      "connected", FALSE,
		      "title", title,
		      "saved", TRUE,
		      "virtual", virtual,
		      "colorspace", colorspace,
		      NULL);

	/* add device */
	ret = mcm_client_add_device (client, device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (kind_text);
	g_free (colorspace_text);
	g_free (title);
}

/**
 * mcm_client_possibly_migrate_config_file:
 *
 * Copy the configuration file from ~/.config/mate-color-manager to ~/.config/color
 **/
static gboolean
mcm_client_possibly_migrate_config_file (McmClient *client)
{
	gchar *dest = NULL;
	gchar *source = NULL;
	GFile *gdest = NULL;
	GFile *gsource = NULL;
	gboolean ret = FALSE;
	gint config_version;
	GError *error = NULL;

	/* have we already attempted this (check first to avoid stating a file */
	config_version = g_settings_get_int (client->priv->settings,
					MCM_SETTINGS_MIGRATE_CONFIG_VERSION);
	if (config_version >= MCM_CONFIG_VERSION_SHARED_SPEC)
		goto out;

	/* create default path */
	source = g_build_filename (g_get_user_config_dir (),
				"mate-color-manager",
				"device-profiles.conf",
				NULL);
	gsource = g_file_new_for_path (source);

	/* no old profile */
	ret = g_file_query_exists (gsource, NULL);
	if (!ret) {
		g_settings_set_int (client->priv->settings,
				MCM_SETTINGS_MIGRATE_CONFIG_VERSION,
				MCM_CONFIG_VERSION_SHARED_SPEC);
		goto out;
	}

	/* ensure new root exists */
	dest = mcm_utils_get_default_config_location ();
	ret = mcm_utils_mkdir_for_filename (dest, &error);
	if (!ret) {
		egg_warning ("failed to create new tree: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* copy to new root */
	gdest = g_file_new_for_path (dest);
	ret = g_file_copy (gsource, gdest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
	if (!ret) {
		/* does the file already exist? -- i.e. the schema version was reset */
		if (error->domain != G_IO_ERROR ||
		    error->code != G_IO_ERROR_EXISTS) {
			egg_warning ("failed to copy: %s", error->message);
			g_error_free (error);
			goto out;
		}
		g_error_free (error);
	}

	/* do not attempt to migrate this again */
	g_settings_set_int (client->priv->settings,
			MCM_SETTINGS_MIGRATE_CONFIG_VERSION,
			MCM_CONFIG_VERSION_SHARED_SPEC);
out:
	g_free (source);
	g_free (dest);
	if (gsource != NULL)
		g_object_unref (gsource);
	if (gdest != NULL)
		g_object_unref (gdest);
	return ret;
}

/**
 * mcm_client_add_saved:
 **/
static gboolean
mcm_client_add_saved (McmClient *client, GError **error)
{
	gboolean ret;
	gchar *filename;
	GKeyFile *keyfile;
	gchar **groups = NULL;
	guint i;
	McmDevice *device;

	/* copy from old location */
	mcm_client_possibly_migrate_config_file (client);

	/* get the config file */
	filename = mcm_utils_get_default_config_location ();
	egg_debug ("using %s", filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* get groups */
	groups = g_key_file_get_groups (keyfile, NULL);
	if (groups == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "failed to get groups");
		goto out;
	}

	/* add each device if it's not already connected */
	for (i=0; groups[i] != NULL; i++) {
		device = mcm_client_get_device_by_id (client, groups[i]);
		if (device == NULL) {
			egg_debug ("not found %s", groups[i]);
			mcm_client_add_unconnected_device (client, keyfile, groups[i]);
		} else {
			egg_debug ("found already added %s", groups[i]);
			mcm_device_set_saved (device, TRUE);
		}
	}
out:
	/* inform the UI */
	mcm_client_done_loading (client);
	g_strfreev (groups);
	g_free (filename);
	g_key_file_free (keyfile);
	return ret;
}

/**
 * mcm_client_coldplug:
 **/
gboolean
mcm_client_coldplug (McmClient *client, McmClientColdplug coldplug, GError **error)
{
	gboolean ret = TRUE;
	GThread *thread;
	gboolean enable;

	g_return_val_if_fail (MCM_IS_CLIENT (client), FALSE);

	/* copy from old location */
	mcm_client_possibly_migrate_config_file (client);

	/* reset */
	client->priv->loading_refcount = 0;

	/* XRandR */
	if (!coldplug || coldplug & MCM_CLIENT_COLDPLUG_XRANDR) {
		mcm_client_add_loading (client);
		egg_debug ("adding devices of type XRandR");
		ret = mcm_client_coldplug_devices_xrandr (client, error);
		if (!ret)
			goto out;
	}

	/* UDEV */
	if (!coldplug || coldplug & MCM_CLIENT_COLDPLUG_UDEV) {
		mcm_client_add_loading (client);
		egg_debug ("adding devices of type UDEV");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) mcm_client_coldplug_devices_udev_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = mcm_client_coldplug_devices_udev (client, error);
			if (!ret)
				goto out;
		}
	}

	/* CUPS */
	enable = g_settings_get_boolean (client->priv->settings, MCM_SETTINGS_ENABLE_CUPS);
	if (enable && (!coldplug || coldplug & MCM_CLIENT_COLDPLUG_CUPS)) {
		mcm_client_add_loading (client);
		egg_debug ("adding devices of type CUPS");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) mcm_client_coldplug_devices_cups_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = mcm_client_coldplug_devices_cups (client, error);
			if (!ret)
				goto out;
		}
	}

#ifdef HAVE_SANE
	/* SANE */
	enable = g_settings_get_boolean (client->priv->settings, MCM_SETTINGS_ENABLE_SANE);
	if (enable && (!coldplug || coldplug & MCM_CLIENT_COLDPLUG_SANE)) {
		mcm_client_add_loading (client);
		egg_debug ("adding devices of type SANE");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) mcm_client_coldplug_devices_sane_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = mcm_client_coldplug_devices_sane (client, error);
			if (!ret)
				goto out;
		}
	}
#endif
out:
	return ret;
}

/**
 * mcm_client_add_device:
 **/
gboolean
mcm_client_add_device (McmClient *client, McmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;
	McmDevice *device_tmp = NULL;
	GPtrArray *array;

	g_return_val_if_fail (MCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (MCM_IS_DEVICE (device), FALSE);

	device_id = mcm_device_get_id (device);
	device_tmp = mcm_client_get_device_by_id (client, device_id);
	if (device_tmp != NULL) {
		egg_debug ("already exists, copy settings and remove old device: %s", device_id);
		array = mcm_device_get_profiles (device_tmp);
		if (array != NULL)
			mcm_device_set_profiles (device, array);
		mcm_device_set_saved (device, mcm_device_get_saved (device_tmp));
		ret = mcm_client_remove_device_internal (client, device_tmp, FALSE, error);
		if (!ret)
			goto out;
	}

	/* load the device */
	ret = mcm_device_load (device, error);
	if (!ret)
		goto out;

	/* add to the array */
	g_ptr_array_add (client->priv->array, g_object_ref (device));

	/* emit a signal */
	egg_debug ("emit added: %s", device_id);
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (mcm_client_device_changed_cb), client);

	/* all okay */
	ret = TRUE;
out:
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	return ret;
}

/**
 * mcm_client_remove_device:
 **/
gboolean
mcm_client_remove_device (McmClient *client, McmDevice *device, GError **error)
{
	return mcm_client_remove_device_internal (client, device, TRUE, error);
}

/**
 * mcm_client_delete_device:
 **/
gboolean
mcm_client_delete_device (McmClient *client, McmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;
	gchar *data = NULL;
	gchar *filename = NULL;
	GKeyFile *keyfile = NULL;

	g_return_val_if_fail (MCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (MCM_IS_DEVICE (device), FALSE);

	/* check device is saved */
	device_id = mcm_device_get_id (device);
	if (!mcm_device_get_saved (device))
		goto out;

	/* get the config file */
	filename = mcm_utils_get_default_config_location ();
	egg_debug ("removing %s from %s", device_id, filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* remove from the config file */
	ret = g_key_file_remove_group (keyfile, device_id, error);
	if (!ret)
		goto out;

	/* convert to string */
	data = g_key_file_to_data (keyfile, NULL, error);
	if (data == NULL) {
		ret = FALSE;
		goto out;
	}

	/* save contents */
	ret = g_file_set_contents (filename, data, -1, error);
	if (!ret)
		goto out;

	/* update status */
	mcm_device_set_saved (device, FALSE);

	/* remove device */
	ret = mcm_client_remove_device_internal (client, device, TRUE, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	g_free (filename);
	if (keyfile != NULL)
		g_key_file_free (keyfile);
	return ret;
}

/**
 * mcm_client_get_property:
 **/
static void
mcm_client_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	McmClient *client = MCM_CLIENT (object);
	McmClientPrivate *priv = client->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_value_set_string (value, priv->display_name);
		break;
	case PROP_LOADING:
		g_value_set_boolean (value, priv->loading);
		break;
	case PROP_USE_THREADS:
		g_value_set_boolean (value, priv->use_threads);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_client_set_property:
 **/
static void
mcm_client_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	McmClient *client = MCM_CLIENT (object);
	McmClientPrivate *priv = client->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_free (priv->display_name);
		priv->display_name = g_strdup (g_value_get_string (value));
		break;
	case PROP_USE_THREADS:
		priv->use_threads = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * mcm_client_randr_event_cb:
 **/
static void
mcm_client_randr_event_cb (McmScreen *screen, McmClient *client)
{
	MateRROutput **outputs;
	guint i;

	egg_debug ("screens may have changed");

	/* replug devices */
	outputs = mcm_screen_get_outputs (screen, NULL);
	for (i=0; outputs[i] != NULL; i++)
		mcm_client_xrandr_add (client, outputs[i]);
}

/**
 * mcm_client_class_init:
 **/
static void
mcm_client_class_init (McmClientClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_client_finalize;
	object_class->get_property = mcm_client_get_property;
	object_class->set_property = mcm_client_set_property;

	/**
	 * McmClient:display-name:
	 */
	pspec = g_param_spec_string ("display-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

	/**
	 * McmClient:loading:
	 */
	pspec = g_param_spec_boolean ("loading", NULL, NULL,
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LOADING, pspec);

	/**
	 * McmClient:use-threads:
	 */
	pspec = g_param_spec_boolean ("use-threads", NULL, NULL,
				      TRUE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_THREADS, pspec);

	/**
	 * McmClient::added
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmClientClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	/**
	 * McmClient::removed
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmClientClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	/**
	 * McmClient::changed
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	g_type_class_add_private (klass, sizeof (McmClientPrivate));
}

/**
 * mcm_client_init:
 **/
static void
mcm_client_init (McmClient *client)
{
	const gchar *subsystems[] = {"usb", "video4linux", NULL};

	client->priv = MCM_CLIENT_GET_PRIVATE (client);
	client->priv->display_name = NULL;
	client->priv->loading_refcount = 0;
	client->priv->use_threads = FALSE;
	client->priv->init_cups = FALSE;
	client->priv->init_sane = FALSE;
	client->priv->settings = g_settings_new (MCM_SETTINGS_SCHEMA);
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->screen = mcm_screen_new ();
	g_signal_connect (client->priv->screen, "outputs-changed",
			  G_CALLBACK (mcm_client_randr_event_cb), client);

	/* use GUdev to find devices */
	client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (client->priv->gudev_client, "uevent",
			  G_CALLBACK (mcm_client_uevent_cb), client);
}

/**
 * mcm_client_finalize:
 **/
static void
mcm_client_finalize (GObject *object)
{
	McmClient *client = MCM_CLIENT (object);
	McmClientPrivate *priv = client->priv;
	McmDevice *device;
	guint i;

	/* disconnect anything that's about to fire */
	if (priv->refresh_id != 0)
		g_source_remove (priv->refresh_id);

	/* do not respond to changed events */
	for (i=0; i<priv->array->len; i++) {
		device = g_ptr_array_index (priv->array, i);
		g_signal_handlers_disconnect_by_func (device, G_CALLBACK (mcm_client_device_changed_cb), client);
	}

	g_free (priv->display_name);
	g_ptr_array_unref (priv->array);
	g_object_unref (priv->gudev_client);
	g_object_unref (priv->screen);
	g_object_unref (priv->settings);
	if (client->priv->init_cups)
		httpClose (priv->http);
#ifdef HAVE_SANE
	if (client->priv->init_sane)
		sane_exit ();
#endif

	G_OBJECT_CLASS (mcm_client_parent_class)->finalize (object);
}

/**
 * mcm_client_new:
 *
 * Return value: a new McmClient object.
 **/
McmClient *
mcm_client_new (void)
{
	if (mcm_client_object != NULL) {
		g_object_ref (mcm_client_object);
	} else {
		mcm_client_object = g_object_new (MCM_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (mcm_client_object, &mcm_client_object);
	}
	return MCM_CLIENT (mcm_client_object);
}

