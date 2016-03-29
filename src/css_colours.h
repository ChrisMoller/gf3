#ifndef CSS_COLOURS_H
#define CSS_COLOURS_H

enum {
  COLOUR_NAME,
  COLOUR_HEX,
  COLOUR_ALPHA,
  COLOUR_RED,
  COLOUR_GREEN,
  COLOUR_BLUE,
  COLOUR_HUE,
  COLOUR_SATURATION,
  COLOUR_VALUE,
  COLOUR_N_COLUMNS
};

#define ppd_css(p)	((p)->css_button)

const gchar  *get_css_colour_name (gint idx);
GtkWidget    *css_button_new (GtkTreeSelection **css_button_p,
			      gchar *which, pen_s *pen);
gboolean colour_table_modified (void);
void store_custom_colours (GKeyFile *key_file);
void build_colours_store (void);
void append_colour_ety (css_colours_s *ety);
css_colours_s *find_css_colour_rgba (const gchar *str, GdkRGBA *rgba);

#endif   /* CSS_COLOURS_H */

