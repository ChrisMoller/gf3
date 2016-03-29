#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#define _BSD_SOURCE
#include <stdarg.h>
#include <string.h>

char *strsep(char **stringp, const char *delim);

#include "gf.h"
#include "fallbacks.h"
#include "utilities.h"
#include "select_paper.h"
#include "select_pen.h"
#include "select_scale.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "rgbhsv.h"
#include "python.h"
#include "entities.h"
#include "view.h"
#include "../pluginsrcs/plugin.h"

typedef struct {
  gchar 		*script;
  sheet_s 		*sheet;
} script_ctl_s;

void
do_config (environment_s *env)
{
  paper_s      *paper = environment_paper (env);
  GtkPaperSize *size =  paper_size (paper);
  gdouble paper_width, paper_height;
  
  if (paper_orientation (paper) == GTK_PAGE_ORIENTATION_LANDSCAPE) {
    paper_h_dim (paper) =
      paper_width  = gtk_paper_size_get_height (size, environment_dunit (env));
    paper_v_dim (paper) =
      paper_height = gtk_paper_size_get_width (size, environment_dunit (env));
  }
  else {
    paper_h_dim (paper) =
      paper_width  = gtk_paper_size_get_width (size, environment_dunit (env));
    paper_v_dim (paper) =
      paper_height = gtk_paper_size_get_height (size, environment_dunit (env));
  }

  // pixels / drawing unit
  gdouble wppdu = environment_da_wid (env) / paper_width;
  gdouble hppdu = environment_da_ht (env)  / paper_height;
  gdouble minppdu = fmin (wppdu, hppdu);

  gdouble pw_in_pix = minppdu * paper_width;	// pix/du * du = w of ppr in pix
  gdouble ph_in_pix = minppdu * paper_height;	// pix/du * du = h of ppr in pix

  gdouble voff = (environment_da_ht (env)  - ph_in_pix) / 2.0;
  gdouble hoff = (environment_da_wid (env) - pw_in_pix) / 2.0;

  environment_tf_xx (env) = minppdu;
  environment_tf_yx (env) = 0.0;
  environment_tf_xy (env) = 0.0;
  environment_tf_yy (env) = minppdu;
  environment_tf_x0 (env) = hoff + environment_hoff (env);
  environment_tf_y0 (env) = voff + environment_voff (env);

  if (paper_orientation (paper) == GTK_PAGE_ORIENTATION_LANDSCAPE) {
    cairo_matrix_translate (&environment_tf (env), 0.0, paper_height);
    cairo_matrix_scale     (&environment_tf (env), 1.0, -1.0);
  }
}

void
force_redraw (sheet_s *sheet)
{
  environment_s *env = sheet_environment (sheet);
  GtkWidget *da = GTK_WIDGET (environment_da (env));
  if (da && gtk_widget_get_mapped (da))
    gtk_widget_queue_draw_area (da, 0, 0,
				gtk_widget_get_allocated_width (da),
				gtk_widget_get_allocated_height (da));
}

static void
python_script (GtkWidget *widget,
	       gpointer   data)
{
  sheet_s *sheet = data;
  // project_s *project = sheet_project (sheet);

  GtkWidget *dialog;
  gint response;
  
  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.py");
  

  dialog =
    gtk_file_chooser_dialog_new (_ ("Python script"),
				 NULL,
				 GTK_FILE_CHOOSER_ACTION_OPEN,
				 _ ("_OK"), GTK_RESPONSE_ACCEPT,
				 _ ("_Cancel"), GTK_RESPONSE_CANCEL,
				 NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
				   GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    gchar *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    execute_python (file, sheet);
    g_free (file);
  }
  gtk_widget_destroy (dialog);
}

static void
add_sheet (GtkWidget *widget,
	   gpointer   data)
{
  project_s *project =  (project_s *)data;
  sheet_s *this_sheet;
  
  if (project_current_iter (project))
    gtk_tree_iter_free (project_current_iter (project));
  project_current_iter (project) =
    append_sheet (project, NULL, NULL, &this_sheet);
  
  create_new_view (project, this_sheet, NULL);
}

static void
add_detail (GtkWidget *widget,
	    gpointer   data)
{
  sheet_s *sheet = data;
  project_s *project =  sheet_project (sheet);
  sheet_s *this_sheet;
  
  if (project_current_iter (project))
    gtk_tree_iter_free (project_current_iter (project));
  project_current_iter (project) =
    append_sheet (project, sheet_iter (sheet), NULL, &this_sheet);
  
  create_new_view (project, this_sheet, NULL);
}

