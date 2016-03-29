#ifndef PYTHON_H
#define PYTHON_H

typedef void (*set_button_tags)(guint state);

void  term_python ();
void  init_python ();
void  evaluate_python (gchar *pystr, sheet_s *sheet);
void  execute_python (const gchar *pyfile, sheet_s *sheet);
void *get_local_pydict (sheet_s *sheet);
gboolean python_evaluate_point (gchar *pystr, sheet_s *sheet,
	  		        gdouble *xvp, gdouble *yvp);
void evaluate_point (sheet_s *sheet, gdouble *pxip, gdouble *pyip,
		     set_button_tags sbt);
#endif  /* PYTHON_H */

