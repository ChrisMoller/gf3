#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <values.h>
#include <math.h>
#include <string.h>

#include "gf.h"
#include "utilities.h"
#include "fallbacks.h"
#include "select_pen.h"

#define Q1_A (4.0 / 6.0)
#define Q1_B (2.0 / 6.0)
#define Q2_A (2.0 / 6.0)
#define Q2_B (4.0 / 6.0)
#define Q3_A (1.0 / 6.0)
#define Q3_B (4.0 / 6.0)
#define Q3_C (1.0 / 6.0)

static void
draw_segments (gpointer data, gpointer user_data)
{
  cairo_t *cr = user_data;
  point_s *px = data;
  cairo_line_to (cr, point_x (px), point_y (px));
}

/***

      A = P1 - PC = [xa, ya]    B = P2 - PC = [xb, yb]

      return if |A| == 0 or |B| == 0

      dot prodoct:
      
      A * B = |A| |B| cos(T)

      cos(T) = (A * B) / (|A| |B|)

      cos(T) = (xa * xb  +  ya * yb) / (hypot (xa, ya) * hypot (xb, yb))

      T = acos()  // angle between constituent vecs and avg vec

      t = T / 2

      return if tan(t) == 0  or  sin(t) == 0

      r/q = tan(t)
      q = r / tan(t)	// dist along polyline segs from intersection

      r/w = sin(t)
      w = r / sin(t)	// dist from intersect to centre of arc

      pV = atan2 ((sin (phaA) + sin (phaB))/2.0,
                  (cos (phaA) + cos (phaB))/2.0)

      pA = pha(A) = atan2 (ya, xa)		// phases of constituent vecs
      pB = pha(B) = atan2 (yb, xb)

      C = PC + [w cos(pV), w sin(pV)]		// centre of arc

      D = PC + [q cos(pA), q sin(pA)]		// trimmed endpoints of 
      E = PC + [q cos(pB), q sin(pB)]		// constituent vecs

      pC = 2pi - (pA - pB)

      
 ***/

typedef struct {
  point_s p0;
  point_s p1;
} segment_s;
#define segment_p0_x(s)	point_x (&((s).p0))
#define segment_p0_y(s)	point_y (&((s).p0))
#define segment_p1_x(s)	point_x (&((s).p1))
#define segment_p1_y(s)	point_y (&((s).p1))

