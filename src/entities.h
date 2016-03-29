#ifndef ENTITIES_H
#define ENTITIES_H

void draw_entities (gpointer data, gpointer user_data);
void clear_entities (GList **entities);

void entity_append_text (sheet_s *sheet, gdouble x, gdouble yy,
			 gchar *text, int size, gdouble theta,
			 gdouble txt_size, gboolean filled, gchar *font,
			 gint alignment, gboolean justify,
			 gint spread, gint lead, pen_s *pen);
entity_text_s *entity_build_text (sheet_s *sheet, gdouble x, gdouble yy,
			 gchar *text, int size, gdouble theta,
			 gdouble txt_size, gboolean filled, gchar *font,
			 gint alignment, gboolean justify,
			 gint spread, gint lead, pen_s *pen);

				  
void entity_append_circle (sheet_s *sheet, gdouble x, gdouble y,
			   gdouble r, gdouble start, gdouble stop,
			   gboolean negative, gboolean filled, pen_s *pen);
entity_circle_s *entity_build_circle (sheet_s *sheet, gdouble x, gdouble y,
				      gdouble r, gdouble start, gdouble stop,
				      gboolean negative, gboolean filled,
				      pen_s *pen);
				  
void entity_append_ellipse (sheet_s *sheet, gdouble x, gdouble y,
			    gdouble a, gdouble b,
			    gdouble t, gdouble start, gdouble stop,
			    gboolean negative, gboolean filled, pen_s *pen);
entity_ellipse_s *entity_build_ellipse (sheet_s *sheet, gdouble x, gdouble y,
					gdouble a, gdouble b,
					gdouble t, gdouble start, gdouble stop,
					gboolean negative, gboolean filled,
					pen_s *pen);

void entity_append_polyline (sheet_s *sheet, GList *verts,
			     gboolean closed, gboolean filled,
			     gboolean spline, pen_s *pen,
			     gint intersect, gdouble radius);
entity_polyline_s *entity_build_polyline (sheet_s *sheet, GList *verts,
					  gboolean closed, gboolean filled,
					  gboolean spline, pen_s *pen,
					  gint intersect, gdouble radius);

void entity_append_group (sheet_s *sheet, cairo_matrix_t *matrix,
			  point_s *centre, GList *entities);
entity_group_s *entity_build_group (sheet_s *sheet, cairo_matrix_t *matrix,
				    point_s *centre, GList *entities);

void entity_append_transform (sheet_s *sheet, cairo_matrix_t *matrix,
			      gboolean unset);
void entity_append_entity (sheet_s *sheet, void *entity);
void delete_entities (gpointer data);
point_s *copy_point (point_s *orig);

#endif  /* ENTITIES_H */