static void
close_view (GtkWidget *object, gpointer data)
{
  // fixme save stuff
  gtk_widget_destroy (object);
}

static void
custom_sheet_renderer_func (GtkTreeViewColumn *tree_column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer data)
{
  sheet_s *sheet = NULL;
  gtk_tree_model_get(model, iter,
		     SHEET_STRUCT_COL, &sheet,
		     -1);
  gchar *value = sheet ? sheet_name (sheet) : "(unnamed)";
  value = value ? : "(unknown)";
  g_object_set (G_OBJECT (cell), "text", value, NULL);
}


static void
custom_sheet_renderer_destroy (gpointer data)
{
  // do nothing
}


static void
edit_sheet (sheet_s *sheet)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *grid;
  GtkWidget *item;
  GtkWidget *name_ety;
  gint response;
  

  dialog =  gtk_dialog_new_with_buttons (_ ("Edit sheet"),
                                         NULL,
					 GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
				   GTK_RESPONSE_ACCEPT);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (content), grid);

  item = gtk_label_new (_ ("Sheet name"));
  gtk_grid_attach (GTK_GRID (grid), item, 0, 0, 1, 1);
  //                                      l  t  w  h
  
  name_ety = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (name_ety), TRUE);
  
  if (sheet && sheet_name (sheet))
    gtk_entry_set_text (GTK_ENTRY (name_ety), sheet_name (sheet));
  gtk_grid_attach (GTK_GRID (grid), name_ety, 1, 0, 1, 1);

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    if (sheet && sheet_name (sheet)) {
      g_free (sheet_name (sheet));
      project_s *project = sheet_project (sheet);
      GtkWidget *window = sheet_window (sheet);
      sheet_name (sheet) =
	g_strdup (gtk_entry_get_text (GTK_ENTRY (name_ety)));
      gchar *ps_name =
	g_strdup_printf ("%s %s", project_name (project), sheet_name (sheet));
      gtk_window_set_title (GTK_WINDOW (window), ps_name);
      g_free (ps_name);
    }
  }
  gtk_widget_destroy (dialog);
}


static gboolean
tree_clicked_cb (GtkWidget *treeview,
		 GdkEventButton *event,
		 gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  sheet_s *sheet;
  gboolean rc = GDK_EVENT_PROPAGATE;

#if 0
  project_s *project =  (project_s *)data;
#endif
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, SHEET_STRUCT_COL, &sheet, -1);
    if (event->type == GDK_BUTTON_PRESS) {
      switch(event->button) {
      case GDK_BUTTON_SECONDARY:
	edit_sheet (sheet);
	rc = GDK_EVENT_STOP;
	break;
#if 0
      case GDK_BUTTON_PRIMARY:
	if (project_current_iter (project))
	  gtk_tree_iter_free (project_current_iter (project));
	project_current_iter (project) = gtk_tree_iter_copy (&iter);
	return GDK_EVENT_STOP;
	break;
#endif
      default:
	break;
      }
    }
  }
  return rc;
}

static void
sheet_selection_changed_cb (GtkTreeSelection *treeselection,
			    gpointer          data)
{
  project_s *project =  (project_s *)data;
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (treeselection,
				       &model, &iter)) {
    if (project_current_iter (project))
      gtk_tree_iter_free (project_current_iter (project));
    project_current_iter (project) = gtk_tree_iter_copy (&iter);
  }
}

static void
show_sheets (GtkWidget *object, gpointer data)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *scroll = NULL;

  project_s *project =  (project_s *)data;
  dialog =  gtk_dialog_new_with_buttons (_ ("Sheets"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_Close"), GTK_RESPONSE_CLOSE,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  
  scroll =  gtk_scrolled_window_new (NULL,NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll),
					     160);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll),
					     100);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_end (GTK_BOX (content), scroll, TRUE, TRUE, 0);
  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  GtkWidget *tree =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (project_sheets (project)));
  GtkTreeSelection *select =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
                    G_CALLBACK (sheet_selection_changed_cb),
                    project);
  g_signal_connect(tree, "button-press-event",
		   G_CALLBACK (tree_clicked_cb), project);

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Sheet name");
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           custom_sheet_renderer_func,
					   NULL,
                                           custom_sheet_renderer_destroy);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
  






  gtk_container_add (GTK_CONTAINER (scroll), tree);

  gtk_widget_show_all (dialog);
}

