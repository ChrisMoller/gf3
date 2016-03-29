#ifndef PLUGINIF_H
#define PLUGINIF_H

/***
    These are the same as the main gf delarations with all the struct pointers
    replaced by void pointers, the objective being to avoid having to include
    all that stuff.
 ***/
void entity_append_circle (void *sheet, gdouble x, gdouble y,
			   gdouble r, gboolean filled);
void entity_append_polyline (void *sheet, GList *verts,
			     gboolean closed, gboolean filled);

void *get_active_sheet ();
void force_redraw (void *env);

#endif  /* PLUGINIF_H */
