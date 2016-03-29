#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "utilities.h"
#include "select_pen.h"
#include "fallbacks.h"

#include "xml-kwds.h"

typedef struct {
  GString *string;
  gint indent;
} context_s;

GQuark parse_quark = 0;

GHashTable *element_hash = NULL;

static void write_entities (gpointer data, gpointer user_data);

static gboolean
test_boolean (const gchar *str)
{
  return (KWD_YES == GPOINTER_TO_INT (g_hash_table_lookup (element_hash, str)));
}

static gint
get_kwd (const gchar *str)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (element_hash, str));
}

static gdouble
get_double (const gchar *str)
{
  return str ? g_ascii_strtod (str, NULL) : 0.0;
}

static gint
get_int (const gchar *str)
{
  return str ? (gint)g_ascii_strtoll (str, NULL, 0) : 0;
}

#if 0 // comments

     Structure:

         drawing = environment  project*
         project = environment? sheet*
         sheet   = environment? detail* entity*
         detail  = environment? detail* entity*

	 entity = text | polyline | circle | ellipsecabt | ellipseffae

	 text = point string size alignment justify lead spread

	 polyline = closed filled spline pen point+

	 circle = point r filled pen
	 
	 ellipsecabt = point a b filled pen

	 ellipseffae = point point a e filled pen

         environment = paper pen drawing_unit origin font

         paper = ISO_name colour orientation

	 pen = colour line_style line_width

	 colour = rgba | CSS_name

	 orientatioon = PORTRAIT | LANDSCAPE

	 drawing_unit = INCH | MM

	 origin = point

	 point = x y

	 line_style = line_style_name | line_style_spec

	 line_width = lw_index | lw


     Terminals:

         ISO_name	/* letter, legal, A4,...*/
         CSS_name	/* red, green,... */
	 rgba		/* [r, g, b, a], 0.0 -1.0 */
	 PORTRAIT
	 LANDSCAPE
	 INCH
	 MM
	 x		/* double */
	 y		/* double */
	 r		/* double */
	 a		/* double */
	 b		/* double */
	 e		/* double */
	 font		/* font name, Sans, Serif,... */
	 line_style_name  /* Solid, Long Dash, Short dash,... */
	 line_style_spec  /* double, double, double... */
         lw_index       /* int */
	 lw             /* double */
	 filled		/* boolean */
	 closed		/* boolean */
	 spline		/* boolean */
	 string		/* char * */
	 size		/* double */
	 lead		/* double */
	 spread		/* double */
	 alignment	/* LEFT CENTER CENTRE RIGHT */
	 justify	/* boolean */
#endif

/*************** write it **************/

static void
write_header_open (GString *string)
{
  g_string_append_printf (string, "<%s>\n", DRAWING);
}

       
static void
write_header_close (GString *string)
{
  g_string_append_printf (string, "</%s>\n", DRAWING);
}

static void
write_font_spec (GString *string, gint indent,  gchar *font_name,
		 gdouble font_size)
{
  g_string_append_printf (string, "%*s<%s %s=\"%f\" %s=\"%s\"/>\n",
			  indent, " ", FONT,
			  SIZE, font_size,
			  NAME, font_name ? : "");
}

static void
write_colour_spec (GString *string, gint indent,  GdkRGBA *colour,
		   gchar *colour_name)
{
  g_string_append_printf (string,
	"%*s<%s %s=\"%f\" %s=\"%f\" %s=\"%f\" %s=\"%f\"\n\t%s=\"%s\"/>\n",
			  indent, " ",
			  COLOUR,
			  RED,   colour->red,
			  GREEN, colour->green,
			  BLUE,  colour->blue,
			  ALPHA, colour->alpha,
			  NAME,  colour_name);
}

static void
write_paper_spec (GString *string, gint indent,  paper_s *paper)
{
  g_string_append_printf (string, "%*s<%s>\n",  indent, " ", PAPER);

  g_string_append_printf (string, "%*s<%s %s=\"%s\" %s=\"%s\"/>\n",
			  indent + 2, " ", PAPERSIZE, ORIENTATION,
			  (paper_orientation (paper) ==
			   GTK_PAGE_ORIENTATION_LANDSCAPE) ?
			  LANDSCAPE : PORTRAIT,
			  NAME, gtk_paper_size_get_name (paper_size (paper)));
  write_colour_spec (string, indent + 2, paper_colour (paper),
		     paper_colour_name (paper));
  
  g_string_append_printf (string, "%*s</%s>\n", indent, " ", PAPER);
}

static void
write_linestyle_spec (GString *string, gint indent,  gint style)
{
  g_string_append_printf (string, "%*s<%s %s=\"%s\"/>\n",
			  indent, " ", LINESTYLE,
			  NAME, get_ls_from_idx (style));
}

