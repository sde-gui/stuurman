NULL=

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/stuurman"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/stuurman/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = stuurman

stuurman_SOURCES = \
	pcmanfm.c \
	pcmanfm.h \
	app-config.c \
	app-config.h \
	main-win.c \
	main-win.h \
	tab-page.c \
	tab-page.h \
	volume-manager.c \
	volume-manager.h \
	pref.c \
	pref.h \
	utils.c \
	utils.h \
	gseal-gtk-compat.h \
	single-inst.c \
	single-inst.h \
	$(NULL)

EXTRA_DIST= \
	main-win-ui.c \
	$(NULL)

stuurman_CFLAGS = \
	$(XLIB_CFLAGS) \
	$(FM_CFLAGS) \
	$(G_CAST_CHECKS) \
	-Wall \
	-Werror-implicit-function-declaration \
	$(NULL)

stuurman_LDADD = \
	$(XLIB_LIBS) \
	$(FM_LIBS) \
	$(NULL)

noinst_PROGRAMS=xml-purge
xml_purge_SOURCES=xml-purge.c
xml_purge_CFLAGS=$(GIO_CFLAGS)
xml_purge_LDADD=$(GIO_LIBS)
