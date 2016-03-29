#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#define __USE_GNU
#include <math.h>
#include <search.h>
#include <stdlib.h>
#include <strings.h>

#include "gf.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "persistents.h"
#include "rgbhsv.h"

static void *colours_hash = NULL;
static gboolean colours_hash_modified = FALSE;
static GtkListStore *colour_store = NULL;
static gint name_seqr = 1;


static gchar *css_col_names[] = {
  "Label",
  "Hex value",
  "Alpha",
  "Red",
  "Green",
  "Blue",
  "Hue",
  "Saturation",
  "Value"
};

const gchar *
get_css_colour_name (gint idx)
{
  return (idx >= 0 && idx < CSS_COLOUR_COUNT) ? css_colours[idx].name : "";
}


static int
colours_hash_compare_struct (const void *a, const void *b)
{
  const css_colours_s *aa = (const css_colours_s *)a;
  const css_colours_s *ba = (const css_colours_s *)b;
  return strcasecmp (aa->name, ba->name);
}

static int
colours_hash_compare_string (const void *a, const void *b)
{
  const gchar         *aa = (const gchar *)a;
  const css_colours_s *ba = (const css_colours_s *)b;
  return strcasecmp (aa, ba->name);
}

css_colours_s *
find_css_colour_rgba (const gchar *str, GdkRGBA *rgba)
{
  g_assert (rgba != NULL);
  
  css_colours_s *rc = NULL;

  void *res = tfind ((const void *)str, &colours_hash,
		     colours_hash_compare_string);
  if (res) {
    rc = *(css_colours_s **)res;
    rgba->red   = rc->red;
    rgba->green = rc->green;
    rgba->blue  = rc->blue;
    rgba->alpha = rc->alpha;
  }

  return rc;
}

static  css_colours_s *
find_css_colour (const gchar *str,
		 gdouble *r, gdouble *g, gdouble *b, gdouble *a)
{
  css_colours_s *rc = NULL;

  void *res = tfind ((const void *)str, &colours_hash,
		     colours_hash_compare_string);
  if (res) rc = *(css_colours_s **)res;

  return rc;
}

gboolean colour_table_modified () {
  return colours_hash_modified;
}

void
append_colour_ety (css_colours_s *ety)
{
  tsearch (ety, &colours_hash, colours_hash_compare_struct);

  colours_hash_modified = TRUE;
      
  GtkTreeIter   iter;

  gtk_list_store_append (colour_store, &iter);
  gtk_list_store_set (colour_store, &iter,
		      COLOUR_NAME,		ety->name,
		      COLOUR_HEX,		ety->hex,
		      COLOUR_ALPHA,		ety->alpha,
		      COLOUR_RED,		ety->red,
		      COLOUR_GREEN,		ety->green,
		      COLOUR_BLUE,		ety->blue,
		      COLOUR_HUE,		ety->hue,
		      COLOUR_SATURATION,	ety->saturation,
		      COLOUR_VALUE,		ety->value,
		      -1);
}

static void
append_colour (gchar *aname, gchar *hex,
	       GdkRGBA* rgba, gdouble h, gdouble s, gdouble v)
{
  css_colours_s *ety = gfig_try_malloc0 (sizeof(css_colours_s));
  ety->name		= aname;
  ety->preset		= FALSE;
  ety->hex		= hex;
  ety->alpha		= rgba->alpha;
  ety->red		= rgba->red;
  ety->green		= rgba->green;
  ety->blue		= rgba->blue;
  ety->hue		= h;
  ety->saturation	= s;
  ety->value		= v;

  append_colour_ety (ety);
}

void
store_custom_colours (GKeyFile *key_file)
{
  void colours_hash_walk (const void *nodep,
		   const VISIT which,
		   const int depth) {
    switch(which) {
    case leaf:
      {
        css_colours_s *ety = *(css_colours_s **) nodep;
        if (!ety->preset) save_colour_entry (ety, key_file);
      }
      break;
    default:
      break;
    }
  }

  twalk (colours_hash, colours_hash_walk);
}



static void
build_colours_list ()
{
  for (gint i = 0; i < CSS_COLOUR_COUNT; i++) {
    GtkTreeIter   iter;

    tsearch (&css_colours[i], &colours_hash, colours_hash_compare_struct);
    gtk_list_store_append (colour_store, &iter);
    gtk_list_store_set (colour_store, &iter,
			COLOUR_NAME,		css_colours[i].name,
			COLOUR_HEX,		css_colours[i].hex,
			COLOUR_ALPHA,		css_colours[i].alpha,
			COLOUR_RED,		css_colours[i].red,
			COLOUR_GREEN,		css_colours[i].green,
			COLOUR_BLUE,		css_colours[i].blue,
			COLOUR_HUE,		css_colours[i].hue,
			COLOUR_SATURATION,	css_colours[i].saturation,
			COLOUR_VALUE,		css_colours[i].value,
			-1);
  }
}