static void
scale_settings (GtkWidget *object, gpointer data)
{
  GtkWidget     *dialog;
  GtkWidget     *content;
  gint           response;

  sheet_s *sheet = data;
  environment_s *env = sheet_environment (sheet);

  g_assert (env != NULL);

  dialog =  gtk_dialog_new_with_buttons (_ ("Units & Scale settings"),
                                         NULL,
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  void *scale_private_data = NULL;
  GtkWidget *scale_button = select_scale (&scale_private_data, env);
  gtk_container_add (GTK_CONTAINER (content), scale_button);

  gtk_widget_show_all (dialog); 

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    extract_scale   (scale_private_data, env);
    do_config (env);
    force_redraw (sheet);
  }
  if (scale_private_data) g_free (scale_private_data);
  gtk_widget_destroy (dialog);
}


static void
build_view_menu (GtkWidget *vbox, project_s *project,
		 sheet_s *sheet, environment_s *env)
{
  GtkWidget *menubar;
  GtkWidget *menu;
  GtkWidget *item;

  menubar = gtk_menu_bar_new();

  /********* file menu ********/

  menu = gtk_menu_new();
  item = gtk_menu_item_new_with_label (_ ("File"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

  item = gtk_menu_item_new_with_label (_ ("Script"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (python_script), project);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Add sheet"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (add_sheet), project);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = gtk_menu_item_new_with_label (_ ("Add detail"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (add_detail), sheet);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Show sheet structure"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (show_sheets), project);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);

  item = gtk_menu_item_new_with_label (_ ("Quit"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (close_view), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  
  /********* settings ********/

  menu = gtk_menu_new();
  item = gtk_menu_item_new_with_label (_ ("Settings"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

  item = gtk_menu_item_new_with_label (_ ("Paper"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (paper_settings), sheet);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Units & Scale"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (scale_settings), sheet);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Pen"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (pen_settings), sheet);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (menubar), FALSE, FALSE, 2);
}

#ifdef USE_SCALE
static void
zoom_changed (GtkRange *range,
	      gpointer  user_data)
{
  sheet_s *sheet = user_data;
  environment_s *env = sheet_environment (sheet);
  //  paper_s      *paper = environment_paper (env);
  environment_zoom (env) = gtk_range_get_value (range);
  gtk_adjustment_set_page_size (environment_hadj (env),
				1.0 / environment_zoom (env));
  gtk_adjustment_set_page_size (environment_vadj (env),
				1.0 / environment_zoom (env));
  //  if (paper_orientation (paper) == GTK_PAGE_ORIENTATION_LANDSCAPE)
    gtk_adjustment_set_value (environment_vadj (env),
			      1.0 - 1.0 / environment_zoom (env));
  force_redraw (sheet);
}
#else
void
zoom_changed (GtkSpinButton *spin_button,
	      gpointer       user_data)
{
  sheet_s *sheet = user_data;
  environment_s *env = sheet_environment (sheet);

  environment_zoom (env) = gtk_spin_button_get_value (spin_button);
  gtk_adjustment_set_page_size (environment_hadj (env),
				1.0 / environment_zoom (env));
  gtk_adjustment_set_page_size (environment_vadj (env),
				1.0 / environment_zoom (env));
  //  if (paper_orientation (paper) == GTK_PAGE_ORIENTATION_LANDSCAPE)
#if 0
  gtk_adjustment_set_value (environment_vadj (env),
			    1.0 - 1.0 / environment_zoom (env));
  gtk_adjustment_set_value (environment_hadj (env),
			    1.0 - 1.0 / environment_zoom (env));
#endif
#if 0
  GtkAdjustment *adj = gtk_spin_button_get_adjustment (spin_button);
  gdouble incr = (environment_zoom (env) > 1.0) ? 1.0 : 0.1;
  gtk_spin_button_configure (spin_button, adj, incr, 3);
#endif
  force_redraw (sheet);
}
#endif

#define MARKUP_FORMAT \
  "<span size=\"xx-small\" background=\"#%02x%02x%02x\">    </span>"

void
set_current_line_colour (environment_s *env, pen_s *pen)
{
  guint rv, gv, bv;
  gboolean sensitive;

  GList *kids =
    gtk_container_get_children (GTK_CONTAINER (environment_lcb (env)));
  GtkLabel *lbl = kids ? kids->data : NULL;
  if (kids) g_list_free (kids);

  if (!lbl) return;
  
  
  if (pen) {
    rv = (unsigned int)lrint (255.0 * pen_colour_red (pen));
    gv = (unsigned int)lrint (255.0 * pen_colour_green (pen));
    bv = (unsigned int)lrint (255.0 * pen_colour_blue (pen));
    sensitive = TRUE;
  }
  else {
    rv = gv = bv = 127;
    sensitive = FALSE;
  }
  gchar *markup = g_strdup_printf (MARKUP_FORMAT, rv, gv, bv);
  gtk_label_set_markup (lbl, markup);
  g_free (markup);
  gtk_widget_set_sensitive (environment_lcb (env), sensitive);
}

static void
colour_button_clicked_cb (GtkButton *widget,
			  gpointer   user_data)
{
  g_assert_nonnull (user_data);
  pen_s *pen = NULL;
  sheet_s *sheet = user_data;
  entity_none_s *ety = sheet_tool_entity (sheet);
  if (ety) {
    switch (entity_type (ety)) {
    case ENTITY_TYPE_CIRCLE:
      pen = entity_circle_pen ((entity_circle_s *)ety);
      break;
    case ENTITY_TYPE_ELLIPSE:
      pen = entity_ellipse_pen ((entity_ellipse_s *)ety);
      break;
    case ENTITY_TYPE_TEXT:
      pen = entity_text_pen ((entity_text_s *)ety);
      break;
    case ENTITY_TYPE_POLYLINE:
      pen = entity_polyline_pen ((entity_polyline_s *)ety);
      break;
    case ENTITY_TYPE_NONE:
    case ENTITY_TYPE_TRANSFORM:
    case ENTITY_TYPE_GROUP:
      break;
    }
  }
  else pen = sheet_environment (sheet) ?
      environment_pen (sheet_environment (sheet)) : NULL;

  if (pen) {
    GtkWidget     *dialog;
    GtkWidget     *content;
    dialog =  gtk_dialog_new_with_buttons (_ ("Pen settings"),
					   GTK_WINDOW (sheet_window (sheet)),
					   0,
					   _ ("_Cancel"), GTK_RESPONSE_CANCEL,
					   NULL);

    // catch sig to close -- fixme
    
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
    content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    void *pen_private_data = NULL;
    GtkWidget *pen_button = select_pen (&pen_private_data, pen);
    gtk_container_add (GTK_CONTAINER (content), pen_button);
    
    gtk_widget_show_all (dialog); 
  }
}

static void
build_info_bar (GtkWidget *vbox, project_s *project,
		sheet_s *sheet, environment_s *env)
{
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 2);

  
  GtkWidget *lbl = gtk_label_new (_ ("Loc"));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, TRUE, 2);
  
  environment_readout (env) = gtk_label_new ("00.00 00.00");
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (environment_readout (env)),
		      FALSE, FALSE, 2);


  
  lbl = gtk_label_new (_ ("Zoom"));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, TRUE, 2);

#ifdef USE_SCALE
  GtkWidget *zoom = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
					      1.0, 20.0, 0.1);
  g_signal_connect (G_OBJECT (zoom), "value-changed",
                    G_CALLBACK (zoom_changed), sheet);
  gtk_scale_set_draw_value (GTK_SCALE (zoom), TRUE);
  gtk_scale_set_value_pos (GTK_SCALE (zoom), GTK_POS_TOP);
  gtk_scale_set_digits (GTK_SCALE (zoom), 2);
  gtk_range_set_value (GTK_RANGE (zoom), 1.0);
