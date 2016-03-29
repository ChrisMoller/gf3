#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <math.h>


#include "gf.h"
#include "view.h"
#include "fallbacks.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "persistents.h"
#include "preferences.h"
#include "select_pen.h"
#include "entities.h"
#include "utilities.h"
#include "python.h"
#include "xml.h"
#include "drawing_header.h"
#include "../pluginsrcs/plugin.h"


enum {
  PRIMARY_KEY,
  MIDDLE_KEY,
  SECONDARY_KEY,
  KEY_COUNT
};
static GtkListStore *projects =  NULL;
static gint project_nr = 0;
static environment_s *global_environment = NULL;
static sheet_s *active_sheet = NULL;
static GtkTextBuffer *buffer = NULL;
static GtkWidget *view;
static GList *plugins = NULL;
static GtkWidget *prompt = NULL;
static GtkWidget *tool_specific_frame = NULL;
static GtkWidget *empty_tool_label = NULL;
static GtkWidget *state_labels[KEY_COUNT] = {NULL, NULL, NULL};
static const gchar *state_values[KEY_COUNT] = {NULL, NULL, NULL};

GtkListStore *get_projects () {return projects;}
environment_s *get_global_environment () {return global_environment;}

static button_f current_bf = NULL;

button_f return_current_tool () { return current_bf; }

#include "drawing.h"
#include "drawing_struct.h"

#ifdef USE_SPC
void
set_pen_controls (pen_s *pen)
{
  static GtkWidget *content;
  static GtkWidget *dialog = NULL;

  if (pen) {
    if (!dialog) {
      dialog = gtk_dialog_new_with_buttons ("Pen",
					    NULL,
					    GTK_DIALOG_DESTROY_WITH_PARENT,
					    _("_Close"), GTK_RESPONSE_CLOSE,
					    NULL);
      //gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
      g_signal_connect_swapped (dialog,
				"response",
				G_CALLBACK (gtk_widget_destroy),
				dialog);
      content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    }
  }
  else {
  }
}
#endif

void
set_prompt (const gchar *string)
{
  gtk_entry_set_text (GTK_ENTRY (prompt), string);
}

void
set_tsf (GtkWidget *tsf)
{
  GtkWidget *kid = gtk_bin_get_child (GTK_BIN (tool_specific_frame));
  if (kid) gtk_container_remove (GTK_CONTAINER (tool_specific_frame), kid);

  GtkWidget *use = tsf ?: empty_tool_label;
  gtk_container_add (GTK_CONTAINER (tool_specific_frame), use);
}

void
set_key (const gchar *primary, const gchar *middle, const gchar *secondary)
{
  state_values[PRIMARY_KEY]   = primary;
  state_values[MIDDLE_KEY]    = middle;
  state_values[SECONDARY_KEY] = secondary;
  gtk_label_set_text (GTK_LABEL (state_labels[PRIMARY_KEY]),
		      state_values[PRIMARY_KEY] ?: "");
  gtk_label_set_text (GTK_LABEL (state_labels[MIDDLE_KEY]),
		      state_values[MIDDLE_KEY] ?: "");
  gtk_label_set_text (GTK_LABEL (state_labels[SECONDARY_KEY]),
		      state_values[SECONDARY_KEY] ?: "");
}

void
notify_key_set (gint etype)
{
  switch(etype) {
  case GDK_ENTER_NOTIFY:
    gtk_label_set_text (GTK_LABEL (state_labels[PRIMARY_KEY]),
			state_values[PRIMARY_KEY] ?: "");
    gtk_label_set_text (GTK_LABEL (state_labels[MIDDLE_KEY]),
			state_values[MIDDLE_KEY] ?: "");
    gtk_label_set_text (GTK_LABEL (state_labels[SECONDARY_KEY]),
			state_values[SECONDARY_KEY] ?: "");
    break;
  case GDK_LEAVE_NOTIFY:
    gtk_label_set_text (GTK_LABEL (state_labels[PRIMARY_KEY]), "");
    gtk_label_set_text (GTK_LABEL (state_labels[MIDDLE_KEY]), "");
    gtk_label_set_text (GTK_LABEL (state_labels[SECONDARY_KEY]), "");
    break;
  }
}

static void
gfig_quit (GtkWidget *object, gpointer data)
{
  save_persistents (global_environment);
  term_python ();
  gtk_main_quit ();
}

void
set_active_sheet (sheet_s *sheet)
{
  active_sheet = sheet;
}

sheet_s *
get_active_sheet ()
{
  return active_sheet;
}

gpointer
gfig_try_malloc0 (gsize n_bytes)
{
  gpointer v = g_try_malloc0 (n_bytes);
  if (!v) {
    log_string (LOG_GFIG_ERROR, NULL,  _ ("Memory allocation failure"));
    gtk_main_quit ();	// shut down as gracefully as possible
  }
  return v;
}