void
build_colours_store ()
{
  colour_store = gtk_list_store_new (COLOUR_N_COLUMNS,
				     G_TYPE_STRING,
				     G_TYPE_STRING,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE,
				     G_TYPE_DOUBLE);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (colour_store),
					COLOUR_NAME, GTK_SORT_ASCENDING);

  build_colours_list ();
}

static void
custom_cell_func (GtkTreeViewColumn *tree_column,
		  GtkCellRenderer *cell,
		  GtkTreeModel *model,
		  GtkTreeIter *iter,
		  gpointer data) {
  gchar *name;
  GdkRGBA col = {.alpha = 1.0};
  GdkRGBA ccol;
  gtk_tree_model_get (model, iter,
		      COLOUR_NAME,  &name,
		      COLOUR_RED,   &col.red,
		      COLOUR_GREEN, &col.green,
		      COLOUR_BLUE,  &col.blue,
		      -1);
  memmove (&ccol, &col, sizeof(GdkRGBA));
  compliment (&ccol);
  g_object_set (cell,
		"foreground-rgba", &ccol,
		"cell-background-rgba", &col,
		"text", name,
		NULL);
  g_free (name);
}

static void
custom_cell_destroy (gpointer data)
{
  // do nothing
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

static void
column_clicked_cb (GtkTreeViewColumn *treeviewcolumn,
		   gpointer user_data)
{
  gint sc = GPOINTER_TO_INT (user_data);
  gint cs;
  GtkSortType order;

  gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (colour_store),
					&cs, &order);

  if (cs == sc)
    order = (order == GTK_SORT_ASCENDING) ?
      GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
  else order = GTK_SORT_ASCENDING;

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (colour_store),
					sc, order);
}

enum {
  COLOUR_FMT_ALL,
  COLOUR_FMT_HEX,
  COLOUR_FMT_RGB,
  COLOUR_FMT_HSV
};

typedef struct {
  gint which;
  gint idx;
} cols_func_s;

static void
cols_func (gpointer data, gpointer user_data)
{
  cols_func_s *cfs = user_data;
  GtkTreeViewColumn *tree_column = GTK_TREE_VIEW_COLUMN (data);
  gboolean doit = FALSE;
  switch(cfs->which) {
  case COLOUR_FMT_ALL:
    doit = TRUE;
    break;
  case COLOUR_FMT_HEX:
    doit = (cfs->idx == COLOUR_NAME || cfs->idx == COLOUR_HEX) ? TRUE : FALSE;
    break;
  case COLOUR_FMT_RGB:
    doit = (cfs->idx == COLOUR_NAME  ||
	    cfs->idx == COLOUR_RED   || cfs->idx == COLOUR_BLUE ||
	    cfs->idx == COLOUR_GREEN || cfs->idx == COLOUR_ALPHA)
      ? TRUE : FALSE;
    break;
  case COLOUR_FMT_HSV:
    doit = (cfs->idx == COLOUR_NAME  ||
	    cfs->idx == COLOUR_HUE   || cfs->idx == COLOUR_SATURATION ||
	    cfs->idx == COLOUR_VALUE || cfs->idx == COLOUR_ALPHA)
      ? TRUE : FALSE;
    break;
  }
  gtk_tree_view_column_set_visible (tree_column, doit);
  cfs->idx++;
}

static void
colour_fmt_changed (GtkToggleButton *togglebutton,
		    GtkTreeSelection *selection,
		    gint which)
{
  if (gtk_toggle_button_get_active (togglebutton)) {
    cols_func_s cfs;
    cfs.which = which;
    cfs.idx   = 0;
    GtkTreeView *tv = gtk_tree_selection_get_tree_view (selection);
    GList *cols = gtk_tree_view_get_columns (tv);
    g_list_foreach (cols, cols_func, &cfs);
    g_list_free (cols);
  }
}

static void
colour_fmt_changed_all (GtkToggleButton *togglebutton,
			gpointer         user_data)
{
  if (gtk_toggle_button_get_active (togglebutton))
    colour_fmt_changed (togglebutton, GTK_TREE_SELECTION (user_data),
			COLOUR_FMT_ALL);
}

