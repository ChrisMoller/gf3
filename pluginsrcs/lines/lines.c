#include <gtk/gtk.h>

#include "../plugin.h"
#include "../pluginif.h"

GtkWidget *nitem;

typedef struct {
  gdouble x;
  gdouble y;
} point_s;

static void
gtk_plugin_destroy (GtkWidget *object,
		    gpointer   user_data)
{
  gtk_widget_destroy (GTK_WIDGET (user_data));
}

GList *pline = NULL;
gint count;

static void
hilbert(double x, double y, double xi, double xj, double yi, double yj, int n)
/* x and y are the coordinates of the bottom left corner */
/* xi & xj are the i & j components of the unit x vector of the frame */
/* similarly yi and yj */
{
  if (n <= 0) {
    point_s *point = g_malloc (sizeof(point_s));
    point->x = 10.0 * (x + (xi + yi)/2);
    point->y = 10.0 * (y + (xj + yj)/2);
    pline = g_list_append (pline, point);
    count++;
  }
  else {
    hilbert(x,           y,           yi/2, yj/2,  xi/2,  xj/2, n-1);
    hilbert(x+xi/2,      y+xj/2 ,     xi/2, xj/2,  yi/2,  yj/2, n-1);
    hilbert(x+xi/2+yi/2, y+xj/2+yj/2, xi/2, xj/2,  yi/2,  yj/2, n-1);
    hilbert(x+xi/2+yi,   y+xj/2+yj,  -yi/2,-yj/2, -xi/2, -xj/2, n-1);
  }
}

void
draw_button_clicked (GtkButton *button,
		     gpointer   user_data)
{
  pline = NULL;
  count = 0;

  gint n = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (nitem));
  hilbert (0.0, 0.0, 1.0, 0.0, 0.0, 1.0, n);
    
  entity_append_polyline (get_active_sheet (), pline, 1, 0);
  force_redraw (NULL);
  g_print ("count = %d\n", count);
}

void
query_plugin (gchar **name_p, gchar **desc_p, gchar ***icon_xpm_p)
{
  if (name_p) *name_p = "Lines";
  if (desc_p) *desc_p = "A demo pluging that draws lines";
  if (icon_xpm_p) *icon_xpm_p = NULL;
}

int
start_plugin (gfig_cb_fcn gfig_cb)
{
  GtkWidget *hbox;
  GtkWidget *label;

  pline = NULL;
  
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Lines Demo");
  gtk_window_set_default_size (GTK_WINDOW (window), 320, 240);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_plugin_destroy), window);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);


  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 2);

  label = gtk_label_new ("Depth");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 2);
  nitem = gtk_spin_button_new_with_range (2.0, 10.0, 1.0);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (nitem), 0);
  gtk_box_pack_start (GTK_BOX (hbox), nitem, FALSE, TRUE, 2);

  
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 2);

  GtkWidget *button;

  button = gtk_button_new_with_label ("Draw it!");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (draw_button_clicked), gfig_cb);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 2);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (gtk_plugin_destroy), window);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 2);

  gtk_widget_show_all (window);

  return 0;
}
