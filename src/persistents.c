#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "select_pen.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "fallbacks.h"

// read  persistents here;
// search DATAROOTDIR/gfig.rc
//        $HOME/.gfig/preferences
//        ./.preferences

#define RC_FILE         "gfig.rc"
#define USER_PREFS      "preferences"
#define LOCAL_PREFS     "./.preferences"

#define GROUP_PAPER			"Paper"
#define KEY_RC_PAPER_NAME		"Name"
#define KEY_RC_PAPER_COLOUR		"Colour"
#define KEY_RC_PAPER_COLOUR_NAME	"ColourName"
#define KEY_RC_PAPER_ORIENTATION	"Orientation"
#define KEY_VALUE_PORTRAIT		"Portrait"
#define KEY_VALUE_LANDSCAPE		"Landscape"

#define GROUP_PEN			"Pen"
#define KEY_RC_PEN_COLOUR		"Colour"
#define KEY_RC_PEN_COLOUR_NAME		"ColourName"
#define KEY_RC_PEN_LINESTYLE		"Style"
#define KEY_RC_PEN_LINEWIDTH		"Width"
#define KEY_RC_PEN_STD_LINEWIDTH	"StandardWidth"

#define GROUP_COLOUR_TABLE		"ColourTable"
#define KEY_RC_COLOUR_TABLE_VALUES	"ColourTableValues"

#define GROUP_DRAWING			"Drawing"
#define KEY_RC_DRAWING_UNITS		"DrawingUnit"
#define KEY_RC_USER_UNITS_PER_DU	"UserUnitsPerDrawingUnit"
#define KEY_RC_USER_UNIT_NAME		"UserUnitName"
#define KEY_RC_ORIGIN_X			"OriginX"
#define KEY_RC_ORIGIN_Y			"OriginY"
#define KEY_RC_SHOW_ORIGIN		"ShowOrigin"
#define KEY_VALUE_INCH			"Inch"
#define KEY_VALUE_MM			"Millimetre"

#define colour_ety_pfmt "%s;%s;%g;%g;%g;%g;%g;%g;%g;"
#define colour_ety_sfmt "%m[^;];%m[^;];%lg;%lg;%lg;%lg;%lg;%lg;%lg;"

void
save_colour_entry (css_colours_s *ety, GKeyFile *key_file)
{
  gchar *str = g_strdup_printf (colour_ety_pfmt,
				ety->name,
				ety->hex,
				ety->alpha,
				ety->red,
				ety->green,
				ety->blue,
				ety->hue,
				ety->saturation,
				ety->value);
				
  g_key_file_set_value (key_file,
			GROUP_COLOUR_TABLE,
			KEY_RC_COLOUR_TABLE_VALUES,
			str);

  g_free (str);
}

void
save_persistents (environment_s *env)
{
  GKeyFile *key_file = NULL;
  paper_s *current_paper = environment_paper (env);
  pen_s *current_pen     = environment_pen (env);

  g_assert (current_paper != NULL);
  g_assert (current_pen   != NULL);

  key_file = g_key_file_new ();

  g_key_file_set_comment (key_file,
                          NULL,
                          NULL,
                          "Gfig resources",
                          NULL);

  /********************* colour table *************/

  if (colour_table_modified ())
    store_custom_colours (key_file);
  
  
  /****************** drawing ***************/
  
  g_key_file_set_value (key_file,
			GROUP_DRAWING,
			KEY_RC_DRAWING_UNITS,
			(environment_dunit (env) == GTK_UNIT_MM)
			? KEY_VALUE_MM : KEY_VALUE_INCH);
  
  g_key_file_set_double (key_file,
			 GROUP_DRAWING,
			 KEY_RC_USER_UNITS_PER_DU,
			 environment_uupdu (env));

  if (environment_uuname (env))
    g_key_file_set_value (key_file,
			  GROUP_DRAWING,
			  KEY_RC_USER_UNIT_NAME,
			  environment_uuname (env));
  
  g_key_file_set_double (key_file,
			 GROUP_DRAWING,
			 KEY_RC_ORIGIN_X,
			 environment_org_x (env));
  
  g_key_file_set_double (key_file,
			 GROUP_DRAWING,
			 KEY_RC_ORIGIN_Y,
			 environment_org_y (env));
  
  g_key_file_set_boolean (key_file,
			  GROUP_DRAWING,
			  KEY_RC_SHOW_ORIGIN,
			  environment_show_org (env));
  
  /****************** paper ***************/
    
  /****** paper name ******/

  if (paper_size (current_paper))
    g_key_file_set_value (key_file,
			  GROUP_PAPER,
			  KEY_RC_PAPER_NAME,
			  gtk_paper_size_get_name (paper_size (current_paper)));


  /****** paper colour name ******/

  if (paper_colour_name (current_paper))
    g_key_file_set_value (key_file,
			  GROUP_PAPER,
			  KEY_RC_PAPER_COLOUR_NAME,
			  paper_colour_name (current_paper));

    
  /****** paper orientation ******/

  g_key_file_set_value (key_file,
			GROUP_PAPER,
			KEY_RC_PAPER_ORIENTATION,
			(paper_orientation (current_paper) ==
			   GTK_PAGE_ORIENTATION_PORTRAIT) ?
			KEY_VALUE_PORTRAIT : KEY_VALUE_LANDSCAPE);

  
  /****************** pen ***************/

    
  /****** pen colour name ******/

  if (pen_colour_name (current_pen))
    g_key_file_set_value (key_file,
			  GROUP_PEN,
			  KEY_RC_PEN_COLOUR_NAME,
			  pen_colour_name (current_pen));

    
  /****** pen line style ******/
    
  g_key_file_set_integer (key_file,
			  GROUP_PEN,
			  KEY_RC_PEN_LINESTYLE,
			  pen_line_style (current_pen));
  g_key_file_set_comment (key_file,
			  GROUP_PEN,
			  KEY_RC_PEN_LINESTYLE,
			  line_style_name (get_environment_line_style (env)),
			  NULL);

    
  /****** pen line width ******/
    
  g_key_file_set_double (key_file,
			 GROUP_PEN,
			 KEY_RC_PEN_LINEWIDTH,
			 pen_lw (current_pen));
    
  g_key_file_set_integer (key_file,
			  GROUP_PEN,
			  KEY_RC_PEN_STD_LINEWIDTH,
			  pen_lw_std_idx (current_pen));
  
  {
    gchar *data = NULL;;
    gsize data_length;
    GError *error = NULL;
    gchar *path;

    data = g_key_file_to_data (key_file, &data_length, &error);

    path = g_strdup_printf ("%s/.%s",
                            g_get_home_dir (),
                            g_get_prgname());

    g_mkdir_with_parents (path, 0777);
    g_free (path);

    path = g_strdup_printf ("%s/.%s/%s",
                            g_get_home_dir (),
                            g_get_prgname(),
                            USER_PREFS);

    g_file_set_contents (path,
                         data,
                         data_length,
                         &error);

    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }

    g_free (path);

    if (data) g_free (data);
  }
}

