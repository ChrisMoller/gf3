/* DO NOT MODIFY THIS FILE! */
/* It was automatically generated. */
/* Modify instead colours/buildcolourtable.c */
/* or colours/css-colours.txt. */

#ifndef CSS_COLOUR_TABLE_H
#define CSS_COLOUR_TABLE_H

typedef struct {
  gchar   *name;
  gboolean preset;
  gchar   *hex;
  gdouble  alpha;
  gdouble  red;
  gdouble  green;
  gdouble  blue;
  gdouble  hue;
  gdouble  saturation;
  gdouble  value;
} css_colours_s;

#define colour_name(c)   ((c).name)
#define colour_preset(c) ((c).preset)
#define colour_hex(c)    ((c).hex)
#define colour_alpha(c)  ((c).alpha)
#define colour_red(c)    ((c).red)
#define colour_green(c)  ((c).green)
#define colour_blue(c)   ((c).blue)
#define colour_hue(c)    ((c).hue)
#define colour_saturation(c) ((c).saturation)
#define colour_value(c)  ((c).value)

#define CSS_COLOUR_COUNT  140
css_colours_s css_colours[CSS_COLOUR_COUNT];
#endif   /* CSS_COLOUR_TABLE_H */

