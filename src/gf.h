#ifndef GF_H
#define GF_H

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

#define DRAW_FUNCTION(i,func,a,t)					\
  void func (GdkEvent *event, sheet_s *sheet, gdouble pxi, gdouble pyi)

typedef enum {
  COLOUR_SOURCE_UNSET,
  COLOUR_SOURCE_USER,
  COLOUR_SOURCE_CSS
} colour_source_e;

typedef struct {
  GtkPaperSize	*size;		// paper descriptor
  gdouble	h_dim;		// in selected units, redundant, but faster
  gdouble	v_dim;		// in selected units, redundant, but faster
  GdkRGBA 	*colour;	// paper colour
  gchar 	*colour_name;
  gint		 orientation;	// 
} paper_s;
#define paper_size(p)		(p)->size
#define paper_h_dim(p)		(p)->h_dim
#define paper_v_dim(p)		(p)->v_dim
#define paper_colour(p)		(p)->colour
#define paper_colour_red(p)	(p)->colour->red
#define paper_colour_green(p)	(p)->colour->green
#define paper_colour_blue(p)	(p)->colour->blue
#define paper_colour_alpha(p)	(p)->colour->alpha
#define paper_colour_name(p)	(p)->colour_name
#define paper_orientation(p)	(p)->orientation

enum {
  STANDARD_LINE_WIDTH_CUSTOM = 0,
  STANDARD_LINE_WIDTH_0,
  STANDARD_LINE_WIDTH_1,
  STANDARD_LINE_WIDTH_2,
  STANDARD_LINE_WIDTH_3,
  STANDARD_LINE_WIDTH_4,
  STANDARD_LINE_WIDTH_5,
  STANDARD_LINE_WIDTH_6,
  STANDARD_LINE_WIDTH_7,
  STANDARD_LINE_WIDTH_8
};

enum {
  STANDARD_LINE_STYLE_LINE,
  STANDARD_LINE_STYLE_LONG_DASH,
  STANDARD_LINE_STYLE_MEDIUM_DASH,
  STANDARD_LINE_STYLE_SHORT_DASH,
  STANDARD_LINE_STYLE_SPARSE_DOTS,
  STANDARD_LINE_STYLE_NORMAL_DOTS,
  STANDARD_LINE_STYLE_DASH_DOT,
  STANDARD_LINE_STYLE_DASH_DOT_DOT
};

typedef struct {
  const gchar   *file_name;
  const gchar   *style_name;
  const gint     num_dashes;
  const gdouble *dashes;
} line_style_s;
#define line_style_file(l)              (l)->file_name
#define line_style_name(l)              (l)->style_name
#define line_style_dash_count(l)        (l)->num_dashes
#define line_style_dashes(l)            (l)->dashes
gint line_style_count;

typedef struct {
  GdkRGBA	*colour;	// pen colour
  gchar         *colour_name;
  gint           line_style;
  gint           standard_line_width;
  gdouble        line_width;
} pen_s;
#define pen_colour(p)		(p)->colour
#define pen_colour_red(p)	(p)->colour->red
#define pen_colour_green(p)	(p)->colour->green
#define pen_colour_blue(p)	(p)->colour->blue
#define pen_colour_alpha(p)	(p)->colour->alpha
#define pen_colour_name(p)	(p)->colour_name
#define pen_line_style(p)	(p)->line_style
#define pen_lw_std_idx(p)	(p)->standard_line_width
#define pen_lw(p)		(p)->line_width

