SUBDIRS = src pluginsrcs \
              pluginsrcs/circle \
              pluginsrcs/lines
ACLOCAL_AMFLAGS = -I m4
dist_doc_DATA = README
gf3imagedir = $(prefix)/share/@PACKAGE@/images
gf3image_DATA=src/images/*.png
gf3plugindir = $(prefix)/share/@PACKAGE@/plugins
#gf3plugin_DATA=pluginsrcs/circle/.libs/circle.so
EXTRA_DIST = autogen.sh src/colours src/images pluginsrcs src/tests \
             src/df_header.pl src/df_struct.pl
sysconfdir = @sysconfdir@

install-exec-local:
	fc-cache