#else
  GtkWidget *zoom = gtk_spin_button_new_with_range (0.0, 10.0, 0.01);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (zoom), TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (zoom), 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (zoom), 2);
  g_signal_connect (G_OBJECT (zoom), "value-changed",
                    G_CALLBACK (zoom_changed), sheet);
#endif
  
  environment_scale (env) = zoom;
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (zoom), FALSE, TRUE, 2);


  /*********** colour button ************/
  
  environment_lcb (env) = gtk_button_new ();
  lbl = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (environment_lcb (env)), lbl);
  set_current_line_colour (env, NULL);
  g_signal_connect (G_OBJECT (environment_lcb (env)), "clicked",
                    G_CALLBACK (colour_button_clicked_cb), sheet);
  gtk_box_pack_start (GTK_BOX (hbox), environment_lcb (env),
		      FALSE, FALSE, 2);
}

static gboolean
da_configure_cb (GtkWidget *widget,
		 GdkEvent  *event,
		 gpointer   user_data)
{
  environment_s *env = user_data;

  if (environment_surface (env))
    cairo_surface_destroy (environment_surface (env));
  environment_surface (env) =
    gdk_window_create_similar_surface (gtk_widget_get_window (widget),
				   CAIRO_CONTENT_COLOR,
				   gtk_widget_get_allocated_width (widget),
				   gtk_widget_get_allocated_height (widget));

  gint wx, wy;
  gtk_widget_translate_coordinates(widget, gtk_widget_get_toplevel(widget),
				   0, 0, &wx, &wy);
  environment_voff (env) = (gdouble)wy;
  environment_hoff (env) = (gdouble)wx;

  environment_da_ht (env) =
    (gdouble)gtk_widget_get_allocated_height (widget);
  environment_da_wid (env) =
    (gdouble)gtk_widget_get_allocated_width (widget);

  do_config (env);
  
  return GDK_EVENT_PROPAGATE;	
}

