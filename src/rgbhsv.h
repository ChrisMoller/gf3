#ifndef RGBHSV_H
#define RGBHSV_H

void HSVtoRGB (gdouble *r, gdouble *g, gdouble *b,
	       gdouble h, gdouble s, gdouble v);
void RGBtoHSV (gdouble r, gdouble g, gdouble b,
	       gdouble *h, gdouble *s, gdouble *v);
gboolean compliment (GdkRGBA *col);
gboolean compliment2 (GdkRGBA *fgcol, GdkRGBA *bgcol);

#endif  /* RGBHSV_H */