static void
write_linewidth_spec (GString *string, gint indent,  gint idx, gdouble width)
{
  g_string_append_printf (string, "%*s<%s %s=\"%d\" %s=\"%f\"/>\n",
			  indent, " ", LINEWIDTH,
			  LWIDX, idx, WIDTH, width);
}

static void
write_drawing_unit (GString *string, gint indent,  gint unit)
{
  g_string_append_printf (string, "%*s<%s %s=\"%s\"/>\n",
			  indent, " ", DRAWING_UNIT,
			  UNIT, unit == GTK_UNIT_MM ? MILLIMETRE : INCH);
}

static void
write_pen_spec (GString *string, gint indent,  pen_s *pen)
{
  g_string_append_printf (string, "%*s<%s>\n",  indent, " ", PEN);

  write_colour_spec (string, indent + 2, pen_colour (pen),
		     pen_colour_name (pen));
  write_linestyle_spec (string, indent + 2, pen_line_style (pen));
  write_linewidth_spec (string, indent + 2, pen_lw_std_idx (pen),
			pen_lw (pen));
  
  g_string_append_printf (string, "%*s</%s>\n", indent, " ", PEN);
}

static void
write_grid_spec (GString *string, gint indent,  environment_s *env)
{
  g_string_append_printf (string, "%*s<%s %s=\"%s\" %s=\"%s\" %s=\"%f\"/>\n",
			  indent, " ", GRID,
			  SHOW, environment_show_grid (env) ? YES : NO,
			  SNAP, environment_snap_grid (env) ? YES : NO,
			  VALUE, environment_target_grid (env));
}

static void
write_environment (GString *string, gint indent, environment_s *env)
{
  g_string_append_printf (string, "%*s<%s>\n",  indent, " ", ENVIRONMENT);

  write_paper_spec   (string, indent + 2, environment_paper (env));
  write_pen_spec     (string, indent + 2, environment_pen (env));
  write_drawing_unit (string, indent + 2, environment_dunit (env));
  write_font_spec    (string, indent + 2, environment_fontname (env),
		      environment_textsize (env));
  write_grid_spec    (string, indent + 2, env);

  g_string_append_printf (string, "%*s</%s>\n", indent, " ", ENVIRONMENT);
}

//	 text = point string size alignment justify lead spread

static void
write_text (GString *string, gpointer type, gint indent)
{
  entity_text_s *text = type;
  gchar *alignment = "unset";
  switch(entity_text_alignment (text)) {
  case PANGO_ALIGN_LEFT:   alignment = LEFT;	break;
  case PANGO_ALIGN_CENTER: alignment = CENTRE;	break;
  case PANGO_ALIGN_RIGHT:  alignment = RIGHT;	break;
  }
  g_string_append_printf (string, "%*s<%s %s=\"%f\" %s=\"%f\" %s=\"%s\" \
  %s=\"%s\" %s=\"%d\"\n\
 \t%s=\"%d\" %s=\"%f\" %s=\"%s\">\n",
			  indent, " ", TEXT,
			  X,  entity_text_x (text),
			  Y,  entity_text_y (text),
			  ALIGNMENT, alignment,
			  JUSTIFY,
			  entity_text_alignment (text) ? YES : NO,
			  LEAD,    entity_text_lead (text),
			  SPREAD,  entity_text_spread (text),
			  ANGLE,   entity_text_t (text),
			  FILLED,  entity_text_filled (text) ? YES : NO);
  write_pen_spec (string, indent + 2, entity_text_pen (text));
  write_font_spec (string, indent + 2, entity_text_font (text),
		   entity_text_txtsize (text));
  g_string_append_printf (string, "%*s<%s>%s</%s>\n",
			  indent + 2, " ",
			  STRING, entity_text_string (text), STRING);
  g_string_append_printf (string, "%*s</%s>\n", indent, " ", TEXT);
	   
}

static void
write_ellipse (GString *string, gpointer type, gint indent)
{
  entity_ellipse_s *ellipse = type;
  g_string_append_printf (string,
			  "%*s<%s %s=\"%f\" %s=\"%f\" %s=\"%f\" \
  %s=\"%f\" %s=\"%f\" %s=\"%s\" %s=\"%s\">\n",
			  indent, " ", ELLIPSE,
			  AAXIS, entity_ellipse_a (ellipse),
			  BAXIS, entity_ellipse_b (ellipse),
			  ANGLE, entity_ellipse_t (ellipse),
			  START, entity_ellipse_start (ellipse),
			  STOP,  entity_ellipse_stop (ellipse),
			  FILLED,
			  entity_ellipse_fill (ellipse) ? YES : NO,
			  NEGATIVE,
			  entity_ellipse_negative (ellipse) ? YES : NO);
  g_string_append_printf (string, "%*s<%s %s=\"%f\" %s=\"%f\"/>\n",
			  indent +2, " ", CENTRE,
			  X, entity_ellipse_x (ellipse),
			  Y, entity_ellipse_y (ellipse));
  write_pen_spec (string, indent + 2, entity_ellipse_pen (ellipse));
  g_string_append_printf (string, "%*s</%s>\n",  indent , " ", ELLIPSE);
}

