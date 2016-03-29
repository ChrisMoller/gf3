bin_PROGRAMS = gf3
AM_YFLAGS = -d
AM_LFLAGS = -o$(LEX_OUTPUT_ROOT).c
DRAWING_SOURCES = ellipses.c \
                  lines.c
gf3_SOURCES = css_colour_table.c css_colour_table.h \
             css_colours.c css_colours.h \
             entities.c entities.h \
             fallbacks.h \
             gf.c gf.h \
             persistents.c persistents.h \
             preferences.c preferences.h \
             python.c python.h \
             rgbhsv.c rgbhsv.h \
             select_paper.c select_paper.h \
             select_pen.c select_pen.h \
             select_scale.c select_scale.h \
             utilities.c utilities.h \
             view.c view.h \
             intersects.c intersects.h \
             quartic.c quartic.h \
             xml.c xml.h xml-kwds.m4 \
             drawing.h \
             $(DRAWING_SOURCES)
BUILT_SOURCES = xml-kwds.h drawing_header.h drawing_struct.h

#   removing -Werror because the colour widget is depricated
#  gf_CFLAGS = -std=c99 -Wall -Werror 

gf3_CFLAGS = -std=c99 -Wall  \
   $(GTK_CFLAGS) $(CAIRO_CFLAGS) $(GDL_CFLAGS) $(PYTHON_CFLAGS) \
   $(GSL_CFLAGS) $(FONTCONFIG_CFLAGS) $(PYGOBJECT_CFLAGS) \
   -DSYSCONFDIR='"$(sysconfdir)/$(PACKAGE_NAME)"' \
   -DDATAROOTDIR='"$(datarootdir)/$(PACKAGE_NAME)"' \
   -DTOPSRCDIR='"$(abs_top_srcdir)"'

gf3_LDFLAGS = -export-dynamic $(GTK_LIBS) $(CAIRO_LIBS) $(GDL_LIBS) \
   $(PYTHON_LIBS) $(PYGOBJECT_LIBS) $(GSL_LIBS) $(FONTCONFIG_LIBS) \
   -lm -lhistory -ldl

xml-kwds.h : xml-kwds.m4
	m4 $< >$@

drawing_header.h : $(DRAWING_SOURCES)
	@rm -f $@
	@touch $@
	@echo "building $@"
	@echo "/* Generated file.  Do not modify. */" >> $@
	@echo "#ifndef DRAWING_HEADER_H" >> $@
	@echo "#define DRAWING_HEADER_H" >> $@
	@for i in $(DRAWING_SOURCES); do ./df_header.pl $$i >> $@; done
	@echo "#endif /* DRAWING_HEADER_H */" >> $@

drawing_struct.h : $(DRAWING_SOURCES)
	@rm -f $@
	@touch $@
	@echo "building $@"
	@echo "/* Generated file.  Do not modify. */" >> $@
	@echo "#ifndef DRAWING_STRUCT_H" >> $@
	@echo "#define DRAWING_STRUCT_H" >> $@
	@echo "static button_s buttons[] = {" >> $@
	@for i in $(DRAWING_SOURCES); do ./df_struct.pl $$i >> $@; done
	@echo '  {"off.png", NULL, TRUE, "Inactive."}' >> $@
	@echo "};" >> $@
	@echo "static gint button_count = sizeof(buttons) / sizeof(button_s);" >> $@
	@echo "#endif /* DRAWING_STRUCT_H */" >> $@

# -leak-check=full
#  algrind --tool=memcheck --leak-check=full --leak-resolution=high -
#                -num-callers=20 --log-file=vgdump ./gfig
#  
#VALGRIND_FLAGS = --gen-suppressions=all --run-libc-freeres=yes \
#    --suppressions=valgrind.supp
VALGRIND_FLAGS = --gen-suppressions=all --run-libc-freeres=yes

get-gsettings:
	gsettings get org.gtk.Settings.ColorChooser custom-colors

# the purpose of this is to clear out the junk colours created
# in testing gfig.  there's probably a way to do it from within
# gfig, but I'm lazy
reset-gsettings:
	gsettings reset org.gtk.Settings.ColorChooser custom-colors

valgrind.log : gf3
	valgrind $(VALGRIND_FLAGS) --log-file=$@ ./gf

clean-local: clean-local-check
.PHONY: clean-local-check reset-gsettings get-gsettings
clean-local-check:
	rm -f valgrind.log drawing_header.h drawing_struct.h