environment_s *
copy_environment (environment_s *source_environment)
{
  environment_s *copy = gfig_try_malloc0 (sizeof(environment_s));

  paper_s *paper = environment_paper (copy)
    = gfig_try_malloc0 (sizeof(paper_s));
  paper_size (paper) =
    gtk_paper_size_copy (paper_size (environment_paper (source_environment)));

  paper_colour (paper) =
    gdk_rgba_copy (paper_colour (environment_paper (source_environment)));
  paper_colour_name (paper) =
    g_strdup (paper_colour_name (environment_paper (source_environment)));
  paper_orientation (paper) =
    paper_orientation (environment_paper (source_environment));
  
  pen_s *pen = environment_pen (copy)
    = gfig_try_malloc0 (sizeof(pen_s));
  pen_colour (pen) =
    gdk_rgba_copy (pen_colour (environment_pen (source_environment)));
  pen_colour_name (pen) =
    g_strdup (pen_colour_name (environment_pen (source_environment)));
  pen_line_style (pen) =
    pen_line_style (environment_pen (source_environment));
  pen_lw_std_idx (pen) =
    pen_lw_std_idx (environment_pen (source_environment));
  pen_lw (pen) =
    pen_lw (environment_pen (source_environment));
  
  environment_dunit (copy) = environment_dunit (source_environment);
  environment_uupdu (copy) = environment_uupdu (source_environment);
  if (environment_uuname (source_environment))
    environment_uuname (copy) =
      g_strdup (environment_uuname (source_environment));
  environment_org_x (copy) = environment_org_x (source_environment);
  environment_org_y (copy) = environment_org_y (source_environment);
  environment_show_org (copy) = environment_show_org (source_environment);
  environment_show_grid (copy) = environment_show_grid (source_environment);
  environment_snap_grid (copy) = environment_snap_grid (source_environment);
  environment_target_grid (copy) = environment_target_grid (source_environment);
  environment_readout (copy) = environment_readout (source_environment);
  environment_surface (copy) = NULL;
  environment_history (copy) = NULL;
  environment_histptr (copy) = NULL;
  environment_fontname (copy) =		// fixme -- not used
    g_strdup (environment_fontname (source_environment));
  environment_textsize (copy) = environment_textsize (source_environment);
  
  return copy;
}

static gboolean
init_global_environment ()
{
  paper_s	*paper		= NULL;
  GdkRGBA	*paper_colour_t = NULL;
  pen_s		*pen		= NULL;
  GdkRGBA	*pen_colour_t	= NULL;
  gboolean 	 rc 		= TRUE;
  GtkPaperSize	*paper_size_t	= NULL;

  enum {
    OK_PAPER,
    OK_PEN
  };

  enum {
    OK_PAPER_MASK	= 1 << OK_PAPER,
    OK_PEN_MASK		= 1 << OK_PEN,
  };
#define ALL_COOL_MASK (OK_PAPER_MASK | OK_PEN_MASK)

  build_colours_store ();
  
  guint ok_status = 0;
  global_environment = gfig_try_malloc0 (sizeof(environment_s));
  if (global_environment) {
    environment_paper (global_environment) = paper =
      gfig_try_malloc0 (sizeof(paper_s));
    if (paper) {
      paper_colour (paper) = paper_colour_t =
	gfig_try_malloc0 (sizeof(GdkRGBA));
      if (paper_colour_t) {
	GdkRGBA rgba = {
	  FALLBACK_PAPER_COLOUR_RED,
	  FALLBACK_PAPER_COLOUR_GREEN,
	  FALLBACK_PAPER_COLOUR_BLUE,
	  FALLBACK_PAPER_COLOUR_ALPHA
	};
	paper_colour (paper) = gdk_rgba_copy (&rgba);
	paper_orientation (paper)	= FALLBACK_PAPER_ORIENTATION;
	paper_size (paper) = gtk_paper_size_new (FALLBACK_PAPER_SIZE);
	ok_status |= OK_PAPER_MASK;
      }
    }
	
    environment_pen (global_environment) = pen =
      gfig_try_malloc0 (sizeof(pen_s));
    if (pen) {
      pen_line_style (pen) = FALLBACK_PEN_LS_IDX;
      pen_colour (pen) = pen_colour_t =
	gfig_try_malloc0 (sizeof(GdkRGBA));
      if (pen_colour_t) {
	pen_colour_red (pen)		= FALLBACK_PEN_COLOUR_RED;
	pen_colour_green (pen)		= FALLBACK_PEN_COLOUR_GREEN;
	pen_colour_blue (pen)		= FALLBACK_PEN_COLOUR_BLUE;
	pen_colour_alpha (pen)		= FALLBACK_PEN_COLOUR_ALPHA;
	pen_lw_std_idx (pen)		= FALLBACK_PEN_LW_IDX;
	pen_lw (pen) = get_lw_from_idx (pen_lw_std_idx (pen));
	ok_status |= OK_PEN_MASK;
      }
    }	
    environment_dunit (global_environment)	= FALLBACK_DRAWING_UNIT;
    environment_uupdu (global_environment)	= FALLBACK_DRAWING_UUPDU;
    environment_org_x (global_environment)	= FALLBACK_DRAWING_ORG_X;
    environment_org_y (global_environment)	= FALLBACK_DRAWING_ORG_Y;
    environment_show_org (global_environment)	= FALLBACK_DRAWING_SHOW_ORG;
    environment_show_grid (global_environment)	= FALLBACK_DRAWING_SHOW_GRID;
    environment_snap_grid (global_environment)	= FALLBACK_DRAWING_SNAP_GRID;
    environment_target_grid (global_environment)= FALLBACK_DRAWING_TARGET_GRID;
    environment_surface  (global_environment)	= NULL;
    environment_history  (global_environment)	= NULL;
    environment_histptr  (global_environment)	= NULL;
    environment_fontname (global_environment)	= g_strdup (FALLBACK_FONT);
    environment_textsize (global_environment)   = FALLBACK_TEXTSIZE;
  }
  rc = (ok_status == ALL_COOL_MASK) ? TRUE : FALSE;
  if (!rc) {
    if (paper)		g_free (paper);
    if (paper_colour_t) g_free (paper_colour_t);
    if (pen)		g_free (pen);
    if (pen_colour_t)	g_free (pen_colour_t);
    if (paper_size_t)	gtk_paper_size_free (paper_size_t);
  }
  return rc;
}

