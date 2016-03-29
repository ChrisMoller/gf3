#ifndef SELECT_PEN_H
#define SELECT_PEN_H

typedef struct {
  GtkWidget        *style;
  GtkWidget        *lw_combo;
  gulong	    combo_id;
  GtkWidget        *lw_spin;
  gulong	    spin_id;
  GtkTreeSelection *css_button;
} pen_private_data_s;
#define ppd_lw_combo(p)	 	 (p)->lw_combo
#define ppd_combo_id(p)	 	 (p)->combo_id
#define ppd_lw_spin(p)	 	 (p)->lw_spin
#define ppd_spin_id(p)	 	 (p)->spin_id
#define ppd_style(p)	 	 (p)->style
#define ppd_css(p)		((p)->css_button)

GtkWidget *select_pen (void **private_data, pen_s *pen);
void extract_pen (void *private_data, pen_s *pen);
const line_style_s *get_environment_line_style (environment_s *env);
const line_style_s *get_pen_line_style (pen_s *pen);
gdouble get_lw_from_idx (gint idx);
GtkWidget *linestyle_button_new (pen_s *current_pen);
GtkWidget *linewidth_buttons_new (pen_private_data_s **ppd,
				  pen_s *current_pen); 
const gchar *get_ls_from_idx (gint idx);
gint get_idx_from_ls (const gchar *name);
void pen_settings (GtkWidget *object, gpointer data);
#endif  /* SELECT_PEN_H */