static void
trim_segs (cairo_t *cr, entity_polyline_s *polyline)
{
  if (entity_polyline_isect_radius (polyline) > 0.0) {
    guint length = g_list_length (entity_polyline_verts (polyline));
    if (length > 2) {
      segment_s *segments = gfig_try_malloc0 (length * sizeof(segment_s));
      for (gint pcidx = 0; pcidx < length; pcidx++) {
	gint p1idx = (pcidx + length - 1) % length;
	gint p2idx = (pcidx + 1) % length;
	

	point_s *PC =
	  g_list_nth_data (entity_polyline_verts (polyline), pcidx);
	gdouble pcx = point_x (PC);
	gdouble pcy = point_y (PC);
	segment_p0_x (segments[pcidx]) = pcx;
	segment_p0_y (segments[pcidx]) = pcy;
	segment_p1_x (segments[p1idx]) = pcx;
	segment_p1_y (segments[p1idx]) = pcy;

	point_s *P1 =
	  g_list_nth_data (entity_polyline_verts (polyline), p1idx);
	gdouble p1x = point_x (P1);
	gdouble p1y = point_y (P1);

	point_s *P2 =
	  g_list_nth_data (entity_polyline_verts (polyline), p2idx);
	gdouble p2x = point_x (P2);
	gdouble p2y = point_y (P2);
	
	gdouble xa = p1x - pcx;
	gdouble ya = p1y - pcy;
	gdouble xb = p2x - pcx;
	gdouble yb = p2y - pcy;

	gdouble magA = hypot (ya, xa);
	gdouble magB = hypot (yb, xb);

	gdouble cosT = (xa * xb + ya * yb) / (magA * magB);
	gdouble T = acos (cosT) / 2.0;
	gdouble sinT = sin (T);
	gdouble tanT = tan (T);

	if (sinT != 0.0 && tanT != 0.0) {
	  gdouble Q = entity_polyline_isect_radius (polyline) / tanT;
	  gdouble W = entity_polyline_isect_radius (polyline) / sinT;

	  gdouble phaA = atan2 (ya, xa);
	  gdouble phaB = atan2 (yb, xb);
	  gdouble phaV = atan2 ((sin (phaA) + sin (phaB))/2.0,
				(cos (phaA) + cos (phaB))/2.0);

	  point_s C;
	  point_x (&C) = pcx + W * cos (phaV);
	  point_y (&C) = pcy + W * sin (phaV);
	  point_s D;
	  point_x (&D) = pcx + Q * cos (phaA);
	  point_y (&D) = pcy + Q * sin (phaA);
	  point_s E;
	  point_x (&E) = pcx + Q * cos (phaB);
	  point_y (&E) = pcy + Q * sin (phaB);
	  
	  if (entity_polyline_closed (polyline) || pcidx != 0) {
	    segment_p0_x (segments[pcidx]) = point_x (&E);
	    segment_p0_y (segments[pcidx]) = point_y (&E);
	  }
	  if (entity_polyline_closed (polyline) || p1idx != length - 2) {
	    segment_p1_x (segments[p1idx]) = point_x (&D);
	    segment_p1_y (segments[p1idx]) = point_y (&D);
	  }


	  if (entity_polyline_intersect (polyline) == INTERSECT_ARC) {
	    gdouble phaD = atan2 (point_y (&D) - point_y (&C),
				  point_x (&D) - point_x (&C));
	    gdouble phaE = atan2 (point_y (&E) - point_y (&C),
				  point_x (&E) - point_x (&C));
	    if (entity_polyline_closed (polyline) ||
		(pcidx != 0 && pcidx != length - 1)) {
	      cairo_new_sub_path (cr);
	      cairo_arc (cr, point_x (&C), point_y (&C),
			 entity_polyline_isect_radius (polyline),
			 phaE, phaD);
	    }
	  }
	  else {		// bevel
	    if (entity_polyline_closed (polyline) ||
		(pcidx != 0 && pcidx != length - 1)) {
	      cairo_move_to (cr, point_x (&E), point_y (&E));
	      cairo_line_to (cr, point_x (&D), point_y (&D));
	    }
	  }
	}
      }
      gint last = entity_polyline_closed (polyline) ? length : length - 1;
      for (gint i = 0; i < last; i++) {
	cairo_move_to (cr, segment_p0_x (segments[i]),
		       segment_p0_y (segments[i]));
	cairo_line_to (cr, segment_p1_x (segments[i]),
		       segment_p1_y (segments[i]));
      }
      cairo_stroke (cr);
    }
  }
}

static void
set_dashes (cairo_t *cr, pen_s *pen, environment_s *env)
{
  const line_style_s *ls = get_pen_line_style (pen);
  if (line_style_dash_count (ls) > 0) {
    gdouble dash_scale =
      (environment_dunit (env) == GTK_UNIT_INCH) ? 25.4 : 1.0;
    gdouble *dashes =
      gfig_try_malloc0 (line_style_dash_count (ls) * sizeof(gdouble));
    memmove (dashes, line_style_dashes (ls),
	     line_style_dash_count (ls) * sizeof(gdouble));
    for (gint i =  0; i < line_style_dash_count (ls); i++)
      dashes[i] /= dash_scale;
    cairo_set_dash (cr,
		    dashes,
		    line_style_dash_count (ls),
		    0);
  }
  else cairo_set_dash (cr, NULL, 0, 0);
}