typedef struct {
  project_s *project;
  notify_e type;
} notify_context_s;

static gboolean
loop_sheets (GtkTreeModel *model,
	     GtkTreePath *path,
	     GtkTreeIter *iter,
	     gpointer data)
{
  notify_context_s *notify_context = data;
  project_s *project = notify_context->project;
  notify_e type = notify_context->type;
  sheet_s *sheet = NULL;

  gtk_tree_model_get (model, iter, SHEET_STRUCT_COL, &sheet, -1);

  if (project && sheet) {
    switch(type) {
    case NOTIFY_MAP:
      create_new_view (project, sheet, NULL);	// fixme default script?
      break;
    case NOTIFY_TOOL_STOP:
      if (sheet_transients (sheet))
	clear_entities (&sheet_transients (sheet));
      sheet_tool_state (sheet) = 0;
      point_x (&sheet_current_point (sheet)) = NAN;
      point_y (&sheet_current_point (sheet)) = NAN;
      break;
    }
  }
  
  return GDK_EVENT_PROPAGATE;
}

static gboolean
loop_projects (GtkTreeModel *model,
	       GtkTreePath *path,
	       GtkTreeIter *iter,
	       gpointer data)
{
  project_s *project = NULL;
  notify_context_s notify_context;

  gtk_tree_model_get (model, iter, PROJECT_STRUCT_COL, &project, -1);

  if (project && project_sheets (project)) {
    notify_context.project = project;
    notify_context.type    = GPOINTER_TO_INT (data);
    gtk_tree_model_foreach (GTK_TREE_MODEL (project_sheets (project)),
			    loop_sheets, &notify_context);
  }
  return GDK_EVENT_PROPAGATE;
}

void
notify_projects (notify_e type)
{
  if (projects)
    gtk_tree_model_foreach (GTK_TREE_MODEL (projects), loop_projects,
			    GINT_TO_POINTER (type));
}

project_s *
create_project (const gchar *name)
{
  project_s *project = gfig_try_malloc0 (sizeof(project_s));
  project_environment (project) = copy_environment (global_environment);
  project_name (project) = g_strdup (name);
  
  GtkTreeIter iter;
  gtk_list_store_append (projects, &iter);
  gtk_list_store_set (projects, &iter,
		      PROJECT_STRUCT_COL, project, -1);
  project_sheets (project) =
    gtk_tree_store_new (SHEET_COL_COUNT, G_TYPE_POINTER);
  return project;
}

project_s *
initialise_project (const gchar *name, gchar *script)
{
  project_s *project = create_project (name);

  sheet_s *this_sheet;
  project_current_iter (project) =
    append_sheet (project, NULL, NULL, &this_sheet);
  environment_s	*env = copy_environment (project_environment (project));
  sheet_environment (this_sheet) = env;
  
  create_new_view (project, this_sheet, script);
  return project;
}

GtkTreeIter *
append_sheet (project_s *project, GtkTreeIter *parent, gchar *name,
	      sheet_s **new_sheet_p)
{
  sheet_s *new_sheet = gfig_try_malloc0 (sizeof(sheet_s));

  set_active_sheet (new_sheet);
  
  GtkTreeIter iter;
  gtk_tree_store_append (project_sheets (project), &iter, parent);

  if (name) sheet_name (new_sheet) = g_strdup (name);
  else {
    gchar *ppath =
 gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (project_sheets (project)),
				      &iter);
  
    sheet_name (new_sheet) = g_strdup_printf ("Sheet %s", ppath);
    g_free (ppath);
  }
  environment_s	*env = copy_environment (project_environment (project));
  sheet_environment (new_sheet) = env;
  sheet_pydict (new_sheet)      = get_local_pydict (new_sheet);
  sheet_project (new_sheet)     = project;
  sheet_entities (new_sheet)    = NULL;
  point_x (&sheet_current_point (new_sheet)) = NAN;
  point_y (&sheet_current_point (new_sheet)) = NAN;
  if (new_sheet_p) *new_sheet_p = new_sheet;
  
  gtk_tree_store_set (project_sheets (project), &iter,
		      SHEET_STRUCT_COL, new_sheet, -1);
  if (unlikely(sheet_iter (new_sheet)))
    gtk_tree_iter_free (sheet_iter (new_sheet));
  sheet_iter (new_sheet) = gtk_tree_iter_copy (&iter);
  return gtk_tree_iter_copy (&iter);
}

