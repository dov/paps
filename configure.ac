dnl Process this file with autoconf to produce a configuration script
AC_INIT(paps, 0.8.0, dov.grobgeld@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_SRCDIR(src/paps.cc)
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_CC
AX_COMPILER_FLAGS
WARN_CFLAGS="$WARN_CFLAGS -Wno-format-y2k"

dnl Requires to declare wcwidth()
AC_GNU_SOURCE

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([PANGO], [pangocairo pangoft2])
PKG_CHECK_MODULES(FMT, fmt >= 6.0)
AC_SUBST(FMT_CFLAGS)
AC_SUBST(FMT_LIBS)
AC_PROG_INTLTOOL([0.23])
LT_INIT

ad_FUNC_SYSTEMPAPERNAME
AS_IF([test "$ac_cv_lib_paper_systempapername" = "no" -o "$ac_cv_header_paper_h" = "no"],
  [AC_MSG_ERROR([paps needs libpaper to work])])

GETTEXT_PACKAGE=paps
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Package name for gettext.])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT

datadir='${prefix}/${DATADIRNAME}'
AC_SUBST(datadir)

AC_CONFIG_FILES([Makefile src/Makefile po/Makefile.in])
AC_OUTPUT()