static gboolean
da_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  sheet_s       *sheet = (sheet_s *)data;
  environment_s *env   = sheet_environment (sheet);
  paper_s       *paper = environment_paper (env);
  pen_s         *pen   = environment_pen (env);


  
  cairo_set_source_surface (cr, environment_surface (env), 0.0, 0.0);

  cairo_set_matrix (cr, &environment_tf (env));

  // factor in zoom
  cairo_scale (cr,  environment_zoom (env), environment_zoom (env));

  // factor in scroll
  cairo_translate (cr,
		   -gtk_adjustment_get_value (environment_hadj (env))
		   * paper_h_dim (paper),
		   -gtk_adjustment_get_value (environment_vadj (env))
		   * paper_v_dim (paper));

  // draw paper
  cairo_set_source_rgba (cr,
			 paper_colour_red (paper),
			 paper_colour_green (paper),
			 paper_colour_blue (paper),
			 paper_colour_alpha (paper));
  cairo_rectangle (cr, 0.0, 0.0, paper_h_dim (paper), paper_v_dim (paper));
  cairo_fill (cr);

  // move origin
  cairo_translate (cr, environment_org_x (env), environment_org_y (env));

  // draw origin 
  if (environment_show_org (env)) {
    GdkRGBA rgba;
    memmove (&rgba,  paper_colour (paper), sizeof(GdkRGBA));
    compliment (&rgba);
    cairo_set_source_rgba (cr, rgba.red, rgba.green, rgba.blue, 1.0);
    cairo_arc (cr, 0.0, 0.0, 0.06, 0.0, 2.0 * G_PI);
    cairo_fill (cr);
    cairo_set_line_width (cr,  0.02);
    cairo_move_to (cr, -0.25,  0.0);
    cairo_line_to (cr,  0.25,  0.0);
    cairo_move_to (cr,  0.0, -0.25);
    cairo_line_to (cr,  0.0,  0.25);
    cairo_stroke (cr);
  }

  // factor in user scale
  cairo_scale (cr, 1.0/environment_uupdu (env), 1.0/environment_uupdu (env));

  cairo_get_matrix (cr, &environment_inv_tf (env));
  cairo_matrix_invert (&environment_inv_tf (env));

  double clip_min_x = -environment_org_x (env) * environment_uupdu (env);
  double clip_min_y = -environment_org_y (env) * environment_uupdu (env);
  double clip_max_w =  paper_h_dim (paper) * environment_uupdu (env);
  double clip_max_h =  paper_v_dim (paper) * environment_uupdu (env);

  cairo_rectangle (cr, clip_min_x, clip_min_y, clip_max_w, clip_max_h);
  cairo_clip (cr);
		      
  cairo_set_source_rgba (cr,
			 pen_colour_red (pen),
			 pen_colour_green (pen),
			 pen_colour_blue (pen),
			 pen_colour_alpha (pen));

  cairo_set_line_width (cr,  pen_lw_cvt (env));

  cairo_save (cr);
  environment_cr (env) = cr;
  if (sheet_entities (sheet))
    g_list_foreach (sheet_entities (sheet), draw_entities, env);
  if (sheet_transients (sheet))
    g_list_foreach (sheet_transients (sheet), draw_entities, env);
  cairo_restore (cr);
 