static void
create_new_project (GtkWidget *widget,
		    gpointer   data)
{
  gchar *name = g_strdup_printf ("Project %d", project_nr++);
  initialise_project (name, data); // data -> script name
}

static void
custom_project_renderer_func (GtkTreeViewColumn *tree_column,
			      GtkCellRenderer *cell,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer data)
{
  project_s *project = NULL;
  gtk_tree_model_get(model, iter,
		     PROJECT_STRUCT_COL, &project,
		     -1);
  const gchar *value = project ? project_name (project) : "(unnamed)";
  g_object_set (G_OBJECT (cell), "text", value, NULL);
}

static void
custom_project_renderer_destroy (gpointer data)
{
  // do nothing
}

/**** part of plug-in i/f  ****/
/* may or may not be retained */
void
gfig_cb (gfig_op_e gfig_op, ...)
{
  switch(gfig_op) {
  case GFIG_OP_NONE:
    break;
  case GFIG_OP_APPEND_CIRCLE:
    {
      va_list ap;
      va_start (ap, gfig_op);
      gdouble x = va_arg (ap, gdouble);
      gdouble y = va_arg (ap, gdouble);
      gdouble r = va_arg (ap, gdouble);
      gboolean f = va_arg (ap, gboolean);
      va_end (ap);

      sheet_s *ss = get_active_sheet ();
      entity_append_circle (ss, x, y, r, 0.0, 2.0 * G_PI, FALSE, f, NULL);
      force_redraw (ss);
    }
    break;
  case GFIG_OP_APPEND_LINE:
    break;
  }
}
/* end of plug-in i/f */

static void
plugin_clicked (GtkButton *button,
               gpointer   user_data)
{
  gint pidx = GPOINTER_TO_INT (user_data);
  if (plugins) {
    plugin_s *plugin = g_list_nth_data (plugins, pidx);
    void *dl = dlopen (plugin_path (plugin), RTLD_LAZY);
    if (dl) {
      plugin_start ps = dlsym (dl, "start_plugin");
      if (!ps) log_string (LOG_GFIG_ERROR, NULL, dlerror());
      else (*ps) (gfig_cb);
    }
    else log_string (LOG_GFIG_ERROR, NULL, dlerror());
  }
}

static void
select_plugin (GtkWidget *widget,
	       gpointer   data)
{
  if (plugins) {
    guint nr_plugins = g_list_length (plugins);
    if (0 < nr_plugins) {
      GtkWidget *content;
      GtkWidget *dialog =
	gtk_dialog_new_with_buttons ("Plugins",
				     NULL,
				     GTK_DIALOG_DESTROY_WITH_PARENT,
				     _("_Close"), GTK_RESPONSE_CLOSE,
				     NULL);
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
      g_signal_connect_swapped (dialog,
				"response",
				G_CALLBACK (gtk_widget_destroy),
				dialog);
      content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
      GtkWidget *grid = gtk_grid_new ();
      gtk_container_add (GTK_CONTAINER (content), grid);
      gint nr_cols = (gint)(1 + lrint (sqrt ((double)nr_plugins)));
      GList *sol = g_list_first (plugins);
      gint count, row, col;
      for (count = 0, row = 0; count < nr_plugins; row++) {
	for (col = 0;
	     count < nr_plugins && col < nr_cols;
	     col++, count++) {

	  plugin_s *plugin = g_list_nth_data (sol, count);
	  GtkWidget *button;
	  if (plugin_icon (plugin)) {
	    button = gtk_button_new ();
	    GtkWidget *pi = gtk_image_new_from_pixbuf (plugin_icon (plugin));
	    gtk_button_set_image (GTK_BUTTON (button), pi);
	  }
	  else 
	    button = gtk_button_new_with_label (plugin_name (plugin));
	  if (plugin_desc (plugin))
	    gtk_widget_set_tooltip_text (button, plugin_desc (plugin));
	  g_signal_connect (button,
			    "clicked",
			    G_CALLBACK (plugin_clicked),
			    GINT_TO_POINTER (count));
	  gtk_grid_attach (GTK_GRID (grid),
			   button,
			   col, row, 1, 1);
	}
      }
      gtk_widget_show_all (dialog);
    }
  }
}

