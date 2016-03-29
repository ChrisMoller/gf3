#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "select_paper.h"
#include "select_pen.h"
#include "select_scale.h"

void
preferences (GtkWidget *object, gpointer data)
{
  GtkWidget     *dialog;
  GtkWidget     *content;
  GtkWidget     *notebook;
  GtkWidget     *label;
  gint           response;

  environment_s *env = (environment_s *)data;

  g_assert (env != NULL);

  dialog =  gtk_dialog_new_with_buttons (_ ("Preferences"),
                                         NULL,
					 GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);
  gtk_container_add (GTK_CONTAINER (content), notebook);

  label = gtk_label_new (_ ("Paper"));
  void *paper_private_data = NULL;
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    select_paper (&paper_private_data, env),
			    label);

  label = gtk_label_new (_ ("Units & Scale"));
  void *scale_private_data = NULL;
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    select_scale (&scale_private_data, env),
			    label);

  label = gtk_label_new (_ ("Pen"));
  void *pen_private_data = NULL;
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    select_pen (&pen_private_data,
					environment_pen (env)),
			    label);
 
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    extract_paper (paper_private_data, env);
    extract_pen   (pen_private_data, /*env*/ environment_pen (env));
    extract_scale (scale_private_data, env);
  }
  if (paper_private_data) g_free (paper_private_data);
  if (pen_private_data)   g_free (pen_private_data);
  if (scale_private_data) g_free (scale_private_data);
  gtk_widget_destroy (dialog);
}