void
draw_entities (gpointer data, gpointer user_data)
{
  environment_s *env = user_data;
  cairo_t *cr = environment_cr (env);

  entity_type_e type = entity_type (data);
  switch (type) {
  case ENTITY_TYPE_GROUP:
    {
      entity_group_s *tf = data;

      if (entity_group_centre (tf)) {
	cairo_save (cr);
	cairo_translate (cr,
			 -point_x (entity_group_centre (tf)),
			 -point_y (entity_group_centre (tf)));
      }

      if (entity_group_transform (tf)) {
	cairo_save (cr);
	cairo_transform (cr, entity_group_transform (tf));
      }

      g_list_foreach (entity_group_entities (tf), draw_entities, env);

      if (entity_group_transform (tf)) 
	cairo_restore (cr);
      if (entity_group_centre (tf)) 
	cairo_restore (cr);
    }
    break;
  case ENTITY_TYPE_TRANSFORM:
    {
      entity_transform_s *tf = data;
      if (entity_tf_unset (tf))
	cairo_restore (cr);
      else {
	cairo_save (cr);
	cairo_transform (cr, entity_tf_matrix (tf));
      }
    }
    break;
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_TEXT:
    {
      PangoLayout *layout;
      entity_text_s *text = data;
      pen_s *pen = entity_text_pen (text);
      gdouble lw = pen_lw (pen);
      PangoAttribute *lsp = NULL;
      PangoAttrList *pal  = NULL;

      
      cairo_save (cr);

      layout = pango_cairo_create_layout (cr);


      gchar *tfont = entity_text_font (text);
      if (!tfont || !*tfont) tfont = environment_fontname (env);
      if (!tfont) tfont = FALLBACK_FONT;
      PangoFontDescription *desc =
	pango_font_description_from_string (tfont);

      /****
	   There's something weird either in pango layout or in my
	   understanding thereof that if the font size is set too small
	   the letter spacing seems to go to zero.  I'm getting around
	   this by scaling up the font size by an arbitrary amount and
	   then scaling it back down when I stroke or fill the text.
       ****/
#define FIXIT	256.0
      
      gdouble font_size = entity_text_txtsize (text);
      if (font_size == 0.0) font_size = environment_textsize (env);
      font_size *= FIXIT * (double)PANGO_SCALE;

      
      pango_font_description_set_absolute_size (desc, font_size);

      pango_layout_set_font_description (layout, desc);
      pango_font_description_free (desc);

      if (entity_text_lead (text) != 0) {
	// fixme -- may need to scale by FIXIT
        gint sp = pango_layout_get_spacing (layout);
	sp += entity_text_lead (text);
	pango_layout_set_spacing (layout, PANGO_SCALE * sp);
      }

      if (!entity_text_justify (text))
	pango_layout_set_alignment (layout, entity_text_alignment (text));

      pango_layout_set_justify (layout, entity_text_justify (text));
      
      if (entity_text_spread (text) != 0) {
	// fixme -- may need to scale by FIXIT
	lsp =
	  pango_attr_letter_spacing_new (entity_text_spread (text) *
					 PANGO_SCALE);
	pal  = pango_attr_list_new ();
	pango_attr_list_insert (pal, lsp);
	pango_layout_set_attributes (layout, pal);
      }
      
      pango_layout_set_text (layout, entity_text_string (text), -1);
      // pango_layout_set_markup (layout, entity_text_text (t), -1);

      PangoRectangle ink_rect;
      PangoRectangle logical_rect;
      pango_layout_get_extents (layout, &ink_rect, &logical_rect);

      pango_cairo_layout_path (cr, layout);
      cairo_path_t *path = cairo_copy_path (cr);
      g_object_unref (layout);
      cairo_new_path (cr);  // kill the default


      cairo_translate (cr, entity_text_x (text), entity_text_y (text));
      cairo_rotate (cr, -entity_ellipse_t (text));
      cairo_scale (cr, 1.0 / FIXIT, 1.0 / FIXIT);

#if 0
      double dashes[] = {100., 50., 50, 50.};
      cairo_set_dash (cr, dashes, sizeof(dashes) / sizeof(double), 0);
      cairo_set_line_width (cr, 10.0);
      cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 1.0);
      cairo_rectangle (cr,
	       ((double)ink_rect.x)      / ( (double)PANGO_SCALE),
	       ((double)ink_rect.y)      / ( (double)PANGO_SCALE),
	       ((double)ink_rect.width)  / ( (double)PANGO_SCALE),
	       ((double)ink_rect.height) / ( (double)PANGO_SCALE));
      cairo_stroke (cr);
#endif
      
      cairo_set_dash (cr, NULL, 0, 0.0);
      cairo_set_line_width (cr, lw);
      cairo_set_source_rgba (cr,
			     pen_colour_red (pen),
			     pen_colour_green (pen),
			     pen_colour_blue (pen),
			     pen_colour_alpha (pen));

      for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
	cairo_path_data_t *data;
	data = &path->data[i];
	switch (data->header.type) {
	case CAIRO_PATH_MOVE_TO:
	  cairo_move_to (cr, data[1].point.x, data[1].point.y);
	  break;
	case CAIRO_PATH_LINE_TO:
	  cairo_line_to (cr, data[1].point.x, data[1].point.y);
	  break;
	case CAIRO_PATH_CURVE_TO:
	  cairo_curve_to (cr,
			  data[1].point.x, data[1].point.y,
			  data[2].point.x, data[2].point.y,
			  data[3].point.x, data[3].point.y);
	  break;
	case CAIRO_PATH_CLOSE_PATH:
	  cairo_close_path (cr);
	  break;
	}
      }
	      

      if (lsp) {
	pango_attribute_destroy (lsp);
	lsp = NULL;
      }
      if (pal) {
	pal  = NULL;
	pango_attr_list_unref (pal);
      }
              
      if (entity_text_filled (text)) cairo_fill (cr);
      else cairo_stroke (cr);
      cairo_path_destroy (path);

      cairo_restore (cr);
    }
    break;
  case ENTITY_TYPE_CIRCLE:
    {
      entity_circle_s *circle = data;
      pen_s *pen = entity_circle_pen (circle);
      gdouble lw = pen_lw (pen);
      if (environment_dunit (env) == GTK_UNIT_INCH) lw /= 25.4;
      cairo_set_line_width (cr, lw);
      cairo_set_source_rgba (cr,
			     pen_colour_red (pen),
			     pen_colour_green (pen),
			     pen_colour_blue (pen),
			     pen_colour_alpha (pen));
      if (entity_circle_negative (circle))
	cairo_arc_negative (cr,
			    entity_circle_x (circle),
			    entity_circle_y (circle),
			    entity_circle_r (circle),
			    entity_circle_start (circle),
			    entity_circle_stop (circle));
      else
	cairo_arc (cr,
		   entity_circle_x (circle),
		   entity_circle_y (circle),
		   entity_circle_r (circle),
		   entity_circle_start (circle),
		   entity_circle_stop (circle));
      if (entity_circle_fill (circle)) cairo_fill (cr);
      else cairo_stroke (cr);
    }	
    break;
  case ENTITY_TYPE_ELLIPSE:
    {
      entity_ellipse_s *ellipse = data;
      pen_s *pen = entity_ellipse_pen (ellipse);
      gdouble lw = pen_lw (pen);
      if (environment_dunit (env) == GTK_UNIT_INCH) lw /= 25.4;
      cairo_save (cr);
      cairo_set_line_width (cr, lw);
      cairo_set_source_rgba (cr,
			     pen_colour_red (pen),
			     pen_colour_green (pen),
			     pen_colour_blue (pen),
			     pen_colour_alpha (pen));
      
      gdouble xscale, yscale, radius;
      if (entity_ellipse_a (ellipse) > entity_ellipse_b (ellipse)) {
	radius = entity_ellipse_a (ellipse);
	xscale = 1.0;
	yscale = entity_ellipse_b (ellipse) / entity_ellipse_a (ellipse);
      }
      else {
	radius = entity_ellipse_b (ellipse);
	xscale = entity_ellipse_a (ellipse) / entity_ellipse_b (ellipse);
	yscale = 1.0;
      }
      cairo_translate (cr,
		       entity_ellipse_x (ellipse), entity_ellipse_y (ellipse));
      cairo_rotate (cr, -entity_ellipse_t (ellipse));
      cairo_scale (cr, xscale, yscale);
      if (entity_ellipse_negative (ellipse))
	cairo_arc_negative (cr, 0.0, 0.0, radius,
			    entity_ellipse_start (ellipse),
			    entity_ellipse_stop (ellipse));
      else
	cairo_arc (cr, 0.0, 0.0, radius,
		   entity_ellipse_start (ellipse),
		   entity_ellipse_stop (ellipse));
      if (entity_ellipse_fill (ellipse)) cairo_fill (cr);
      else cairo_stroke (cr);
      cairo_restore (cr);
    }	
    break;
  case ENTITY_TYPE_POLYLINE:
    {
      entity_polyline_s *polyline = data;
      pen_s *pen = entity_polyline_pen (polyline);
      set_dashes (cr, pen, env);
      gdouble lw = pen_lw (pen);
      if (environment_dunit (env) == GTK_UNIT_INCH) lw /= 25.4;
      cairo_set_line_width (cr, lw);
      cairo_set_source_rgba (cr,
			     pen_colour_red (pen),
			     pen_colour_green (pen),
			     pen_colour_blue (pen),
			     pen_colour_alpha (pen));
      if (entity_polyline_verts (polyline)) {
	point_s *p0 = g_list_nth_data (entity_polyline_verts (polyline), 0);
	guint nr_verts = g_list_length (entity_polyline_verts (polyline));
	if (entity_polyline_spline (polyline)) {	/* spline */
	  point_s *p1;
	  point_s *p2;
	  point_s *p3;
	  gboolean closed =
	    entity_polyline_closed (polyline) ||
	    entity_polyline_filled (polyline);
	  if (( closed && nr_verts >= 3) ||
	      (!closed && nr_verts >= 4)) {
	    cairo_move_to (cr, point_x (p0), point_y (p0));
	    for (int i = 1; i + 2 < nr_verts; i++) {
	      p1 = g_list_nth_data (entity_polyline_verts (polyline), i);
	      p2 = g_list_nth_data (entity_polyline_verts (polyline), i + 1);
	      p3 = g_list_nth_data (entity_polyline_verts (polyline), i + 2);
	      cairo_curve_to (cr,
			      point_x (p1), point_y (p1), 
			      point_x (p2), point_y (p2), 
			      point_x (p3), point_y (p3));
	      if (closed) {
		p1 = g_list_nth_data (entity_polyline_verts (polyline),
				      nr_verts - 2);
		p2 = g_list_nth_data (entity_polyline_verts (polyline),
				      nr_verts - 1);
		p3 = g_list_nth_data (entity_polyline_verts (polyline), 0);
		cairo_curve_to (cr,
				point_x (p1), point_y (p1), 
				point_x (p2), point_y (p2), 
				point_x (p3), point_y (p3));
	      }
	    }
	    if (entity_polyline_filled (polyline)) cairo_fill (cr);
	    else cairo_stroke (cr);
	  }
	  else {		// spline, but not enough verts
#define MARKER_RADIUS	3.0		// in pixels
	    gdouble xradius = MARKER_RADIUS;
	    gdouble yradius = MARKER_RADIUS;
	    cairo_device_to_user_distance (cr, &xradius, &yradius);
	    for (int i = 0; i < nr_verts; i++) {
	      p1 = g_list_nth_data (entity_polyline_verts (polyline), i);
	      cairo_arc (cr, point_x (p1), point_y (p1), xradius,
			 0.0, 2.0 * G_PI);
	      cairo_fill (cr);
	    }
	  }
	}
	else {						/* polyline */
	  switch(entity_polyline_intersect (polyline)) {
	  case INTERSECT_POINT:
	    cairo_move_to (cr, point_x (p0), point_y (p0));
	    g_list_foreach (g_list_nth (entity_polyline_verts (polyline), 1),
			    draw_segments, cr);
	    if (entity_polyline_closed (polyline)) cairo_close_path (cr);
	    if (entity_polyline_filled (polyline)) cairo_fill (cr);
	    else cairo_stroke (cr);
	    break;
	  case INTERSECT_ARC:
	  case INTERSECT_BEVEL:
	    trim_segs (cr, polyline);
	    break;
	  }
	}
      }
    }
    break;
  }
}

