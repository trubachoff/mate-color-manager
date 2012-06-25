themedir = $(datadir)/icons/mate
size = 64x64
context = apps

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA = 			  	\
	mate-color-manager.png

install-data-hook:
	mkdir -p $(DESTDIR)$(DATADIR)/icons/mate/$(size)/mimetypes;
	ln -fs $(iconsdir)/mate-color-manager.png $(DESTDIR)$(DATADIR)/icons/mate/$(size)/mimetypes/application-vnd.iccprofile.png;

EXTRA_DIST =		\
	$(icons_DATA)
