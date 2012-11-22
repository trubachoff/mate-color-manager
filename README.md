mate-color-manager
==================

fork of gnome-color-manager-2.32.0

Installation:

./autogen.sh

make

sudo make install


Build dependencies:

gtk2-devel, scrollkeeper, desktop-file-utils, gettext, libtool, vte-devel, mate-doc-utils,
unique-devel, intltool, libgudev1-devel, dbus-glib-devel, libXxf86vm-devel, libXrandr-devel,
mate-desktop-devel, lcms-devel, cups-devel, sane-backends-devel, libtiff-devel, libcanberra-devel,
mate-common, gtk-doc, mate-conf-devel, libmatenotify-devel

If start of mcm-prefs fails after installation use the --sync option

mcm-prefs --sync


News: 22.11.2012

-Update to 1.5.0
  -- Port to GSettings

