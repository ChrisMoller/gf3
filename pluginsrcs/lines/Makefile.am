libexecdir = $(gf3plugindir)/plug-ins

lib_LTLIBRARIES = lines.la

lines_la_SOURCES = lines.c

# I doubt this is how you're supposed to get libtool to stick plugins in the
# right place, but I can't think of anything else.
libdir=$(datarootdir)/$(PACKAGE_NAME)/plugins

lines_la_CFLAGS = -std=c99 -Wall  \
   $(GTK_CFLAGS) $(CAIRO_CFLAGS) $(GDL_CFLAGS) $(PYTHON_CFLAGS) \
   $(GSL_CFLAGS) $(FONTCONFIG_CFLAGS) $(PYGOBJECT_CFLAGS) \
   -DSYSCONFDIR='"$(sysconfdir)/$(PACKAGE_NAME)"' \
   -DDATAROOTDIR='"$(datarootdir)/$(PACKAGE_NAME)"'

lines_la_LDFLAGS = -module $(GTK_LIBS) $(CAIRO_LIBS) $(GDL_LIBS) \
    $(PYTHON_LIBS) $(PYGOBJECT_LIBS) $(GSL_LIBS) $(FONTCONFIG_LIBS) -lm

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(includedir)
