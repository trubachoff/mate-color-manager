themedir = $(datadir)/icons/mate
size = scalable
context = apps

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = 		 		\
	mate-color-manager.svg

appiconsdir = $(datadir)/mate-color-manager/icons
appicons_DATA = 	 		\
	lcd.svg	  			\
	crt.svg 			\
	projector.svg

install-data-hook:
	mkdir -p $(DESTDIR)$(DATADIR)/icons/mate/$(size)/mimetypes;
	ln -fs $(iconsdir)/mate-color-manager.svg $(DESTDIR)$(DATADIR)/icons/mate/$(size)/mimetypes/application-vnd.iccprofile.svg;

EXTRA_DIST =				\
	$(icons_DATA)			\
	$(appicons_DATA)

