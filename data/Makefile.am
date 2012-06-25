SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

@INTLTOOL_SCHEMAS_RULE@
schemadir = $(sysconfdir)/mateconf/schemas
schema_in_files = mate-color-manager.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

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

pkgdata_DATA =						\
	mcm-calibrate.ui				\
	mcm-spawn.ui					\
	mcm-prefs.ui

EXTRA_DIST =						\
	$(schema_in_files)				\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

if MATECONF_SCHEMAS_INSTALL
install-data-local:
	-MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) \
	$(MATECONFTOOL) --makefile-install-rule $(schema_DATA)
endif

if MATECONF_SCHEMAS_INSTALL
uninstall-local:
	-MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) \
	$(MATECONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.mate.ColorManager.service			\
	mate-color-manager.schemas			\
	mcm-apply.desktop				\
	mcm-prefs.desktop				\
	mcm-import.desktop

