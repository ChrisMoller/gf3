#ifndef PLUGIN_H
#define PLUGIN_H

// gfig-provided services

typedef enum {
  GFIG_OP_NONE,
  GFIG_OP_APPEND_CIRCLE,
  GFIG_OP_APPEND_LINE,
} gfig_op_e;

typedef void (*gfig_cb_fcn) (gfig_op_e gfig_op, ...);


// plugin-provided ops

typedef void (*plugin_query) (gchar **name_p, gchar **desc_p,
			      gchar ***icon_xpm_p);
typedef void (*plugin_start) (gfig_cb_fcn gfig_cb);

#endif /* PLUGIN_H */