entity_polyline_s *
entity_build_polyline (sheet_s *sheet, GList *verts,
		       gboolean closed, gboolean filled,
		       gboolean spline, pen_s *pen,
		       gint intersect, gdouble radius)
{
  environment_s *env = sheet_environment (sheet);
  entity_polyline_s *polyline = gfig_try_malloc0 (sizeof(entity_polyline_s));
  entity_polyline_type (polyline)	= ENTITY_TYPE_POLYLINE;
  entity_polyline_closed (polyline)	= closed;
  entity_polyline_filled (polyline)	= filled;
  entity_polyline_spline (polyline)	= spline;
  entity_polyline_verts (polyline)	= verts;
  entity_polyline_intersect (polyline)	= intersect;
  entity_polyline_isect_radius (polyline) = radius;
  entity_polyline_pen (polyline) = pen ? copy_pen (pen) :
    copy_environment_pen (env);

  return polyline;
}

void
entity_append_polyline (sheet_s *sheet, GList *verts,
			gboolean closed, gboolean filled,
			gboolean spline, pen_s *pen,
			gint intersect, gdouble radius)
{
  entity_polyline_s *polyline =
    entity_build_polyline (sheet, verts, closed, filled, spline, pen,
			   intersect, radius);
			   

  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), polyline);  
}

