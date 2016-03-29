#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "drawing.h"
#include "drawing_header.h"

// void draw_ellipses (GdkEvent *event, sheet_s *sheet)
DRAW_FUNCTION ("draw_ellipses.png", draw_ellipses, FALSE, \
	       "Draw circles, ellipses, and elliptical and circular arcs")
{
  switch (GPOINTER_TO_INT (event)) {
  case DRAWING_STOP:
    g_print ("stopping ellipses\n");
    // reset state
    break;
  case DRAWING_START:
    g_print ("starting ellipses\n");
    // initialise
    break;
  default:
    {
      switch(event->type) {
      case GDK_BUTTON_PRESS:
	{
	  GdkEventButton *button = (GdkEventButton *)event;
	  environment_s *env = sheet_environment (sheet);
	  gdouble px = button->x + environment_hoff (env);
	  gdouble py = button->y + environment_voff (env);
	  cairo_matrix_transform_point (&environment_inv_tf (env), &px, &py);
	  g_print ("%g %g\n", px, py);
	  g_print ("ellipse state %08x button %u x %g y %g\n",
		   button->state,
		   button->button,
		   button->x,
		   button->y);
	}
	break;
      case GDK_MOTION_NOTIFY:
	break;
      default:
	// fixme 
	break;
      }
    }
    break;
  }

}
