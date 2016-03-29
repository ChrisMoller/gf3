#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "view.h"

enum {
  PAPER_NAME_COL,
  PAPER_DISPLAY_NAME_COL,
  PAPER_WIDTH_MM_COL,
  PAPER_HEIGHT_MM_COL,
  PAPER_WIDTH_INCH_COL,
  PAPER_HEIGHT_INCH_COL,
  PAPER_IS_CUSTOM_COL,
  PAPER_SIZE_COL,		// not shown
  PAPER_SPEC_COUNT
} paper_spec_e;

static const gchar *paper_col_names[] = {
  "Name",
  "Display name",
  "Width (mm)",
  "Height (mm)",
  "Width (in)",
  "Height (in)",
  "Custom"
};

typedef struct {
  GtkListStore *store;
  const gchar *target;
  GtkTreeIter *select_iter;
} paper_func_s;
  
static void
paper_func (gpointer data, gpointer user_data)
{
  paper_func_s *pfsp = (paper_func_s *)user_data;
  g_assert (pfsp != NULL);

  GtkTreeIter iter;
  GtkListStore *store = pfsp->store;
  const gchar *target = pfsp->target;
  GtkPaperSize *size  = (GtkPaperSize *)data;


  /***
      na_letter
      na_legal
      iso_a4
   ***/
  const gchar *name = gtk_paper_size_get_name (size);
  if (!g_strcmp0 (name, GTK_PAPER_NAME_LETTER) ||
      !g_strcmp0 (name, GTK_PAPER_NAME_LEGAL)  ||
      !g_strcmp0 (name, GTK_PAPER_NAME_A4))
    gtk_list_store_prepend (store, &iter);
  else gtk_list_store_append (store, &iter);

  if (target && !g_strcmp0 (name, target)) {
    if (!pfsp->select_iter) pfsp->select_iter = gtk_tree_iter_copy (&iter);
  }

  gtk_list_store_set (store, &iter,
		      PAPER_NAME_COL,
		        gtk_paper_size_get_name (size),
		      PAPER_DISPLAY_NAME_COL,
		        gtk_paper_size_get_display_name (size),
		      PAPER_WIDTH_MM_COL,
		        gtk_paper_size_get_width (size, GTK_UNIT_MM),
		      PAPER_HEIGHT_MM_COL,
		        gtk_paper_size_get_height (size, GTK_UNIT_MM),
		      PAPER_WIDTH_INCH_COL,
		        gtk_paper_size_get_width (size, GTK_UNIT_INCH),
		      PAPER_HEIGHT_INCH_COL,
		        gtk_paper_size_get_height (size, GTK_UNIT_INCH),
		      PAPER_IS_CUSTOM_COL,
		        gtk_paper_size_is_custom (size),
		      PAPER_SIZE_COL,
		        size,
		      -1);
}

static void
paper_tree_realised (GtkWidget *widget,
		     //		     GdkEvent  *event,
		     gpointer data)
{
  /* fixme -- figure out how wide it ought to be */
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (data), 600);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (data), 200);
}

static void
custom_double_renderer_func (GtkTreeViewColumn *tree_column,
			     GtkCellRenderer *cell,
			     GtkTreeModel *model,
			     GtkTreeIter *iter,
			     gpointer data)
{
  gint which = GPOINTER_TO_INT (data);
  double val;
  gtk_tree_model_get(model, iter,
		     which, &val,
		     -1);
#define CVT_LENGTH	64
  gchar cvt[CVT_LENGTH];
  g_ascii_formatd (cvt, CVT_LENGTH, "%.2f", val);
  g_object_set (G_OBJECT (cell), "text", cvt, NULL);
}

static void
custom_double_renderer_destroy (gpointer data)
{
  // do nothing
}

static gboolean
paper_search_func (GtkTreeModel *model,
		   gint column,
		   const gchar *key,
		   GtkTreeIter *iter,
		   gpointer search_data)
{
  gchar *val = NULL;
  gboolean rc;
  gtk_tree_model_get(model, iter,
		     column, &val,
		     -1);
  if (!val) return TRUE;
  gchar *val_lc = g_ascii_strdown (val, -1);
  gchar *key_lc  = g_ascii_strdown (key, -1);
  rc = (g_strstr_len (val_lc, -1, key_lc) == NULL);
  g_free (val);
  g_free (val_lc);
  g_free (key_lc);
  return rc;
}



