#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "utilities.h"
#include "fallbacks.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "view.h"
#include "select_pen.h"

// https://en.wikipedia.org/wiki/Technical_pen#Technical_information
// iso 128 0.13, 0.18, 0.25, 0.35, 0.5, 0.7, 1.0, 1.5, and 2.0 mm,

static GtkListStore *line_store = NULL;

typedef struct {
  const gchar  *name;
  const gdouble width;
} iso_line_width_s;

static iso_line_width_s iso_line_widths[] = {
  {"Custom", -1.00},
  {"0.13",    0.13},
  {"0.18",    0.18},
  {"0.25",    0.25},
  {"0.35",    0.35},
  {"0.50",    0.50},
  {"0.70",    0.70},
  {"1.00",    1.00},
  {"1.50",    1.50},
  {"2.00",    2.00},
};
#define iso_line_width_name(i) iso_line_widths[i].name
#define iso_line_width_width(i) iso_line_widths[i].width
static gint  iso_line_width_count =
  sizeof(iso_line_widths) / sizeof(iso_line_width_s);
#define MIN_LINE_WIDTH 0.01
#define MAX_LINE_WIDTH 2.0

#define VC(v) sizeof(v) / sizeof(gdouble)

static const double long_dash[]    = {9.0, 3.0};
static const double medium_dash[]  = {6.0, 3.0};
static const double short_dash[]   = {3.0, 9.0};
static const double sparse_dots[]  = {1.0, 5.0, 1.0, 5.0};
static const double normal_dots[]  = {1.0, 3.0, 1.0, 3.0, 1.0, 3.0};
static const double dash_dot[]     = {7.0, 2.0, 1.0, 2.0};
static const double dash_dot_dot[] = {7.0, 1.0, 1.0, 1.0, 1.0};

static const line_style_s line_style_line =
  {"line0.png", "Solid", 0, 0};
static const line_style_s line_style_long_dash =
  {"line1.png", "Long dash", VC (long_dash), long_dash};
static line_style_s line_style_medium_dash =
  {"line2.png", "Medium dash", VC (medium_dash), medium_dash};
static line_style_s line_style_short_dash =
  {"line3.png", "Short dash", VC (short_dash), short_dash};
static line_style_s line_style_sparse_dots =
  {"line4.png", "Sparse dots", VC (sparse_dots), sparse_dots};
static line_style_s line_style_normal_dots =
  {"line5.png", "Normal dots", VC (normal_dots), normal_dots};
static line_style_s line_style_dash_dot =
  {"line6.png", "Dash dot", VC (dash_dot), dash_dot};
static line_style_s line_style_dash_dot_dot =
  {"line7.png", "Dash dot dot", VC (dash_dot_dot), dash_dot_dot};

static const line_style_s *line_styles[] = {
  &line_style_line,
  &line_style_long_dash,
  &line_style_medium_dash,
  &line_style_short_dash,
  &line_style_sparse_dots,
  &line_style_normal_dots,
  &line_style_dash_dot,
  &line_style_dash_dot_dot
};
#define ls_file(l)              (line_styles[l]->file_name)
#define ls_name(l)              (line_styles[l]->style_name)
#define ls_dash_count(l)        (line_styles[l]->num_dashes)
#define ls_dashes(l)            (line_styles[l]->dashes)

gint line_style_count = sizeof(line_styles) / sizeof(line_style_s *);

enum {
  LS_COLUMN_NAME,
  LS_COLUMN_PIXBUF,
  LS_COLUMN_COUNT
};

const line_style_s *
get_pen_line_style (pen_s *pen)
{
  const line_style_s *ls =  NULL;

  if (pen &&
      pen_line_style (pen) >= 0 &&
      pen_line_style (pen) < line_style_count)
    ls = line_styles[pen_line_style (pen)];
  return ls;
}

const line_style_s *
get_environment_line_style (environment_s *env)
{
  pen_s         *current_pen  = NULL;
  const line_style_s *rc = NULL;
  
  g_assert (env != NULL);

  current_pen = environment_pen (env);

  g_assert (current_pen != NULL);

  if (pen_line_style (current_pen) >= 0 &&
      pen_line_style (current_pen) < line_style_count)
    rc = line_styles[pen_line_style (current_pen)];
  return rc;
}