static gboolean
tree_clicked_cb (GtkWidget *treeview,
		 GdkEventButton *event,
		 gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  project_s *project;
  
  g_print ("tcb %d\n", event->button);

  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    project = NULL;
    gtk_tree_model_get (model, &iter, PROJECT_STRUCT_COL, &project, -1);
    if (project) {
      g_print ("proj %s\n", project_name (project));
    }
  }

  return GDK_EVENT_PROPAGATE;
}

static void
show_projects (GtkWidget *object, gpointer data)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *scroll = NULL;

  dialog =  gtk_dialog_new_with_buttons (_ ("Projects"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_Close"), GTK_RESPONSE_CLOSE,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  
  scroll =  gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll),
					     160);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll),
					     100);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_end (GTK_BOX (content), scroll, TRUE, TRUE, 0);

  GtkWidget *tree =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (projects));
  g_signal_connect(tree, "button-press-event",
		   G_CALLBACK (tree_clicked_cb), NULL);
#if 0
  GtkTreeSelection *select =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
		    G_CALLBACK (sheet_selection_changed_cb),
		    project);
#endif
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn *column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Project name");
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           custom_project_renderer_func,
                                           NULL,
                                           custom_project_renderer_destroy);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  gtk_container_add (GTK_CONTAINER (scroll), tree);

  gtk_widget_show_all (dialog);
}

enum {
  STRUCT_NAME_COL,
  STRUCT_POINTER_COL,
  STRUCT_COL_COUNT
};

typedef struct {
  GtkTreeStore *store;
  GtkTreeIter *iter;
} capsule_s;

static gboolean
sheet_func (GtkTreeModel *model,
	    GtkTreePath *path,
	    GtkTreeIter *iter,
	    gpointer data)
{
  sheet_s *sheet;
  capsule_s *capsule = data;
  GtkTreeIter set_iter;

  sheet = NULL;
  gtk_tree_model_get (model, iter, SHEET_STRUCT_COL, &sheet, -1);
  if (sheet) {
    gtk_tree_store_append (capsule->store, &set_iter, capsule->iter);
    gtk_tree_store_set (capsule->store, &set_iter,
			STRUCT_NAME_COL, sheet_name (sheet),
			STRUCT_POINTER_COL, sheet,
			-1);
  }
  return GDK_EVENT_PROPAGATE;
}

static gboolean
proj_func (GtkTreeModel *model,
	   GtkTreePath *path,
	   GtkTreeIter *iter,
	   gpointer data)
{
  project_s *project;
  GtkTreeIter set_iter;
  GtkTreeStore *store = data;
  
  project = NULL;
  gtk_tree_model_get (model, iter, PROJECT_STRUCT_COL, &project, -1);

  if (project) {
    capsule_s capsule;
    gtk_tree_store_append (store, &set_iter, NULL);
    gtk_tree_store_set (store, &set_iter,
			STRUCT_NAME_COL, project_name (project),
			STRUCT_POINTER_COL, project,
			-1);
    capsule.store = store;
    capsule.iter  = &set_iter;
    gtk_tree_model_foreach (GTK_TREE_MODEL (project_sheets (project)),
			    sheet_func, &capsule);
  }
  return GDK_EVENT_PROPAGATE;
}

static void
show_structure (GtkWidget *object, gpointer data)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *scroll = NULL;

  dialog =  gtk_dialog_new_with_buttons (_ ("Structure"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _ ("_Close"), GTK_RESPONSE_CLOSE,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  
  scroll =  gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll),
					     160);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll),
					     100);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_end (GTK_BOX (content), scroll, TRUE, TRUE, 0);

  enum {
    STRUCT_NAME_COL,
    STRUCT_POINTER_COL,
    STRUCT_COL_COUNT
  };
  
  GtkTreeStore *model = gtk_tree_store_new (STRUCT_COL_COUNT,
					    G_TYPE_STRING,
					    G_TYPE_POINTER);
  gtk_tree_model_foreach (GTK_TREE_MODEL (projects), proj_func, model);

  GtkWidget *tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
#if 0
  g_signal_connect(tree, "button-press-event",
		   G_CALLBACK (tree_clicked_cb), NULL);
  GtkTreeSelection *select =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
		    G_CALLBACK (sheet_selection_changed_cb),
		    project);
#endif
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn *column =
    gtk_tree_view_column_new_with_attributes ("Name", renderer,
					      "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree));

  gtk_container_add (GTK_CONTAINER (scroll), tree);

  gtk_widget_show_all (dialog);
}

static void
catch_intr (int sig)
{
  gfig_quit (NULL, NULL);
}

static void
load_dialogue (GtkWidget *widget, gpointer data)
{
  gint response;

  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.xml");
  
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new (_ ("Project Load"),
				 NULL,
				 GTK_FILE_CHOOSER_ACTION_OPEN,
                                 _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                 _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                 NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    gchar *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    load_drawing (file);
    g_free (file);
  }
  gtk_widget_destroy (dialog);
}