typedef struct {
  GtkTreeSelection *selection;
#if 0
  GtkWidget        *colour;
#endif
  GtkTreeSelection *css_button;
  GtkWidget        *portrait;
} paper_private_data_s;
#define ppd_selection(p) (p)->selection
#if 0
#define ppd_colour(p)    (p)->colour
#endif
#define ppd_css(p)      ((p)->css_button)
#define ppd_portrait(p)  (p)->portrait

  
GtkWidget *
select_paper (void **private_data, environment_s *env)
{
  GtkWidget     *content;
  GList         *paper_list;
  GtkListStore  *paper_store;
  GtkWidget     *frame;
  paper_s       *current_paper  = NULL;
  GtkPaperSize  *current_size   = NULL;
  GdkRGBA       *current_colour = NULL;

  g_assert (env != NULL);
  
  current_paper = environment_paper (env);

  g_assert (current_paper != NULL);

  current_size = paper_size (current_paper);

  g_assert (current_size != NULL);

  current_colour = paper_colour (current_paper);

  g_assert (current_colour != NULL);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  paper_private_data_s *ppd = gfig_try_malloc0 (sizeof(paper_private_data_s));
  *private_data = ppd;
  

  /************ paper selection *******************/

  paper_list = gtk_paper_size_get_paper_sizes (TRUE);
  paper_store = gtk_list_store_new (PAPER_SPEC_COUNT,
				    G_TYPE_STRING,		// name
				    G_TYPE_STRING,		// display name
				    G_TYPE_DOUBLE,		// w mm
				    G_TYPE_DOUBLE,		// h mm
				    G_TYPE_DOUBLE,		// w in
				    G_TYPE_DOUBLE,		// h in
				    G_TYPE_BOOLEAN,		// custom
				    G_TYPE_POINTER);		// size struct

  const gchar *current_paper_name =
    current_size ? gtk_paper_size_get_name (current_size) : NULL;
  paper_func_s pfs = {paper_store, current_paper_name, NULL};
  g_list_foreach (paper_list, paper_func, &pfs);
  g_list_free (paper_list);

  GtkWidget *paper_tree =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (paper_store));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (paper_tree), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (paper_tree), PAPER_NAME_COL);
  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (paper_tree),
				       paper_search_func,
				       NULL,
				       NULL);
  ppd_selection (ppd) =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (paper_tree));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (ppd_selection (ppd)),
			       GTK_SELECTION_SINGLE);
  if (pfs.select_iter) {
    /*  pre-select existing paper */
    gtk_tree_selection_select_iter (ppd_selection (ppd), pfs.select_iter);
    gtk_tree_iter_free (pfs.select_iter);
  }

 
  for (int i = PAPER_NAME_COL; i <= PAPER_IS_CUSTOM_COL; i++) {
    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *column;
    switch (gtk_tree_model_get_column_type (GTK_TREE_MODEL (paper_store), i)) {
    case G_TYPE_DOUBLE:
      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, paper_col_names[i]);
      gtk_tree_view_column_pack_start (column, renderer, FALSE);
      g_object_set (G_OBJECT (renderer), "xalign", 1.0, NULL);
      gtk_tree_view_column_set_cell_data_func (column,
					       renderer,
					       custom_double_renderer_func,
					       GINT_TO_POINTER (i),
					       custom_double_renderer_destroy);
      break;
    default:
      column =
	gtk_tree_view_column_new_with_attributes (paper_col_names[i],
						  renderer,
						  "text", i,
						  NULL);
    }
    gtk_tree_view_append_column (GTK_TREE_VIEW (paper_tree), column);
  }

  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
  g_signal_connect (G_OBJECT (paper_tree), "realize",
                    G_CALLBACK (paper_tree_realised),
                    scroll);
  frame = gtk_frame_new (_ ("Paper size"));
  gtk_container_add (GTK_CONTAINER (scroll), paper_tree);
  gtk_container_add (GTK_CONTAINER (frame), scroll);
  gtk_box_pack_start (GTK_BOX (content), frame, TRUE, TRUE, 8);
  




  
  /************ css colour *******************/

  frame = gtk_frame_new (_ ("Paper colour"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, TRUE, 8);

  paper_s *paper    = environment_paper (env);
  gchar   *colour_name = paper_colour_name (paper);
  GtkWidget *css_widget = css_button_new (&ppd_css (ppd), colour_name, NULL);
  gtk_container_add (GTK_CONTAINER (frame), css_widget);



  /************ paper orientation *******************/

  frame = gtk_frame_new (_ ("Orientation"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, TRUE, TRUE, 8);
  
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  ppd_portrait (ppd) =
    gtk_radio_button_new_with_label (NULL, _ ("Portrait"));
  GtkWidget *landscape =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (ppd_portrait (ppd)),
						 _ ("Landscape"));

  GtkWidget *active =
    (paper_orientation (current_paper) == GTK_PAGE_ORIENTATION_PORTRAIT) ?
    ppd_portrait (ppd) : landscape;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), ppd_portrait (ppd), TRUE, TRUE, 8);
  gtk_box_pack_start (GTK_BOX (hbox), landscape, TRUE, TRUE, 8);
  


  return content;
}