static void
colour_fmt_changed_hex (GtkToggleButton *togglebutton,
			gpointer         user_data)
{
  if (gtk_toggle_button_get_active (togglebutton))
    colour_fmt_changed (togglebutton, GTK_TREE_SELECTION (user_data),
			COLOUR_FMT_HEX);
}

static void
colour_fmt_changed_rgb (GtkToggleButton *togglebutton,
			gpointer         user_data)
{
  if (gtk_toggle_button_get_active (togglebutton))
    colour_fmt_changed (togglebutton, GTK_TREE_SELECTION (user_data),
			COLOUR_FMT_RGB);
}

static void
colour_fmt_changed_hsv (GtkToggleButton *togglebutton,
			gpointer         user_data)
{
  if (gtk_toggle_button_get_active (togglebutton))
    colour_fmt_changed (togglebutton, GTK_TREE_SELECTION (user_data),
			COLOUR_FMT_HSV);
}

static void
custom_clicked (GtkButton *button,
		gpointer   user_data)
{
  GtkWidget *dialog;
  GtkWidget *content;
  gboolean response;
  GdkRGBA rgba;
  
  dialog =  gtk_dialog_new_with_buttons (_ ("Custom colour"),
					 NULL,
					 GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
					 _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);
  gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  GtkWidget *custom_colour = gtk_color_selection_new ();
  gtk_box_pack_start (GTK_BOX (content), GTK_WIDGET (custom_colour),
		      FALSE, FALSE, 2);

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (content), GTK_WIDGET (hbox),
		      FALSE, FALSE, 2);
  GtkWidget *ety = gtk_entry_new ();
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (ety), FALSE, FALSE, 2);
  GtkWidget *lbl = gtk_label_new (_ ("Colour label"));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    gdouble h, s, v;
    gchar *aname;
      
    gtk_color_selection_get_current_rgba (GTK_COLOR_SELECTION (custom_colour),
					    &rgba);
    gchar *hex = g_strdup_printf ("#%02x%02x%02x%02x",
				  (unsigned int)lrint (rgba.red   * 255.0),
				  (unsigned int)lrint (rgba.green * 255.0),
				  (unsigned int)lrint (rgba.blue  * 255.0),
				  (unsigned int)lrint (rgba.alpha * 255.0));

    RGBtoHSV (rgba.red, rgba.green, rgba.blue, &h, &s, &v);

    const gchar *name = gtk_entry_get_text (GTK_ENTRY (ety));
      
    if (!name || !*name)	// invent a name
      aname = g_strdup_printf ("Custom%d", name_seqr++);
    else {
      css_colours_s *cn = find_css_colour (name, NULL, NULL, NULL, NULL);
      if (cn)		// name exists, invent unique variation
	aname = g_strdup_printf ("%s%d", name, name_seqr++);
      else		// name is non null && unique
	aname = g_strdup (name);
    }
    append_colour (aname, hex, &rgba, h, s, v);
  }
  gtk_widget_destroy (dialog);
}

typedef struct {
  gchar *tgt;
  GtkTreeIter *iter;
} fnc_s;

static gboolean
find_named_colour (GtkTreeModel *model,
		   GtkTreePath *path,
		   GtkTreeIter *iter,
		   gpointer data)
{
  fnc_s *fnc = (fnc_s *)data;
  gchar *tgt = fnc->tgt;
  gchar *name;
  gboolean rc;
  gtk_tree_model_get (model, iter,
		      COLOUR_NAME,  &name,
		      -1);
  rc = (!g_strcmp0 (name, tgt)) ? TRUE : FALSE;
  if (rc) fnc->iter = gtk_tree_iter_copy (iter);
  g_free (name);
  return rc;
}

static void
css_row_activated (GtkTreeView *tree_view,
		   GtkTreePath *path,
		   GtkTreeViewColumn *column,
		   gpointer user_data)
{
  pen_s *pen = user_data;
  g_print ("row activated %p\n", pen);
  if (pen) {
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
    if (gtk_tree_model_get_iter (model, &iter, path)) {
      GdkRGBA rgba;
      gchar *colour = NULL;
      gtk_tree_model_get (model, &iter,
			  COLOUR_NAME,  &colour,
			  COLOUR_RED,   &rgba.red,
			  COLOUR_GREEN, &rgba.green,
			  COLOUR_BLUE,  &rgba.blue,
			  COLOUR_ALPHA, &rgba.alpha,
			  -1);
      if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
      pen_colour_name (pen) = colour;
      if (pen_colour (pen)) gdk_rgba_free (pen_colour (pen));
      pen_colour (pen) = gdk_rgba_copy (&rgba);
    }
  }
}