static void
save_dialogue (GtkWidget *widget, gpointer data)
{
  gint response;

  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.xml");
  
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new (_ ("Project Save"),
				 NULL,
				 GTK_FILE_CHOOSER_ACTION_SAVE,
                                 _ ("_OK"), GTK_RESPONSE_ACCEPT,
                                 _ ("_Cancel"), GTK_RESPONSE_CANCEL,
                                 NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT) {
    gchar *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    save_drawing (file);
    g_free (file);
  }
  gtk_widget_destroy (dialog);
}

static void
build_menu (GtkWidget *vbox)
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

  item = gtk_menu_item_new_with_label (_ ("New project"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (create_new_project), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Save drawing"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (save_dialogue), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Load drawing"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (load_dialogue), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Show projects"));
  g_signal_connect (G_OBJECT(item), "activate",
                    G_CALLBACK (show_projects), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = gtk_menu_item_new_with_label (_ ("Show structure"));
  g_signal_connect (G_OBJECT(item), "activate",
                    G_CALLBACK (show_structure), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Plugins"));
  g_signal_connect (G_OBJECT(item), "activate",
                    G_CALLBACK (select_plugin), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_ ("Quit"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (gfig_quit), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  

  /********* edit ********/

  menu = gtk_menu_new();
  item = gtk_menu_item_new_with_label (_ ("Preferences"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);

  item = gtk_menu_item_new_with_label (_ ("Environment"));
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (preferences), global_environment);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);


  /********* end of menus ********/

  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (menubar), FALSE, FALSE, 2);
}

static void
insert_plugin (gchar *eafile)
{
  void *dl = dlopen (eafile, RTLD_LAZY);
  if (dl) {
    plugin_query pq = dlsym (dl, "query_plugin");
    if (!pq) {
      log_string (LOG_GFIG_ERROR, NULL, dlerror());
      g_free (eafile);
    }
    else {
      gchar *name = NULL;
      gchar *desc = NULL;
      gchar **icon_xpm = NULL;
      (*pq) (&name, &desc, &icon_xpm);
      GdkPixbuf *pb = icon_xpm ?
	gdk_pixbuf_new_from_xpm_data ((const gchar **)icon_xpm)
	: NULL;
      plugin_s *plugin = gfig_try_malloc0 (sizeof (plugin_s));
      plugin_path (plugin) = eafile;
      plugin_name (plugin) = g_strdup (name);
      plugin_desc (plugin) = g_strdup (desc);
      plugin_icon (plugin) = pb;
      plugin_open (plugin) = FALSE;
      plugins = g_list_append (plugins, plugin);
      dlclose (dl);
    }
  }
  else {
    log_string (LOG_GFIG_ERROR, NULL, dlerror());
    g_free (eafile);
  }
}

static void
find_plugins ()
{
  gboolean search_local  = FALSE;
#ifdef TOPSRCDIR
  gchar *cwd = g_get_current_dir ();
  if (cwd) {
    if (TOPSRCDIR) {
      search_local =
	(0 == strncmp (cwd, TOPSRCDIR, strlen (TOPSRCDIR))) ? TRUE : FALSE;
    }
    g_free (cwd);
  }
  if (search_local) {
    gchar *sdir = g_strdup_printf ("%s/%s", TOPSRCDIR, "pluginsrcs");
    if (sdir) {
      GDir *pdirs = g_dir_open (sdir, 0, NULL);
      if (pdirs) {
	const gchar *tdir;
	while ((tdir = g_dir_read_name (pdirs))) {
	  gchar *adir = g_strdup_printf ("%s/%s/.libs", sdir, tdir);
	  if (adir) {
	    if (g_file_test (adir, G_FILE_TEST_IS_DIR)) {
	      GDir *ldirs = g_dir_open (adir, 0, NULL);
	      if (ldirs) {
		const gchar *efile;
		while ((efile = g_dir_read_name (ldirs))) {
		  gchar *eafile = g_strdup_printf ("%s/%s", adir, efile);
		  if (eafile) {
		    if (g_file_test (eafile, G_FILE_TEST_IS_REGULAR) &&
			g_str_has_suffix (eafile, ".so")) {
		      insert_plugin (eafile);
		    }
		    else g_free (eafile);
		  }
		}
		g_dir_close (ldirs);
	      }
	    }
	    g_free (adir);
	  }
	}
	g_dir_close (pdirs);
      }
      g_free (sdir);
    }
  }
#endif
  if (!plugins) {    // local search failed
    gchar *sdir = g_strdup_printf ("%s/%s", DATAROOTDIR, "plugins");
    if (sdir) {
      GDir *ldirs = g_dir_open (sdir, 0, NULL);
      if (ldirs) {
	const gchar *efile;
	while ((efile = g_dir_read_name (ldirs))) {
	  gchar *eafile = g_strdup_printf ("%s/%s", sdir, efile);
	  if (eafile) {
	    if (g_file_test (eafile, G_FILE_TEST_IS_REGULAR) &&
		g_str_has_suffix (eafile, ".so")) {
	      insert_plugin (eafile);
	    }
	    else g_free (eafile);
	  }
	}
	g_dir_close (ldirs);
      }
      g_free (sdir);
    }
  }
}


static GtkTextTag *logtags[LOG_NR_TAGS] = { NULL };

void
log_string (log_level_e log_level, sheet_s *sheet, gchar *str)
{
  if (!str || !*str) return;
  
  const char *projname = NULL;
  char *sheetname = NULL;
  GtkTextIter iter;
  
  if (logtags[LOG_NORMAL]) {
    logtags[LOG_NORMAL] = gtk_text_buffer_create_tag (buffer,
						      "normaltag",
						      "foreground", "black",
						      NULL);
    logtags[LOG_PYTHON_ERROR] = gtk_text_buffer_create_tag (buffer,
							    "py_errtag",
							    "foreground",
							    "red",
							    NULL);
    logtags[LOG_GFPY_ERROR] = gtk_text_buffer_create_tag (buffer,
							  "gfpy_errtag",
							  "foreground",
							  "blue",
							  NULL);
    logtags[LOG_GFIG_ERROR] = gtk_text_buffer_create_tag (buffer,
							  "gfig_errtag",
							  "foreground",
							  "green",
							  NULL);
  }
  
  if (sheet) {
    project_s *proj = sheet_project (sheet);
    projname = project_name (proj);
    sheetname = sheet_name (sheet);
  }

  gtk_text_buffer_get_end_iter (buffer, &iter);

  gboolean has_nl = (str[strlen (str) - 1] == '\n');
  gchar oln = (has_nl || log_level == LOG_PYTHON_ERROR) ? 0 : '\n';
  gchar *ostr;
  ostr = NULL;
  if (projname && sheetname)
    ostr = g_strdup_printf ("%s %s: %s%c", projname, sheetname, str, oln);
  else if (projname)
    ostr = g_strdup_printf ("%s: %s%c", projname, str, oln);
  else if (sheetname)
    ostr = g_strdup_printf ("%s: %s%c", sheetname, str, oln);
  else
    ostr = g_strdup_printf ("%s%c", str, oln);

  gtk_text_buffer_insert_with_tags (buffer, &iter, ostr, -1,
				     logtags[log_level] , NULL);
  g_free (ostr);

  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
                                gtk_text_buffer_get_mark (buffer, "insert"),
                                0.1,
                                FALSE,
                                0.0,
                                0.0);
}

static void
build_log_window (GtkWidget *outer_vbox)
{
  GtkWidget *scroll;
  
  scroll =  gtk_scrolled_window_new (NULL,NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scroll),
					     320);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroll),
					      100);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_box_pack_start (GTK_BOX (outer_vbox), GTK_WIDGET (scroll),
		      TRUE, TRUE, 2);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  gtk_text_buffer_set_text (buffer, "Welcome to gfig version 3\n", -1);
}



