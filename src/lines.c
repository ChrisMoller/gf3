#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <math.h>

#include "gf.h"
#include "drawing.h"
#include "drawing_header.h"
#include "entities.h"
#include "view.h"
#include "utilities.h"
#include "python.h"

typedef enum {
  // initial idle state must always be TOOL_IDLE (= 0)
  LINES_IDLE = TOOL_IDLE,
  LINES_NEXT_POINT
} lines_state_e;

static GtkWidget *tool_box        = NULL;
static GtkWidget *intersect_combo = NULL;
static GtkWidget *intersect_spin  = NULL;
static GtkWidget *spline_button   = NULL;
static GtkWidget *closed_button   = NULL;
static GtkWidget *fill_button     = NULL;

typedef enum {
  TERMINATE_OFF,
  TERMINATE_BEFORE,
  TERMINATE_AFTER
} terminate_e;


static void
derive_key (guint state)
{
  const gchar *primary   = "-0-";
  const gchar *middle    = "-0-";
  const gchar *secondary = "-0-";
  
  switch (state) {
  case STATE_UNSHIFTED:
    primary   = "Point";
    middle    = "Last point";
    secondary = "New polyline";
    break;
  case STATE_SHIFT:
    primary   = "Key entry point";
    middle    = "Key entry last point";
    secondary = "Key entry new polyline";
    break;
  case STATE_CONTROL:
    primary   = "Cancel point";
    secondary = "Cancel line";
    break;
  case STATE_CONTROL_SHIFT:
    break;
  case STATE_ALT:
    break;
  case STATE_ALT_SHIFT:
    break;
  case STATE_ALT_CONTROL:
    break;
  case STATE_ALT_CONTROL_SHIFT:
    break;
  }
  
  set_key (primary, middle, secondary);
}

static void
spline_toggled (GtkToggleButton *togglebutton,
		gpointer         user_data)
{
  gboolean active = gtk_toggle_button_get_active (togglebutton);
  gtk_widget_set_sensitive (intersect_combo, !active);
  gtk_widget_set_sensitive (intersect_spin, !active);
}

static void
set_tsf_lines ()
{
  GtkWidget *frame;

  if (!tool_box) {
    tool_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    g_object_ref (tool_box);	// make sure it never goes away

    /******** mode switches *********/

    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start (GTK_BOX (tool_box), GTK_WIDGET (vbox),
			FALSE, FALSE, 2);

    /************ spline ************/

    spline_button = gtk_check_button_new_with_label (_ ("Spline"));
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (spline_button),
			FALSE, TRUE, 0);
    g_signal_connect (spline_button, "toggled",
		      G_CALLBACK (spline_toggled), NULL);
    
    /************ closed ************/

    closed_button = gtk_check_button_new_with_label (_ ("Closed"));
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (closed_button),
			FALSE, TRUE, 0);
 
    /************ fill ************/

    fill_button = gtk_check_button_new_with_label (_ ("Fill"));
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (fill_button),
			FALSE, TRUE, 0);
    
    /******** vertex type *******/

    frame = gtk_frame_new (_ ("Vertex type"));
    gtk_box_pack_start (GTK_BOX (tool_box), GTK_WIDGET (frame), FALSE, TRUE, 2);
    
    intersect_combo = gtk_combo_box_text_new ();
    gtk_container_add (GTK_CONTAINER (frame), intersect_combo);
    // the order of the following have to be in the same order as the 
    // intersect_e enum in gf.h
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (intersect_combo),
			       NULL, _ ("Point"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (intersect_combo),
			       NULL, _ ("Arc"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (intersect_combo),
			       NULL, _ ("Bevel"));
    gtk_combo_box_set_active (GTK_COMBO_BOX (intersect_combo), 0);

    frame = gtk_frame_new (_ ("Vertex radius"));
    gtk_box_pack_start (GTK_BOX (tool_box), GTK_WIDGET (frame),
			FALSE, TRUE, 2);


    /************ vertex radius **********/
    
    intersect_spin = gtk_spin_button_new_with_range (0.0, 1.0e50, .1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (intersect_spin), 4);
    gtk_entry_set_width_chars (GTK_ENTRY (intersect_spin), 10);
    gtk_container_add (GTK_CONTAINER (frame), intersect_spin);

    gtk_widget_show_all (tool_box);
  }

  set_tsf (tool_box);
}