const gchar *
get_ls_from_idx (gint idx)
{
  gint ix = (idx >= 0 && idx < line_style_count)
    ? idx : FALLBACK_PEN_LS_IDX;
  return ls_name (ix);
}

gint 
get_idx_from_ls (const gchar *name)
{
  for (gint ix = 0; ix < line_style_count; ix++)
    if (!!g_strcmp0 (name, ls_name (ix))) return ix;
  return FALLBACK_PEN_LS_IDX;
}

gdouble
get_lw_from_idx (gint idx)
{
  gint ix = (idx >= 0 && idx < iso_line_width_count)
    ? idx : FALLBACK_PEN_LW_IDX;
  return iso_line_widths [ix].width;
}

static void lw_idx_changed_cb (GtkComboBox *widget, gpointer user_data);
static void lw_changed_cb (GtkSpinButton *widget, gpointer user_data);

static void					// combobox changed
lw_idx_changed_cb (GtkComboBox *widget,
		   gpointer     user_data)
{
  gint active = gtk_combo_box_get_active (widget);
  g_print ("lw_idx_changed_cb active = %d\n", active);
  
  pen_private_data_s *ppd = user_data;
  GtkSpinButton *spin = GTK_SPIN_BUTTON (ppd_lw_spin (ppd));

  g_signal_handler_block (spin, ppd_spin_id (ppd));
  
  gtk_spin_button_set_value (spin, get_lw_from_idx (active));
  
  g_signal_handler_unblock (spin, ppd_spin_id (ppd));
}


static void					// spinbutton changed
lw_changed_cb (GtkSpinButton *widget,
	       gpointer       user_data)
{
  pen_private_data_s *ppd = user_data;
  GtkComboBox *combo = GTK_COMBO_BOX (ppd_lw_combo (ppd));

  g_signal_handler_block (combo, ppd_combo_id (ppd));
  
  gtk_combo_box_set_active (combo, STANDARD_LINE_WIDTH_CUSTOM);
  
  g_signal_handler_unblock (combo, ppd_combo_id (ppd));
}

GtkWidget *
linestyle_button_new (pen_s *current_pen)
{
  GtkWidget *ls_button;
  
  if (!line_store) {
    line_store = gtk_list_store_new (LS_COLUMN_COUNT,
				     G_TYPE_STRING,
				     GDK_TYPE_PIXBUF);

    for (gint i= 0; i < line_style_count; i++) {
      GtkTreeIter iter;
      gchar *path;

      gtk_list_store_append (line_store, &iter);

      path = find_image_file (ls_file (i));
      if (path) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file (path, NULL);
        g_free (path);
        gtk_list_store_set (line_store, &iter,
                            LS_COLUMN_NAME,   ls_name (i),
                            LS_COLUMN_PIXBUF, pb,
                            -1);
      }
      else
        gtk_list_store_set (line_store, &iter,
                            LS_COLUMN_NAME,   ls_name (i),
                            -1);
    }
  }
  
  ls_button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (line_store));
  
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ls_button),
			      renderer, FALSE );
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ls_button),
				  renderer,
				  "text", LS_COLUMN_NAME,
				  NULL );

  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (ls_button),
			      renderer, FALSE );
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (ls_button),
				  renderer,
				  "pixbuf", LS_COLUMN_PIXBUF,
				  NULL );
  gtk_combo_box_set_active (GTK_COMBO_BOX (ls_button),
			    pen_line_style (current_pen));
  return ls_button;
}

