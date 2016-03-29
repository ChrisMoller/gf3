#include <alloca.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <values.h>

/********
	 make buildcolortable
	 ./buildcolortable <css-colours.txt
*******/

#define MIN3(a,b,c) (fmin (a, fmin (b, c)))
#define MAX3(a,b,c) (fmax (a, fmax (b, c)))

static void
RGBtoHSV (double r, double g, double b, double *h, double *s, double *v)
{
  double min, max, delta;

  min = MIN3 (r, g, b );
  max = MAX3 (r, g, b );
  *v = max;				// v

  delta = max - min;

  if (max != 0) *s = delta / max;		// s
  else {
    *s = 0.0;
    *h = 0.0;
    return;
  }

  if (delta == 0.0)  *h = 0.0;			   // some shade of grey
  else if (r == max) *h = (g - b) / delta;	   // between yellow & magenta
  else if (g == max) *h = 2 + ( b - r ) / delta;   // between cyan & yellow
  else               *h = 4 + (r - g) / delta;	   // between magenta & cyan

#if 0
  if (isnan (*h)) {
    printf ("isnan %f %f %f %f %f %f\n", r, g, b, delta, max, min);
  }
#endif

  *h *= M_PI / 3.0;				// degrees
  if (*h < 0) *h += 2.0 * M_PI;

}

main ()
{
  char *name = alloca (256);
  char  red[3];
  char  green[3];
  char  blue[3];
  int   count = 0;

  FILE *file;

  file = fopen ("css_colour_table.c", "w");
  fprintf (file, "/* DO NOT MODIFY THIS FILE! */\n");
  fprintf (file, "/* It was automatically generated. */\n");
  fprintf (file, "/* Modify instead colours/buildcolourtable.c */\n");
  fprintf (file, "/* or colours/css-colours.txt. */\n");
  fprintf (file, "\n");
  fprintf (file, "#include <math.h>\n");
  fprintf (file, "#include <gtk/gtk.h>\n");
  fprintf (file, "#include \"css_colour_table.h\"\n");
  fprintf (file, "css_colours_s css_colours[CSS_COLOUR_COUNT] = {\n");

  while (EOF != scanf ("%s #%2s%2s%2s \n", name, &red, &green, &blue)) {
    double rv = ((double)strtol (red,   NULL, 16)) / 255.0;
    double gv = ((double)strtol (green, NULL, 16)) / 255.0;
    double bv = ((double)strtol (blue,  NULL, 16)) / 255.0;
    double hv, sv, vv;
    
    RGBtoHSV (rv, gv, bv, &hv, &sv, &vv);
    
#if USE_COLOUR_MARKUP
    fprintf (file, "  {\"<span background=\\\"#%2s%2s%2s\\\">%s</span>\", \"#%s%s%sFF\", TRUE, 1.0, %f, %f, %f, %f, %f, %f},\n",
	     red, green, blue,
	     name,
	     red, green, blue,
	     rv, gv, bv, hv, sv, vv);
#else
    fprintf (file, "  {\"%s\", TRUE, \"#%s%s%sFF\", 1.0, %f, %f, %f, %f, %f, %f},\n",
	     name,
	     red, green, blue,
	     rv, gv, bv, hv, sv, vv);
#endif
    count++;
  }
  fprintf (file, "};\n");
  fprintf (file, "\n");
  //  fprintf (file, "gint css_colour_count = %d;\n", count);
  fclose (file);

  file = fopen ("css_colour_table.h", "w");
  fprintf (file, "/* DO NOT MODIFY THIS FILE! */\n");
  fprintf (file, "/* It was automatically generated. */\n");
  fprintf (file, "/* Modify instead colours/buildcolourtable.c */\n");
  fprintf (file, "/* or colours/css-colours.txt. */\n");
  fprintf (file, "\n");
  fprintf (file, "#ifndef CSS_COLOUR_TABLE_H\n");
  fprintf (file, "#define CSS_COLOUR_TABLE_H\n");
  fprintf (file, "\n");
  fprintf (file, "typedef struct {\n");
  fprintf (file, "  gchar   *name;\n");
  fprintf (file, "  gboolean preset;\n");
  fprintf (file, "  gchar   *hex;\n");
  fprintf (file, "  gdouble  alpha;\n");
  fprintf (file, "  gdouble  red;\n");
  fprintf (file, "  gdouble  green;\n");
  fprintf (file, "  gdouble  blue;\n");
  fprintf (file, "  gdouble  hue;\n");
  fprintf (file, "  gdouble  saturation;\n");
  fprintf (file, "  gdouble  value;\n");
  fprintf (file, "} css_colours_s;\n");
  fprintf (file, "\n");
  fprintf (file, "#define colour_name(c)   ((c).name)\n");
  fprintf (file, "#define colour_preset(c) ((c).preset)\n");
  fprintf (file, "#define colour_hex(c)    ((c).hex)\n");
  fprintf (file, "#define colour_alpha(c)  ((c).alpha)\n");
  fprintf (file, "#define colour_red(c)    ((c).red)\n");
  fprintf (file, "#define colour_green(c)  ((c).green)\n");
  fprintf (file, "#define colour_blue(c)   ((c).blue)\n");
  fprintf (file, "#define colour_hue(c)    ((c).hue)\n");
  fprintf (file, "#define colour_saturation(c) ((c).saturation)\n");
  fprintf (file, "#define colour_value(c)  ((c).value)\n");
  fprintf (file, "\n");
  fprintf (file, "#define CSS_COLOUR_COUNT  %d\n", count);
  fprintf (file, "css_colours_s css_colours[CSS_COLOUR_COUNT];\n");
  fprintf (file, "#endif   /* CSS_COLOUR_TABLE_H */\n\n");
  fclose (file);
}

