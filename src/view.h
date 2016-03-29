#ifndef VIEW_H
#define VIEW_H

void create_new_view (project_s *project, sheet_s *this_sheet,
		      const gchar *script);
void pen_settings (GtkWidget *object, gpointer data);
void force_redraw (sheet_s *sheet);
void do_config (environment_s *env);
void set_current_line_colour (environment_s *env, pen_s *pen);

#endif  /* VIEW_H */