entity_text_s *
entity_build_text (sheet_s *sheet, gdouble x, gdouble y,
		    gchar *string, int size, gdouble theta,
		    gdouble txt_size, gboolean filled, gchar *font,
		    gint alignment, gboolean justify,
		    gint spread, gint lead, pen_s *pen)
{
  environment_s *env = sheet_environment (sheet);
  entity_text_s *text = gfig_try_malloc0 (sizeof(entity_text_s));
  entity_text_type (text)	= ENTITY_TYPE_TEXT;
  entity_text_x (text)		= x;
  entity_text_y (text)		= y;
  entity_text_string (text)	= g_strdup (string);
  entity_text_t (text)		= theta;
  entity_text_font (text)	= font;
  entity_text_size (text)	= size;
  entity_text_spread (text)	= spread;
  entity_text_lead (text)	= lead;
  entity_text_alignment (text)	= alignment;
  entity_text_justify (text)	= justify;
  entity_text_txtsize (text)	= txt_size;
  entity_text_filled (text)	= filled;
  entity_circle_pen (text) = pen ? copy_pen (pen) :
    copy_environment_pen (env);

  return text;
}

void
entity_append_text (sheet_s *sheet, gdouble x, gdouble y,
		    gchar *string, int size, gdouble theta,
		    gdouble txt_size, gboolean filled, gchar *font,
		    gint alignment, gboolean justify,
		    gint spread, gint lead, pen_s *pen)
{
  entity_text_s *text = entity_build_text (sheet, x, y, string, size, theta,
					   txt_size, filled, font, alignment,
					   justify, spread, lead, pen);
  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), text);
}

