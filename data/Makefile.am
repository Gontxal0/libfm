NULL=

SUBDIRS=ui

man_MANS = libfm-pref-apps.1 lxshortcut.1

mimedir = $(datadir)/mime/packages
mime_DATA = libfm.xml

libfmconfdir = $(sysconfdir)/xdg/libfm
libfmconf_DATA = \
	libfm.conf \
	$(NULL)

desktopdir=$(datadir)/applications
desktop_in_files= \
	libfm-pref-apps.desktop.in \
	lxshortcut.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

libfmdatafilesdir = $(datadir)/libfm
libfmdatafiles_DATA = \
	archivers.list \
	terminals.list \
	$(NULL)

EXTRA_DIST= \
	$(libfmconf_DATA) \
	$(mime_DATA) \
	$(libfmdatafiles_DATA) \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(NULL)

test:
