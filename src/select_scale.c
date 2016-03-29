#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "gf.h"
#include "utilities.h"

typedef struct {
  GtkWidget        *inches;
  GtkWidget        *scale;
  GtkWidget        *uupdu;
  GtkWidget        *uuname;
  GtkWidget        *origin_x;
  GtkWidget        *origin_y;
  GtkWidget        *origin_draw;
  GtkWidget        *grid_draw;
  GtkWidget        *grid_snap;
  GtkWidget        *grid_value;
} scale_private_data_s;
#define scale_inches(p)		(p)->inches
#define scale_scale(p)		(p)->scale)
#define scale_uupdu(p)		(p)->uupdu
#define scale_uuname(p)		(p)->uuname
#define scale_org_x(p)		(p)->origin_x
#define scale_org_y(p)		(p)->origin_y
#define scale_org_draw(p)	(p)->origin_draw
#define scale_grid_draw(p)	(p)->grid_draw
#define scale_grid_snap(p)	(p)->grid_snap
#define scale_grid_value(p)	(p)->grid_value

GtkWidget *
select_scale (void **private_data, environment_s *env)
{
  GtkWidget     *content;
  GtkWidget     *frame;

  g_assert (env != NULL);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);

  scale_private_data_s *ppd = gfig_try_malloc0 (sizeof(scale_private_data_s));
  *private_data = ppd;
  
  /************ drawing units and grid *******************/


  GtkWidget *outer_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);


  /*******  drawing units ******/
  
  frame = gtk_frame_new (_ ("Drawing Units"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (outer_hbox), frame, FALSE, TRUE, 8);
  
  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  GtkWidget *button = scale_inches (ppd) =
    gtk_radio_button_new_with_label (NULL, _ ("Inches"));
  if (environment_dunit (env) == GTK_UNIT_INCH)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (scale_inches (ppd)),
		    FALSE, FALSE, 2);

  button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button),
						 _ ("Millimetres"));
  if (environment_dunit (env) == GTK_UNIT_MM)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (button), FALSE, FALSE, 2);



  /********** grid *********/

  

  frame = gtk_frame_new (_ ("Grid"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (outer_hbox), frame, FALSE, TRUE, 8);
  
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);


  scale_grid_draw (ppd) = gtk_check_button_new_with_label (_ ("Draw Grid"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scale_grid_draw (ppd)),
						   environment_show_grid (env));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scale_grid_draw (ppd)),
		    FALSE, FALSE, 2);

  scale_grid_snap (ppd) = gtk_check_button_new_with_label (_ ("Snap to Grid"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scale_grid_snap (ppd)),
						   environment_snap_grid (env));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scale_grid_snap (ppd)),
		    FALSE, FALSE, 2);


  GtkWidget *lbl = gtk_label_new (_ ("Grid point"));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  scale_grid_value (ppd) = gtk_spin_button_new_with_range (0.0, 1000, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (scale_grid_value (ppd)),
			     environment_target_grid (env));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scale_grid_value (ppd)),
		      FALSE, FALSE, 2);

  
  gtk_box_pack_start (GTK_BOX (content), outer_hbox, FALSE, TRUE, 8);







  /************ user units *******************/

  frame = gtk_frame_new (_ ("User units"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, TRUE, 8);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  scale_uuname (ppd) = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (scale_uuname (ppd)),
					     _ ("Enter unit label"));
  if (environment_uuname (env))
    gtk_entry_set_text (GTK_ENTRY (scale_uuname (ppd)),
			environment_uuname (env));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (scale_uuname (ppd)),
		    FALSE, FALSE, 2);

  lbl = gtk_label_new (_ ("User unit label"));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  scale_uupdu (ppd) = gtk_spin_button_new_with_range (1.0, 1000, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (scale_uupdu (ppd)),
			     environment_uupdu (env));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (scale_uupdu (ppd)),
		    FALSE, FALSE, 2);

  lbl = gtk_label_new (_ ("User units per drawing unit"));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);
  
  
  /************ origin *******************/

  frame = gtk_frame_new (_ ("Origin"));
  gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 0.9);
  gtk_box_pack_start (GTK_BOX (content), frame, FALSE, TRUE, 8);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  lbl = gtk_label_new (_ ("X:"));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  scale_org_x (ppd) = gtk_spin_button_new_with_range (-1000.0, 1000, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (scale_org_x (ppd)),
			     environment_org_x (env));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scale_org_x (ppd)),
		      FALSE, FALSE, 2);

  lbl = gtk_label_new (_ ("Y"));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (lbl), FALSE, FALSE, 2);

  scale_org_y (ppd) = gtk_spin_button_new_with_range (-1000.0, 1000, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (scale_org_y (ppd)),
			     environment_org_y (env));
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (scale_org_y (ppd)),
		      FALSE, FALSE, 2);

  scale_org_draw (ppd) = gtk_check_button_new_with_label (_ ("Draw Origin"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scale_org_draw (ppd)),
						   environment_show_org (env));
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (scale_org_draw (ppd)),
		    FALSE, FALSE, 2);
  

  return content;
}

void
extract_scale (void *private_data, environment_s *env)
{
  scale_private_data_s *ppd = private_data;

  if (scale_inches (ppd)) {
    gint active =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scale_inches (ppd)));
    environment_dunit (env) = active ? GTK_UNIT_INCH : GTK_UNIT_MM;
  }

  if (scale_uupdu (ppd)) {
    environment_uupdu (env) =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (scale_uupdu (ppd)));
  }

  if (scale_uuname (ppd)) {
    const gchar *str = gtk_entry_get_text (GTK_ENTRY (scale_uuname (ppd)));
    if (str) {
      if (environment_uuname (env)) g_free (environment_uuname (env));
      environment_uuname (env) = g_strdup (str);
    }
  }

  if (scale_org_x (ppd)) {
    environment_org_x (env) =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (scale_org_x (ppd)));
  }

  if (scale_org_y (ppd)) {
    environment_org_y (env) =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (scale_org_y (ppd)));
  }

  if (scale_org_draw (ppd)) {
    environment_show_org (env) =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scale_org_draw (ppd)));
  }

  if (scale_grid_draw (ppd)) {
    environment_show_grid (env) =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scale_grid_draw (ppd)));
  }

  if (scale_grid_snap (ppd)) {
    environment_snap_grid (env) =
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scale_grid_snap (ppd)));
  }

  if (scale_grid_value (ppd)) {
    environment_target_grid (env) =
      gtk_spin_button_get_value (GTK_SPIN_BUTTON (scale_grid_value (ppd)));
  }
}