entity_circle_s *
entity_build_circle (sheet_s *sheet, gdouble x, gdouble y,
		     gdouble r, gdouble start, gdouble stop,
		     gboolean negative, gboolean filled, pen_s *pen)
{
  environment_s *env = sheet_environment (sheet);
  entity_circle_s *circle = gfig_try_malloc0 (sizeof(entity_circle_s));
  entity_circle_type (circle)	= ENTITY_TYPE_CIRCLE;
  entity_circle_x (circle)		= x;
  entity_circle_y (circle)		= y;
  entity_circle_r (circle)		= r;
  entity_circle_start (circle)		= start;
  entity_circle_stop (circle)		= stop;
  entity_circle_negative (circle)	= negative;
  entity_circle_fill (circle)		= filled;
  entity_circle_pen (circle) = pen ? copy_pen (pen) :
    copy_environment_pen (env);

  return circle;
}

void
entity_append_circle (sheet_s *sheet, gdouble x, gdouble y,
		      gdouble r, gdouble start, gdouble stop,
		      gboolean negative, gboolean filled, pen_s *pen)
{
  entity_circle_s *circle = entity_build_circle (sheet, x, y, r, start,
						 stop, negative,
						 filled, pen);
  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), circle);
}

void
entity_append_entity (sheet_s *sheet, void *entity)
{
  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), entity);
}

entity_ellipse_s *
entity_build_ellipse (sheet_s *sheet, gdouble x, gdouble y,
		      gdouble a, gdouble b, gdouble t, 
		      gdouble start, gdouble stop, gboolean negative,
		      gboolean filled, pen_s *pen)
		      
{
  environment_s *env = sheet_environment (sheet);
  entity_ellipse_s *ellipse = gfig_try_malloc0 (sizeof(entity_ellipse_s));
  entity_ellipse_type (ellipse)		= ENTITY_TYPE_ELLIPSE;
  entity_ellipse_x (ellipse)		= x;
  entity_ellipse_y (ellipse)		= y;
  entity_ellipse_a (ellipse)		= a;
  entity_ellipse_b (ellipse)		= b;
  entity_ellipse_t (ellipse)		= t;
  entity_ellipse_start (ellipse)	= start;
  entity_ellipse_stop (ellipse)		= stop;
  entity_ellipse_negative (ellipse)	= negative;
  entity_ellipse_fill (ellipse)		= filled;
  entity_ellipse_pen (ellipse) = pen ? copy_pen (pen) :
    copy_environment_pen (env);

  return ellipse;
}