GtkWidget *
css_button_new (GtkTreeSelection **css_button_p, gchar *which, pen_s *pen)
{
  GtkWidget *css_button;

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  GtkWidget *lbl =
    gtk_label_new (_ ("Click column headers to sort.  Click again to reverse sort order\nCtrl-f to search."));
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  
  
  css_button = gtk_tree_view_new_with_model (GTK_TREE_MODEL (colour_store));
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (css_button), TRUE);
  g_signal_connect (G_OBJECT (css_button), "row-activated",
		    G_CALLBACK (css_row_activated), pen);
  
  
                                   
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (css_button));
  *css_button_p = selection;
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_SINGLE);

  fnc_s fnc;
  fnc.tgt = which;
  fnc.iter = NULL;
  gtk_tree_model_foreach (GTK_TREE_MODEL (colour_store),
			  find_named_colour, &fnc);
  if (fnc.iter) {
    gtk_tree_selection_select_iter (selection, fnc.iter);
    gtk_tree_iter_free (fnc.iter);
  }

  {
    GtkWidget *button;
    GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 2);
    
    button = gtk_radio_button_new_with_label_from_widget (NULL, _ ("All"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button), FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "toggled",
		      G_CALLBACK (colour_fmt_changed_all),
		      selection);
    
    button =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button),
						   _ ("Hex"));
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button), FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "toggled",
		      G_CALLBACK (colour_fmt_changed_hex),
		      selection);
    
    button =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button),
						   _ ("RGB"));
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button), FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "toggled",
		      G_CALLBACK (colour_fmt_changed_rgb),
		      selection);
    
    button =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button),
				       _ ("HSV"));
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button), FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (button), "toggled",
		      G_CALLBACK (colour_fmt_changed_hsv),
		      selection);



    /****** custom colour *********/

#if 1
    GtkWidget *custom = gtk_button_new_with_label (_ ("Custom colour"));
    gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (custom), FALSE, FALSE, 2);
    g_signal_connect (G_OBJECT (custom), "clicked",
		      G_CALLBACK (custom_clicked),
		      NULL);
#else
    //    GtkWidget *custom_colour = gtk_hsv_new ();
    GtkWidget *custom_colour = gtk_color_selection_new ();
    gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (custom_colour),
			FALSE, FALSE, 2);
    
    lbl = gtk_label_new (_ ("Custom colour"));
    gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);
#endif
  }

  for (int i = COLOUR_NAME; i <= COLOUR_VALUE; i++) {
    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *column = NULL;
    switch (gtk_tree_model_get_column_type (GTK_TREE_MODEL (colour_store), i)) {
    case G_TYPE_STRING:
      if (i == COLOUR_NAME) {
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, css_col_names[i]);
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	g_object_set (G_OBJECT (renderer), "xalign", 1.0, NULL);
	gtk_tree_view_column_set_cell_data_func (column,
						 renderer,
						 custom_cell_func,
						 GINT_TO_POINTER (i),
						 custom_cell_destroy);
	gtk_tree_view_column_set_expand (column, TRUE);
      }
      else
	column =
	  gtk_tree_view_column_new_with_attributes (css_col_names[i],
						    renderer,
						    "text", i,
						    NULL);
      break;
    case G_TYPE_DOUBLE:
      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, css_col_names[i]);
      gtk_tree_view_column_pack_start (column, renderer, FALSE);
      g_object_set (G_OBJECT (renderer), "xalign", 1.0, NULL);
      gtk_tree_view_column_set_cell_data_func (column,
					       renderer,
					       custom_double_renderer_func,
					       GINT_TO_POINTER (i),
					       custom_double_renderer_destroy);
    }
    gtk_tree_view_append_column (GTK_TREE_VIEW (css_button), column);
    gtk_tree_view_column_set_clickable (column, TRUE);
    g_signal_connect(G_OBJECT (column), "clicked",
		     G_CALLBACK (column_clicked_cb),
		     GINT_TO_POINTER (i));
  }
  
  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll),
					      200);
#if 0 // to resize the list if necessary
  g_signal_connect (G_OBJECT (paper_tree), "realize",
                    G_CALLBACK (paper_tree_realised),
                    scroll);
#endif
  gtk_container_add (GTK_CONTAINER (scroll), css_button);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (scroll), FALSE, FALSE, 2);
  
  return vbox;
}