static void
write_circle (GString *string, gpointer type, gint indent)
{
  entity_circle_s *circle = type;
  g_string_append_printf (string, "%*s<%s %s=\"%f\" \
   %s=\"%f\" %s=\"%f\" %s=\"%s\" %s=\"%s\">\n",
			  indent, " ", CIRCLE,
			  RADIUS, entity_circle_r (circle),
			  START,  entity_circle_start (circle),
			  STOP,   entity_circle_stop (circle),
			  FILLED,
			  entity_circle_fill (circle) ? YES : NO,
			  NEGATIVE,
			  entity_circle_negative (circle) ? YES : NO);
  g_string_append_printf (string, "%*s<%s %s=\"%f\" %s=\"%f\"/>\n",
			  indent +2, " ", CENTRE,
			  X, entity_circle_x (circle),
			  Y, entity_circle_y (circle));
  write_pen_spec (string, indent + 2, entity_circle_pen (circle));
  g_string_append_printf (string, "%*s</%s>\n",  indent , " ", CIRCLE);
}

static void
write_polyline (GString *string, gpointer type, gint indent)
{
  entity_polyline_s *polyline = type;
  g_string_append_printf (string, "%*s<%s %s=\"%s\" %s=\"%s\" %s=\"%s\">\n",
			  indent, " ", POLYLINE,
			  CLOSED,
			  entity_polyline_closed (polyline) ? YES : NO,
			  SPLINE,
			  entity_polyline_spline (polyline) ? YES : NO,
			  FILLED,
			  entity_polyline_filled (polyline) ? YES : NO);
  
  guint nr_verts = g_list_length (entity_polyline_verts (polyline));
  for (int i = 0; i < nr_verts; i++) {
    point_s *p0 = g_list_nth_data (entity_polyline_verts (polyline), i);
    g_string_append_printf (string, "%*s<%s %s=\"%f\" %s=\"%f\"/>\n",
			    indent+2 , " ", POINT,
			    X, point_x (p0),
			    Y, point_y (p0));
  }

  write_pen_spec (string, indent + 2, entity_polyline_pen (polyline));
  g_string_append_printf (string, "%*s</%s>\n",  indent , " ", POLYLINE);
}

static void
write_transform (GString *string, gpointer type, gint indent)
{
  cairo_matrix_t *matrix = type;
  g_string_append_printf (string, "%*s<%s \
   %s=\"%f\" %s=\"%f\" %s=\"%f\" %s=\"%f\" %s=\"%f\" %s=\"%f\">\n",
			  indent, " ", TRANSFORM,
			  XX,  matrix->xx,
			  YX,  matrix->yx,
			  YY,  matrix->yy,
			  XY,  matrix->xy,
			  X0,  matrix->x0,
			  Y0,  matrix->y0);
}

static void
write_close_transform (GString *string, gpointer type, gint indent)
{
  g_string_append_printf (string, "%*s</%s>\n", indent, " ", TRANSFORM);
}

static void
write_group (GString *string, gpointer type, gint indent)
{
  entity_group_s *group = type;
  if (entity_group_transform (group))
    write_transform (string, entity_group_transform (group), indent);
  if (entity_group_entities (group)) {
    context_s *context = gfig_try_malloc0 (sizeof(context_s));
    context->string = string;
    context->indent = indent;
    g_list_foreach (entity_group_entities (group), write_entities, context);
    g_free (context);
  }
  if (entity_group_transform (group))
      write_close_transform (string, NULL, indent);
}

static void
write_entities (gpointer data, gpointer user_data)
{
  context_s *context = user_data;
  gint indent = context->indent + 2;
  GString *string = context->string;
  
  entity_type_e type = entity_type (data);
  switch (type) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_TRANSFORM:	// fixme
    //    write_transform (string, data, indent);
    break;
  case ENTITY_TYPE_TEXT:
    write_text (string, data, indent );
    break;
  case ENTITY_TYPE_CIRCLE:
    write_circle (string, data, indent);
    break;
  case ENTITY_TYPE_ELLIPSE:
    write_ellipse (string, data, indent);
    break;
  case ENTITY_TYPE_POLYLINE:
    write_polyline (string, data, indent);
    break;
  case ENTITY_TYPE_GROUP:	// fixme
    write_group (string, data, indent);
    break;
  }
}

