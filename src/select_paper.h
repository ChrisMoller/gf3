#ifndef SELECT_PAPER_H
#define SELECT_PAPER_H

GtkWidget *select_paper (void **private_data, environment_s *env);
void extract_paper (void *private_data, environment_s *env);
void paper_settings (GtkWidget *object, gpointer data);

#endif  /* SELECT_PAPER_H */
