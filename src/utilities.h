#ifndef UTILITIES_H
#define UTILITIES_H

gchar *find_image_file   (const gchar *lpath);
gdouble pen_lw_cvt (environment_s *env);
pen_s *copy_environment_pen (environment_s *env);
pen_s *copy_pen (pen_s *env);
void snap_to_grid (environment_s *env, gdouble *pxp, gdouble *pyp);
guint clear_state_bit (GdkEvent *event);
guint set_state_bit (GdkEvent *event);

#endif /* UTILITIES_H */
