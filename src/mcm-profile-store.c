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
 * SECTION:mcm-profile-store
 * @short_description: object to hold an array of profiles.
 *
 * This object holds an array of %McmProfiles, and watches both the directories
 * for changes.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <mateconf/mateconf-client.h>

#include "mcm-profile-store.h"
#include "mcm-utils.h"

#include "egg-debug.h"

static void     mcm_profile_store_finalize	(GObject     *object);

#define MCM_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_PROFILE_STORE, McmProfileStorePrivate))

static gboolean	mcm_profile_store_add_profiles_for_path	(McmProfileStore *profile_store, const gchar *path);
static void	mcm_profile_store_add_profiles		(McmProfileStore *profile_store);

/**
 * McmProfileStorePrivate:
 *
 * Private #McmProfileStore data
 **/
struct _McmProfileStorePrivate
{
	GPtrArray			*profile_array;
	GPtrArray			*monitor_array;
	GPtrArray			*directory_array;
	GVolumeMonitor			*volume_monitor;
	MateConfClient			*mateconf_client;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (McmProfileStore, mcm_profile_store, G_TYPE_OBJECT)

/**
 * mcm_profile_store_get_array:
 *
 * @profile_store: a valid %McmProfileStore instance
 *
 * Gets the profile list.
 *
 * Return value: an array, free with g_ptr_array_unref()
 **/
GPtrArray *
mcm_profile_store_get_array (McmProfileStore *profile_store)
{
	g_return_val_if_fail (MCM_IS_PROFILE_STORE (profile_store), NULL);
	return g_ptr_array_ref (profile_store->priv->profile_array);
}

/**
 * mcm_profile_store_in_array:
 **/
static gboolean
mcm_profile_store_in_array (GPtrArray *array, const gchar *text)
{
	const gchar *tmp;
	guint i;

	for (i=0; i<array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (text, tmp) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * mcm_profile_store_get_by_filename:
 *
 * @profile_store: a valid %McmProfileStore instance
 * @filename: the profile filename
 *
 * Gets a profile.
 *
 * Return value: a valid %McmProfile or %NULL. Free with g_object_unref()
 **/
McmProfile *
mcm_profile_store_get_by_filename (McmProfileStore *profile_store, const gchar *filename)
{
	guint i;
	McmProfile *profile = NULL;
	McmProfile *profile_tmp;
	const gchar *filename_tmp;
	McmProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (MCM_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		filename_tmp = mcm_profile_get_filename (profile_tmp);
		if (g_strcmp0 (filename, filename_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * mcm_profile_store_notify_filename_cb:
 **/
static void
mcm_profile_store_notify_filename_cb (McmProfile *profile, GParamSpec *pspec, McmProfileStore *profile_store)
{
	const gchar *description;
	McmProfileStorePrivate *priv = profile_store->priv;

	/* remove from list */
	g_ptr_array_remove (priv->profile_array, profile);

	/* emit a signal */
	description = mcm_profile_get_description (profile);
	egg_debug ("emit removed (and changed): %s", description);
	g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, profile);
	g_signal_emit (profile_store, signals[SIGNAL_CHANGED], 0);
}

/**
 * mcm_profile_store_add_profile:
 **/
static gboolean
mcm_profile_store_add_profile (McmProfileStore *profile_store, GFile *file)
{
	gboolean ret = FALSE;
	McmProfile *profile = NULL;
	GError *error = NULL;
	gchar *filename = NULL;
	McmProfileStorePrivate *priv = profile_store->priv;

	/* already added? */
	filename = g_file_get_path (file);
	profile = mcm_profile_store_get_by_filename (profile_store, filename);
	if (profile != NULL)
		goto out;

	/* parse the profile name */
	profile = mcm_profile_default_new ();
	ret = mcm_profile_parse (profile, file, &error);
	if (!ret) {
		egg_warning ("failed to add profile '%s': %s", filename, error->message);
		g_error_free (error);
		goto out;		
	}

	/* add to array */
	egg_debug ("parsed new profile '%s'", filename);
	g_ptr_array_add (priv->profile_array, g_object_ref (profile));
	g_signal_connect (profile, "notify::filename", G_CALLBACK(mcm_profile_store_notify_filename_cb), profile_store);

	/* emit a signal */
	egg_debug ("emit added (and changed): %s", filename);
	g_signal_emit (profile_store, signals[SIGNAL_ADDED], 0, profile);
	g_signal_emit (profile_store, signals[SIGNAL_CHANGED], 0);
out:
	g_free (filename);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * mcm_profile_store_file_monitor_changed_cb:
 **/
static void
mcm_profile_store_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, McmProfileStore *profile_store)
{
	gchar *path = NULL;

	/* only care about created objects */
	if (event_type != G_FILE_MONITOR_EVENT_CREATED)
		goto out;

	/* just rescan everything */
	path = g_file_get_path (file);
	if (g_strrstr (path, ".goutputstream") != NULL) {
		egg_debug ("ignoring gvfs temporary file");
		goto out;
	}
	egg_debug ("%s was added, rescanning everything", path);
	mcm_profile_store_add_profiles (profile_store);
out:
	g_free (path);
}

/**
 * mcm_profile_store_add_profiles_for_path:
 **/
static gboolean
mcm_profile_store_add_profiles_for_path (McmProfileStore *profile_store, const gchar *path)
{
	GDir *dir = NULL;
	GError *error = NULL;
	gboolean ret = TRUE;
	const gchar *name;
	gchar *full_path;
	McmProfileStorePrivate *priv = profile_store->priv;
	GFileMonitor *monitor = NULL;
	GFile *file = NULL;

	/* add if correct type */
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {

		/* check the file actually is a profile */
		file = g_file_new_for_path (path);
		ret = mcm_utils_is_icc_profile (file);
		if (ret) {
			mcm_profile_store_add_profile (profile_store, file);
			goto out;
		}

		/* invalid file */
		egg_debug ("not recognized as ICC profile: %s", path);
		goto out;
	}

	/* get contents */
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		egg_debug ("failed to open: %s", error->message);
		g_error_free (error);
		ret = FALSE;
		goto out;
	}

	/* add an inotify watch if not already added? */
	ret = mcm_profile_store_in_array (priv->directory_array, path);
	if (!ret) {
		file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);
		if (monitor == NULL) {
			egg_debug ("failed to monitor path: %s", error->message);
			g_error_free (error);
			goto out;
		}

		/* don't allow many files to cause re-scan after rescan */
		g_file_monitor_set_rate_limit (monitor, 1000);
		g_signal_connect (monitor, "changed", G_CALLBACK(mcm_profile_store_file_monitor_changed_cb), profile_store);
		g_ptr_array_add (priv->monitor_array, g_object_ref (monitor));
		g_ptr_array_add (priv->directory_array, g_strdup (path));
	}

	/* process entire tree */
	do {
		name = g_dir_read_name (dir);
		if (name == NULL)
			break;

		/* make the compete path */
		full_path = g_build_filename (path, name, NULL);
		mcm_profile_store_add_profiles_for_path (profile_store, full_path);
		g_free (full_path);
	} while (TRUE);
out:
	if (monitor != NULL)
		g_object_unref (monitor);
	if (file != NULL)
		g_object_unref (file);
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * mcm_profile_store_add_profiles_from_mounted_volume:
 **/
static void
mcm_profile_store_add_profiles_from_mounted_volume (McmProfileStore *profile_store, GMount *mount)
{
	GFile *root;
	gchar *path;
	gchar *path_root;
	const gchar *type;
	GFileInfo *info;
	GError *error = NULL;

	/* get the mount root */
	root = g_mount_get_root (mount);
	path_root = g_file_get_path (root);
	if (path_root == NULL)
		goto out;

	/* get the filesystem type */
	info = g_file_query_filesystem_info (root, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, NULL, &error);
	if (info == NULL) {
		egg_warning ("failed to get filesystem type: %s", error->message);
		g_error_free (error);
		goto out;
	}
	type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	egg_debug ("filesystem mounted on %s has type %s", path_root, type);

	/* only scan hfs volumes for OSX */
	if (g_strcmp0 (type, "hfs") == 0) {
		path = g_build_filename (path_root, "Library", "ColorSync", "Profiles", "Displays", NULL);
		mcm_profile_store_add_profiles_for_path (profile_store, path);
		g_free (path);

		/* no more matching */
		goto out;
	}

	/* and fat32 and ntfs for windows */
	if (g_strcmp0 (type, "ntfs") == 0 || g_strcmp0 (type, "msdos") == 0) {

		/* Windows XP */
		path = g_build_filename (path_root, "Windows", "system32", "spool", "drivers", "color", NULL);
		mcm_profile_store_add_profiles_for_path (profile_store, path);
		g_free (path);

		/* Windows 2000 */
		path = g_build_filename (path_root, "Winnt", "system32", "spool", "drivers", "color", NULL);
		mcm_profile_store_add_profiles_for_path (profile_store, path);
		g_free (path);

		/* Windows 98 and ME */
		path = g_build_filename (path_root, "Windows", "System", "Color", NULL);
		mcm_profile_store_add_profiles_for_path (profile_store, path);
		g_free (path);

		/* no more matching */
		goto out;
	}
out:
	g_free (path_root);
	g_object_unref (root);
}

/**
 * mcm_profile_store_add_profiles_from_mounted_volumes:
 **/
static void
mcm_profile_store_add_profiles_from_mounted_volumes (McmProfileStore *profile_store)
{
	GList *mounts, *l;
	GMount *mount;
	McmProfileStorePrivate *priv = profile_store->priv;

	/* get all current mounts */
	mounts = g_volume_monitor_get_mounts (priv->volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		mcm_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
		g_object_unref (mount);
	}
	g_list_free (mounts);
}

/**
 * mcm_profile_store_add_profiles:
 **/
static void
mcm_profile_store_add_profiles (McmProfileStore *profile_store)
{
	gchar *path;
	gboolean ret;
	GError *error;
	McmProfileStorePrivate *priv = profile_store->priv;

	/* get OSX and Linux system-wide profiles */
	mcm_profile_store_add_profiles_for_path (profile_store, "/usr/share/color/icc");
	mcm_profile_store_add_profiles_for_path (profile_store, "/usr/local/share/color/icc");
	mcm_profile_store_add_profiles_for_path (profile_store, "/Library/ColorSync/Profiles/Displays");

	/* get OSX and Windows system-wide profiles when using Linux */
	ret = mateconf_client_get_bool (priv->mateconf_client, MCM_SETTINGS_USE_PROFILES_FROM_VOLUMES, NULL);
	if (ret)
		mcm_profile_store_add_profiles_from_mounted_volumes (profile_store);

	/* get Linux per-user profiles */
	path = g_build_filename (g_get_home_dir (), ".color", "icc", NULL);
	ret = mcm_utils_mkdir_with_parents (path, &error);
	if (!ret) {
		egg_error ("failed to create directory on startup: %s", error->message);
		g_error_free (error);
	} else {
		mcm_profile_store_add_profiles_for_path (profile_store, path);
	}
	g_free (path);

	/* get OSX per-user profiles */
	path = g_build_filename (g_get_home_dir (), "Library", "ColorSync", "Profiles", NULL);
	mcm_profile_store_add_profiles_for_path (profile_store, path);
	g_free (path);
}

/**
 * mcm_profile_store_volume_monitor_mount_added_cb:
 **/
static void
mcm_profile_store_volume_monitor_mount_added_cb (GVolumeMonitor *volume_monitor, GMount *mount, McmProfileStore *profile_store)
{
	mcm_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
}

/**
 * mcm_profile_store_class_init:
 **/
static void
mcm_profile_store_class_init (McmProfileStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_profile_store_finalize;

	/**
	 * McmProfileStore::added
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmProfileStoreClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	/**
	 * McmProfileStore::removed
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmProfileStoreClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	/**
	 * McmProfileStore::changed
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmProfileStoreClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (McmProfileStorePrivate));
}

/**
 * mcm_profile_store_init:
 **/
static void
mcm_profile_store_init (McmProfileStore *profile_store)
{
	profile_store->priv = MCM_PROFILE_STORE_GET_PRIVATE (profile_store);
	profile_store->priv->profile_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->monitor_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
	profile_store->priv->mateconf_client = mateconf_client_get_default ();

	/* watch for volumes to be connected */
	profile_store->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (profile_store->priv->volume_monitor,
			  "mount-added",
			  G_CALLBACK(mcm_profile_store_volume_monitor_mount_added_cb),
			  profile_store);

	/* get profiles */
	mcm_profile_store_add_profiles (profile_store);
}

/**
 * mcm_profile_store_finalize:
 **/
static void
mcm_profile_store_finalize (GObject *object)
{
	McmProfileStore *profile_store = MCM_PROFILE_STORE (object);
	McmProfileStorePrivate *priv = profile_store->priv;

	g_ptr_array_unref (priv->profile_array);
	g_ptr_array_unref (priv->monitor_array);
	g_ptr_array_unref (priv->directory_array);
	g_object_unref (priv->volume_monitor);
	g_object_unref (priv->mateconf_client);

	G_OBJECT_CLASS (mcm_profile_store_parent_class)->finalize (object);
}

/**
 * mcm_profile_store_new:
 *
 * Return value: a new McmProfileStore object.
 **/
McmProfileStore *
mcm_profile_store_new (void)
{
	McmProfileStore *profile_store;
	profile_store = g_object_new (MCM_TYPE_PROFILE_STORE, NULL);
	return MCM_PROFILE_STORE (profile_store);
}

