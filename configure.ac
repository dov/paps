dnl Process this file with autoconf to produce a configuration script
AC_INIT(paps, 0.7.1, dov.grobgeld@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(src/paps.c)
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

AC_PROG_CC
AX_COMPILER_FLAGS
WARN_CFLAGS="$WARN_CFLAGS -Wno-format-y2k"

dnl Requires to declare wcwidth()
AC_GNU_SOURCE

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([PANGO], [pangocairo pangoft2])

AC_PROG_INTLTOOL([0.23])

GETTEXT_PACKAGE=paps
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Package name for gettext.])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT

datadir='${prefix}/${DATADIRNAME}'
AC_SUBST(datadir)

AC_OUTPUT(Makefile src/Makefile po/Makefile.in)
