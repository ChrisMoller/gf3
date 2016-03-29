#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <math.h>
//#include <values.h>

#define MIN3(a,b,c) (fmin (a, fmin (b, c)))
#define MAX3(a,b,c) (fmax (a, fmax (b, c)))

void
HSVtoRGB (gdouble *r, gdouble *g, gdouble *b, gdouble h, gdouble s, gdouble v)
{
  int i;
  gdouble f, p, q, t;

  if (s == 0) {
    // achromatic (grey)
    *r = *g = *b = v;
    return;
  }

  h /= (G_PI / 3.0);			// sector 0 to 5
  i = floor (h);
  f = h - i;			// factorial part of h
  p = v * ( 1 - s );
  q = v * ( 1 - s * f );
  t = v * ( 1 - s * ( 1 - f ) );
  
  switch (i) {
  case 0:    *r = v;    *g = t;    *b = p;    break;
  case 1:    *r = q;    *g = v;    *b = p;    break;
  case 2:    *r = p;    *g = v;    *b = t;    break;
  case 3:    *r = p;    *g = q;    *b = v;    break;
  case 4:    *r = t;    *g = p;    *b = v;    break;
  default:
  case 5:    *r = v;    *g = p;    *b = q;    break;
  }
}

void
RGBtoHSV (gdouble r, gdouble g, gdouble b, gdouble *h, gdouble *s, gdouble *v)
{
  gdouble min, max, delta;

  min = MIN3 (r, g, b );
  max = MAX3 (r, g, b );
  *v = max;				// v

  delta = max - min;

  if (max != 0) *s = delta / max;		// s
  else {
    *s = 0;
    *h = -1;
    return;
  }

  if (r == max) *h = (g - b) / delta;		   // between yellow & magenta
  else if (g == max) *h = 2 + ( b - r ) / delta;   // between cyan & yellow
  else *h = 4 + (r - g) / delta;		   // between magenta & cyan

  *h *= G_PI / 3.0;				// degrees
  if (*h < 0) *h += 2.0 * G_PI;

}

gboolean
compliment (GdkRGBA *col)
{
  gdouble h, s, v;
  gdouble r, g, b;

  if (!col) return FALSE;
  
  r = col->red;
  g = col->green;
  b = col->blue;

  RGBtoHSV (r, g, b, &h, &s, &v);

#if 1
  if (s > .5) {
    h += G_PI;
    if (h >= 2.0 * G_PI) h -= 2.0 * G_PI;
  }
  else v = (v <= .5) ? 1.0 : 0.0;
#else
  h += G_PI;
  if (h >= 2.0 * G_PI) h -= 2.0 * G_PI;
  if (v < .2 || v > .8) v = 1 - v;
  if (s < .2 || s > .8) s = 1 - s;
#endif

  HSVtoRGB (&r, &g, &b, h, s, v);
  
  col->red   = r;
  col->green = g;
  col->blue  = b;

  return TRUE;
}

gboolean
compliment2 (GdkRGBA *fgcol, GdkRGBA *bgcol)
{
  gdouble fr, fg, fb;
  gdouble br, bg, bb;

  if (!fgcol || !bgcol) return FALSE;
  
  fr = fgcol->red   - 0.5;
  fg = fgcol->green - 0.5;
  fb = fgcol->blue  - 0.5;
  br = bgcol->red   - 0.5;
  bg = bgcol->green - 0.5;
  bb = bgcol->blue  - 0.5;

  fr = 0.5 -(fr + br) / 2.0;
  fg = 0.5 -(fg + bg) / 2.0;
  fb = 0.5 -(fb + bb) / 2.0;

  fgcol->red   = fr;
  fgcol->green = fg;
  fgcol->blue  = fb;

  return TRUE;
}
  
