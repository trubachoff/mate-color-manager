INCLUDES =							\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(MATEDESKTOP_CFLAGS)			\
	$(UNIQUE_CFLAGS)				\
	$(VTE_CFLAGS)					\
	$(LCMS_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(CUPS_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(TIFF_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(EXIV_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DMATE_DESKTOP_USE_UNSTABLE_API	\
	$(GUDEV_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"	\
	-DSBINDIR=\"$(sbindir)\"		\
	-DDATADIR=\"$(datadir)\"		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""		\
	-DLOCALEDIR=\""$(localedir)"\"	\
	-DMCM_SYSTEM_PROFILES_DIR="\"$(MCM_SYSTEM_PROFILES_DIR)"\" \
	-DMCM_DATA=\"$(pkgdatadir)\"

noinst_LIBRARIES = libmcmshared.a
libmcmshared_a_SOURCES =		\
	egg-debug.c					\
	egg-debug.h					\
	mcm-image.c					\
	mcm-image.h					\
	mcm-screen.c				\
	mcm-screen.h				\
	mcm-xyz.c					\
	mcm-xyz.h					\
	mcm-exif.c					\
	mcm-exif.h					\
	mcm-print.c					\
	mcm-print.h					\
	mcm-utils.c					\
	mcm-utils.h					\
	mcm-enum.c					\
	mcm-enum.h					\
	mcm-clut.c					\
	mcm-clut.h					\
	mcm-edid.c					\
	mcm-edid.h					\
	mcm-dmi.c					\
	mcm-dmi.h					\
	mcm-tables.c				\
	mcm-tables.h				\
	mcm-xserver.c				\
	mcm-xserver.h				\
	mcm-client.c				\
	mcm-client.h				\
	mcm-colorimeter.c			\
	mcm-colorimeter.h			\
	mcm-device.c				\
	mcm-device.h				\
	mcm-device-xrandr.c			\
	mcm-device-xrandr.h			\
	mcm-device-udev.c			\
	mcm-device-udev.h			\
	mcm-device-cups.c			\
	mcm-device-cups.h			\
	mcm-device-virtual.c		\
	mcm-device-virtual.h		\
	mcm-cie-widget.c			\
	mcm-cie-widget.h			\
	mcm-trc-widget.c			\
	mcm-trc-widget.h			\
	mcm-gamma-widget.c			\
	mcm-gamma-widget.h			\
	mcm-profile-store.c			\
	mcm-profile-store.h			\
	mcm-profile-lcms1.c			\
	mcm-profile-lcms1.h			\
	mcm-profile.c				\
	mcm-profile.h 				\
	mcm-calibrate.c 			\
	mcm-calibrate.h 			\
	mcm-calibrate-argyll.c		\
	mcm-calibrate-argyll.h		\
	mcm-calibrate-manual.c		\
	mcm-calibrate-manual.h 		\
	mcm-calibrate-dialog.c 		\
	mcm-calibrate-dialog.h 		\
	mcm-brightness.c 			\
	mcm-brightness.h

libmcmshared_a_SOURCES +=		\
	mcm-device-sane.c			\
	mcm-device-sane.h

libmcmshared_a_CFLAGS =			\
	$(WARNINGFLAGS_C)

sbin_PROGRAMS =					\
	mcm-install-system-wide

bin_PROGRAMS =					\
	mcm-inspect					\
	mcm-dump-profile			\
	mcm-fix-profile				\
	mcm-dump-edid				\
	mcm-apply					\
	mcm-prefs					\
	mcm-session					\
	mcm-import

noinst_PROGRAMS = \
	mcm-picker

if MCM_USE_EXIV
libexec_PROGRAMS = mcm-helper-exiv
mcm_helper_exiv_SOURCES = mcm-helper-exiv.cpp
mcm_helper_exiv_LDADD = $(EXIV_LIBS)
endif

mcm_install_system_wide_SOURCES =	\
	mcm-install-system-wide.c

mcm_install_system_wide_LDADD =		\
	$(GLIB_LIBS)

mcm_install_system_wide_CFLAGS =	\
	$(WARNINGFLAGS_C)

mcm_dump_edid_SOURCES =				\
	mcm-dump-edid.c

mcm_dump_edid_LDADD =				\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_dump_edid_CFLAGS =				\
	$(WARNINGFLAGS_C)

mcm_dump_profile_SOURCES =			\
	mcm-dump-profile.c

mcm_dump_profile_LDADD =			\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_dump_profile_CFLAGS =			\
	$(WARNINGFLAGS_C)

mcm_fix_profile_SOURCES =			\
	mcm-fix-profile.c

mcm_fix_profile_LDADD =				\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(LCMS_LIBS)					\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_fix_profile_CFLAGS =			\
	$(WARNINGFLAGS_C)

mcm_inspect_SOURCES =				\
	mcm-inspect.c

mcm_inspect_LDADD =					\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_inspect_CFLAGS =				\
	$(WARNINGFLAGS_C)

mcm_apply_SOURCES =					\
	mcm-apply.c

mcm_apply_LDADD =					\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_apply_CFLAGS =					\
	$(WARNINGFLAGS_C)

mcm_import_SOURCES =				\
	mcm-import.c

mcm_import_LDADD =					\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

mcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

mcm_prefs_SOURCES =					\
	mcm-prefs.c

mcm_prefs_LDADD =					\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(VTE_LIBS)						\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	-lm

mcm_prefs_CFLAGS =					\
	$(WARNINGFLAGS_C)

mcm_picker_SOURCES = \
	mcm-picker.c

mcm_picker_LDADD = \
	libmcmshared.a \
	$(GLIB_LIBS) \
	$(X11_LIBS) \
	$(MATEDESKTOP_LIBS) \
	$(UNIQUE_LIBS) \
	$(MATECONF_LIBS) \
	$(VTE_LIBS) \
	$(GUDEV_LIBS) \
	$(LCMS_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(XORG_LIBS) \
	$(GTK_LIBS) \
	$(SANE_LIBS) \
	$(CUPS_LIBS) \
	$(TIFF_LIBS) \
	$(EXIF_LIBS) \
	$(CANBERRA_LIBS) \
	-lm

mcm_picker_CFLAGS = \
	$(WARNINGFLAGS_C)

mcm_session_SOURCES =				\
	mcm-dbus.c						\
	mcm-dbus.h						\
	mcm-session.c

mcm_session_LDADD =					\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)						\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(CUPS_LIBS)					\
	-lm								\
	-lexif							\
	-ltiff

mcm_session_CFLAGS =				\
	$(WARNINGFLAGS_C)

if EGG_BUILD_TESTS

check_PROGRAMS =					\
	mcm-self-test

mcm_self_test_SOURCES =				\
	mcm-self-test.c					\
	$(NULL)

mcm_self_test_LDADD =				\
	libmcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)						\
	$(MATEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)						\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	-lm

mcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = mcm-self-test

endif

org.mate.ColorManager.h: org.mate.ColorManager.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool	\
		--prefix=mcm_dbus			\
		--mode=glib-server			\
		--output=org.mate.ColorManager.h	\
		$(srcdir)/org.mate.ColorManager.xml

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(MCM_SYSTEM_PROFILES_DIR); \
	fi

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =				\
	*~								\
	Makefile.in

EXTRA_DIST =						\
	org.mate.ColorManager.xml

BUILT_SOURCES =						\
	org.mate.ColorManager.h