void
extract_paper (void *private_data, environment_s *env)
{
  paper_s       *current_paper  = NULL;
  GtkPaperSize  *current_size   = NULL;
  GdkRGBA       *current_colour = NULL;
  paper_private_data_s *ppd = private_data;

  if (!ppd) return;

  g_assert (env != NULL);
  
  current_paper = environment_paper (env);

  g_assert (current_paper != NULL);

  current_size = paper_size (current_paper);

  g_assert (current_size != NULL);

  current_colour = paper_colour (current_paper);

  g_assert (current_colour != NULL);
  
  if (ppd_selection (ppd)) {
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (GTK_TREE_SELECTION (ppd_selection (ppd)),
					 &model,
					 &iter)) {
      GtkPaperSize *size = NULL;
      gtk_tree_model_get(model, &iter,
			 PAPER_SIZE_COL, &size,
			 -1);

      if (size) {
	if (current_paper && paper_size (current_paper))
	  gtk_paper_size_free (paper_size (current_paper));
	paper_size (current_paper) = gtk_paper_size_copy (size);
      }
    }
  }


  if (ppd_css (ppd)) {
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (GTK_TREE_SELECTION (ppd_css (ppd)),
					 &model,
					 &iter)) {
      gchar *colour = NULL;
      GdkRGBA rgba;
      gtk_tree_model_get(model, &iter,
			 COLOUR_NAME,  &colour,
			 COLOUR_RED,   &rgba.red,
			 COLOUR_GREEN, &rgba.green,
			 COLOUR_BLUE,  &rgba.blue,
			 COLOUR_ALPHA, &rgba.alpha,
			 -1);
      if (paper_colour_name (current_paper))
	g_free (paper_colour_name (current_paper));
      paper_colour_name (current_paper) = colour;
      if (paper_colour (current_paper))
	gdk_rgba_free (paper_colour (current_paper));
      paper_colour (current_paper) = gdk_rgba_copy (&rgba);
    }
  }

  if (ppd_portrait (ppd)) {
    paper_orientation (current_paper) =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ppd_portrait (ppd))) ?
      GTK_PAGE_ORIENTATION_PORTRAIT : GTK_PAGE_ORIENTATION_LANDSCAPE;
  }
}

void
paper_settings (GtkWidget *object, gpointer data)
{
  GtkWidget     *dialog;
  GtkWidget     *content;
  gint           response;

  sheet_s *sheet = data;
  environment_s *env = sheet_environment (sheet);

  g_assert (env != NULL);

  dialog =  gtk_dialog_new_with_buttons (_ ("Paper settings"),
                                         NULL,
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  void *paper_private_data = NULL;
  GtkWidget *paper_button = select_paper (&paper_private_data, env);
  gtk_container_add (GTK_CONTAINER (content), paper_button);

  gtk_widget_show_all (dialog); 

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    extract_paper (paper_private_data, env);
    do_config (env);
    force_redraw (sheet);
  }
  if (paper_private_data)   g_free (paper_private_data);
  gtk_widget_destroy (dialog);
}
