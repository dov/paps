man_MANS = paps.1
lib_LIBRARIES = libpaps.a
libpaps_a_SOURCES = libpaps.c
libpaps_a_inc_HEADERS = libpaps.h
libpaps_a_incdir = $(includedir)

bin_PROGRAMS = paps
paps_CFLAGS  = -Wall 
paps_SOURCES = paps.c
paps_LDADD = $(lib_LIBRARIES) $(all_libraries)
paps_LDFLAGS = `pkg-config --libs pangoft2`
paps_DEPENDENCIES = $(lib_LIBRARIES)

EXTRA_DIST = test_libpaps.c paps.1

# set the include path found by configure
INCLUDES= $(all_includes) `pkg-config --cflags pangoft2`

# Test program
noinst_PROGRAMS = test_libpaps


test_libpaps_SOURCES = test_libpaps.c
test_libpaps_LDADD = $(lib_LIBRARIES) $(all_libraries)
test_libpaps_LDFLAGS = `pkg-config --libs pangoft2`
test_libpaps_DEPENDENCIES = $(lib_LIBRARIES) 

