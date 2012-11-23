SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

migrationdir = $(datadir)/MateConf/gsettings
dist_migration_DATA = org.mate.color-manager.gschema.migrate

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	mcm-prefs.desktop.in				\
	mcm-import.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = mcm-apply.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.mate.ColorManager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.mate.color-manager.gschema.xml


pkgdata_DATA =						\
	mcm-calibrate.ui				\
	mcm-spawn.ui					\
	mcm-picker.ui 					\
	mcm-prefs.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.mate.ColorManager.service			\
	mcm-apply.desktop				\
	mcm-prefs.desktop				\
	mcm-import.desktop