#if 0
  // dummy stuff for test
  // line to origin
  cairo_move_to (cr, 2.0, 2.0);
  cairo_line_to (cr, 0.0, 0.0);
  cairo_stroke (cr);

  // rect from  1,1 to 10,7.5
  cairo_rectangle (cr, 1.0, 1.0, 9.0, 6.5);
  cairo_stroke (cr);         

  // rect from  2,2 to 9,6.5
  cairo_rectangle (cr, 2.0, 2.0, 7.0, 4.5);
  cairo_stroke (cr);
#endif
  
  return GDK_EVENT_STOP;		// handled
}

static gboolean
da_motion_cb (GtkWidget *widget,
              GdkEvent  *event,
              gpointer   user_data) 
{
  sheet_s       *sheet = (sheet_s *)user_data;
  environment_s *env = sheet_environment (sheet);
  GdkEventMotion *motion = (GdkEventMotion *)event;

  gdouble px = motion->x + environment_hoff (env);
  gdouble py = motion->y + environment_voff (env);
  cairo_matrix_transform_point (&environment_inv_tf (env), &px, &py);
  snap_to_grid (env, &px, &py);

  gchar *lbl = g_strdup_printf ("%#.4g %#.4g\n", px, py);
  gtk_label_set_text (GTK_LABEL (environment_readout (env)), lbl);
  g_free (lbl);

  button_f bf = return_current_tool ();
  if(bf) {
    (*bf)(event, sheet, px, py);
  }
	   
  return GDK_EVENT_PROPAGATE;
}

void
hadj_value_changed_cb (GtkAdjustment *adjustment,
		       gpointer       user_data)
{
  sheet_s *sheet = user_data;
  force_redraw (sheet);
}

void
vadj_value_changed_cb (GtkAdjustment *adjustment,
		       gpointer       user_data)
{
  sheet_s *sheet = user_data;
  force_redraw (sheet);
}

static void
unzoom_clicked (GtkButton *button,
		gpointer   user_data)
{
  sheet_s *sheet = user_data;
  environment_s *env = sheet_environment (sheet);
  
  gtk_adjustment_set_value (environment_hadj (env), 0.0);
  gtk_adjustment_set_value (environment_vadj (env), 0.0);
  gtk_adjustment_set_page_size (environment_hadj (env), 1.0);
  gtk_adjustment_set_page_size (environment_vadj (env), 1.0);
  environment_zoom (env) = 1.0;
#ifdef USE_SCALE
  gtk_range_set_value (GTK_RANGE (environment_scale (env)), 1.0);
#else
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (environment_scale (env)), 1.0);
#endif
  force_redraw (sheet);
}

#if 0
static gboolean
da_window_enter_exit_cb (GtkWidget *widget,
		  GdkEvent  *event,
		  gpointer   user_data)
{
  gint etype = ((GdkEventAny *)event)->type;

  if (etype == GDK_ENTER_NOTIFY) {
    sheet_s *sheet = user_data;
    set_active_sheet (sheet);
  }
  
  notify_key_set (etype);
  return GDK_EVENT_PROPAGATE;
}
#endif

static gboolean
da_enter_exit_cb (GtkWidget *widget,
		  GdkEvent  *event,
		  gpointer   user_data)
{
  gint etype = ((GdkEventAny *)event)->type;

  if (etype == GDK_ENTER_NOTIFY) {
    sheet_s *sheet = user_data;
    set_active_sheet (sheet);
  }
  
  notify_key_set (etype);
  return GDK_EVENT_PROPAGATE;
}


static gboolean
da_button_cb (GtkWidget *widget,
	      GdkEvent  *event,
	      gpointer   user_data)
{
  sheet_s *sheet = user_data;
  //  GdkEventButton *button = (GdkEventButton *)event;

  button_f bf = return_current_tool ();
  if(bf) {
    (*bf)(event, sheet, NAN, NAN);
  }
  return GDK_EVENT_PROPAGATE;
}

static gboolean
da_key_cb (GtkWidget *widget,
	   GdkEvent  *event,
	   gpointer   user_data)
{
  sheet_s *sheet = user_data;
  button_f bf = return_current_tool ();
  if(bf) {
    (*bf)(event, sheet, NAN, NAN);
  }
  return GDK_EVENT_PROPAGATE;
}