void
entity_append_ellipse (sheet_s *sheet, gdouble x, gdouble y,
		       gdouble a, gdouble b, gdouble t,
		       gdouble start, gdouble stop, gboolean negative,
		       gboolean filled, pen_s *pen)
{
  entity_ellipse_s *ellipse =
    entity_build_ellipse (sheet, x, y, a, b, t, start, stop, negative,
			  filled, pen);
  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), ellipse); 
}

void
entity_append_transform (sheet_s *sheet, cairo_matrix_t *matrix,
			 gboolean unset)
{
  entity_transform_s *tf = gfig_try_malloc0 (sizeof(entity_transform_s));
  entity_tf_type (tf) = ENTITY_TYPE_TRANSFORM;
  entity_tf_unset (tf) = unset;
  entity_tf_matrix (tf) = matrix;
  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), tf);
}

entity_group_s *
entity_build_group (sheet_s *sheet, cairo_matrix_t *matrix,
		    point_s *centre, GList *entities)
{
  entity_group_s *group = gfig_try_malloc0 (sizeof(entity_group_s));
  entity_group_type (group) = ENTITY_TYPE_GROUP;
  entity_group_entities (group) = entities;
  entity_group_centre (group) = centre;
  entity_group_transform (group) = matrix;

  return group;
}

void
entity_append_group (sheet_s *sheet, cairo_matrix_t *matrix,
		     point_s *centre, GList *entities)
{
  entity_group_s *group = entity_build_group (sheet, matrix,
					      centre, entities);

  sheet_entities (sheet) = g_list_append (sheet_entities (sheet), group);
}

static void
delete_pen_copy (pen_s *pen)
{
  if (pen) {
    if (pen_colour (pen))      gdk_rgba_free (pen_colour (pen));
    if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
    g_free (pen);
  }
}

void
delete_entities (gpointer data)
{
  entity_type_e type = entity_type (data);
  switch (type) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_TEXT:
    {
      entity_text_s *text = data;
      pen_s *pen = entity_text_pen (text);
      delete_pen_copy (pen);
      if (entity_text_string (text)) g_free (entity_text_string (text));
      entity_text_string (text) = NULL;
      g_free (text);
      text = NULL;
    }
    break;
  case ENTITY_TYPE_CIRCLE:
    {
      entity_circle_s *circle = data;
      pen_s *pen = entity_circle_pen (circle);
      delete_pen_copy (pen);
      g_free (circle);
      circle = NULL;
    }
    break;
  case ENTITY_TYPE_ELLIPSE:
    {
      entity_ellipse_s *ellipse = data;
      pen_s *pen = entity_ellipse_pen (ellipse);
      delete_pen_copy (pen);
      g_free (ellipse);
      ellipse = NULL;
    }	
    break;
  case ENTITY_TYPE_POLYLINE:
    {
      entity_polyline_s *polyline = data;
      pen_s *pen = entity_polyline_pen (polyline);
      delete_pen_copy (pen);
      if (entity_polyline_verts (polyline)) {
	g_list_free_full (entity_polyline_verts (polyline), g_free);
	entity_polyline_verts (polyline) = NULL;
      }
      g_free (polyline);
      polyline = NULL;
    }
    break;
  case ENTITY_TYPE_GROUP:
    {
      entity_group_s *group = data;
      if (entity_group_entities (group)) {
	g_list_free_full (entity_group_entities (group), delete_entities);
	entity_group_entities (group) = NULL;
      }
      if (entity_group_transform (group))
	g_free (entity_group_transform (group));
      if (entity_group_centre (group))
	g_free (entity_group_centre (group));
      g_free (group);
      group = NULL;
    }
    break;
  case ENTITY_TYPE_TRANSFORM:
    {
      entity_transform_s *tf = data;
      if (entity_tf_matrix (tf)) g_free (entity_tf_matrix (tf));
      entity_tf_matrix (tf) = NULL;
      g_free (tf);
      tf = NULL;
    }
    break;
  }
}

void
clear_entities (GList **entities)
{
  if (entities && *entities) {
    g_list_free_full (*entities, delete_entities);
    *entities = NULL;
  }
}

point_s *
copy_point (point_s *orig)
{
  point_s *new = gfig_try_malloc0 (sizeof(point_s));
  point_x (new) = point_x (orig);
  point_y (new) = point_y (orig);
  return new;
}