static void
button_cb (GtkButton *button,
	   gpointer   user_data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
    button_s *bi = user_data;
    button_f new_bf = button_function (bi);
    // kill the working func
    if (!current_bf || current_bf != new_bf) {
      notify_projects (NOTIFY_TOOL_STOP);
      if (current_bf) (*current_bf)(GINT_TO_POINTER (DRAWING_STOP), NULL,
				    NAN, NAN);
      current_bf = new_bf;
      if (current_bf) 
	(*current_bf)(GINT_TO_POINTER (DRAWING_START), NULL, NAN, NAN);
    }
  }
}

static void
build_tool_grid (GtkWidget *outer_vbox)
{
  GtkWidget *frame = gtk_frame_new (_ ("Operations"));
  gtk_box_pack_start (GTK_BOX (outer_vbox), GTK_WIDGET (frame),
		      TRUE, TRUE, 2);
  GtkWidget *grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (frame), grid);

  gint ncols = (gint)lround ((gdouble)button_count);

  gint row, cnt;
  GtkWidget *btn0 = NULL;
  for(row = 0, cnt = 0; cnt < button_count; row++) {
    for(gint col = 0; col < ncols && cnt < button_count; col++, cnt++) {
      gchar *img = find_image_file (button_image (&buttons[cnt]));
      GtkWidget *image = gtk_image_new_from_file (img);
      g_free (img);
      GtkWidget *button;
      if (cnt == 0) {
	button = gtk_radio_button_new (NULL);
	btn0 = button;
      }
      else button =
	     gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (btn0));
      gtk_button_set_image (GTK_BUTTON (button), image);
      gtk_widget_set_tooltip_text (button, button_tip (&buttons[cnt]));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				    button_active (&buttons[cnt]));
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      g_signal_connect (button, "clicked",
			G_CALLBACK (button_cb), &buttons[cnt]);
      gtk_grid_attach (GTK_GRID (grid), button, col, row, 1, 1);
    }
  } 
}
/***
    |         |        mid	|	|
    |       left	|    right	|
 ***/
