#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <string.h>

#include "gf.h"

guint
set_state_bit (GdkEvent *event)
{
  GdkEventKey *key = (GdkEventKey *)event;
  guint state = key->state;
  switch(key->keyval) {
  case GDK_KEY_Shift_L:
  case GDK_KEY_Shift_R:
    state |= GDK_SHIFT_MASK;    // 1
    break;
  case GDK_KEY_Control_L:
  case GDK_KEY_Control_R:
    state |= GDK_CONTROL_MASK;  // 4
    break;
  case GDK_KEY_Alt_L:
  case GDK_KEY_Meta_L:
  case GDK_KEY_Meta_R:
  case GDK_KEY_Alt_R:
    state |= GDK_MOD1_MASK;     // 8
    break;
  }
  return state;
}

guint
clear_state_bit (GdkEvent *event)
{
  GdkEventKey *key = (GdkEventKey *)event;
  guint state = key->state;
  switch(key->keyval) {
  case GDK_KEY_Shift_L:
  case GDK_KEY_Shift_R:
    state &= ~GDK_SHIFT_MASK;   // 1
    break;
  case GDK_KEY_Control_L:
  case GDK_KEY_Control_R:
    state &= ~GDK_CONTROL_MASK; // 4
    break;
  case GDK_KEY_Alt_L:
  case GDK_KEY_Meta_L:
  case GDK_KEY_Meta_R:
  case GDK_KEY_Alt_R:
    state &= ~GDK_MOD1_MASK;    // 8
    break;
  }
  return state;
}

void
snap_to_grid (environment_s *env, gdouble *pxp, gdouble *pyp)
{
  if (pxp && pyp && environment_snap_grid (env) &&
      environment_target_grid (env) != 0.0 &&
      !isnan (environment_target_grid (env))) {
    *pxp -= remainder (*pxp, environment_target_grid (env));
    *pyp -= remainder (*pyp, environment_target_grid (env));
  }
}

pen_s *
copy_pen (pen_s *oldpen)
{
  pen_s *newpen = gfig_try_malloc0 (sizeof(pen_s));
  memcpy (newpen, oldpen, sizeof(pen_s));
  pen_colour (newpen) = gdk_rgba_copy (pen_colour (oldpen));
  pen_colour_name (newpen) = g_strdup (pen_colour_name (oldpen));
  return newpen;
}

pen_s *
copy_environment_pen (environment_s *env)
{
  pen_s *newpen = gfig_try_malloc0 (sizeof(pen_s));
  pen_s *oldpen = environment_pen (env);
  memcpy (newpen, oldpen, sizeof(pen_s));
  pen_colour (newpen) = gdk_rgba_copy (pen_colour (oldpen));
  pen_colour_name (newpen) = g_strdup (pen_colour_name (oldpen));
  return newpen;
}

gchar *
find_image_file (const gchar *lpath)
{
  gchar *path = NULL;

#ifdef DATAROOTDIR
  if (g_file_test (DATAROOTDIR, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    path = g_strdup_printf ("%s/images/%s", DATAROOTDIR, lpath);
#endif
  if (!path)
    path = g_strdup_printf ("./images/%s", lpath);

  return path;
}

gdouble
pen_lw_cvt (environment_s *env)
{
  pen_s *pen   = environment_pen (env);
  gdouble lw   = pen_lw (pen);
  if (environment_dunit (env) == GTK_UNIT_INCH)
    lw /= 25.4;
  return lw;
}