static void
build_view_grid (GtkWidget *vbox,
		 project_s *project,
		 sheet_s *sheet,
		 environment_s *env,
		 const gchar *script)
{
  GtkWidget *grid;
  GtkWidget *hscroll;
  GtkWidget *vscroll;
  GtkWidget *da;
  GtkWidget *unzoom_button;
  
//  environment_s *env = project_environment (project);

  grid = gtk_grid_new ();

  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (grid), TRUE, TRUE, 2);

  GtkAdjustment *vadj = gtk_adjustment_new (1.0,	// value
					    0.0,	// lower
					    1.0,	// upper
					    0.01,	// step_increment
					    0.01,	// page_increment
					    1.0);	// page_size
  environment_vadj (env) = vadj;
  g_signal_connect (vadj, "value-changed",
		    G_CALLBACK (vadj_value_changed_cb), sheet);

  vscroll = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, vadj);
  gtk_grid_attach (GTK_GRID (grid), vscroll, 1, 0, 1, 1);
  
  GtkAdjustment *hadj = gtk_adjustment_new (1.0,	// value
					    0.0,	// lower
					    1.0,	// upper
					    0.01,	// step_increment
					    0.01,	// page_increment
					    1.0);	// page_size
  environment_hadj (env) = hadj;
  g_signal_connect (hadj, "value-changed",
		    G_CALLBACK (hadj_value_changed_cb), sheet);
  
  hscroll = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadj);
  gtk_grid_attach (GTK_GRID (grid), hscroll, 0, 1, 1, 1);

  unzoom_button = gtk_button_new ();
  g_signal_connect (unzoom_button, "clicked",
		    G_CALLBACK (unzoom_clicked), sheet);
  gtk_widget_set_tooltip_text (unzoom_button, _ ("Unzoom"));
  gtk_grid_attach (GTK_GRID (grid), unzoom_button, 1, 1, 1, 1);

  da = gtk_drawing_area_new ();
  gtk_widget_set_hexpand (da, TRUE);
  gtk_widget_set_vexpand (da, TRUE);
  gtk_widget_add_events (da,
			 GDK_ENTER_NOTIFY_MASK		|
			 GDK_LEAVE_NOTIFY_MASK		|
			 GDK_SCROLL_MASK		|
			 GDK_POINTER_MOTION_MASK	|
			 GDK_POINTER_MOTION_HINT_MASK	|
			 GDK_BUTTON_PRESS_MASK		|
			 GDK_BUTTON_RELEASE_MASK)	;
  environment_da (env) = da;
  environment_zoom (env) = 1.0;

#if 1
  g_signal_connect (da, "enter-notify-event",
		    G_CALLBACK (da_enter_exit_cb), sheet);
  g_signal_connect (da, "leave-notify-event",
		    G_CALLBACK (da_enter_exit_cb), sheet);
#endif
  g_signal_connect (da, "configure-event",
		    G_CALLBACK (da_configure_cb), env);
  g_signal_connect (da, "draw",
		    G_CALLBACK (da_draw_cb), sheet);
  g_signal_connect (da, "motion-notify-event",
		    G_CALLBACK (da_motion_cb), sheet);
  g_signal_connect (da, "button-press-event",
		    G_CALLBACK (da_button_cb), sheet);
  gtk_grid_attach (GTK_GRID (grid), da, 0, 0, 1, 1);
}

static void
eval_python_expr (GtkEntry *entry,
		  gpointer  user_data)
{
  sheet_s *sheet = user_data;
  environment_s *env = sheet_environment (sheet);
  
  if (0 == gtk_entry_get_text_length (entry)) return;
						
  const gchar *text = gtk_entry_get_text (entry);
  
  environment_history (env) =	// fixme free it up
    g_list_append (environment_history (env), g_strdup (text));
  environment_histptr (env) = g_list_last (environment_history (env));
  
  gchar *phrase = g_strdup (text);

  gchar *ptr; 
  gchar *copy =  phrase;
  while ((ptr = strsep (&copy, ";"))) {
    size_t off = strspn(ptr, " \t");	// skip leading ws
    evaluate_python (ptr + off, sheet);
  }

  gtk_entry_set_text (entry, "");
  g_free (phrase);
}

static void
icon_pressed (GtkEntry            *entry,
              GtkEntryIconPosition icon_pos,
              GdkEvent            *event,
              gpointer             user_data)
{
  environment_s *env = user_data;

  if (environment_history (env)) {
    gchar *text = NULL;

    if (!environment_histptr (env))
      environment_histptr (env) = environment_history (env);
    switch(icon_pos) {
    case GTK_ENTRY_ICON_SECONDARY:
      if ((environment_histptr (env))->next)
	environment_histptr (env) = (environment_histptr (env))->next;
      text = (environment_histptr (env))->data;
      break;
    case GTK_ENTRY_ICON_PRIMARY:
      text = (environment_histptr (env))->data;
      if ((environment_histptr (env))->prev)
	environment_histptr (env) = (environment_histptr (env))->prev;
      break;
    }
    if (text) gtk_entry_set_text (entry, text);
  }
}

