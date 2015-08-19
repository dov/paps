man_MANS = paps.1

bin_PROGRAMS = paps
paps_CFLAGS  = $(WARN_CFLAGS) $(PANGO_CFLAGS)
paps_SOURCES = paps.c
paps_LDADD = $(lib_LIBRARIES) $(all_libraries)
paps_LDFLAGS = $(WARN_LDFLAGS) $(PANGO_LIBS)
paps_DEPENDENCIES = $(lib_LIBRARIES)

EXTRA_DIST = paps.1


-include $(top_srcdir)/git.mk
