EXTRA_DIST =						\
	mcm-apply.sgml					\
	mcm-prefs.sgml					\
	mcm-import.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	mcm-apply.1					\
	mcm-prefs.1					\
	mcm-import.1
endif

if HAVE_DOCBOOK2MAN
mcm-apply.1: mcm-apply.sgml
	docbook2man $? > /dev/null
mcm-prefs.1: mcm-prefs.sgml
	docbook2man $? > /dev/null
mcm-import.1: mcm-import.sgml
	docbook2man $? > /dev/null
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*