static gboolean
key_pressed (GtkWidget *widget,
	     GdkEvent  *event,
	     gpointer   user_data)
{
  GdkEventKey *keyevent = (GdkEventKey *)event;
  guint keyval = keyevent->keyval;
  GtkEntryIconPosition pos = -1;
  switch(keyval) {
  case GDK_KEY_Up:
    pos = GTK_ENTRY_ICON_PRIMARY;
    break;
  case GDK_KEY_Down:
    pos = GTK_ENTRY_ICON_SECONDARY;
    break;
  }
  if (pos != -1) {
    icon_pressed (GTK_ENTRY (widget), pos, event, user_data);
    return GDK_EVENT_STOP;
  }
  else return GDK_EVENT_PROPAGATE;
}

#if 0
static gboolean
map_event (GtkWidget *widget,
	   GdkEvent  *event,
	   gpointer   user_data)
{
  script_ctl_s *script_ctl = user_data;
  
  if (script_ctl->sheet && script_ctl->script) {
    execute_python (script_ctl->script, script_ctl->sheet);
  }
  g_free (script_ctl);

  return GDK_EVENT_PROPAGATE;
}
#endif
  
void
create_new_view (project_s *project, sheet_s *this_sheet, const gchar *script)
{
  GtkWidget *window;
  GtkWidget *vbox;

  g_assert (project != NULL);
  
  environment_s	*env = sheet_environment (this_sheet);
  
  g_assert (env != NULL);
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
#if 0
  g_signal_connect (window, "enter-notify-event",
		    G_CALLBACK (da_enter_exit_cb), this_sheet);
  g_signal_connect (window, "leave-notify-event",
		    G_CALLBACK (da_enter_exit_cb), this_sheet);
#endif
  g_signal_connect (window, "key-press-event",
		    G_CALLBACK (da_key_cb), NULL);
  g_signal_connect (window, "key-release-event",
		    G_CALLBACK (da_key_cb), NULL);
  sheet_window (this_sheet) = window;
  gchar *ps_name =
    g_strdup_printf ("%s %s", project_name (project), sheet_name (this_sheet));
  gtk_window_set_title (GTK_WINDOW (window), ps_name);
  g_free (ps_name);
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  gint ww, wh;
  paper_s *paper = environment_paper (env);
  if (paper_orientation (paper) == GTK_PAGE_ORIENTATION_LANDSCAPE) {
    ww = INITIAL_WINDOW_LONG_AXIS;
    wh = INITIAL_WINDOW_SHORT_AXIS;
  }
  else {
    wh = INITIAL_WINDOW_LONG_AXIS;
    ww = INITIAL_WINDOW_SHORT_AXIS;
  }
  gtk_window_set_default_size (GTK_WINDOW (window), ww, wh);

  g_signal_connect (window, "destroy",
		    G_CALLBACK (close_view), NULL);
#if 0
  script_ctl_s *script_ctl = gfig_try_malloc0 (sizeof(script_ctl_s));
  // will be freed by map_event
  script_ctl->script = script;
  script_ctl->sheet  = this_sheet;
  g_signal_connect (window, "map-event",
		    G_CALLBACK (map_event), script_ctl);
#else
  if (script) execute_python (script, this_sheet);
#endif
  
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  build_view_menu (vbox, project, this_sheet, env);
  
  build_info_bar (vbox, project, this_sheet, env);
  
#ifdef USE_BUILD_CONTROLS
  build_view_controls (vbox, project);
#endif

  build_view_grid (vbox, project, this_sheet, env, script);

  GtkWidget *entry = gtk_entry_new ();
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
				     GTK_ENTRY_ICON_PRIMARY,
				     "go-up");
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
				     GTK_ENTRY_ICON_SECONDARY,
				     "go-down");
  g_signal_connect (entry, "icon-press", G_CALLBACK (icon_pressed), env);
  g_signal_connect (entry, "key-press-event", G_CALLBACK (key_pressed), env);
  
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
				  (_ ("Python expression")));
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (entry), FALSE, FALSE, 2);

  g_signal_connect (entry, "activate",
		    G_CALLBACK (eval_python_expr),
		    this_sheet);

  gtk_widget_show_all (window);
}