static void
lines_primary_button_press (GdkEvent *event, sheet_s *sheet,
			    gdouble pxi, gdouble pyi, terminate_e terminate)
{
  if (!sheet) return;		// fixme internal err

  /***
      unshifted : point
      shift	: key point
      control:	: cancel point
  ***/

  environment_s *env = sheet_environment (sheet);
  GdkEventButton *button = (GdkEventButton *)event;
  gdouble px = NAN;
  gdouble py = NAN;

  GdkEventButton *buttonevent = (GdkEventButton *)event;
  guint state  = buttonevent->state;
  switch (state) {
  case STATE_UNSHIFTED:
    if (isnan (pxi)) {
      px = button->x + environment_hoff (env);
      py = button->y + environment_voff (env);
      cairo_matrix_transform_point (&environment_inv_tf (env), &px, &py);
      snap_to_grid (env, &px, &py);
    }
    else {
      px = pxi; py = pyi;
    }
    break;
  case STATE_SHIFT:
    evaluate_point (sheet, &px, &py, derive_key);
    break;
  case STATE_CONTROL:	// cancel point
    // do nothing
    break;
  }

  if (isnan (px)) return;	// only cancel point
  
  point_s *point = gfig_try_malloc0 (sizeof(point_s));
  point_x (point) = px;
  point_y (point) = py;
  entity_polyline_s *polyline = NULL;

  if (sheet_tool_state (sheet) != LINES_IDLE &&
      terminate == TERMINATE_BEFORE) {
    sheet_tool_state (sheet) = LINES_IDLE;
    set_prompt (_ ("Initial point"));
    polyline = sheet_tool_entity (sheet);
    GList *last = g_list_last (entity_polyline_verts (polyline));
    if (last) {
      point_s *last_point = last->data;
      if (last_point) g_free (last_point);
      entity_polyline_verts (polyline) =
	g_list_delete_link (entity_polyline_verts (polyline), last);
    }
    sheet_tool_entity (sheet) = NULL;
    set_current_line_colour (env, NULL);
  }
  
  if (sheet_tool_state (sheet) == LINES_IDLE) {
    sheet_tool_state (sheet) = LINES_NEXT_POINT;
    set_prompt (_ ("Next point"));
    
    if (sheet_tool_entity (sheet))
      delete_entities (sheet_tool_entity (sheet));
    gdouble radius =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (intersect_spin));
    gint intersect_type =
      gtk_combo_box_get_active (GTK_COMBO_BOX (intersect_combo));
    gboolean spline =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spline_button));
    gboolean closed =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (closed_button));
    gboolean fill =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (fill_button));
    
    polyline = sheet_tool_entity (sheet) =
      entity_build_polyline (sheet, NULL, closed, fill, spline, NULL,
			     intersect_type, radius);
    sheet_entities (sheet) =
      g_list_append (sheet_entities (sheet), sheet_tool_entity (sheet));
    
    entity_polyline_verts (polyline) =		// set initial point
      g_list_append (entity_polyline_verts (polyline), point);
    point = copy_point (point);
    set_current_line_colour (env, entity_polyline_pen (polyline));
  }

  polyline = sheet_tool_entity (sheet);
  entity_polyline_verts (polyline) =
    g_list_append (entity_polyline_verts (polyline), point);
  point_x (&sheet_current_point (sheet)) = px;
  point_y (&sheet_current_point (sheet)) = py;

  if (terminate == TERMINATE_AFTER) {
    sheet_tool_state (sheet) = LINES_IDLE;
    sheet_tool_entity (sheet) = NULL;
    set_current_line_colour (env, NULL);
    set_prompt (_ ("Initial point"));
  }
  force_redraw (sheet);
}

// void draw_lines (GdkEvent *event, sheet_s *sheet)
DRAW_FUNCTION ("draw_lines.png", draw_lines, FALSE, \
	       "Draw lines, polylines, and splines")
{
  switch (GPOINTER_TO_INT (event)) {
  case DRAWING_STOP:
    set_prompt ("");
    set_tsf (NULL);
    break;
  case DRAWING_START:
    {
      set_tsf_lines ();
      derive_key (0);
      set_prompt (_ ("Initial point"));
      environment_s *env = sheet_environment (get_active_sheet ());
      set_current_line_colour (env, environment_pen (env));
    }
    break;
  default:
    {
      switch(event->type) {
      case GDK_KEY_RELEASE:
	derive_key (clear_state_bit (event));
	break;
      case GDK_KEY_PRESS:
	derive_key (set_state_bit (event));
	break;
      case GDK_BUTTON_PRESS:
	{
	  GdkEventButton *buttonevent = (GdkEventButton *)event;
	  guint button = buttonevent->button;

	  switch (button) {
	  case GDK_BUTTON_PRIMARY:
	    lines_primary_button_press (event, sheet, pxi, pyi,
					TERMINATE_OFF);
	    break;
	  case GDK_BUTTON_MIDDLE:	// last point
	    lines_primary_button_press (event, sheet, pxi, pyi,
					TERMINATE_AFTER);
	    break;
	  case GDK_BUTTON_SECONDARY:	// new poly
	    lines_primary_button_press (event, sheet, pxi, pyi,
					TERMINATE_BEFORE);
	    break;
	  }
	}
	break;
      case GDK_MOTION_NOTIFY:
	{
	  g_assert (sheet != NULL);		// fixme internal err
	  entity_polyline_s *polyline = sheet_tool_entity (sheet);
	  if (polyline) {
	    environment_s *env = sheet_environment (sheet);
	    GdkEventMotion *motion = (GdkEventMotion *)event;
	    gdouble px, py;
	    //if (isnan (pxi)) {
	      px = motion->x + environment_hoff (env);
	      py = motion->y + environment_voff (env);
	      cairo_matrix_transform_point (&environment_inv_tf (env),
					    &px, &py);
	      snap_to_grid (env, &px, &py);
	      //}
	      //else {
	      //px = pxi; py = pyi;
	      //}
	    point_s *last =
	      g_list_last (entity_polyline_verts (polyline))->data;
	    point_x (last) = px;
	    point_y (last) = py;
	    force_redraw (sheet);
	  }
#if 0
	  if (sheet_transients (sheet) &&
	      !isnan (point_x (&sheet_current_point (sheet)))) {

	    entity_polyline_s *transient =
	      g_list_nth_data (sheet_transients (sheet), 0);
	    point_s *p0 =
	      g_list_nth_data (entity_polyline_verts (transient), 0);
	    point_s *p1 =
	      g_list_nth_data (entity_polyline_verts (transient), 1);
	    memmove (p0, &sheet_current_point (sheet), sizeof(point_s));
	    point_x (p1) = px;
	    point_y (p1) = py;
	    force_redraw (sheet);
	  }
#endif
	}
        break;
      default:
        // fixme 
        break;
      }
    }
    break;
  }
}
