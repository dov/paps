dnl ## --------------------------------------------------------- ##
dnl ##  Check if libpaper is available                           ##
dnl ##                           demaille@inf.enst.fr            ##
dnl ## --------------------------------------------------------- ##
dnl
dnl

# serial 1

AC_DEFUN([ad_FUNC_SYSTEMPAPERNAME],
[
  AC_CHECK_LIB(paper, systempapername, 
    dnl Action if found
    [
      AC_DEFINE([HAVE_SYSTEMPAPERNAME], 1,
                [Define if you have the systempapername function])
      LIBS="$LIBS -lpaper"
      AC_CHECK_HEADERS(paper.h)
    ])
])