static gboolean
save_sheet_func (GtkTreeModel *model,
		 GtkTreePath *path,
		 GtkTreeIter *iter,
		 gpointer data)
{
  context_s *context = data;
  gint indent = context->indent;
  GString *string = context->string;
  sheet_s *sheet =  NULL;

  GtkTreeIter parent;
  gchar *ppath_string = NULL;
  if (gtk_tree_model_iter_parent (model, &parent, iter)) {
    GtkTreePath *ppath = gtk_tree_model_get_path (model, &parent);
    ppath_string = gtk_tree_path_to_string (ppath);
    gtk_tree_path_free (ppath);
  }
  else ppath_string = NULL;
  

  gtk_tree_model_get (model, iter,
		      SHEET_STRUCT_COL, &sheet, -1);
  if (sheet) {
    g_string_append_printf (string, "%*s<%s %s=\"%s\" %s=\"%s\">\n",
			    indent, " ", SHEET,
			    NAME, sheet_name (sheet),
			    PARENT, ppath_string ? : "");
    write_environment (string, indent + 2, sheet_environment (sheet));
    g_list_foreach (sheet_entities (sheet), write_entities, context);
    g_string_append_printf (string, "%*s</%s>\n", indent, " ", SHEET);
  }
  if (ppath_string) g_free (ppath_string);
  return FALSE;
}

static gboolean
save_project_func (GtkTreeModel *model,
		   GtkTreePath *path,
		   GtkTreeIter *iter,
		   gpointer data)
{
  context_s *context = data;
  project_s *project =  NULL;
  
  gtk_tree_model_get (model, iter, PROJECT_STRUCT_COL, &project, -1);
  
  if (project) {
    gint indent = context->indent = 2;
    GString *string = context->string;

    g_string_append_printf (string, "%*s<%s %s=\"%s\">\n",
			    indent, " ", PROJECT,
			    NAME, project_name (project));

    write_environment (string, indent + 2, project_environment (project));

    context->indent += 2;
    gtk_tree_model_foreach (GTK_TREE_MODEL (project_sheets (project)),
			    save_sheet_func,
			    context);

    g_string_append_printf (string, "%*s</%s>\n", indent, " ", PROJECT);
  }
  return FALSE;
}

void
save_drawing (gchar *file)
{
  GString *string = g_string_new (NULL);
  gint indent = 0;
  
  context_s *context = gfig_try_malloc0 (sizeof(context_s));
  context->string = string;
  context->indent = indent;
  
  write_header_open (string);
  write_environment (string, indent + 2, get_global_environment ());
  gtk_tree_model_foreach (GTK_TREE_MODEL (get_projects ()),
			  save_project_func,
			  context);
  write_header_close (string);
  gchar *content = g_string_free (string, FALSE);
  g_file_set_contents (file,
		       content,
		       -1,
		       NULL);
  g_free (content);
  g_free (context);
}	 


/*************  read it **************/

static void
parser_error (GMarkupParseContext *context,
              GError  *error,
              gpointer user_data)
{
  GtkWidget *dialog;
  gint line;
  gint offset;

  if (error->domain != parse_quark) return;
  
  g_markup_parse_context_get_position (context, &line, &offset);
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK,
           _ ("Parsing error near line %d (near line offset %d): %s"),
                                   line, offset,
                                   error->message);
  gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_run (GTK_DIALOG (dialog));
  g_clear_error (&error);
  gtk_widget_destroy (dialog);
}


static void
parser_end_element (GMarkupParseContext *context,
                    const gchar *element_name,
                    gpointer      user_data,
                    GError      **error)
{
  
  switch(get_kwd (element_name)) {
  case KWD_TRANSFORM:
    {
      sheet_s *sheet = user_data;
      entity_transform_s *transform =
	gfig_try_malloc0 (sizeof(entity_transform_s));
      entity_tf_type (transform) = ENTITY_TYPE_TRANSFORM;
      entity_tf_unset (transform) = TRUE;
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), transform);
    }
    break;
  case KWD_DRAWING:
  case KWD_PAPER:
  case KWD_PEN:
  case KWD_ENVIRONMENT:
  case KWD_SHEET:
  case KWD_CIRCLE:
  case KWD_ELLIPSE:
  case KWD_TEXT:
  case KWD_POLYLINE:
  case KWD_STRING:
  case KWD_PROJECT:
    g_markup_parse_context_pop (context);
    break;
  default:
    break;
  }
}


