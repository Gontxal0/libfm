NULL=

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)../ \
	-I$(srcdir)/../base \
	-I$(srcdir)/../actions \
	-I$(srcdir)/../job \
	-I$(srcdir)/../jobs \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)/libfm"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-include $(CONFIG_HEADER) \
	$(NULL)

noinst_LTLIBRARIES = libfmjobs.la

libfmjobs_la_SOURCES = \
	jobs.vala \
	job.vala \
	file-job.vala \
	copy-job.vala \
	delete-job.vala \
	trash-job.vala \
	change-attr-job.vala \
	query-info-job.vala \
	create-file-job.vala \
	ui.vala \
	$(NULL)

libfmjobs_la_CFLAGS = \
	$(GIO_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

VALAFLAGS = \
	--thread \
	--vapidir=$(srcdir)/../vapi \
	--pkg config \
	--pkg gio-2.0 \
    --pkg posix \
    --pkg libfm \
    --vapi $(srcdir)/../vapi/fm-jobs.vapi \
    --header fm-jobs.h \
	--disable-warnings \
	$(NULL)

libfmjobsincludedir = $(includedir)/libfm/libfm
libfmjobsinclude_HEADERS = \
	fm-jobs.h \
	$(NULL)

libfmjobs_la_LDFLAGS = \
	$(NULL)

libfmjobs_la_LIBADD = \
	$(GIO_LIBS) \
	$(INTLLIBS) \
	$(NULL)