typedef struct {
  paper_s	*paper;
  pen_s		*pen;
  gint		 drawing_unit;		// GTK_UNIT_MM or GTK_UNIT_INCH
  gdouble	 user_units_per_drawing_unit;
  gchar		*user_unit_name;
  cairo_matrix_t base_transform;
  cairo_matrix_t inverse_transform;
  gdouble	 origin_x;
  gdouble	 origin_y;
  gboolean	 show_origin;
  gboolean	 show_grid;
  gboolean	 snap_grid;
  gdouble	 target_grid;
  cairo_surface_t *surface;
  gdouble	 hoff;
  gdouble	 voff;
  GtkWidget	*readout;
  gdouble	 zoom_factor;
  GtkWidget	*da;
  GtkWidget	*scale;
  GtkWidget	*lcb;		// line_colour_button
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  gdouble	 da_height;
  gdouble	 da_width;
  GList		*history;
  GList		*histptr;
  cairo_t	*cr;
  gchar 	*font_name;
  gdouble 	 text_size;
} environment_s;
#define environment_paper(e)	(e)->paper
#define environment_pen(e)	(e)->pen
#define environment_dunit(e)	(e)->drawing_unit
#define environment_uupdu(e)	(e)->user_units_per_drawing_unit
#define environment_uuname(e)	(e)->user_unit_name
#define environment_inv_tf(e)	(e)->inverse_transform
#define environment_inv_xx(e)	(e)->inverse_transform.xx
#define environment_inv_yx(e)	(e)->inverse_transform.yx
#define environment_inv_xy(e)	(e)->inverse_transform.xy
#define environment_inv_yy(e)	(e)->inverse_transform.yy
#define environment_inv_x0(e)	(e)->inverse_transform.x0
#define environment_inv_y0(e)	(e)->inverse_transform.y0
#define environment_tf(e)	(e)->base_transform
#define environment_tf_xx(e)	(e)->base_transform.xx
#define environment_tf_yx(e)	(e)->base_transform.yx
#define environment_tf_xy(e)	(e)->base_transform.xy
#define environment_tf_yy(e)	(e)->base_transform.yy
#define environment_tf_x0(e)	(e)->base_transform.x0
#define environment_tf_y0(e)	(e)->base_transform.y0
#define environment_org_x(e)	(e)->origin_x
#define environment_org_y(e)	(e)->origin_y
#define environment_show_org(e)	(e)->show_origin
#define environment_show_grid(e)	(e)->show_grid
#define environment_snap_grid(e)	(e)->snap_grid
#define environment_target_grid(e)	(e)->target_grid
#define environment_surface(e)	(e)->surface
#define environment_hoff(e)	(e)->hoff
#define environment_voff(e)	(e)->voff
#define environment_readout(e)	(e)->readout
#define environment_zoom(e)	(e)->zoom_factor
#define environment_da(e)	(e)->da
#define environment_scale(e)	(e)->scale
#define environment_lcb(e)	(e)->lcb
#define environment_hadj(e)	(e)->hadj
#define environment_vadj(e)	(e)->vadj
#define environment_da_ht(e)	(e)->da_height
#define environment_da_wid(e)	(e)->da_width
#define environment_history(e)	(e)->history
#define environment_histptr(e)	(e)->histptr
#define environment_cr(e)	(e)->cr
#define environment_fontname(e)	(e)->font_name
#define environment_textsize(e)	(e)->text_size

typedef enum {
  ENTITY_TYPE_NONE,
  ENTITY_TYPE_CIRCLE,
  ENTITY_TYPE_ELLIPSE,
  ENTITY_TYPE_TEXT,
  ENTITY_TYPE_POLYLINE,
  ENTITY_TYPE_TRANSFORM,
  ENTITY_TYPE_GROUP
} entity_type_e;

typedef struct {
  entity_type_e	type;
} entity_none_s;
#define entity_none_type(e)	(e)->type

typedef struct {
  gdouble x;
  gdouble y;
} point_s;
#define point_x(p)	(p)->x
#define point_y(p)	(p)->y


typedef struct {
  entity_type_e	type;
  GList		 *entities;
  cairo_matrix_t *transform;
  point_s	 *centre;
} entity_group_s;
#define entity_group_type(p)		(p)->type
#define entity_group_entities(p)	(p)->entities
#define entity_group_transform(p)	(p)->transform
#define entity_group_centre(p)		(p)->centre

typedef enum {
  INTERSECT_POINT,	// be sure to keep in sync with python.c
  INTERSECT_ARC,
  INTERSECT_BEVEL
} intersect_e;