static void
paper_start_element (GMarkupParseContext *context,
		     const gchar *element_name,
		     const gchar **attribute_names,
		     const gchar **attribute_values,
		     gpointer      user_data,
		     GError      **error)
{
  paper_s *paper = user_data;
  // fixme crash if null

  switch(get_kwd (element_name)) {
  case KWD_PAPERSIZE:
    {
      if (attribute_names && attribute_values) {
	for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_NAME:
	    if (paper_size (paper)) gtk_paper_size_free (paper_size (paper));
	    paper_size (paper) = gtk_paper_size_new (attribute_values[i]);
	    break;
	  case KWD_ORIENTATION:
	    {
	      gint orient = get_kwd (attribute_values[i]);
	      paper_orientation (paper) =
		orient == KWD_LANDSCAPE ?
		GTK_PAGE_ORIENTATION_LANDSCAPE :
		GTK_PAGE_ORIENTATION_PORTRAIT;
	    }
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid paper size attribute %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  case KWD_COLOUR:
    {
      if (attribute_names && attribute_values) {
	if (!paper_colour (paper))
	  paper_colour (paper) = gfig_try_malloc0 (sizeof(GdkRGBA));
	for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_RED:
	    paper_colour_red (paper) = get_double (attribute_values[i]);
	    break;
	  case KWD_GREEN:
	    paper_colour_green (paper) = get_double (attribute_values[i]);
	    break;
	  case KWD_BLUE:
	    paper_colour_blue (paper) = get_double (attribute_values[i]);
	    break;
	  case KWD_ALPHA:
	    paper_colour_alpha (paper) = get_double (attribute_values[i]);
	    break;
	  case KWD_NAME:
	    if (paper_colour_name (paper)) g_free (paper_colour_name (paper));
	    paper_colour_name (paper) = g_strdup (attribute_values[i]) ? : NULL;
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid paper colour attribute %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in PAPER: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser paper_parser_ops = {
  paper_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
pen_start_element (GMarkupParseContext *context,
		   const gchar *element_name,
		   const gchar **attribute_names,
		   const gchar **attribute_values,
		   gpointer      user_data,
		   GError      **error)
{
  pen_s *pen = user_data;
  if (!pen) g_set_error (error, parse_quark, 1,
			 _ ("Internal error element in PEN."));

  switch(get_kwd (element_name)) {
  case KWD_LINESTYLE:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_NAME:
	    pen_line_style (pen) = get_idx_from_ls (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid pen linestyle attribute %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  case KWD_LINEWIDTH:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_LWIDX:
	    pen_lw_std_idx (pen) = get_int (attribute_values[i]);
	    break;
	  case KWD_WIDTH:
	    pen_lw (pen) = get_double (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid pen linewidth attribute %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  case KWD_COLOUR:
    {
      if (attribute_names && attribute_values) {
	if (!pen_colour (pen))
	  pen_colour (pen) = gfig_try_malloc0 (sizeof(GdkRGBA));
	for (gint i = 0; attribute_names && attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_RED:
	    pen_colour_red (pen) = get_double (attribute_values[i]);
	    break;	
	  case KWD_GREEN:
	    pen_colour_green (pen) = get_double (attribute_values[i]);
	    break;
	  case KWD_BLUE:
	    pen_colour_blue (pen) = get_double (attribute_values[i]);
	    break;
	  case KWD_ALPHA:
	    pen_colour_alpha (pen) = get_double (attribute_values[i]);
	    break;
	  case KWD_NAME:
	    if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
	    pen_colour_name (pen) = g_strdup (attribute_values[i]) ? : NULL;
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid pen colour attribute %s"),
			 attribute_names[i]);
	    break;
	  }
        }
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in PEN: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser pen_parser_ops = {
  pen_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};

static void
environment_start_element (GMarkupParseContext *context,
			   const gchar *element_name,
			   const gchar **attribute_names,
			   const gchar **attribute_values,
			   gpointer      user_data,
			   GError      **error)
{
  environment_s *env = user_data;
  if (!env) g_set_error (error, parse_quark, 1,
			 _ ("Internal error element in ENVIRONMENT."));
  
  switch(get_kwd (element_name)) {
  case KWD_PAPER:
    g_markup_parse_context_push (context, &paper_parser_ops,
				 environment_paper (env));
    break;
  case KWD_PEN:
    if (!environment_pen (env))
      environment_pen (env) = gfig_try_malloc0 (sizeof(pen_s));
    g_markup_parse_context_push (context, &pen_parser_ops,
				 environment_pen (env));	 
    break;
  case KWD_DRAWING_UNIT:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_UNIT:
	    environment_dunit (env) =
	      (KWD_INCH == get_kwd (attribute_values[i])) ?
	      GTK_UNIT_INCH : GTK_UNIT_MM;
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid environment drawing unit attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  case KWD_FONT:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_SIZE:
	    environment_textsize (env) = get_double (attribute_values[i]);
	    break;
	  case KWD_NAME:
	    environment_fontname (env) = g_strdup (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid environment font attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  case KWD_GRID:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_SHOW:
	    environment_show_grid (env) = test_boolean (attribute_values[i]);
	    break;
	  case KWD_SNAP:
	    environment_snap_grid (env) = test_boolean (attribute_values[i]);
	    break;
	  case KWD_VALUE:
	    environment_target_grid (env) = get_double (attribute_values[i]);
	    break;
	  }
	}
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1,
		 _ ("Invalid element in ENVIRONMENT: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser environment_parser_ops = {
  environment_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
polyline_start_element (GMarkupParseContext *context,
			const gchar *element_name,
		        const gchar **attribute_names,
		        const gchar **attribute_values,
		        gpointer      user_data,
		        GError      **error)
{
  entity_polyline_s *polyline = user_data;
  GList **verts = &entity_polyline_verts (polyline);
  
  switch(get_kwd (element_name)) {
  case KWD_PEN:
    g_markup_parse_context_push (context, &pen_parser_ops,
				 entity_polyline_pen (polyline));
    break;
  case KWD_POINT:
    {
      if (attribute_names && attribute_values) {
	point_s *point = gfig_try_malloc0 (sizeof(point_s));
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_X:
	    point_x (point) = get_double (attribute_values[i]);
	    break;
	  case KWD_Y:
	    point_y (point) = get_double (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid polyline point attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
	if (verts) *verts = g_list_append (*verts, point);
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in POLYLINE: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser polyline_parser_ops = {
  polyline_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
ellipse_start_element (GMarkupParseContext *context,
		       const gchar *element_name,
		       const gchar **attribute_names,
		       const gchar **attribute_values,
		       gpointer      user_data,
		       GError      **error)
{
  entity_ellipse_s *ellipse = user_data;
  
  switch(get_kwd (element_name)) {
  case KWD_PEN:
    g_markup_parse_context_push (context, &pen_parser_ops,
				 entity_ellipse_pen (ellipse));
    break;
  case KWD_CENTRE:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_X:
	    entity_ellipse_x (ellipse) = get_double (attribute_values[i]);
	    break;
	  case KWD_Y:
	    entity_ellipse_y (ellipse) = get_double (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid ellipse centre attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in ELLIPSE: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser ellipse_parser_ops = {
  ellipse_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
circle_start_element (GMarkupParseContext *context,
		     const gchar *element_name,
		     const gchar **attribute_names,
		     const gchar **attribute_values,
		     gpointer      user_data,
		     GError      **error)
{
  entity_circle_s *circle = user_data;
  
  switch(get_kwd (element_name)) {
  case KWD_PEN:
    g_markup_parse_context_push (context, &pen_parser_ops,
				 entity_circle_pen (circle));
    break;
  case KWD_CENTRE:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_X:
	    entity_circle_x (circle) = get_double (attribute_values[i]);
	    break;
	  case KWD_Y:
	    entity_circle_y (circle) = get_double (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid circle centre attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in CIRCLE: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser circle_parser_ops = {
  circle_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};

static void
string_text (GMarkupParseContext *context,
	     const gchar         *text,
	     gsize                text_len,
	     gpointer             user_data,
	     GError             **error)
{
  gchar **text_p = user_data;
  if (text_p) *text_p = g_strdup (text);
}

const GMarkupParser string_parser_ops = {
  NULL,
  parser_end_element,
  string_text,                  // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
text_start_element (GMarkupParseContext *context,
		    const gchar *element_name,
		    const gchar **attribute_names,
		    const gchar **attribute_values,
		    gpointer      user_data,
		    GError      **error)
{
  entity_text_s *text = user_data;
  
  switch(get_kwd (element_name)) {
  case KWD_STRING:
    g_markup_parse_context_push (context, &string_parser_ops,
				 &entity_text_string (text));
    break;
  case KWD_PEN:
    g_markup_parse_context_push (context, &pen_parser_ops,
				 entity_text_pen (text));
    break;
  case KWD_FONT:
    {
       if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_SIZE:
	    entity_text_size (text) = get_double (attribute_values[i]);
	    break;
	  case KWD_NAME:
	    entity_text_font (text) = g_strdup (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid text font attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in TEXT: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser text_parser_ops = {
  text_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
sheet_start_element (GMarkupParseContext *context,
		     const gchar *element_name,
		     const gchar **attribute_names,
		     const gchar **attribute_values,
		     gpointer      user_data,
		     GError      **error)
{
  sheet_s *sheet = user_data;
  if (!sheet) g_set_error (error, parse_quark, 1,
			 "Internal error element in ENVIRONMENT.");
  environment_s *env = sheet_environment (sheet);

  switch(get_kwd (element_name)) {
    break;
  case KWD_ENVIRONMENT:
    g_markup_parse_context_push (context, &environment_parser_ops, env);
    break;
  case KWD_TRANSFORM:
    {
      entity_transform_s *transform =
	gfig_try_malloc0 (sizeof(entity_transform_s));
      entity_tf_type (transform) = ENTITY_TYPE_TRANSFORM;
      entity_tf_unset (transform) = FALSE;
      entity_tf_matrix (transform) = gfig_try_malloc0 (sizeof(cairo_matrix_t));
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_XX:
	    entity_tf_xx (transform) = get_double (attribute_values[i]);
	    break;
	  case KWD_YX:
	    entity_tf_yx (transform) = get_double (attribute_values[i]);
	    break;
	  case KWD_XY:
	    entity_tf_xy (transform) = get_double (attribute_values[i]);
	    break;
	  case KWD_YY:
	    entity_tf_yy (transform) = get_double (attribute_values[i]);
	    break;
	  case KWD_X0:
	    entity_tf_x0 (transform) = get_double (attribute_values[i]);
	    break;
	  case KWD_Y0:
	    entity_tf_y0 (transform) = get_double (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid sheet transform attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), transform);
    }
    break;
  case KWD_TEXT:
    {
      entity_text_s *text =
	gfig_try_malloc0 (sizeof(entity_text_s));
      entity_text_type (text) = ENTITY_TYPE_TEXT;
      entity_text_pen (text) = copy_environment_pen (env);
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_JUSTIFY:
	    entity_text_justify (text) = test_boolean (attribute_values[i]);
	    break;
	  case KWD_FILLED:
	    entity_text_filled (text) = test_boolean (attribute_values[i]);
	    break;
	  case KWD_X:
	    entity_text_x (text) = get_double (attribute_values[i]);
	    break;
	  case KWD_Y:
	    entity_text_y (text) = get_double (attribute_values[i]);
	    break;
	  case KWD_LEAD:
	    entity_text_lead (text) = get_int (attribute_values[i]);
	    break;
	  case KWD_SPREAD:
	    entity_text_spread (text) = get_int (attribute_values[i]);
	    break;
	  case KWD_ANGLE:
	    entity_text_t (text) = get_double (attribute_values[i]);
	    break;
	  case KWD_ALIGNMENT:
	    entity_text_alignment (text) = get_int (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid sheet text attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), text);
      g_markup_parse_context_push (context, &text_parser_ops, text);
    }
    break;
  case KWD_POLYLINE:
    {
      entity_polyline_s *polyline =
	gfig_try_malloc0 (sizeof(entity_polyline_s));
      entity_polyline_type (polyline)	= ENTITY_TYPE_POLYLINE;
      entity_polyline_pen (polyline) = copy_environment_pen (env);
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_CLOSED:
	    entity_polyline_closed (polyline) =
	      test_boolean (attribute_values[i]);
	    break;
	  case KWD_SPLINE:
	    entity_polyline_spline (polyline) =
	      test_boolean (attribute_values[i]);
	    break;
	  case KWD_FILLED:
	    entity_polyline_filled (polyline) = 
	      test_boolean (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid sheet polyline attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), polyline);
      g_markup_parse_context_push (context, &polyline_parser_ops, polyline);
    }
    break;
  case KWD_CIRCLE:
    {
      entity_circle_s *circle =
	gfig_try_malloc0 (sizeof(entity_circle_s));
      entity_circle_type (circle) = ENTITY_TYPE_CIRCLE;
      entity_circle_pen (circle) = copy_environment_pen (env);
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_RADIUS:
	    entity_circle_r (circle) = get_double (attribute_values[i]);
	    break;
	  case KWD_START:
	    entity_circle_start (circle) = get_double (attribute_values[i]);
	    break;
	  case KWD_STOP:
	    entity_circle_stop (circle) = get_double (attribute_values[i]);
	    break;
	  case KWD_FILLED:
	    entity_circle_fill (circle) = 
	      test_boolean (attribute_values[i]);
	    break;
	  case KWD_NEGATIVE:
	    entity_circle_negative (circle) = 
	      test_boolean (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid sheet circle attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), circle);
      g_markup_parse_context_push (context, &circle_parser_ops, circle);
    }
    break;
  case KWD_ELLIPSE:
    {
      entity_ellipse_s *ellipse =
	gfig_try_malloc0 (sizeof(entity_ellipse_s));
      entity_ellipse_type (ellipse) = ENTITY_TYPE_ELLIPSE;
      entity_ellipse_pen (ellipse) = copy_environment_pen (env);
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_AAXIS:
	    entity_ellipse_a (ellipse) = get_double (attribute_values[i]);
	    break;
	  case KWD_BAXIS:
	    entity_ellipse_b (ellipse) = get_double (attribute_values[i]);
	    break;
	  case KWD_ANGLE:
	    entity_ellipse_t (ellipse) = get_double (attribute_values[i]);
	  case KWD_START:
	    entity_ellipse_start (ellipse) = get_double (attribute_values[i]);
	  case KWD_STOP:
	    entity_ellipse_stop (ellipse) = get_double (attribute_values[i]);
	  case KWD_FILLED:
	    entity_ellipse_fill (ellipse) = 
	      test_boolean (attribute_values[i]);
	  case KWD_NEGATIVE:
	    entity_ellipse_negative (ellipse) = 
	      test_boolean (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid sheet ellipse attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      sheet_entities (sheet) =
	g_list_append (sheet_entities (sheet), ellipse);
      g_markup_parse_context_push (context, &ellipse_parser_ops, ellipse);
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in SHEET: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser sheet_parser_ops = {
  sheet_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
project_start_element (GMarkupParseContext *context,
		       const gchar *element_name,
		       const gchar **attribute_names,
		       const gchar **attribute_values,
		       gpointer      user_data,
		       GError      **error)
{
  project_s *project = user_data;
  
  switch(get_kwd (element_name)) {
  case KWD_SHEET:
    {
      sheet_s *sheet;
      gchar *name;
      GtkTreeIter parent;
      gboolean valid_parent = FALSE;

      name = NULL;
      
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_NAME:
	    name = (gchar *)attribute_values[i];
	    break;
	  case KWD_PARENT:
	   
	    if (*attribute_values[i])
	      valid_parent = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (project_sheets (project)), &parent, attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid project sheet attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      append_sheet (project, valid_parent ? &parent : NULL, name, &sheet);
      g_markup_parse_context_push (context, &sheet_parser_ops, sheet);
    }
    break;
  case KWD_ENVIRONMENT:
    g_markup_parse_context_push (context, &environment_parser_ops,
				 project_environment (project));
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in PROJECT: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser project_parser_ops = {
  project_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
drawing_start_element (GMarkupParseContext *context,
		       const gchar *element_name,
		       const gchar **attribute_names,
		       const gchar **attribute_values,
		       gpointer      user_data,
		       GError      **error)
{
  project_s *project = NULL;
  environment_s *global_env = get_global_environment ();
  
  switch(get_kwd (element_name)) {
  case KWD_ENVIRONMENT:
    g_markup_parse_context_push (context, &environment_parser_ops, global_env);
    break;
  case KWD_PROJECT:
    {
      if (attribute_names && attribute_values) {
        for (gint i = 0; attribute_names[i]; i++) {
	  switch(get_kwd (attribute_names[i])) {
	  case KWD_NAME:
	    project = create_project (attribute_values[i]);
	    break;
	  default:
	    g_set_error (error, parse_quark, 1,
			 _ ("Invalid drawing project attribute: %s"),
			 attribute_names[i]);
	    break;
	  }
	}
      }
      g_markup_parse_context_push (context, &project_parser_ops, project);
    }
    break;
  default:
    g_set_error (error, parse_quark, 1, _ ("Invalid element in DRAWING: %s"),
		 element_name);
    break;
  }
}


const GMarkupParser drawing_parser_ops = {
  drawing_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};


static void
initial_start_element (GMarkupParseContext *context,
                      const gchar *element_name,
                      const gchar **attribute_names,
                      const gchar **attribute_values,
                      gpointer      user_data,
                      GError      **error)
{
  if (get_kwd (element_name) == KWD_DRAWING)
    g_markup_parse_context_push (context, &drawing_parser_ops, NULL);
  else
    g_set_error (error, parse_quark, 1, _ ("Invalid element in INITIAL: %s"),
		 element_name);

}


const GMarkupParser initial_parser_ops = {
  initial_start_element,
  parser_end_element,
  NULL,                         // parser_text,
  NULL,                         // passthrough
  parser_error,
};

void
load_drawing (gchar *filename)
{
  GMarkupParseContext *context = NULL;
  gchar   *contents = NULL;
  gsize    length;
  GError  *error = NULL;

  if (!element_hash) {
    element_hash = g_hash_table_new (g_str_hash,  g_str_equal);
    for (gint i = 0; i < nr_keys; i++)
      g_hash_table_insert (element_hash, keywords[i].keyword,
			   GINT_TO_POINTER (keywords[i].keyvalue));
  }
  
  if (parse_quark == 0)
    parse_quark = g_quark_from_string ("Parsing error");

  g_file_get_contents (filename,
                       &contents,
                       &length,
                       &error);

 if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_OK,
                                     _ ("Error reading drawing file %s: %s"),
                                     filename, error->message);
    gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_clear_error (&error);
    return;
  }
 
 // fixme -- might need G_MARKUP_PREFIX_ERROR_POSITION in flags
  context = g_markup_parse_context_new (&initial_parser_ops,
                                        0,      // GMarkupParseFlags flags
                                        NULL,   // gpointer user_data,
                                        NULL);  // GDestroyNotify

  g_markup_parse_context_parse (context,
                                contents,
                                length,
                                &error);
  if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_OK,
                                     _ ("Error reading drawing file %s: %s"),
                                     filename, error->message);
    gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_clear_error (&error);
    return;
  }

  if (context)  g_markup_parse_context_free (context);
  context = NULL;
  if (contents) g_free (contents);
  contents = NULL;

  notify_projects (NOTIFY_MAP);
}

