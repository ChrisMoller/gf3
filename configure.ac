
# then do ./configure
# followed by make

AC_INIT([gf3], [1.0], [moller@mollerware.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
AM_PROG_AR
LT_INIT
AC_PROG_CC

AM_PROG_CC_C_O

GETTEXT_PACKAGE=gtk30
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

AC_CONFIG_HEADERS([config.h])

AM_MAINTAINER_MODE([enable])

AM_PROG_LEX
if test "x$LEX" != xflex; then
  LEX="$SHELL $missing_dir/missing flex"
 AC_SUBST([LEX_OUTPUT_ROOT], [lex.yy])
 AC_SUBST([LEXLIB], [''])
fi
AC_PROG_YACC

PKG_CHECK_MODULES([CAIRO], [cairo >= 1.13])
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

PKG_CHECK_MODULES([GTK],   [gtk+-3.0 >= 3.18.8])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES([PYTHON], [python3])
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)

PKG_CHECK_MODULES([PYTHON], [pygobject-3.0 >= 3.10])
AC_SUBST(PYGOBJECT_CFLAGS)
AC_SUBST(PYGOBJECT_LIBS)

PKG_CHECK_MODULES([GSL], [gsl >= 1.15])
AC_SUBST(GSL_CFLAGS)
AC_SUBST(GSL_LIBS)

PKG_CHECK_MODULES([FONTCONFIG], [fontconfig >= 2.11])
AC_SUBST(FONTCONFIG_CFLAGS)
AC_SUBST(FONTCONFIG_LIBS)

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  pluginsrcs/Makefile
  pluginsrcs/circle/Makefile
  pluginsrcs/lines/Makefile
])
AC_OUTPUT