typedef struct {
  entity_type_e	type;
  GList		*verts;
  gboolean	 closed;
  gboolean	 filled;
  gboolean	 spline;
  intersect_e	 intersect;
  gdouble	 isect_radius;
  pen_s		*pen;
} entity_polyline_s;
#define entity_polyline_type(p)		(p)->type
#define entity_polyline_verts(p)	(p)->verts
#define entity_polyline_closed(p)	(p)->closed
#define entity_polyline_filled(p)	(p)->filled
#define entity_polyline_spline(p)	(p)->spline
#define entity_polyline_intersect(p)	(p)->intersect
#define entity_polyline_isect_radius(p)	(p)->isect_radius
#define entity_polyline_pen(p)		(p)->pen

typedef struct {
  entity_type_e	type;
  gdouble	 x;
  gdouble	 y;
  gdouble	 r;
  gdouble	 start;
  gdouble	 stop;
  gboolean	 negative;
  gboolean	 fill;
  pen_s		*pen;
} entity_circle_s;
#define entity_circle_type(e)		(e)->type
#define entity_circle_x(e)		(e)->x
#define entity_circle_y(e)		(e)->y
#define entity_circle_r(e)		(e)->r
#define entity_circle_start(e)		(e)->start
#define entity_circle_stop(e)		(e)->stop
#define entity_circle_negative(e)	(e)->negative
#define entity_circle_fill(e)		(e)->fill
#define entity_circle_pen(e)		(e)->pen

typedef struct {
  entity_type_e	type;
  gdouble	 x;
  gdouble	 y;
  gchar		*string;
  gchar		*font_name;
  gint		 size;
  PangoAlignment alignment;
  gboolean	 justify;
  gint	 	 line_spacing;
  gint	 	 letter_spacing;
  gdouble	 t;
  gdouble	 txt_size;
  gboolean	 filled;
  pen_s		*pen;
} entity_text_s;
#define entity_text_type(e)	(e)->type
#define entity_text_x(e)	(e)->x
#define entity_text_y(e)	(e)->y
#define entity_text_string(e)	(e)->string
#define entity_text_font(e)	(e)->font_name
#define entity_text_size(e)	(e)->size
#define entity_text_alignment(e) (e)->alignment
#define entity_text_justify(e)	(e)->justify
#define entity_text_lead(e)	(e)->line_spacing
#define entity_text_spread(e)	(e)->letter_spacing
#define entity_text_t(e)	(e)->t
#define entity_text_txtsize(e)	(e)->txt_size
#define entity_text_filled(e)	(e)->filled
#define entity_text_pen(e)	(e)->pen

typedef struct {		// fixme -- add cabt/ffae flag
  entity_type_e	type;
  gdouble	 x;
  gdouble	 y;
  gdouble	 a;
  gdouble	 b;
  gdouble	 t;
  gdouble	 start;
  gdouble	 stop;
  gboolean	 negative;
  gboolean	 fill;
  pen_s		*pen;
} entity_ellipse_s;
#define entity_ellipse_type(e)		(e)->type
#define entity_ellipse_x(e)		(e)->x
#define entity_ellipse_y(e)		(e)->y
#define entity_ellipse_a(e)		(e)->a
#define entity_ellipse_b(e)		(e)->b
#define entity_ellipse_t(e)		(e)->t
#define entity_ellipse_start(e)		(e)->start
#define entity_ellipse_stop(e)		(e)->stop
#define entity_ellipse_negative(e)	(e)->negative
#define entity_ellipse_fill(e)		(e)->fill
#define entity_ellipse_pen(e)		(e)->pen

typedef struct {
  entity_type_e	 type;
  cairo_matrix_t *tf;
  gboolean unset;
} entity_transform_s;
#define entity_tf_type(e)	(e)->type
#define entity_tf_matrix(e)	(e)->tf
#define entity_tf_xx(e)		(e)->tf->xx
#define entity_tf_yx(e)		(e)->tf->yx
#define entity_tf_xy(e)		(e)->tf->xy
#define entity_tf_yy(e)		(e)->tf->yy
#define entity_tf_x0(e)		(e)->tf->x0
#define entity_tf_y0(e)		(e)->tf->y0
#define entity_tf_unset(e)	(e)->unset

