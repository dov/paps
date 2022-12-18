man_MANS = paps.1

bin_PROGRAMS = paps
paps_CXXFLAGS  = $(WARN_CFLAGS) $(PANGO_CFLAGS) $(FMT_CFLAGS)
paps_SOURCES = paps.cc format_from_dict.cc
paps_LDADD =  $(WARN_LDFLAGS) $(PANGO_LIBS) $(FMT_LIBS) $(lib_LIBRARIES) $(all_libraries)
paps_DEPENDENCIES = $(lib_LIBRARIES)

AM_CPPFLAGS = -DGETTEXT_PACKAGE='"$(GETTEXT_PACKAGE)"' -DDATADIR='"$(datadir)"'

EXTRA_DIST = paps.1


-include $(top_srcdir)/git.mk
