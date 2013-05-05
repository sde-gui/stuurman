NULL=

SUBDIRS=ui

man_MANS=

desktopdir=$(datadir)/applications
desktop_in_files= \
	stuurman.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

defconfdir=$(sysconfdir)/xdg/stuurman/default
defconf_DATA = \
	$(NULL)

EXTRA_DIST= \
	$(desktop_DATA) \
	$(defconf_DATA) \
	$(NULL)
