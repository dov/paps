dnl Process this file with autoconf to produce a configuration script
AC_INIT(paps, 0.7.0, dov.grobgeld@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/paps.c)

AM_INIT_AUTOMAKE

AC_PROG_CC
AX_COMPILER_FLAGS
WARN_CFLAGS="$WARN_CFLAGS -Wno-format-y2k"

dnl Requires to declare wcwidth()
AC_GNU_SOURCE

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([PANGO], [pangocairo pangoft2])

AC_OUTPUT(Makefile src/Makefile)
