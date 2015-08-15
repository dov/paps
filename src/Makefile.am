man_MANS = paps.1

bin_PROGRAMS = paps
paps_CFLAGS  = -Wall `pkg-config --cflags pangocairo pangoft2`
paps_SOURCES = paps.c
paps_LDADD = $(lib_LIBRARIES) $(all_libraries)
paps_LDFLAGS = `pkg-config --libs pangocairo pangoft2`
paps_DEPENDENCIES = $(lib_LIBRARIES)

EXTRA_DIST = paps.1

