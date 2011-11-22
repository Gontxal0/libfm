NULL=

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)../ \
	-I$(srcdir)/../../base \
	-I$(srcdir)/../../actions \
	-I$(srcdir)/../../job \
	-I$(srcdir)/../../jobs \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/libfm/ui"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)/libfm"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-include $(CONFIG_HEADER) \
	$(NULL)

noinst_LTLIBRARIES = libfmgtkui.la

libfmgtkui_la_SOURCES = \
	gtk-ui.vala \
	rename-dlg.vala \
	$(NULL)

libfmgtkui_la_CFLAGS = \
	$(GIO_CFLAGS) \
	$(GTK_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

VALAFLAGS = \
	--thread \
	--vapidir=$(srcdir)/../../vapi \
	--pkg config \
	--pkg gtk+-2.0 \
	--pkg pango \
	--pkg gio-2.0 \
    --pkg posix \
    --pkg libfm \
    --pkg fm-jobs \
    --vapi $(srcdir)/../../vapi/fm-gtk-ui.vapi \
    --header fm-gtk-ui.h \
	$(NULL)

libfmgtkuiincludedir = $(includedir)/libfm/libfm
libfmgtkuiinclude_HEADERS = \
	fm-gtk-ui.h \
	$(NULL)

libfmgtkui_la_LDFLAGS = \
	$(NULL)

libfmgtkui_la_LIBADD = \
	$(GIO_LIBS) \
	$(GTK_LIBS) \
	$(INTLLIBS) \
	$(NULL)