static gint
parse_rc (environment_s *env, const gchar *path)
{
  GKeyFile *key_file;
  gboolean rc;
  gint changes = 0;
  paper_s *current_paper = environment_paper (env);
  pen_s   *current_pen   = environment_pen (env);

  g_assert (current_paper != NULL);
  g_assert (current_pen != NULL);
  g_assert (paper_colour (current_paper) != NULL);
  g_assert (pen_colour (current_pen) != NULL);

  key_file = g_key_file_new ();
  rc = g_key_file_load_from_file (key_file,
                                  path,
                                  G_KEY_FILE_KEEP_COMMENTS
                                  | G_KEY_FILE_KEEP_TRANSLATIONS,
                                  NULL);

  if (rc) {
    gboolean bool;
    gdouble  dbl;
    gchar   *string;
    gint     integer;
    //    gdouble *dbl_list;
    //    gsize    list_length;
    GError  *error = NULL;


    /********************* colour table *************/
    
    string = g_key_file_get_string (key_file,
				    GROUP_COLOUR_TABLE,
				    KEY_RC_COLOUR_TABLE_VALUES,
				    &error);
    if (string) {
      css_colours_s *ety = gfig_try_malloc0 (sizeof(css_colours_s));
      sscanf (string,
	      colour_ety_sfmt,
	      &ety->name,
	      &ety->hex,
	      &ety->alpha,
	      &ety->red,
	      &ety->green,
	      &ety->blue,
	      &ety->hue,
	      &ety->saturation,
	      &ety->value);
      ety->preset = FALSE;
      append_colour_ety (ety);
      g_free (string);
    }
  
    /****************** drawing ***************/
  
    string = g_key_file_get_string (key_file,
				    GROUP_DRAWING,
				    KEY_RC_DRAWING_UNITS,
				    &error);
    if (string) {
      environment_dunit (env) =
	!g_strcmp0 (string, KEY_VALUE_INCH) ? GTK_UNIT_INCH : GTK_UNIT_MM;
      g_free (string);
    }
    
    dbl = g_key_file_get_double (key_file,
				 GROUP_DRAWING,
				 KEY_RC_USER_UNITS_PER_DU,
				 &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    environment_uupdu (env) = dbl;

    string = g_key_file_get_string (key_file,
				    GROUP_DRAWING,
				    KEY_RC_USER_UNIT_NAME,
				    &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (string) {
      if (environment_uuname (env)) g_free (environment_uuname (env));
      environment_uuname (env) = string;
    }
    
    dbl = g_key_file_get_double (key_file,
				 GROUP_DRAWING,
				 KEY_RC_ORIGIN_X,
				 &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    environment_org_x (env) = dbl;
    
    dbl = g_key_file_get_double (key_file,
				 GROUP_DRAWING,
				 KEY_RC_ORIGIN_Y,
				 &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    environment_org_y (env) = dbl;
    
    bool = g_key_file_get_boolean (key_file,
				   GROUP_DRAWING,
				   KEY_RC_SHOW_ORIGIN,
				   &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    environment_show_org (env) = bool;
    
    /****** paper name ******/
    
    string = g_key_file_get_string (key_file,
				    GROUP_PAPER,
				    KEY_RC_PAPER_NAME,
				    &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (string) {
      GtkPaperSize *size = gtk_paper_size_new (string);
      if (size) {
	if (paper_size (current_paper))
	  gtk_paper_size_free (paper_size (current_paper));
	paper_size (current_paper) = size;
      }
      else log_string (LOG_GFIG_ERROR, NULL, _ ("Paper size lookup error."));
      g_free (string);
    }

    
    
    /****** paper colour name ******/

    string = g_key_file_get_string (key_file,
				    GROUP_PAPER,
				    KEY_RC_PAPER_COLOUR_NAME,
				    NULL);
    if (string) {
      if (paper_colour_name (current_paper))
	g_free (paper_colour_name (current_paper));
      paper_colour_name (current_paper) = string;
      if (!paper_colour (current_paper))
	paper_colour (current_paper) = gfig_try_malloc0 (sizeof(GdkRGBA));
      if (!find_css_colour_rgba (string, paper_colour (current_paper))) {
	paper_colour_red (current_paper)   = FALLBACK_PAPER_COLOUR_RED;
	paper_colour_green (current_paper) = FALLBACK_PAPER_COLOUR_GREEN;
	paper_colour_blue (current_paper)  = FALLBACK_PAPER_COLOUR_BLUE;
	paper_colour_alpha (current_paper) = FALLBACK_PAPER_COLOUR_ALPHA;
	log_string (LOG_GFIG_ERROR, NULL,
		    _ ("Paper colour lookup error.  Using fallback."));
      }
    }

    
    /****** paper orientation ******/
    
    string = g_key_file_get_string (key_file,
				    GROUP_PAPER,
				    KEY_RC_PAPER_ORIENTATION,
				    &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (string) {
      paper_orientation (current_paper) =
	!g_strcmp0 (string, KEY_VALUE_PORTRAIT)
	? GTK_PAGE_ORIENTATION_PORTRAIT : GTK_PAGE_ORIENTATION_LANDSCAPE;
      g_free (string);
    }

    
    
    /****** pen colour name ******/

    string = g_key_file_get_string (key_file,
				    GROUP_PEN,
				    KEY_RC_PEN_COLOUR_NAME,
				    &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (string) {
      if (pen_colour_name (current_pen))
	g_free (pen_colour_name (current_pen));
      pen_colour_name (current_pen) = string;
      if (!pen_colour (current_pen))
	pen_colour (current_pen) = gfig_try_malloc0 (sizeof(GdkRGBA));
      if (!find_css_colour_rgba (string, pen_colour (current_pen))) {
	pen_colour_red (current_pen)   = FALLBACK_PEN_COLOUR_RED;
	pen_colour_green (current_pen) = FALLBACK_PEN_COLOUR_GREEN;
	pen_colour_blue (current_pen)  = FALLBACK_PEN_COLOUR_BLUE;
	pen_colour_alpha (current_pen) = FALLBACK_PEN_COLOUR_ALPHA;
	log_string (LOG_GFIG_ERROR, NULL,
		    _ ("Pen colour lookup error.  Using fallback."));
      }
    }

    
    /****** pen line style ******/

    integer = g_key_file_get_integer (key_file,
				      GROUP_PEN,
				      KEY_RC_PEN_LINESTYLE,
				      &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (integer >= 0 && integer < line_style_count) 
      pen_line_style (current_pen) = integer;
    

    
    /****** pen line width ******/

    dbl = g_key_file_get_double (key_file,
				 GROUP_PEN,
				 KEY_RC_PEN_LINEWIDTH,
				 &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    pen_lw (current_pen) = dbl;
    pen_lw_std_idx  (current_pen) = STANDARD_LINE_WIDTH_CUSTOM;

    integer = g_key_file_get_double (key_file,
				     GROUP_PEN,
				     KEY_RC_PEN_STD_LINEWIDTH,
				     &error);
    if (error) {
      log_string (LOG_GFIG_ERROR, NULL, error->message);
      g_clear_error (&error);
    }
    if (integer >= STANDARD_LINE_WIDTH_0 &&
	integer <= STANDARD_LINE_WIDTH_8)
      pen_lw_std_idx (current_pen) = integer;
    else {
      pen_lw_std_idx (current_pen) = FALLBACK_PEN_LW_IDX;
      log_string (LOG_GFIG_ERROR, NULL,
		  _ ("Pen standard linewidth error.  Using fallback."));
    }
  }

  g_key_file_free (key_file);
  
  return changes;
}

void
load_persistents (environment_s *env)
{
  gchar *path;
  gint changes = 0;

  g_assert (env != NULL);

#ifdef DATAROOTDIR				// read system rc
  path = g_strdup_printf ("%s/%s", DATAROOTDIR, RC_FILE);
  changes |= parse_rc (env, path);
  g_free (path);
#endif

  path = g_strdup_printf ("%s/.%s/%s",		// read home rc
			  g_get_home_dir (),
			  g_get_prgname(),
			  USER_PREFS);
  changes |= parse_rc (env, path);
  g_free (path);

  changes |= parse_rc (env, LOCAL_PREFS);	// read local rc

}