static void
build_state_keys (GtkWidget *outer_vbox)
{
  GtkWidget *frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (outer_vbox), GTK_WIDGET (frame),
		      FALSE, FALSE, 2);
  
  GtkWidget *grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_box_pack_start (GTK_BOX (outer_vbox), GTK_WIDGET (grid), FALSE, FALSE, 2);

  PangoAttrList *attr_list = pango_attr_list_new();
  PangoAttribute *attr;
  attr = pango_attr_size_new (8 * PANGO_SCALE);
  pango_attr_list_insert(attr_list, attr);
  
  for (gint i =  0; i < 3; i++) {
    state_labels[i] = gtk_label_new (NULL);
    gtk_label_set_attributes (GTK_LABEL (state_labels[i]), attr_list);
  }

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame), state_labels[1]);
  gtk_grid_attach (GTK_GRID (grid), frame, 1, 0, 2, 1);
  
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame), state_labels[0]);
  gtk_grid_attach (GTK_GRID (grid), frame, 0, 1, 2, 1);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (frame), state_labels[2]);
  gtk_grid_attach (GTK_GRID (grid), frame, 2, 1, 2, 1);

  set_key ("", "",  "");
}

static void
build_tool_specifics (GtkWidget *outer_vbox)
{
  empty_tool_label = gtk_label_new (_ ("(Empty)"));
  g_object_ref (empty_tool_label);	// make sure it never goes away
  
  tool_specific_frame = gtk_frame_new (_ ("Tool specific options"));
  g_object_ref (tool_specific_frame);	// make sure it never goes away
  gtk_container_add (GTK_CONTAINER (tool_specific_frame), empty_tool_label);
  gtk_box_pack_start (GTK_BOX (outer_vbox), GTK_WIDGET (tool_specific_frame),
		      FALSE, FALSE, 2);
}

static void
build_prompt (GtkWidget *outer_vbox)
{
  prompt = gtk_entry_new ();
  GValue editable = G_VALUE_INIT;
  g_value_init (&editable, G_TYPE_BOOLEAN);
  g_value_set_boolean (&editable, FALSE);
  g_object_set_property ((GObject *)prompt, "editable", &editable);
  gtk_entry_set_has_frame (GTK_ENTRY (prompt), FALSE);
  gtk_widget_set_can_focus (prompt, FALSE);
  gtk_entry_set_placeholder_text (GTK_ENTRY (prompt), _ ("Idle"));
  gtk_box_pack_start (GTK_BOX (outer_vbox), prompt, FALSE, FALSE, 2);
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *window;
  GtkWidget *outer_vbox;
  gchar *write_xml = NULL;
  gchar *load_xml = NULL;
  GError *error = NULL;
  GOptionEntry entries[] = {
    { "dump-xml", 'x', 0, G_OPTION_ARG_FILENAME,
      &write_xml, "Dump XML after loading.", NULL },
    { "load-xml", 'l', 0, G_OPTION_ARG_FILENAME,
      &load_xml, "Load XML.", NULL },
    { NULL }
  };


  init_python ();

  signal(SIGINT, catch_intr);	// python grabs it if we don't

  GOptionContext *context = g_option_context_new ("file1 file2...");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  
  gtk_init (&argc, &argv);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_warning ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
  }

  find_plugins ();

  if (!init_global_environment ()) {
    log_string (LOG_GFIG_ERROR, NULL, _ ("Initialisation error."));
    return 1;
  }

  load_persistents (global_environment);

#if 0
  PangoFontMap *fontmap = pango_cairo_font_map_get_default();
  PangoFontFamily **font_families = NULL;
  gint n_families = -1;
  pango_font_map_list_families (fontmap, &font_families, &n_families);
  for (int i = 0; i < n_families; i++) {
    PangoFontFamily *family = font_families[i];
    const char * family_name = pango_font_family_get_name (family);
    PangoFontFace **faces;
    gint n_faces;
    pango_font_family_list_faces (family,
				  &faces,
				  &n_faces);

    for (int j = 0; j < n_faces; j++) {
      PangoFontDescription *desc = pango_font_face_describe (faces[j]);
      gchar *fname = pango_font_description_to_string (desc);
      g_print ("\"%s\" \"%s\"\n", family_name, fname);
      g_free (fname);
    }
    g_free (faces);
  }
#endif
    
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  //gtk_window_set_default_size (GTK_WINDOW (window), 480, 320);
  gtk_window_set_title (GTK_WINDOW (window), "gf");
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gfig_quit), NULL);
  
  outer_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), outer_vbox);
  
  build_menu (outer_vbox);

  build_tool_grid (outer_vbox);
  
  build_prompt (outer_vbox);

  build_state_keys (outer_vbox);

  build_tool_specifics (outer_vbox);
  
  build_log_window (outer_vbox);

  projects = gtk_list_store_new (PROJECT_COL_COUNT, G_TYPE_POINTER);

  gtk_widget_show_all (window);

  
  if (load_xml) {
    load_drawing (load_xml);
    g_free (load_xml);
  }

  if (argc > 1) {
    for (int i = 1; i < argc; i++)
      create_new_project (NULL, argv[i]);
  }

  if (!load_xml && argc <= 1) create_new_project (NULL, NULL);
  
  if (write_xml) {
    save_drawing (write_xml);
    g_free (write_xml);
    return 0;
  }
  
  gtk_main ();
    
  return 0;
}