GtkWidget *
linewidth_buttons_new (pen_private_data_s **ppdp, pen_s *pen)
{
  g_assert (ppdp != NULL && pen != NULL);
  pen_private_data_s *ppd = *ppdp;
  g_assert (ppd != NULL);
  GtkWidget *frame = gtk_frame_new (_ ("Line width (mm)"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

  gtk_container_add (GTK_CONTAINER (frame), hbox);
  ppd_lw_spin (ppd) = gtk_spin_button_new_with_range (MIN_LINE_WIDTH,
						      MAX_LINE_WIDTH,
						      0.01);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (ppd_lw_spin (ppd)),
			     pen_lw (pen));

  ppd_lw_combo (ppd) = gtk_combo_box_text_new ();

  for (int i = 0; i < iso_line_width_count; i++)
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (ppd_lw_combo (ppd)),
			       NULL, iso_line_widths[i].name);
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (ppd_lw_combo (ppd)),
			    pen_lw_std_idx (pen));

  ppd_combo_id (ppd) = g_signal_connect (G_OBJECT ( ppd_lw_combo (ppd)),
					 "changed",
					 G_CALLBACK (lw_idx_changed_cb), ppd);

  ppd_spin_id (ppd)  = g_signal_connect (G_OBJECT (ppd_lw_spin (ppd)),
					 "value-changed",
					 G_CALLBACK (lw_changed_cb), ppd);
  
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ppd_lw_combo (ppd)),
		      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ppd_lw_spin (ppd)),
		      FALSE, FALSE, 2);

  return frame;
}

GtkWidget *
select_pen (void **private_data, pen_s *pen)
{
  GtkWidget     *content;
  GtkWidget     *frame;

  g_assert (pen != NULL && private_data != NULL);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  pen_private_data_s *ppd = gfig_try_malloc0 (sizeof(pen_private_data_s));
  *private_data = ppd;
  
  /************ css colour *******************/

  frame = gtk_frame_new (_ ("Pen colour"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, TRUE, 8);

  gchar *colour_name = pen_colour_name (pen);
  GtkWidget *css_widget = css_button_new (&ppd_css (ppd), colour_name, pen);
  gtk_container_add (GTK_CONTAINER (frame), css_widget);

  /************ line style *******************/

  frame = gtk_frame_new (_ ("Line style"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, FALSE, 8);

  ppd_style (ppd) = linestyle_button_new (pen);
  gtk_container_add (GTK_CONTAINER (frame), ppd_style (ppd));

  /************ line width *******************/

  frame = linewidth_buttons_new (&ppd, pen);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, FALSE, 8);

  return content;
}

void
extract_pen (void *private_data, pen_s *pen)
{
  GdkRGBA       *current_colour = NULL;
  pen_private_data_s *ppd = private_data;

  g_assert (pen != NULL);

  current_colour = pen_colour (pen);

  g_assert (current_colour != NULL);

  if (!ppd) return;

  //  g_assert (env != NULL);

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
      if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
      pen_colour_name (pen) = colour;
      if (pen_colour (pen)) gdk_rgba_free (pen_colour (pen));
      pen_colour (pen) = gdk_rgba_copy (&rgba);
    }
  }

  if (ppd_style (ppd)) {
    gint ls = gtk_combo_box_get_active (GTK_COMBO_BOX (ppd_style (ppd)));
    if (ls >= 0 && ls < line_style_count) pen_line_style (pen) = ls;
  }

  if (ppd_lw_spin (ppd)) {
    pen_lw (pen) = 
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (ppd_lw_spin (ppd)));
  }
  
  if (ppd_lw_combo (ppd)) {
    pen_lw_std_idx (pen) =
      gtk_combo_box_get_active (GTK_COMBO_BOX (ppd_lw_combo (ppd)));
  }
}


void
pen_settings (GtkWidget *object, gpointer data)
{
  GtkWidget     *dialog;
  GtkWidget     *content;
  gint           response;

  sheet_s *sheet = data;
  environment_s *env = sheet_environment (sheet);
  g_print ("pen_settings for %s %p\n", sheet_name (sheet), sheet);

  g_assert (env != NULL);

  dialog =  gtk_dialog_new_with_buttons (_ ("Pen settings"),
                                         NULL,
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                         _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  void *pen_private_data = NULL;
  
  GtkWidget *pen_button = select_pen (&pen_private_data,
				      environment_pen (env));
  gtk_container_add (GTK_CONTAINER (content), pen_button);

  gtk_widget_show_all (dialog); 

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    extract_pen   (pen_private_data, environment_pen (env));
    force_redraw (sheet);
  }
  if (pen_private_data)   g_free (pen_private_data);
  gtk_widget_destroy (dialog);
}