#if 0
typedef union {
  entity_none_s    *entity_none;
  entity_circle_s  *entity_circle;
  entity_ellipse_s *entity_ellipse;
} entity_u;
#define entity_none(u)		(u).entity_none
#define entity_circle(u)	(u).entity_circle
#define entity_ellipse(u)	(u).entity_ellipse
#endif
#define entity_type(u)		entity_none_type ((entity_none_s *)u)


  
typedef struct {
  gchar		*name;
  environment_s	*environment;
  GList		*entities;
  GList		*transients;
  GtkTreeIter	*iter;
  void		*py_local_dict;		// actually a PyObject
  void		*project;		// actually project_s *
  gint		 tool_state;		// for private use by tools
  void		*tool_entity;		// for private use by tools
  point_s	 current_point;
  GtkWidget	*window;
} sheet_s;		// add more stuff later
#define sheet_name(s)		(s)->name
#define sheet_environment(s)	(s)->environment
#define sheet_entities(s)	(s)->entities
#define sheet_transients(s)	(s)->transients
#define sheet_iter(s)	        (s)->iter
#define sheet_pydict(s)		(s)->py_local_dict
#define sheet_project(s)	(s)->project
#define sheet_window(s)		(s)->window
#define sheet_tool_state(s)	(s)->tool_state
#define sheet_tool_entity(s)	(s)->tool_entity
#define sheet_current_point(s)	(s)->current_point
#define TOOL_IDLE	0

typedef void (*button_f)(GdkEvent *event, sheet_s *sheet,
			 gdouble px, gdouble py);

typedef struct {
  const gchar 	*image;
  button_f 	 bf;
  gboolean	 active;
  const gchar	*tip;
} button_s;
#define button_image(b)		(b)->image
#define button_function(b)	(b)->bf
#define button_active(b)	(b)->active
#define button_tip(b)		(b)->tip

typedef struct {
  const gchar *name;
  GtkTreeStore	*sheets;
  GtkTreeIter	*current_iter;
  environment_s	*environment;
} project_s;		// add more stuff later
#define project_name(p)   		(p)->name
#define project_sheets(p) 		(p)->sheets
#define project_current_iter(p)		(p)->current_iter
#define project_environment(p) 		(p)->environment

enum {
  PROJECT_STRUCT_COL,
  PROJECT_COL_COUNT
};

enum {
  SHEET_STRUCT_COL,
  SHEET_COL_COUNT
};

typedef struct {
  gchar *path;
  gchar *name;
  gchar *desc;
  GdkPixbuf *icon;
  gboolean open;
} plugin_s;
#define plugin_path(p)	(p)->path
#define plugin_name(p)	(p)->name
#define plugin_desc(p)	(p)->desc
#define plugin_icon(p)	(p)->icon
#define plugin_open(p)	(p)->open

typedef enum {
  LOG_NORMAL,
  LOG_PYTHON_ERROR,
  LOG_GFPY_ERROR,
  LOG_GFIG_ERROR,
  LOG_NR_TAGS
} log_level_e;

typedef enum {
  NOTIFY_MAP,
  NOTIFY_TOOL_STOP
} notify_e;

#define STATE_UNSHIFTED		0x0
#define STATE_SHIFT		0x1
#define STATE_CONTROL		0x4
#define STATE_CONTROL_SHIFT	0x5
#define STATE_ALT		0x8
#define STATE_ALT_SHIFT		0x9
#define STATE_ALT_CONTROL	0xc
#define STATE_ALT_CONTROL_SHIFT	0xd

environment_s *copy_environment (environment_s *source_environment);
void set_active_sheet (sheet_s *sheet);
sheet_s *get_active_sheet ();
void log_string (log_level_e log_level, sheet_s *sheet, gchar *str);
gpointer gfig_try_malloc0 (gsize n_bytes);
GtkListStore *get_projects (void);
environment_s *get_global_environment (void);
project_s *initialise_project (const gchar *name, gchar *script);
project_s *create_project (const gchar *name);
GtkTreeIter *append_sheet (project_s *project, GtkTreeIter *parent,
			   gchar *name, sheet_s **new_sheet_p);
void notify_projects (notify_e type);
button_f return_current_tool (void);
void notify_key_set (gint etype);
void set_key (const gchar *primary, const gchar *middle,
	      const gchar *secondary);
void set_tsf (GtkWidget *tsf);
#ifdef USE_SPC
void set_pen_controls (pen_s *pen);
#endif
void set_prompt (const gchar *prompt);
#endif /* GF_H */
