#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <Python.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>



#include "gf.h"
#include "view.h"
#include "select_pen.h"
#include "entities.h"
#include "intersects.h"
#include "css_colour_table.h"
#include "css_colours.h"
#include "utilities.h"
#include "fallbacks.h"
#include "python.h"

PyObject *global_dict;

#define SHEET_NAME "__sheet__"

typedef enum {
  METHOD_BUILD,
  METHOD_APPEND,
} method_e;

// fixme -- polar coords?


static const gchar group_name[]       = {"gfig.Group"};
static const gchar transform_name[]   = {"gfig.Transform"};
static const gchar scale_name[]       = {"gfig.Scale"};
static const gchar translate_name[]   = {"gfig.Translate"};
static const gchar rotate_name[]      = {"gfig.Rotate"};
static const gchar pen_name[]         = {"gfig.Pen"};
static const gchar point_name[]       = {"gfig.Point"};
static const gchar circle_name[]      = {"gfig.Circle"};
static const gchar text_name[]        = {"gfig.Text"};
static const gchar polyline_name[]    = {"gfig.Polyline"};
static const gchar ellipsecabt_name[] = {"gfig.EllipseCABT"};
static const gchar ellipseffae_name[] = {"gfig.EllipseFFAE"};

static void *method_circle (method_e method, PyObject *self,
			    PyObject *pArgs, PyObject *keywds);

static void *method_text (method_e method, PyObject *self,
			    PyObject *pArgs, PyObject *keywds);

static void *method_polyline (method_e method, PyObject *self,
			    PyObject *pArgs, PyObject *keywds);

static void *method_ellipsecabt (method_e method, PyObject *self,
			    PyObject *pArgs, PyObject *keywds);

static void *method_ellipseffae (method_e method, PyObject *self,
			    PyObject *pArgs, PyObject *keywds);

static void *method_draw (method_e method, PyObject *self,
			  PyObject *pArgs, PyObject *keywds);


/*************************** custom types support *****************/

// https://docs.python.org/3.3/extending/newtypes.html
// https://docs.python.org/3.3/c-api/typeobj.html

typedef struct {
  PyObject_HEAD;
  void *entity;
} gfig_GenericObject;

static gboolean is_gfig_entity  (PyObject *xobj);
static gboolean is_gfig_point (PyObject *xobj);
static gboolean is_gfig_pen   (PyObject *xobj);
static gboolean is_gfig_transform (PyObject *xobj);

static void
generic_dealloc (PyObject *self)
{
  if (self) {
    gfig_GenericObject *gobj = (gfig_GenericObject *)self;
    void *entity = gobj->entity;
    if (entity) delete_entities (entity);
    Py_TYPE(self)->tp_free((PyObject*)self);
  }
}

/********************** utilities ******************/

static sheet_s*
get_sheet ()
{
  sheet_s *sheet = NULL;
  
  PyObject *local_dict = PyEval_GetLocals();
  if (local_dict) {
    PyObject *sheetobj = PyDict_GetItemString (local_dict, SHEET_NAME);
    if (sheetobj) sheet = PyLong_AsVoidPtr(sheetobj);
  }

  PyObject *frame = PyRun_String ("inspect.currentframe().f_back",
				  Py_eval_input, global_dict, local_dict);
  while (frame && (frame != Py_None) && !sheet) {
    PyObject *ldict = PyObject_GetAttrString (frame, "f_locals");
    if (ldict) {
      PyObject *sheetobj = PyDict_GetItemString (ldict, SHEET_NAME);
      if (sheetobj) {
	sheet = PyLong_AsVoidPtr(sheetobj);
      }
      else frame = PyObject_GetAttrString (frame, "f_back");
    }
  }
  return sheet;
}

static gdouble
get_double (PyObject *obj)
{
  gdouble rc = NAN;
  if (PyLong_Check (obj)) rc = PyLong_AsDouble (obj);
  else if (PyFloat_Check (obj)) rc = PyFloat_AsDouble (obj);
  else log_string (LOG_GFPY_ERROR, get_sheet (),
		   _ ("get_double: Non-numeric argument.\n"));
  return rc;
}

static gboolean
get_boolean (PyObject *obj)
{
  gboolean rc = FALSE;
  if (PyLong_Check (obj)) rc = PyLong_AsLong (obj) ? TRUE : FALSE;
  else if (PyBool_Check (obj))
    rc = (obj == Py_True) ? TRUE : FALSE;
  else log_string (LOG_GFPY_ERROR, get_sheet (),
		   _ ("get_boolean: Non-boolean argument.\n"));
  return rc;
}

static void
circle_parse_kwds (PyObject *keywds, gboolean *filled, gdouble *start,
		   gdouble *stop, gboolean *negative, pen_s **pen)
{
  PyObject *fobj = NULL;
  if (keywds) {
    fobj = PyDict_GetItemString(keywds, "filled");
    if (fobj && filled) *filled = get_boolean (fobj);
    
    fobj = PyDict_GetItemString(keywds, "start");
    if (fobj && start) *start = get_double (fobj);
    
    fobj = PyDict_GetItemString(keywds, "stop");
    if (fobj && stop) *stop = get_double (fobj);

    fobj = PyDict_GetItemString(keywds, "negative");
    if (fobj && negative) *negative = get_boolean (fobj);

    fobj = PyDict_GetItemString(keywds, "pen");
    if (fobj && pen) {
      if (is_gfig_pen (fobj)) {
	if (*pen) {
	  if (pen_colour (*pen)) gdk_rgba_free (pen_colour (*pen));
	  if (pen_colour_name (*pen)) g_free (pen_colour_name (*pen));
	  g_free (*pen);
	}
	gfig_GenericObject *cobj = (gfig_GenericObject *)fobj;
	pen_s *e_pen = cobj->entity;
	*pen = copy_pen (e_pen);
      }
    }
  }
}

static point_s **
build_point_vec (gint *cnt_p, PyObject *obj)
{
  point_s **vec = NULL;
  if (PyList_Check (obj)) {	// list of point vals
    Py_ssize_t cnt = PyList_Size (obj);
    if (cnt_p) *cnt_p = cnt;
    vec = gfig_try_malloc0 (cnt * sizeof (point_s *));
    for (int i = 0; i < cnt; i++) {
      PyObject* lobj = PyList_GetItem (obj, i);
      gfig_GenericObject *cobj = (gfig_GenericObject *)lobj;
      vec[i] = cobj->entity;
    }
  }
  else {
    gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
    if (cnt_p) *cnt_p = 1;
    vec = gfig_try_malloc0 (sizeof(point_s *));
    vec[0] = cobj->entity;
  }
  return vec;
}

static gdouble *
build_vec (gint *cnt_p, PyObject *obj)
{
  gdouble *vec = NULL;
  if (PyList_Check (obj)) {	// list of x vals
    Py_ssize_t cnt = PyList_Size (obj);
    if (cnt_p) *cnt_p = cnt;
    vec = gfig_try_malloc0 (cnt * sizeof (gdouble));
    for (int i = 0; i < cnt; i++) {
      PyObject* lobj = PyList_GetItem (obj, i);
      gdouble val = NAN;
      val = get_double (lobj);
      if (!isnan (val)) vec[i] = val;
      else {
	g_free (vec);
	vec = NULL;
	if (cnt_p) *cnt_p = -1;
	break;
      }
    }
  }
  else {			// single x val
    gdouble val = NAN;
    val = get_double (obj);
    if (!isnan (val)) {
      vec = gfig_try_malloc0 (sizeof (gdouble));
      vec[0] = val;
      if (cnt_p) *cnt_p = 1;
    }
    else {
      vec = NULL;
      if (cnt_p) *cnt_p = -1;
    }
  }
  return vec;
}

/************************ misc fcns **************************/

#if 0
static PyObject *
gfig_test (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  PyObject *main_module = PyImport_AddModule ("__main__");
  global_dict = PyModule_GetDict (main_module);
  PyObject *local_dict = PyEval_GetLocals();

  PyObject *rc =
    PyRun_String ("inspect.currentframe().f_back.f_locals\n",
		  Py_eval_input, global_dict, local_dict);
  return rc;
}
#endif

static PyObject *
gfig_clear (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  sheet_s  *sheet = get_sheet ();
  clear_entities (&sheet_entities (sheet));
  force_redraw (sheet);
  return Py_None;
}

/************************ pen *******************/

static PyObject *
pen_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  pen_s *point = cobj->entity;
  if (point) {
    gchar *str =	// fixme
      g_strdup_printf ("%s (width = %g, colour = [%g, %g, %g, %g])\n",
		       pen_name, pen_lw (point),
		       pen_colour_red (point),
		       pen_colour_green (point),
		       pen_colour_blue (point),
		       pen_colour_alpha (point));
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Pen: Invalid value.\n"));
}

static PyObject *
pen_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  pen_s *point = cobj->entity;
  if (point) {
    gchar *str =	// fixme
      g_strdup_printf ("%s (width = %g, colour = [%g, %g, %g, %g])\n",
		       pen_name, pen_lw (point),
		       pen_colour_red (point),
		       pen_colour_green (point),
		       pen_colour_blue (point),
		       pen_colour_alpha (point));
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Pen: Invalid value.\n"));
}

static int
pen_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  int rc = -1;

  if (kwds) {	// lot of fixme
    GdkRGBA rgba = {
      FALLBACK_PEN_COLOUR_RED,
      FALLBACK_PEN_COLOUR_GREEN,
      FALLBACK_PEN_COLOUR_BLUE,
      FALLBACK_PEN_COLOUR_ALPHA,
    };

    pen_s *pen = gfig_try_malloc0 (sizeof(pen_s));
    pen_colour_name (pen) = g_strdup (FALLBACK_PEN_COLOUR_NAME);
    pen_lw_std_idx (pen) = FALLBACK_PEN_LW_IDX;
    // fixme add linestyle kwd
    pen_line_style (pen) = FALLBACK_PEN_LS_IDX;
    pen_lw (pen) = get_lw_from_idx (pen_lw_std_idx (pen));
    
    PyObject *fobj;

    fobj = PyDict_GetItemString(kwds, "width");
    if (fobj) pen_lw (pen) = get_double (fobj);

    fobj = PyDict_GetItemString(kwds, "colour");
    if (fobj) {
      if (PyUnicode_Check (fobj)) {
	Py_ssize_t size;
	if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
	pen_colour_name (pen) =
	  g_strdup (PyUnicode_AsUTF8AndSize (fobj, &size));
	if (pen_colour_name (pen)) {
	  find_css_colour_rgba (pen_colour_name (pen), &rgba);
	}
      }
      else  if (PyList_Check (fobj)) {
	Py_ssize_t cnt = PyList_Size (fobj);
	if (pen_colour_name (pen)) {
	  g_free (pen_colour_name (pen));
	  pen_colour_name (pen) = NULL;
	}
	for (gint i = 0; i < cnt; i++) {
	  gdouble col;
	  PyObject* lobj = PyList_GetItem (fobj, i);
	  col = get_double (lobj);
	  if (!isnan (col)) {
	    switch(i) {
	    case 0: rgba.red	= col;	break;
	    case 1: rgba.green	= col;	break;
	    case 2: rgba.blue	= col;	break;
	    case 3: rgba.alpha	= col;	break;
	    }
	  }
	}
      }
    }
    if (!pen_colour (pen)) pen_colour (pen) = gdk_rgba_copy (&rgba);
    gfig_GenericObject *cobj = (gfig_GenericObject *)self;
    cobj->entity = pen;
    rc = 0;
  }

  if (rc == -1)  PyErr_BadArgument();
  return rc;
}

static void
pen_dealloc (PyObject *self)
{
  if (self) {
    gfig_GenericObject *gobj = (gfig_GenericObject *)self;
    pen_s *pen = gobj->entity;
    if (pen) {
      if (pen_colour (pen)) gdk_rgba_free (pen_colour (pen));
      if (pen_colour_name (pen)) g_free (pen_colour_name (pen));
      g_free (pen);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
  }
}

static PyTypeObject gfig_Pen = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= pen_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= pen_dealloc,
  .tp_repr	= pen_repr,
  .tp_str	= pen_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig pen type",
  .tp_init	= pen_init,
};

/************************ point *******************/

static PyObject *
point_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  point_s *point = cobj->entity;
  if (point) {
    gchar *str =
      g_strdup_printf ("%s (%g,%g)\n",
		       point_name, point_x (point), point_y (point));
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Point: Invalid value.\n"));
}

static PyObject *
point_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  point_s *point = cobj->entity;
  if (point) {
    gchar *str =
      g_strdup_printf ("%s (%g,%g)\n",
		       point_name, point_x (point), point_y (point));
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Point: Invalid value.\n"));
}

static int
point_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  int rc = -1;
  if (PyTuple_Size(args) == 2) {
    PyObject *xobj = PyTuple_GetItem (args, 0);
    gdouble *xvec = NULL; gint xvcnt = -1;
    
    PyObject *yobj =PyTuple_GetItem (args, 1);
    gdouble *yvec = NULL; gint yvcnt = -1;

    xvec = build_vec (&xvcnt, xobj);
    yvec = build_vec (&yvcnt, yobj);

    if (xvec && yvec && xvcnt == 1 && yvcnt == 1) {
      point_s *point = gfig_try_malloc0 (sizeof(point_s));
      point_x (point) = xvec[0];
      point_y (point) = yvec[0];
      gfig_GenericObject *cobj = (gfig_GenericObject *)self;
      cobj->entity = point;
      g_free (xvec);
      g_free (yvec);
      rc = 0;
    }
  }
  if (rc == -1)  PyErr_BadArgument();
  return rc;
}

static void
point_dealloc (PyObject *self)
{
  if (self) {
    gfig_GenericObject *gobj = (gfig_GenericObject *)self;
    void *entity = gobj->entity;
    if (entity) g_free (entity);
    Py_TYPE(self)->tp_free((PyObject*)self);
  }
}

static PyTypeObject gfig_Point = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= point_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= point_dealloc,
  .tp_repr	= point_repr,
  .tp_str	= point_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig point type",
  .tp_init	= point_init,
};

/************************ transform *******************/

static PyObject *
transform_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  cairo_matrix_t *matrix = cobj->entity;
  if (matrix) {
    gchar *str =
      g_strdup_printf ("%s\n\t%g, %g\n\t%g, %g\n\t%g, %g",
		       transform_name,
		       matrix->xx, matrix->yx,
		       matrix->xy, matrix->yy,
		       matrix->x0, matrix->y0);
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Transform: Invalid value.\n"));
}

static PyObject *
transform_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  cairo_matrix_t *matrix = cobj->entity;
  if (matrix) {
    gchar *str =
      g_strdup_printf ("%s\n\t%g, %g\n\t%g, %g\n\t%g, %g",
		       transform_name,
		       matrix->xx, matrix->yx,
		       matrix->xy, matrix->yy,
		       matrix->x0, matrix->y0);
    PyObject *robj = PyUnicode_FromString (str);
    g_free (str);
    return robj;
  }
  else return PyUnicode_FromString (_ ("Transform: Invalid value.\n"));
}


static int
transform_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  int rc = -1;
  Py_ssize_t t_count = PyTuple_Size(args);
  g_print ("in tf\n");
  
  cairo_matrix_t *matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));

  if (t_count != 6) {
    cairo_matrix_init_identity (matrix);
    rc = 0;
  }
  else {
    g_print ("parsing\n");
    if (PyArg_ParseTuple (args, "dddddd",
			  &matrix->xx,
			  &matrix->yx,
			  &matrix->xy,
			  &matrix->yy,
			  &matrix->x0,
			  &matrix->y0))
      rc = 0;
  }
  if (rc == 0) {
    gfig_GenericObject *cobj = (gfig_GenericObject *)self;
    cobj->entity = matrix;
  }
  else {
    PyErr_BadArgument();
    if (matrix) g_free (matrix);
  }
  return rc;
}

static void
transform_dealloc (PyObject *self)
{
  if (self) {
    gfig_GenericObject *gobj = (gfig_GenericObject *)self;
    void *entity = gobj->entity;
    if (entity) g_free (entity);
    Py_TYPE(self)->tp_free((PyObject*)self);
  }
}

static PyTypeObject gfig_Transform = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= transform_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= transform_dealloc,
  .tp_repr	= transform_repr,
  .tp_str	= transform_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig transform type",
  .tp_init	= transform_init,
};

static int
scale_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gdouble sx, sy;
  PyObject *tf = NULL;
  int rc = -1;
  cairo_matrix_t *matrix = NULL;
  
  if (PyArg_ParseTuple (args, "dd|O", &sx, &sy, &tf)) {
    matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
    if (tf) {
      gfig_GenericObject *cobj = (gfig_GenericObject *)tf;
      memmove (matrix, cobj->entity, sizeof(cairo_matrix_t));
      cairo_matrix_scale (matrix, sx, sy);
    }
    else cairo_matrix_init_scale (matrix, sx, sy);
    rc = 0;
  }
  
  if (rc == 0) {
    gfig_GenericObject *cobj = (gfig_GenericObject *)self;
    cobj->entity = matrix;
  }
  else {
    PyErr_BadArgument();
    if (matrix) g_free (matrix);
  }
  return rc;
}

static PyTypeObject gfig_Scale = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= scale_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= transform_dealloc,
  .tp_repr	= transform_repr,
  .tp_str	= transform_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig scale type",
  .tp_init	= scale_init,
};

static int
translate_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gdouble tx, ty;
  PyObject *tf = NULL;
  int rc = -1;
  cairo_matrix_t *matrix = NULL;
  
  if (PyArg_ParseTuple (args, "dd|O", &tx, &ty, &tf)) {
    matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
    if (tf) {
      gfig_GenericObject *cobj = (gfig_GenericObject *)tf;
      memmove (matrix, cobj->entity, sizeof(cairo_matrix_t));
      cairo_matrix_translate (matrix, tx, ty);
    }
    else cairo_matrix_init_translate (matrix, tx, ty);
    rc = 0;
  }
  
  if (rc == 0) {
    gfig_GenericObject *cobj = (gfig_GenericObject *)self;
    cobj->entity = matrix;
  }
  else {
    PyErr_BadArgument();
    if (matrix) g_free (matrix);
  }
  return rc;
}

static PyTypeObject gfig_Translate = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= translate_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= transform_dealloc,
  .tp_repr	= transform_repr,
  .tp_str	= transform_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig translate type",
  .tp_init	= translate_init,
};

static int
rotate_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gdouble rr;
  PyObject *tf = NULL;
  int rc = -1;
  cairo_matrix_t *matrix = NULL;
  
  if (PyArg_ParseTuple (args, "d|O", &rr, &tf)) {
    matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
    if (tf) {
      gfig_GenericObject *cobj = (gfig_GenericObject *)tf;
      memmove (matrix, cobj->entity, sizeof(cairo_matrix_t));
      cairo_matrix_rotate (matrix, rr);
    }
    else cairo_matrix_init_rotate (matrix, rr);
    rc = 0;
  }
  
  if (rc == 0) {
    gfig_GenericObject *cobj = (gfig_GenericObject *)self;
    cobj->entity = matrix;
  }
  else {
    PyErr_BadArgument();
    if (matrix) g_free (matrix);
  }
  return rc;
}

static PyTypeObject gfig_Rotate = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= rotate_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= transform_dealloc,
  .tp_repr	= transform_repr,
  .tp_str	= transform_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig rotate type",
  .tp_init	= rotate_init,
};

static PyObject *
gfig_catenate (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  PyObject *rc = Py_False;

  Py_ssize_t t_count = PyTuple_Size(pArgs);

  cairo_matrix_t *matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
  cairo_matrix_init_identity (matrix);

  for (gint i = 0; i < t_count; i++) {
    PyObject *tobj = PyTuple_GetItem (pArgs, i);
    if (tobj) {
      gfig_GenericObject *cobj = (gfig_GenericObject *)tobj;
      cairo_matrix_t *om = cobj->entity;
      cairo_matrix_multiply (matrix, om, matrix);
    }
  }
  
  
  PyObject *pobj = gfig_try_malloc0 (sizeof(PyObject));
  gfig_GenericObject *obj = (gfig_GenericObject *)pobj;
  // pobj->ob_refcnt = 1; will be inced below
  pobj->ob_type = &gfig_Transform;
  obj->entity = matrix;
  obj->ob_base.ob_refcnt = 1;
  rc = pobj;
  
  Py_INCREF (rc);
  return rc;
}

static PyObject *
gfig_invert (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  PyObject *tf = NULL;
  PyObject *rc = Py_False;

  sheet_s *sheet = get_sheet ();
  if (PyArg_ParseTuple (pArgs, "O", &tf, &tf)) {
    cairo_matrix_t *matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
    if (tf) {
      gfig_GenericObject *cobj = (gfig_GenericObject *)tf;
      memmove (matrix, cobj->entity, sizeof(cairo_matrix_t));
      cairo_status_t crc = cairo_matrix_invert (matrix);

      if (crc == CAIRO_STATUS_SUCCESS) {
	PyObject *pobj = gfig_try_malloc0 (sizeof(PyObject));
	gfig_GenericObject *obj = (gfig_GenericObject *)pobj;
	// pobj->ob_refcnt = 1; will be inced below
	pobj->ob_type = &gfig_Transform;
	obj->entity = matrix;
	obj->ob_base.ob_refcnt = 1;
	rc = pobj;
      }
      else log_string (LOG_GFPY_ERROR, sheet,
		       _ ("Invert: Degenerate matrix.\n"));
    }
  }
  else log_string (LOG_GFPY_ERROR, sheet,
		   _ ("Invert: Missing parameter.\n"));
  Py_INCREF (rc);
  return rc;
}

/*********************** draw *****************/

static PyObject *
group_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  //  entity_group_s *group = cobj->entity;
  gchar *str = g_strdup_printf ("%s\n", group_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
group_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  //  entity_group_s *group = cobj->entity;
  gchar *str = g_strdup_printf ("%s\n", group_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static int
group_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_draw (METHOD_BUILD, self, args, kwds);
  
  return 0;
}

static PyTypeObject gfig_Group = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= group_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= group_repr,
  .tp_str	= group_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig group type",
  .tp_init	= group_init,
};

static void *
method_draw (method_e method, PyObject *self, PyObject *pArgs,
	     PyObject *keywds)
{
  PyObject *rc = Py_False;
  sheet_s *sheet = get_sheet ();
  GList *entities = NULL;
  cairo_matrix_t *matrix = NULL;
  point_s *centre = NULL;
  Py_ssize_t t_count = PyTuple_Size(pArgs);
   
  if (t_count > 0) {
    if (keywds) {
      PyObject *tf = PyDict_GetItemString(keywds, "transform");
      if (!tf) tf = PyDict_GetItemString(keywds, "tf");
      if (tf) {
	if (is_gfig_transform (tf)) {
	  matrix = gfig_try_malloc0 (sizeof(cairo_matrix_t));
	  gfig_GenericObject *cobj = (gfig_GenericObject *)tf;
	  memmove (matrix, cobj->entity, sizeof(cairo_matrix_t));
	}
	else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("Draw: Invalid transform.\n"));
      }
      
      PyObject *cr = PyDict_GetItemString(keywds, "centre");
      if (!cr) cr = PyDict_GetItemString(keywds, "center");
      if (cr) {
	if (is_gfig_point (cr)) {
	  gfig_GenericObject *cobj = (gfig_GenericObject *)cr;
	  centre = copy_point (cobj->entity);
	}
	else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("Draw: Invalid point.\n"));
      }
    }
    
    for (gint i = 0; i < t_count; i++) {
      PyObject *tobj = PyTuple_GetItem (pArgs, i);
      if (is_gfig_entity (tobj)) {
	rc = Py_True;
	gfig_GenericObject *gobj = (gfig_GenericObject *)tobj;
	void *entity = gobj->entity;
	 entities = g_list_append (entities, entity);
      }
    }
  }

  if (entities) {
    if (method == METHOD_APPEND) {
      entity_append_group (sheet, matrix, centre, entities);
      force_redraw (sheet);
    }
    else return (void **) entity_build_group (sheet, matrix,
					      centre, entities);
  }
  Py_INCREF (rc);
  return rc;
}

static PyObject *
gfig_draw (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return (PyObject *)method_draw (METHOD_APPEND, self, pArgs, keywds);
}

/* converts degrees to radians, assumes arg is degrees */
static PyObject*
gfig_degrees (PyObject* self, PyObject* pArgs)
{
  PyObject *angles =PyTuple_GetItem (pArgs, 0);
  gdouble *avec = NULL; gint avcnt = -1;

  avec = build_vec (&avcnt, angles);

  for (int i = 0; i < avcnt; i++) 
    avec[i] *= G_PI / 180.0;

  if (avcnt == 1) {
    return PyFloat_FromDouble (avec[0]);
  }
  else if (avcnt > 1) {
    PyObject *list = PyList_New (0);
    for (int i = 0; i < avcnt; i++)
      PyList_Append (list, PyFloat_FromDouble (avec[i]));
    return list;
  }
  else {
    PyErr_SetString (PyExc_TypeError, "Argument not numeric");
    return NULL;
  }
}

static PyObject*
gfig_iterSIC (PyObject* self, PyObject* pArgs)
{
  PyObject *start;
  PyObject *incr;
  guint count;

  if (PyArg_ParseTuple (pArgs, "OOI", &start, &incr, &count)) {
    // 1 ==> PyLong_Type
    // (2,3) ==> PyTuple_Type
    if (PyTuple_Check (start) && PyTuple_Check (incr)) {
      if (PyTuple_Size (start) == 2 && PyTuple_Size (incr) == 2) {
	gdouble x, y, ix, iy;
	
	if (PyArg_ParseTuple (start, "dd", &x, &y)) {
	  if (PyArg_ParseTuple (incr, "dd", &ix, &iy)) {
	    
	    PyObject * list = PyList_New (0); 

	    for (int i = 0; i < count; i++, x += ix, y += iy) {
	      PyObject *tuple =  PyTuple_New (2);
	      PyTuple_SetItem (tuple, 0, PyFloat_FromDouble (x));
	      PyTuple_SetItem (tuple, 1, PyFloat_FromDouble (y));
	      PyList_Append (list, tuple);
	    }
	    return list;
	  }
	}
      }
    }
    else if (!PyTuple_Check (start) && !PyTuple_Check (incr)) {
      gdouble st, ic; 

      PyObject * list = PyList_New (0); 
      
      st = get_double (start);
      ic = get_double (incr);
      if (!isnan (st) && !isnan (ic)) {
	for (int i = 0; i < count; i++, st += ic)
	  PyList_Append (list, PyFloat_FromDouble (st));
      }
      return list;
    }
  }
  Py_INCREF(Py_None);
  return Py_None;
}


/********************************** intersect ********************/

static void
append_intersect_point (gpointer data, gpointer user_data)
{
  point_s *pt = data;
  PyObject *list = user_data;

  PyObject *pobj = gfig_try_malloc0 (sizeof(PyObject));
  gfig_GenericObject *obj = (gfig_GenericObject *)pobj;
  pobj->ob_refcnt = 1;
  pobj->ob_type = &gfig_Point;
  obj->entity = pt;
  obj->ob_base.ob_refcnt = 1;
  PyList_Append (list, pobj);
}

static PyObject *
gfig_intersect (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  GList *points = NULL;
  //sheet_s *sheet = get_sheet ();
  //environment_s * env = sheet_environment (sheet);
  if (PyTuple_Size (pArgs) == 2) {
    PyObject *o0 = PyTuple_GetItem (pArgs, 0);
    PyObject *o1 = PyTuple_GetItem (pArgs, 1);
    if (is_gfig_entity (o0) && is_gfig_entity (o1)) {
      gfig_GenericObject *p0 = (gfig_GenericObject *)o0;
      gfig_GenericObject *p1 = (gfig_GenericObject *)o1;
      void *e0 = p0->entity;
      void *e1 = p1->entity;
      points = do_intersect (e0, e1);
    }
  }
  if (points) {
    PyObject *list = PyList_New (0);
    g_list_foreach (points, append_intersect_point, list);
    return list;
  }
  else {
    Py_INCREF (Py_None);
    return Py_None;
  }
}


/********************************** pen fcns ********************/

static PyObject *
gfig_get_penwidthidx (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  sheet_s *sheet = get_sheet ();
  environment_s * env = sheet_environment (sheet);
  pen_s *pen = environment_pen (env);
  //  gdouble width = environment_win_wid (env);
  gint width = pen_lw_std_idx (pen);
  return PyLong_FromLong ((long)width);
}

static PyObject *
gfig_set_penwidthidx (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  gint penidx;
  PyObject *rc = Py_False;

  sheet_s *sheet = get_sheet ();
  if (PyArg_ParseTuple (pArgs, "i", &penidx)) {
    if (penidx >= STANDARD_LINE_WIDTH_0 &&
	penidx <= STANDARD_LINE_WIDTH_8) {
      environment_s * env = sheet_environment (sheet);
      pen_s *pen = environment_pen (env);
      pen_lw_std_idx (pen) = penidx;
      pen_lw (pen) = get_lw_from_idx (penidx);
      rc = Py_True;
    }
    else log_string (LOG_GFPY_ERROR, sheet,
		     _ ("Setpenwidthidx: Invalid pen width index.\n"));
  }
  else log_string (LOG_GFPY_ERROR, sheet,
		   _ ("Setpenwidthidx: Missing parameter.\n"));
  Py_INCREF (rc);
  return rc;
}

static PyObject *
gfig_set_penwidth (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  gdouble penwidth;
  PyObject *rc = Py_False;

  sheet_s  *sheet = get_sheet ();
  if (PyArg_ParseTuple (pArgs, "d", &penwidth)) {
    environment_s * env = sheet_environment (sheet);
    pen_s *pen = environment_pen (env);
    pen_lw_std_idx (pen) = STANDARD_LINE_WIDTH_CUSTOM;
    pen_lw (pen) = penwidth;
    rc = Py_True;
  }
  else log_string (LOG_GFPY_ERROR, sheet,
		   _ ("Setpenwidth: Missing parameter.\n"));
  Py_INCREF (rc);
  return rc;
}

static PyObject *
gfig_set_pencolour (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  PyObject *obj = PyTuple_GetItem(pArgs, 0);
  PyObject *rc = Py_False;
  
  sheet_s  *sheet = get_sheet ();
  if (PyFloat_Check (obj)) {
    gdouble red, green , blue, alpha = 1.0;
    if (PyArg_ParseTuple (pArgs, "ddd|d", &red, &green, &blue, &alpha)) {
      environment_s * env = sheet_environment (sheet);
      pen_s *pen = environment_pen (env);
      pen_colour_red (pen)	= red;
      pen_colour_green (pen)	= green;
      pen_colour_blue (pen)	= blue;
      pen_colour_alpha (pen)	= alpha;
      rc = Py_True;
    }
    else log_string (LOG_GFPY_ERROR, sheet,
		     _ ("Setpencolour: Invalid colour specification.\n"));
  }
  else if (PyUnicode_Check(obj)) {
    char *cname;
    if (PyArg_ParseTuple (pArgs, "s", &cname)) {
      GdkRGBA rgba;
      css_colours_s *col = find_css_colour_rgba (cname, &rgba);
      if (col) {
	environment_s * env = sheet_environment (sheet);
	pen_s *pen = environment_pen (env);
	pen_colour_red (pen)	= rgba.red;
	pen_colour_green (pen)	= rgba.green;
	pen_colour_blue (pen)	= rgba.blue;
	pen_colour_alpha (pen)	= rgba.alpha;	
	rc = Py_True;
      }
      else log_string (LOG_GFPY_ERROR, sheet,
		       _ ("Setpencolour: Invalid colour specification.\n"));
    }
    else log_string (LOG_GFPY_ERROR, sheet,
		     _ ("Setpencolour: Incompatible parameter.\n"));
  }
  else log_string (LOG_GFPY_ERROR, sheet,
		   _ ("Setpencolour: Missing or incompatible parameter.\n"));
  Py_INCREF (rc);
  return rc;
}


/************************** circles ***********************/

static PyObject *
circle_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_circle_s *circle = cobj->entity;
  gchar *str = circle ?
    g_strdup_printf ("%s (%g, %g %g, filled = %s)\n",
		     circle_name,
		     entity_circle_x (circle),
		     entity_circle_y (circle),
		     entity_circle_r (circle),
		     entity_circle_fill (circle) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", circle_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
circle_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_circle_s *circle = cobj->entity;
  gchar *str = circle ? g_strdup_printf ("Circle at [%g %g] r = %g",
					 entity_circle_x (circle),
					 entity_circle_y (circle),
					 entity_circle_r (circle))
    : g_strdup ("Null circle");
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static int
circle_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_circle (METHOD_BUILD, self, args, kwds);
  
  return 0;
}


// /usr/include/python3.3m/object.h

static PyTypeObject gfig_Circle = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= circle_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= circle_repr,
  .tp_str	= circle_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig circle type",
  .tp_init	= circle_init,
};

/***
    fixme -- allow sic format:

                    centres
             ---- sic vector -------------  r
    gf_circ (gf_sic( (1,1.5), (.5, .7), 4), 1)
    
***/

static void *
point_circle (method_e method, PyObject *self,
	      PyObject *pArgs, PyObject *keywds)
{
  point_s **pvec = NULL; gint pvcnt = -1;
  gdouble *rvec = NULL; gint rvcnt = -1;
  gdouble start = 0.0;
  gdouble stop = 2.0 * G_PI;
  gboolean negative = FALSE;
  sheet_s *sheet = get_sheet ();
  PyObject *rc = Py_False;
  entity_circle_s *circle = NULL;
  pen_s *pen = NULL;

  gboolean filled = FALSE;

  circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);

  pvec = build_point_vec (&pvcnt, PyTuple_GetItem (pArgs, 0));
  rvec = build_vec (&rvcnt, PyTuple_GetItem (pArgs, 1));
  if (!pvec || !rvec) {
    if (pvec) g_free (pvec);
    if (rvec) g_free (rvec);
    log_string (LOG_GFPY_ERROR, sheet, _ ("Circle: Missing parameter.\n"));
  }
  else if (method == METHOD_BUILD &&
	   (pvcnt > 1 || rvcnt > 1)) {		// fixme free vecs?
    log_string (LOG_GFPY_ERROR, sheet,
		_ ("Circle: Range parameter not valid in this context.\n"));
  }
  else {
    gboolean didit = FALSE;
    for (int p = 0; p < pvcnt; p++) {
      gdouble xx = point_x (pvec[p]);
      gdouble yy = point_y (pvec[p]);
      for (int r = 0; r < rvcnt; r++) {
	if (rvec[r] > 0.0) {
	  if (method == METHOD_APPEND)
	    entity_append_circle (sheet, xx, yy, rvec[r], start,
				  stop, negative, filled, pen);
	  else
	    circle = entity_build_circle (sheet, xx, yy, rvec[r], start,
					  stop, negative, filled, pen);
	  didit = TRUE;
	}
	else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("Circle: Invalid negative radius.\n"));	
      }
    }
    if (pvec) g_free (pvec);
    if (rvec) g_free (rvec);
    if (method == METHOD_APPEND && didit) {
      rc = Py_True;
      force_redraw (sheet);
    }
  }
 
  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return (void *)rc;
  }
  else return (void *)circle;
}

static void *
method_circle (method_e method, PyObject *self,
	       PyObject *pArgs, PyObject *keywds)
{
  PyObject *rc = Py_False;
  entity_circle_s *circle = NULL;
  Py_ssize_t t_count = PyTuple_Size(pArgs);

  if (t_count >= 2) {
    gboolean is_pointy = FALSE;
    PyObject *pobj = PyTuple_GetItem (pArgs, 0);
    if (is_gfig_point (pobj)) is_pointy = TRUE;
    else {
      if (PyList_Check (pobj)) {
	if (PyList_Size (pobj) > 0) {
	  PyObject* lobj = PyList_GetItem (pobj, 0);
	  if (is_gfig_point (lobj)) is_pointy = TRUE;
	}
      }
    }
    if (is_pointy) return point_circle (method, self, pArgs, keywds);
  }
  
  sheet_s *sheet = get_sheet ();

  if (t_count >= 3) {
    gdouble *xvec = NULL; gint xvcnt = -1;
    gdouble *yvec = NULL; gint yvcnt = -1;
    gdouble *rvec = NULL; gint rvcnt = -1;
    gdouble start = 0.0;
    gdouble stop = 2.0 * G_PI;
    gboolean negative = FALSE;
    gboolean filled = FALSE;
    pen_s *pen = NULL;

    circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);
    xvec = build_vec (&xvcnt, PyTuple_GetItem (pArgs, 0));
    yvec = build_vec (&yvcnt, PyTuple_GetItem (pArgs, 1));
    rvec = build_vec (&rvcnt, PyTuple_GetItem (pArgs, 2));
    if (!xvec || !yvec || !rvec) {
      if (xvec) g_free (xvec);
      if (yvec) g_free (yvec);
      if (rvec) g_free (rvec);
      log_string (LOG_GFPY_ERROR, sheet, _ ("Circle: Missing parameter.\n"));
    }
    else if (method == METHOD_BUILD &&
	     (xvcnt > 1 || yvcnt > 1 || rvcnt > 1)) {
      log_string (LOG_GFPY_ERROR, sheet,
		  _ ("Circle: Range parameter not valid in this context.\n"));
    }
    else {
      gboolean didit = FALSE;
      for (int x = 0; x < xvcnt; x++) {
	for (int y = 0; y < yvcnt; y++) {
	  for (int r = 0; r < rvcnt; r++) {
	    if (rvec[r] > 0.0) {
	      if (method == METHOD_APPEND)
		entity_append_circle (sheet, xvec[x], yvec[y], rvec[r],
				      start, stop, negative, filled, pen);
	      else
		circle = 
		  entity_build_circle (sheet, xvec[x], yvec[y], rvec[r],
				       start, stop, negative, filled, pen);
	      didit = TRUE;
	    }
	    else log_string (LOG_GFPY_ERROR, sheet,
			     _ ("Circle: Invalid negative radius.\n"));
	  }
	}
      }
      if (xvec) g_free (xvec);
      if (yvec) g_free (yvec);
      if (rvec) g_free (rvec);
      if (method == METHOD_APPEND && didit) {
	rc = Py_True;
	force_redraw (sheet);
      }
    }
  }
  else log_string (LOG_GFPY_ERROR, sheet, _ ("Circle: Too few parameters.\n"));

  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return (void *)rc;
  }
  else return (void *)circle;
}

static PyObject *
gfig_draw_circle (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return (PyObject *)method_circle (METHOD_APPEND, self, pArgs, keywds);
}

/********************************* text **************************/

static PyObject *
text_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_text_s *text = cobj->entity;
  gchar *str = text ?
    g_strdup_printf ("%s (%g, %g %s)\n",
		     text_name,
		     entity_text_x (text),
		     entity_text_y (text),
		     entity_text_string (text))
    : g_strdup_printf ("%s ()\n", text_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
text_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_text_s *text = cobj->entity;
  gchar *str = text ?
    g_strdup_printf ("%s (%g, %g %s)\n",
		     text_name,
		     entity_text_x (text),
		     entity_text_y (text),
		     entity_text_string (text))
    : g_strdup_printf ("%s ()\n", text_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static int
text_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_text (METHOD_BUILD, self, args, kwds);
  
  return 0;
}

static PyTypeObject gfig_Text = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= text_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= text_repr,
  .tp_str	= text_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig text type",
  .tp_init	= text_init,
};

static void *
method_text (method_e method, PyObject *self,
	     PyObject *pArgs, PyObject *keywds)
{
  PyObject *rc          = Py_False;
  gdouble x		= NAN;
  gdouble y		= NAN;
  gchar *string		= NULL;
  gchar *font		= NULL;
  gdouble theta		= 0.0;
  Py_ssize_t size	= 0;
  gdouble txt_size	= 12.0;
  gboolean filled	= TRUE;
  gboolean justify	= FALSE;
  gint alignment	= PANGO_ALIGN_LEFT;
  gint spread		= 0;
  gint lead		= 0;
  entity_text_s *text   = NULL;
  pen_s *pen = NULL;
  
  
  
  Py_ssize_t t_count = PyTuple_Size(pArgs);
  if (t_count < 3) {
    PyErr_SetString (PyExc_RuntimeError, "Too few arguments");
    return NULL;
  }

  PyObject *xobj = PyTuple_GetItem (pArgs, 0);
  PyObject *yobj = PyTuple_GetItem (pArgs, 1);
  PyObject *sobj = PyTuple_GetItem (pArgs, 2);

  x = get_double (xobj);
  y = get_double (yobj);
  
  if (PyUnicode_Check (sobj))
    string = PyUnicode_AsUTF8AndSize (sobj, &size);
  
  if (t_count >= 4) {
    PyObject *obj = PyTuple_GetItem (pArgs, 3);
    theta = get_double (obj);
  }
  if (t_count >= 5) {
    PyObject *obj = PyTuple_GetItem (pArgs, 4);
    txt_size = get_double (obj);
  }
  if (t_count >= 6) {
    PyObject *obj = PyTuple_GetItem (pArgs, 5);
    filled = get_boolean (obj);
  }

  if (keywds) {
    PyObject *fobj;

    fobj = PyDict_GetItemString(keywds, "filled");
    if (fobj) filled = get_boolean (fobj);

    fobj = PyDict_GetItemString(keywds, "justify");
    if (fobj) justify = get_boolean (fobj);

    fobj = PyDict_GetItemString(keywds, "align");
    if (fobj) {
      if (PyLong_Check (fobj)) alignment = PyLong_AsLong (fobj);
    }

    fobj = PyDict_GetItemString(keywds, "spread");
    if (fobj) {
      if (PyLong_Check (fobj)) spread = PyLong_AsLong (fobj);
    }

    fobj = PyDict_GetItemString(keywds, "lead");
    if (fobj) {
      if (PyLong_Check (fobj)) lead = PyLong_AsLong (fobj);
    }
    
    fobj = PyDict_GetItemString(keywds, "txtsize");
    if (fobj) txt_size = get_double (fobj);
    
    fobj = PyDict_GetItemString(keywds, "pen");
    if (fobj) {
      if (is_gfig_pen (fobj)) {
	gfig_GenericObject *cobj = (gfig_GenericObject *)fobj;
	pen_s *e_pen = cobj->entity;
	pen = copy_pen (e_pen);
      }
    }
  }
  
  sheet_s  *sheet = get_sheet ();

  if (method == METHOD_APPEND) {
    entity_append_text (sheet, x, y, string, (int)size, theta, txt_size,
			filled, font, alignment, justify, spread, lead, pen);
    rc = Py_True;
    force_redraw (sheet);
  }
  else
    text = entity_build_text (sheet, x, y, string, (int)size, theta, txt_size,
			      filled, font, alignment, justify, spread, lead,
			      pen);
  
  if (method == METHOD_APPEND) { 
    Py_INCREF(rc);
    return rc;
  }
  else return (void *)text;
}

static PyObject *
gfig_draw_text (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return (PyObject *)method_text (METHOD_APPEND, self, pArgs, keywds);
}



/********************** ffae ellipse ***********************/


static PyObject *
ellipseffae_repr (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_ellipse_s *ellipse = cobj->entity;
  gchar *str = ellipse ?
    g_strdup_printf ("%s (%g, %g %g, %g, filled = %s)\n",
		     ellipseffae_name,
		     entity_ellipse_x (ellipse),
		     entity_ellipse_y (ellipse),
		     entity_ellipse_a (ellipse),
		     entity_ellipse_b (ellipse),
		     entity_ellipse_fill (ellipse) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", ellipseffae_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
ellipseffae_str (PyObject *obj)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_ellipse_s *ellipse = cobj->entity;
  gchar *str = ellipse ?
    g_strdup_printf ("%s (%g, %g %g, %g, filled = %s)\n",
		     ellipseffae_name,
		     entity_ellipse_x (ellipse),
		     entity_ellipse_y (ellipse),
		     entity_ellipse_a (ellipse),
		     entity_ellipse_b (ellipse),
		     entity_ellipse_fill (ellipse) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", ellipseffae_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}


static int
ellipseffae_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_ellipseffae (METHOD_BUILD, self, args, kwds);
  
  return 0;
}


// /usr/include/python3.3m/object.h

static PyTypeObject gfig_EllipseFFAE = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= ellipseffae_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= ellipseffae_repr,
  .tp_str	= ellipseffae_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig ellipseffae type",
  .tp_init	= ellipseffae_init,
};

/* ellipse specified by f0, f1, a, e  */
/*   c = (f0 + f2) / 2.0           */
/*   t = atan2 (dy, dx)            */
/*   e = sqrt ( (a^2 - b^2) / a^2) */
/*   e^2 = (a^2 - b^2) / a^2       */
/*   e^2 * a^2 = a^2 - b^2         */
/*   b^2 = a^2 - e^2 * a^2         */
/*   b^2 = a^2 * (1 - e^2)         */
/*   b   = a * sqrt (1 - e^2)      */

/***
    fixme -- allow sic format:

                      centres
                ---- foci -------------           
    gf_ellffae (gf_sic( (1,1.5), (.5, .7), 4),
                gf_sic( (1,1.5), (.5, .7), 4), 1, .5)
    
***/

static void *
point_ellipseffae (method_e method, PyObject *self,
		   PyObject *pArgs, PyObject *keywds)
{
  point_s **p0vec = NULL; gint p0vcnt = -1;
  point_s **p1vec = NULL; gint p1vcnt = -1;
  gdouble *avec = NULL; gint avcnt = -1;
  gdouble *evec = NULL; gint evcnt = -1;
  gdouble start = 0.0;
  gdouble stop = 2.0 * G_PI;
  gboolean negative = FALSE;
  sheet_s *sheet = get_sheet ();
  PyObject *rc = Py_False;
  entity_ellipse_s *ellipse = NULL;
  pen_s *pen = NULL;


  gboolean filled = FALSE;

  circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);

  p0vec = build_point_vec (&p0vcnt, PyTuple_GetItem (pArgs, 0));
  p1vec = build_point_vec (&p1vcnt, PyTuple_GetItem (pArgs, 1));
  avec = build_vec (&avcnt, PyTuple_GetItem (pArgs, 2));
  evec = build_vec (&evcnt, PyTuple_GetItem (pArgs, 3));
  if (!p0vec || !p1vec || !avec || ! evec) {
    if (p0vec) g_free (p0vec);
    if (p1vec) g_free (p1vec);
    if (avec) g_free (avec);
    if (evec) g_free (evec);
    log_string (LOG_GFPY_ERROR, sheet,
		_ ("EllipseFFAE: Missing parameter.\n"));
  }
  else if (method == METHOD_BUILD &&
	   (p0vcnt > 1 || p1vcnt > 0 || avcnt > 1 || evcnt > 0)) {
    // fixme free vecs?
    log_string (LOG_GFPY_ERROR, sheet,
	_ ("EllipseFFAE: Range parameter not valid in this context.\n"));
  }
  else {
    gboolean didit = FALSE;
    for (int p0 = 0; p0 < p0vcnt; p0++) {
      gdouble xx0 = point_x (p0vec[p0]);
      gdouble yy0 = point_y (p0vec[p0]);
      for (int p1 = 0; p1 < p1vcnt; p1++) {
	gdouble xx1 = point_x (p1vec[p1]);
	gdouble yy1 = point_y (p1vec[p1]);
	for (int a = 0; a < avcnt; a++) {
	  for (int e = 0; e < evcnt; e++) {
	    if (avec[a] > 0.0 && evec[e] >= 0.0 && evec[e] < 1.0) {
	      gdouble cx = (xx0 + xx1) / 2.0;
	      gdouble cy = (yy0 + yy1) / 2.0;
	      gdouble t  =
		atan2 (yy1 - yy0, xx1 - xx0);
	      gdouble b  = avec[a] * sqrt (1.0 - evec[e] * evec[e]);
	      if (method == METHOD_APPEND)
		entity_append_ellipse (sheet, cx, cy, avec[a], b, t, start,
				       stop, negative, filled, pen);
	      else
		ellipse = entity_build_ellipse (sheet, cx, cy, avec[a], b,
					       t, start, stop, negative,
						filled, pen);
	      didit = TRUE;
	    }
	    else log_string (LOG_GFPY_ERROR, sheet,
			     _ ("EllipseFFAE: Invalid parameter.\n"));	
	  }
	}
	if (p0vec) g_free (p0vec);
	if (p1vec) g_free (p1vec);
	if (avec) g_free (avec);
	if (evec) g_free (evec);
	if (method == METHOD_APPEND && didit) {
	  rc = Py_True;
	  force_redraw (sheet);
	}
      }
    }
  }
 
  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return (void *)rc;
  }
  else return (void *)ellipse;
  
}

static void *
method_ellipseffae (method_e method, PyObject *self,
		    PyObject *pArgs, PyObject *keywds)
{
  PyObject *rc = Py_False;
  Py_ssize_t t_count = PyTuple_Size(pArgs);
  sheet_s  *sheet = get_sheet ();
  entity_ellipse_s *ellipse = NULL;
  gboolean error_set = FALSE;

  if (t_count >= 4) {
    gboolean is_pointy0 = FALSE;
    gboolean is_pointy1 = FALSE;
    PyObject *pobj0 = PyTuple_GetItem (pArgs, 0);
    PyObject *pobj1 = PyTuple_GetItem (pArgs, 1);
    if (is_gfig_point (pobj0)) is_pointy0 = TRUE;
    if (is_gfig_point (pobj1)) is_pointy1 = TRUE;
    if (!is_pointy0) {
      if (PyList_Check (pobj0)) {
	if (PyList_Size (pobj0) > 0) {
	  PyObject* lobj0 = PyList_GetItem (pobj0, 0);
	  if (is_gfig_point (lobj0)) is_pointy0 = TRUE;
	}
      }
    }
    if (!is_pointy1) {
      if (PyList_Check (pobj1)) {
	if (PyList_Size (pobj1) > 0) {
	  PyObject* lobj1 = PyList_GetItem (pobj1, 0);
	  if (is_gfig_point (lobj1)) is_pointy1 = TRUE;
	}
      }
    }
    if (is_pointy0 && is_pointy1)
      return point_ellipseffae (method, self, pArgs, keywds);
    else if (!is_pointy0 != !is_pointy1) {
      log_string (LOG_GFPY_ERROR, sheet,
		  _ ("EllipseFFAE: Inconsistent focus types.\n"));
      error_set = TRUE;
    }
  }

  if (!error_set) {
    if (t_count >= 6) {
      gdouble *x0vec = NULL; gint x0vcnt = -1;
      gdouble *y0vec = NULL; gint y0vcnt = -1;
      gdouble *x1vec = NULL; gint x1vcnt = -1;
      gdouble *y1vec = NULL; gint y1vcnt = -1;
      gdouble *avec = NULL;  gint avcnt  = -1;
      gdouble *evec = NULL;  gint evcnt  = -1;
      gdouble start = 0.0;
      gdouble stop = 2.0 * G_PI;
      gboolean negative = FALSE;
      gboolean filled = FALSE;
      pen_s *pen = NULL;

      circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);
      x0vec = build_vec (&x0vcnt, PyTuple_GetItem (pArgs, 0));
      y0vec = build_vec (&y0vcnt, PyTuple_GetItem (pArgs, 1));
      x1vec = build_vec (&x1vcnt, PyTuple_GetItem (pArgs, 2));
      y1vec = build_vec (&y1vcnt, PyTuple_GetItem (pArgs, 3));
      avec  = build_vec (&avcnt,  PyTuple_GetItem (pArgs, 4));
      evec  = build_vec (&evcnt,  PyTuple_GetItem (pArgs, 5));
      if (!x0vec || !y0vec || !x1vec || !y1vec || !avec || !evec ||	
	  x0vcnt != y0vcnt || x1vcnt != y1vcnt) {
	if (x0vec) g_free (x0vec);
	if (y0vec) g_free (y0vec);
	if (x1vec) g_free (x1vec);
	if (y1vec) g_free (y1vec);
	if (avec)  g_free (avec);
	if (evec)  g_free (evec);
	log_string (LOG_GFPY_ERROR, sheet,
		    _ ("EllipseFFAE: Missing parameter.\n"));
      }
      else if (method == METHOD_BUILD &&
	       (x0vcnt > 1 || y0vcnt > 1 || x1vcnt > 1 || y1vcnt > 1 ||
		avcnt > 1 || evcnt > 1)) {
	log_string (LOG_GFPY_ERROR, sheet,
  _ ("EllipseFFAE: Range parameter not valid in this context.\n"));
      }
      else {
	gboolean didit = FALSE;
      
	for (int p0 = 0; p0 < x0vcnt; p0++) {
	  for (int p1 = 0; p1 < x1vcnt; p1++) {
	    for (int a = 0; a < avcnt; a++) {
	      for (int e = 0; e < evcnt; e++) {
		if (evec[e] >= 0.0 && evec[e] < 1.0 && avec[a] > 0.0) {
		  gdouble cx = (x1vec[p0] + x0vec[p1]) / 2.0;
		  gdouble cy = (y1vec[p0] + y0vec[p1]) / 2.0;
		  gdouble t  =
		    atan2 (y1vec[p0] - y0vec[p1], x1vec[p0] + x0vec[p1]);
		  gdouble b  = avec[a] * sqrt (1.0 - evec[e] * evec[e]);
		  if (method == METHOD_APPEND)
		    entity_append_ellipse (sheet, cx, cy,
					   avec[a], b, t, start, stop,
					   negative, filled, pen);
		  else
		    ellipse = entity_build_ellipse (sheet, cx, cy,
						    avec[a], b, t, start, stop,
						    negative, filled, pen);
		  didit = TRUE;
		}
		else log_string (LOG_GFPY_ERROR, sheet,
	     _ ("EllipseFFAE: Invalid eccentricity or negative semi-axis.\n"));
	      }
	    }
	  }
	}
	if (x0vec) g_free (x0vec);
	if (y0vec) g_free (y0vec);
	if (x1vec) g_free (x1vec);
	if (y1vec) g_free (y1vec);
	if (avec)  g_free (avec);
	if (evec)  g_free (evec);
	if (method == METHOD_APPEND && didit) {
	  rc = Py_True;
	  force_redraw (sheet);
	}
      }
    }
    else log_string (LOG_GFPY_ERROR, sheet,
		     _ ("EllipseFFAE: Too few parameters.\n"));
  }

  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return (void *)rc;
  }
  else return (void *)ellipse;
}

static PyObject *
gfig_draw_ellipseFFAE (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return (PyObject *)method_ellipseffae (METHOD_APPEND, self, pArgs, keywds);
}

/************************* cabt ellipse ***********************/


static PyObject *
ellipsecabt_repr (PyObject *obj)
{// fixme and all other ellipse reprs and strs
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_ellipse_s *ellipse = cobj->entity;
  gchar *str = ellipse ?
    g_strdup_printf ("%s (%g, %g %g, %g, filled = %s)\n",
		     ellipseffae_name,
		     entity_ellipse_x (ellipse),
		     entity_ellipse_y (ellipse),
		     entity_ellipse_a (ellipse),
		     entity_ellipse_b (ellipse),
		     entity_ellipse_fill (ellipse) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", ellipseffae_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
ellipsecabt_str (PyObject *obj)
{// fixme and all other ellipse reprs and strs
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_ellipse_s *ellipse = cobj->entity;
  gchar *str = ellipse ?
    g_strdup_printf ("%s (%g, %g %g, %g, filled = %s)\n",
		     ellipseffae_name,
		     entity_ellipse_x (ellipse),
		     entity_ellipse_y (ellipse),
		     entity_ellipse_a (ellipse),
		     entity_ellipse_b (ellipse),
		     entity_ellipse_fill (ellipse) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", ellipseffae_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static int
ellipsecabt_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_ellipsecabt (METHOD_BUILD, self, args, kwds);
  
  return 0;
}


static PyTypeObject gfig_EllipseCABT = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= ellipsecabt_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= ellipsecabt_repr,
  .tp_str	= ellipsecabt_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig ellipsecabt type",
  .tp_init	= ellipsecabt_init,
};


/***
    fixme -- allow sic format:

                      centres
                ---- sic vector -------------  a  b   t
    gf_ellcabt (gf_sic( (1,1.5), (.5, .7), 4), 1, 2, 45)
    
***/
/* ellipse specified by x, y, a, b, theta */

static void *
point_ellipsecabt (method_e method, PyObject *self,
		   PyObject *pArgs, PyObject *keywds)
{
  point_s **pvec = NULL; gint pvcnt = -1;
  gdouble  *avec = NULL; gint avcnt = -1;
  gdouble  *bvec = NULL; gint bvcnt = -1;
  gdouble  *tvec = NULL; gint tvcnt = -1;
  gdouble start = 0.0;
  gdouble stop = 2.0 * G_PI;
  gboolean negative = FALSE;
  sheet_s *sheet = get_sheet ();
  PyObject *rc = Py_False;
  entity_ellipse_s *ellipse = NULL;
  pen_s *pen = NULL;

  gboolean filled = FALSE;

  circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);

  pvec = build_point_vec (&pvcnt, PyTuple_GetItem (pArgs, 0));
  avec = build_vec (&avcnt, PyTuple_GetItem (pArgs, 1));
  bvec = build_vec (&bvcnt, PyTuple_GetItem (pArgs, 1));
  tvec = build_vec (&tvcnt, PyTuple_GetItem (pArgs, 1));
  if (!pvec || !avec || !bvec || !tvec) {
    if (pvec) g_free (pvec);
    if (avec) g_free (avec);
    if (bvec) g_free (bvec);
    if (tvec) g_free (tvec);
    log_string (LOG_GFPY_ERROR, sheet, _ ("EllipseCABT: Missing parameter.\n"));
  }
  else if (method == METHOD_BUILD &&
	   (pvcnt > 1 || avcnt > 1 || bvcnt > 1 || tvcnt > 1)) {
    // fixme free vecs?
    log_string (LOG_GFPY_ERROR, sheet,
	_ ("EllipseCABT: Range parameter not valid in this context.\n"));
  }
  else {
    gboolean didit = FALSE;
    for (int p = 0; p < pvcnt; p++) {
      gdouble xx = point_x (pvec[p]);
      gdouble yy = point_y (pvec[p]);
      for (int a = 0; a < avcnt; a++) {
	for (int b = 0; b < bvcnt; b++) {
	  for (int t = 0; t < tvcnt; t++) {
	    if (avec[a] > 0.0 && bvec[b] > 0.0) {
	      if (method == METHOD_APPEND)
		entity_append_ellipse (sheet, xx, yy, avec[a], bvec[b],
				       tvec[t], start, stop, negative,
				       filled, pen);
	      else
		ellipse = entity_build_ellipse (sheet, xx, yy, avec[a],
						bvec[b], tvec[t], start,
						stop, negative, filled, pen);
	      didit = TRUE;
	    }
	    else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("Ellipse: Invalid negative semi-axis.\n"));	
	  }
	}
      }
    }
    if (pvec) g_free (pvec);
    if (avec) g_free (avec);
    if (bvec) g_free (bvec);
    if (tvec) g_free (tvec);
    if (method == METHOD_APPEND && didit) {
      rc = Py_True;
      force_redraw (sheet);
    }
  }
 
  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return (void *)rc;
  }
  else return (void *)ellipse;
}


static void *
method_ellipsecabt (method_e method, PyObject *self,
		    PyObject *pArgs, PyObject *keywds)
{
  PyObject *rc = Py_False;
  Py_ssize_t t_count = PyTuple_Size(pArgs);
  sheet_s  *sheet = get_sheet ();
  entity_ellipse_s *ellipse = NULL;

  if (t_count >= 4) {
    gboolean is_pointy = FALSE;
    PyObject *pobj = PyTuple_GetItem (pArgs, 0);
    if (is_gfig_point (pobj)) is_pointy = TRUE;
    else {
      if (PyList_Check (pobj)) {
	if (PyList_Size (pobj) > 0) {
	  PyObject* lobj = PyList_GetItem (pobj, 0);
	  if (is_gfig_point (lobj)) is_pointy = TRUE;
	}
      }
    }
    if (is_pointy) return point_ellipsecabt (method, self, pArgs, keywds);
  }
  
   if (t_count >= 5) {
    gdouble *xvec = NULL; gint xvcnt = -1;
    gdouble *yvec = NULL; gint yvcnt = -1;
    gdouble *avec = NULL; gint avcnt = -1;
    gdouble *bvec = NULL; gint bvcnt = -1;
    gdouble *tvec = NULL; gint tvcnt = -1;
    gdouble start = 0.0;
    gdouble stop = 2.0 * G_PI;
    gboolean negative = FALSE;
    gboolean filled = FALSE;
    pen_s *pen = NULL;

    circle_parse_kwds (keywds, &filled, &start, &stop, &negative, &pen);
    xvec = build_vec (&xvcnt, PyTuple_GetItem (pArgs, 0));
    yvec = build_vec (&yvcnt, PyTuple_GetItem (pArgs, 1));
    avec = build_vec (&avcnt, PyTuple_GetItem (pArgs, 2));
    bvec = build_vec (&bvcnt, PyTuple_GetItem (pArgs, 3));
    tvec = build_vec (&tvcnt, PyTuple_GetItem (pArgs, 4));
    if (!xvec || !yvec || !avec || !bvec || !tvec) {
      if (xvec) g_free (xvec);
      if (yvec) g_free (yvec);
      if (avec) g_free (avec);
      if (bvec) g_free (bvec);
      if (tvec) g_free (tvec);
      log_string (LOG_GFPY_ERROR, sheet,
		  _ ("EllipseCABT: Missing parameter.\n"));
    }
    else if (method == METHOD_BUILD &&
	     (xvcnt > 1 || yvcnt > 1 || 
	      avcnt > 1 || bvcnt > 1 || tvcnt > 1)) {
      log_string (LOG_GFPY_ERROR, sheet,
	_ ("EllipseCABT: Range parameter not valid in this context.\n"));
    }
    else {
      gboolean didit = FALSE;
      
      for (int x = 0; x < xvcnt; x++) {
	for (int y = 0; y < yvcnt; y++) {
	  for (int a = 0; a < avcnt; a++) {
	    for (int b = 0; b < bvcnt; b++) { 
	      for (int t = 0; t < tvcnt; t++) {
		if (avec[b] > 0.0 && bvec[b] > 0) {
		  if (method == METHOD_APPEND)
		    entity_append_ellipse (sheet,
					   xvec[x], yvec[y],
					   avec[a], bvec[b],
					   tvec[t], start, stop,
					   negative, filled, pen);
		  else
		    ellipse = entity_build_ellipse (sheet,
						    xvec[x], yvec[y],
						    avec[a], bvec[b],
						    tvec[t], start,
						    stop, negative,
						    filled, pen);
		  didit = TRUE;
		}
		else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("EllipseCABT: Invalid negative semi-axis.\n"));
	      }
	    }
	  }
	}
      }
      if (xvec) g_free (xvec);
      if (yvec) g_free (yvec);
      if (avec) g_free (avec);
      if (bvec) g_free (bvec);
      if (tvec) g_free (tvec);
      if (method == METHOD_APPEND && didit) {
	rc = Py_True;
	force_redraw (sheet);
      }
    }
  }
  else log_string (LOG_GFPY_ERROR, sheet,
		   _ ("EllipseCABT: Too few parameters.\n"));

  if (method == METHOD_APPEND) {
    Py_INCREF (rc);
    return rc;
  }
  else return (void *)ellipse;
}

static PyObject *
gfig_draw_ellipseCABT (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return method_ellipsecabt (METHOD_APPEND, self, pArgs, keywds);
}

/*********************** polylines and splines *******************/


static PyObject *
polyline_repr (PyObject *obj)
{// fixme and all other ellipse reprs and strs
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_polyline_s *polyline = cobj->entity;
  gchar *str = polyline ?
    g_strdup_printf ("%s (filled = %s)\n",
		     polyline_name,
		     entity_polyline_filled (polyline) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", polyline_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static PyObject *
polyline_str (PyObject *obj)
{// fixme and all other ellipse reprs and strs
  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
  entity_polyline_s *polyline = cobj->entity;
  gchar *str = polyline ?
    g_strdup_printf ("%s (filled = %s)\n",
		     polyline_name,
		     entity_polyline_filled (polyline) ? "True" : "False")
    : g_strdup_printf ("%s ()\n", polyline_name);
  PyObject *robj = PyUnicode_FromString (str);
  g_free (str);
  return robj;
}

static int
polyline_init (PyObject *self, PyObject *args, PyObject *kwds)
{
  gfig_GenericObject *cobj = (gfig_GenericObject *)self;
  cobj->entity = method_polyline (METHOD_BUILD, self, args, kwds);
  
  return 0;
}


static PyTypeObject gfig_Polyline = {
  .ob_base	= PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name	= polyline_name,
  .tp_basicsize	= sizeof(gfig_GenericObject),
  .tp_dealloc	= generic_dealloc,
  .tp_repr	= polyline_repr,
  .tp_str	= polyline_str,
  .tp_flags	= Py_TPFLAGS_DEFAULT,
  .tp_doc	= "gfig polyline type",
  .tp_init	= polyline_init,
};

/* gf_polyline ([ (x,y), (x,y), ...], filled, closed, spline) */
/* gf_pline    ([ (x,y), (x,y), ...], filled, closed, spline) */
static void *
method_polyline (method_e method, PyObject *self,
		 PyObject *pArgs, PyObject *keywds)
{
  PyObject *points;
  gint filled = 0;
  gint closed = 0;
  gint spline = 0;
  PyObject *penobj = NULL;
  pen_s *pen = NULL;
  gint  intersect = INTERSECT_POINT;
  gdouble radius = NAN;
  PyObject *rc = Py_False;

  static char *kwlist[] = {"points", "filled", "closed",
			   "spline", "pen", "intersection", "radius", NULL};

  sheet_s  *sheet = get_sheet ();
  if (!sheet) {
    PyErr_SetString (PyExc_RuntimeError, "Too few arguments");
    return NULL;
  }

  if (PyArg_ParseTupleAndKeywords (pArgs, keywds, "O|pppOid", kwlist,
				   &points, &filled, &closed, &spline,
				   &penobj, &intersect, &radius)) {
    if (PyList_Check (points)) {
      gint len =  (int)PyList_Size (points);
      GList *verts = NULL;	// fixme free on error

      if (penobj && is_gfig_pen (penobj)) {
	gfig_GenericObject *cobj = (gfig_GenericObject *)penobj;
	pen_s *e_pen = cobj->entity;
	pen = copy_pen (e_pen);
      }

      for (gint i = 0; i < len; i++) {
	PyObject *obj = PyList_GetItem (points, i);
	if (PyTuple_Check (obj) && PyTuple_Size (obj) == 2) {
	  PyObject *xpo;
	  PyObject *ypo;
	  gdouble xp, yp;
	  if (PyArg_ParseTuple (obj, "OO", &xpo, &ypo)) {
	    xp = get_double (xpo);
	    yp = get_double (ypo);
	    if (!isnan (xp) && !isnan (yp)) {
	      point_s *point = gfig_try_malloc0 (sizeof(point_s));
	      point_x (point) = xp;
	      point_y (point) = yp;
	      verts = g_list_append (verts, point);
	    }
	  }
	  else log_string (LOG_GFPY_ERROR, sheet,
			   _ ("Polyline: Invalid point format.\n"));
	}
	else if (is_gfig_point (obj)) {
	  gfig_GenericObject *cobj = (gfig_GenericObject *)obj;
	  point_s *point = cobj->entity;
	  verts = g_list_append (verts, copy_point (point));
	}
	else log_string (LOG_GFPY_ERROR, sheet,
			 _ ("Polyline: Invalid point type.\n"));
      }

      if (verts) {
	if (method == METHOD_APPEND) {
	  entity_append_polyline (sheet, verts, closed, filled, spline, pen,
				  intersect, radius);
	  force_redraw (sheet);
	  Py_INCREF (Py_True);
	  return Py_True;
	}
	else {
	  return (void *)entity_build_polyline (sheet, verts,
						closed, filled, spline, pen,
						intersect, radius);
	}
      }
	
    }
    else log_string (LOG_GFPY_ERROR, sheet,
		     _ ("Polyline: Invalid parameter list.\n"));
  }
  Py_INCREF (rc);
  return rc;
}

static PyObject *
gfig_draw_polyline (PyObject *self, PyObject *pArgs, PyObject *keywds)
{
  return method_polyline (METHOD_APPEND, self, pArgs, keywds);
}

/************************ stderr, stdout capture **************/

static PyObject*
log_CaptureStdout (PyObject* self, PyObject* pArgs)
{
  char* LogStr = NULL;
  PyObject *local_dict = NULL;
  //  char *projname = NULL;
  //char *sheetname = NULL;

  if (!PyArg_ParseTuple (pArgs, "s|O", &LogStr, &local_dict)) return NULL;

  sheet_s  *sheet = get_sheet ();

  if (1 != strlen (LogStr) || *LogStr != '\n') {
    if (*LogStr != '\004') log_string (LOG_NORMAL, sheet, LogStr);
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject*
log_CaptureStderr (PyObject* self, PyObject* pArgs)
{
  char* LogStr = NULL;
  PyObject *local_dict = NULL;

  if (!PyArg_ParseTuple (pArgs, "s|O", &LogStr, &local_dict)) return NULL;

  sheet_s *sheet = get_sheet ();

  log_string (LOG_PYTHON_ERROR, sheet, LogStr);

  Py_INCREF(Py_None);
  return Py_None;
}

/************* python initialisation and termination ************/

static PyMethodDef GfigMethods[] = {
  {"CaptureStdout", log_CaptureStdout, METH_VARARGS, "Logs stdout"},
  {"CaptureStderr", log_CaptureStderr, METH_VARARGS, "Logs stderr"},
  {"Clear", (PyCFunction)gfig_clear, METH_VARARGS | METH_KEYWORDS,
   "delete all the entities"},
  {"DrawCircle", (PyCFunction)gfig_draw_circle,
   METH_VARARGS | METH_KEYWORDS, "gfig circle"},
  {"DrawEllipseCABT", (PyCFunction)gfig_draw_ellipseCABT,
   METH_VARARGS | METH_KEYWORDS, "gfig ctr-a-b-theta ellipse"},
  {"DrawEllipseFFAE", (PyCFunction)gfig_draw_ellipseFFAE,
   METH_VARARGS | METH_KEYWORDS, "gfig focus-focus-a-e ellipse"},
  {"DrawText", (PyCFunction)gfig_draw_text,
   METH_VARARGS | METH_KEYWORDS, "gfig text"},
  {"DrawLine", (PyCFunction)gfig_draw_polyline,
   METH_VARARGS | METH_KEYWORDS, "gfig segment or polyline"},
  {"IterSIC", (PyCFunction)gfig_iterSIC, METH_VARARGS | METH_KEYWORDS,
   "generate a list from start with increment incr with count elements"},
  {"Degrees", (PyCFunction)gfig_degrees, METH_VARARGS | METH_KEYWORDS,
   "convert degrees to radians"},
  {"GetPenWidthIndex", (PyCFunction)gfig_get_penwidthidx,
   METH_VARARGS | METH_KEYWORDS, "get the pen width index"},
  {"SetPenWidthIndex", (PyCFunction)gfig_set_penwidthidx,
   METH_VARARGS | METH_KEYWORDS, "set the pen width index"},
  {"SetPenWidth", (PyCFunction)gfig_set_penwidth,
   METH_VARARGS | METH_KEYWORDS, "set the pen width"},
  {"SetPenColour", (PyCFunction)gfig_set_pencolour,
   METH_VARARGS | METH_KEYWORDS, "set the pen rgba"},
  {"Catenate", (PyCFunction)gfig_catenate,
   METH_VARARGS | METH_KEYWORDS, "catenate transforms"},
  {"Invert", (PyCFunction)gfig_invert,
   METH_VARARGS | METH_KEYWORDS, "invert transforms"},
  {"Draw", (PyCFunction)gfig_draw, 
   METH_VARARGS | METH_KEYWORDS, "draw an object"},
  {"Intersect", (PyCFunction)gfig_intersect, 
   METH_VARARGS | METH_KEYWORDS, "find the intersecting points of two objects"},
#if 0
  {"Test", (PyCFunction)gfig_test,
   METH_VARARGS | METH_KEYWORDS, "test"},
#endif
  {NULL, NULL, 0, NULL}
};

//  https://docs.python.org/3.4/c-api/module.html

static PyModuleDef GfigModule = {
  PyModuleDef_HEAD_INIT, "gfig", NULL, -1, GfigMethods
};

static PyMODINIT_FUNC
PyInit_gfig(void)
{
  PyObject* m;
  
  m = PyModule_Create (&GfigModule);

  gfig_Circle.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Circle);
  Py_INCREF(&gfig_Circle);
  PyModule_AddObject(m, "Circle", (PyObject *)&gfig_Circle);

  gfig_Text.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Text);
  Py_INCREF(&gfig_Text);
  PyModule_AddObject(m, "Text", (PyObject *)&gfig_Text);
  
  gfig_Point.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Point);
  Py_INCREF(&gfig_Point);
  PyModule_AddObject(m, "Point", (PyObject *)&gfig_Point);
  
  gfig_Transform.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Transform);
  Py_INCREF(&gfig_Transform);
  PyModule_AddObject(m, "Transform", (PyObject *)&gfig_Transform);
  
  gfig_Scale.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Scale);
  Py_INCREF(&gfig_Scale);
  PyModule_AddObject(m, "Scale", (PyObject *)&gfig_Scale);
  
  gfig_Translate.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Translate);
  Py_INCREF(&gfig_Translate);
  PyModule_AddObject(m, "Translate", (PyObject *)&gfig_Translate);
  
  gfig_Rotate.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Rotate);
  Py_INCREF(&gfig_Rotate);
  PyModule_AddObject(m, "Rotate", (PyObject *)&gfig_Rotate);
  
  gfig_Pen.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Pen);
  Py_INCREF(&gfig_Pen);
  PyModule_AddObject(m, "Pen", (PyObject *)&gfig_Pen);
  
  gfig_EllipseFFAE.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_EllipseFFAE);
  Py_INCREF(&gfig_EllipseFFAE);
  PyModule_AddObject(m, "EllipseFFAE", (PyObject *)&gfig_EllipseFFAE);
  
  gfig_EllipseCABT.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_EllipseCABT);
  Py_INCREF(&gfig_EllipseCABT);
  PyModule_AddObject(m, "EllipseCABT", (PyObject *)&gfig_EllipseCABT);
  
  gfig_Polyline.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Polyline);
  Py_INCREF(&gfig_Polyline);
  PyModule_AddObject(m, "Polyline", (PyObject *)&gfig_Polyline);
  
  gfig_Group.tp_new = PyType_GenericNew;
  PyType_Ready(&gfig_Group);
  Py_INCREF(&gfig_Group);
  PyModule_AddObject(m, "Group", (PyObject *)&gfig_Group);
  
  return m;
}

void *
get_local_pydict (sheet_s *sheet)
{
  PyObject *local_dict = PyDict_New ();
  PyObject *pysheet = PyLong_FromVoidPtr ((void *)sheet);
  PyDict_SetItemString (local_dict, SHEET_NAME, pysheet);
  Py_DECREF(pysheet);
  return (void *)local_dict;
}

void
init_python ()
{
  PyImport_AppendInittab("gfig", &PyInit_gfig);
  //PyImport_AppendInittab("noddy", &PyInit_noddy);
  
  Py_SetProgramName (L"gfig");
  Py_Initialize();

  PyObject *main_module = PyImport_AddModule ("__main__");
  global_dict = PyModule_GetDict (main_module);

  PyRun_SimpleString (
                     "import gfig\n"
		     "import sys\n"
                     "import inspect\n"
		     // "sys.argv = ['/mnt/disk5/personal/tinkering/gf3/src/gfig']\n"
		     //		     "from gi.repository import Gtk\n"
		     "\n"
                     "class StdoutCatcher:\n"
                     "\tdef write(self, str):\n"
		     "\t\tgfig.CaptureStdout(str)\n"	     
                     "\tdef flush(self):\n"
                     "\t\tgfig.CaptureStdout('\004')\n"
		     "\n"
                     "class StderrCatcher:\n"
                     "\tdef write(self, str):\n"
		     "\t\tgfig.CaptureStderr(str)\n"		     
		     "\n"
                     "sys.stdout = StdoutCatcher()\n"
                     "sys.stderr = StderrCatcher()\n"
		     "\n"
		     "gf_degrees   = gfig.Degrees\n"
		     "gf_deg       = gfig.Degrees\n"
		     "gf_sic       = gfig.IterSIC\n"
		     "gf_clear     = gfig.Clear\n"
		     "gf_circ      = gfig.DrawCircle\n"
		     "gf_circle    = gfig.DrawCircle\n"
		     "gf_ellcabt   = gfig.DrawEllipseCABT\n"
		     "gf_ellffae   = gfig.DrawEllipseFFAE\n"
		     "gf_line      = gfig.DrawLine\n"
		     "gf_pline     = gfig.DrawLine\n"
		     "gf_text      = gfig.DrawText\n"
		     "gf_gpwi      = gfig.GetPenWidthIndex\n"
		     "gf_spwi      = gfig.SetPenWidthIndex\n"
		     "gf_spw       = gfig.SetPenWidth\n"
		     "gf_setcolour = gfig.SetPenColour\n"
		     "gf_setcolor  = gfig.SetPenColour\n"
		     "gf_draw      = gfig.Draw\n"
		     "gf_intersect = gfig.Intersect\n"
		     "gf_catenate  = gfig.Catenate\n"
		     "gf_invert    = gfig.Invert\n"
#if 0
		     "gf_scale     = gfig.SetScale\n"
		     "gf_translate = gfig.SetTranslate\n"
		     "gf_rotate    = gfig.SetRotate\n"
#endif
		     "gf_circle_t  = gfig.Circle\n"
		     "gf_ellffae_t = gfig.EllipseFFAE\n"
		     "gf_ellcabt_t = gfig.EllipseCABT\n"
		     "gf_line_t    = gfig.Polyline\n"
		     "gf_pline_t   = gfig.Polyline\n"
		     "gf_text_t    = gfig.Text\n"
		     "gf_point_t   = gfig.Point\n"
		     "gf_pen_t     = gfig.Pen\n"
		     "gf_transform_t = gfig.Transform\n"
		     "gf_trans_t   = gfig.Transform\n"
		     "gf_scale_t   = gfig.Scale\n"
		     "gf_translate_t = gfig.Translate\n"
		     "gf_rotate_t  = gfig.Rotate\n"
		     "gf_group_t   = gfig.Group\n"

		     //                                     from gf.h
		     "GF_INTERSECT_POINT = 0\n"		// INTERSECT_POINT
		     "GF_INTERSECT_ARC   = 1\n"		// INTERSECT_ARC
		     "GF_INTERSECT_BEVEL = 2\n"		// INTERSECT_BEVEL
#if 0
		     "gf_test      = gfig.Test\n"
#endif
                     );
  /***
      
      PANGO_ALIGN_LEFT		Put all available space on the right
      PANGO_ALIGN_CENTER	Center the line within the available space
      PANGO_ALIGN_RIGHT		Put all available space on the left
      
  ***/
  
  char *lcmd;
  
  lcmd = g_strdup_printf ("PANGO_ALIGN_LEFT   = %d\n", PANGO_ALIGN_LEFT);
  PyRun_SimpleString (lcmd);
  g_free (lcmd);
  
  lcmd = g_strdup_printf ("PANGO_ALIGN_CENTER = %d\n", PANGO_ALIGN_CENTER);
  PyRun_SimpleString (lcmd);
  g_free (lcmd);
  
  lcmd = g_strdup_printf ("PANGO_ALIGN_CENTRE = %d\n", PANGO_ALIGN_CENTER);
  PyRun_SimpleString (lcmd);
  g_free (lcmd);
  
  lcmd = g_strdup_printf ("PANGO_ALIGN_RIGHT  = %d\n", PANGO_ALIGN_RIGHT);
  PyRun_SimpleString (lcmd);
  g_free (lcmd);
}

void
term_python ()
{
  Py_Finalize();
}

/************ string and script evaluation for a point **********/

typedef enum {
  POSITION_UNSET,
  POSITION_RELATIVE,
  POSITION_ABSOLUTE
} position_e;

typedef enum {
  COORDINATES_UNSET,
  COORDINATES_RECTANGULAR,
  COORDINATES_POLAR
} coordinates_e;

enum {
  FIELD_COMPLETE,
  FIELD_POSITION,
  FIELD_FIRST_ARG,
  FIELD_COORDINATES,
  FIELD_SECOND_ARG
};

#define RE	"[[:space:]]*([@~]?)([^,@]+)([,@])(.*)"

gboolean
python_evaluate_point (gchar *pystr, sheet_s *sheet,
		       gdouble *xvp, gdouble *yvp)
{
  gdouble frc = FALSE;
  gchar *first_arg;
  gchar *second_arg;
  position_e position  = POSITION_UNSET;
  coordinates_e coords = COORDINATES_UNSET;
  
  first_arg  = NULL;
  second_arg = NULL;
  position   = POSITION_UNSET;
  coords     = COORDINATES_UNSET;
  
  static regex_t preg;
  static gboolean preg_set = FALSE;
  if (!preg_set) {
    regcomp (&preg, RE, REG_ICASE | REG_EXTENDED);
    preg_set = TRUE;
  }

  regmatch_t *match = g_alloca ((1 + preg.re_nsub) * sizeof(regmatch_t));
  if (0 != regexec (&preg, pystr, 1 + preg.re_nsub, match, 0))
    log_string (LOG_GFPY_ERROR, sheet, _ ("Invalid point argument.\n"));
  else {
    if (-1 != match[FIELD_POSITION].rm_so) {
      switch(pystr[match[FIELD_POSITION].rm_so]) {
      case '@': position = POSITION_ABSOLUTE; break;
      case '~': position = POSITION_RELATIVE;  break;
      default:  position = POSITION_UNSET;	break;
      }
      if (-1 != match[FIELD_FIRST_ARG].rm_so) {
	first_arg = pystr + match[FIELD_FIRST_ARG].rm_so;
	if (-1 != match[FIELD_COORDINATES].rm_so) {
	  coords = (pystr[match[FIELD_COORDINATES].rm_so] == ',') ?
	    COORDINATES_RECTANGULAR : COORDINATES_POLAR;
	  pystr[match[FIELD_COORDINATES].rm_so] = 0;
	  if (-1 != match[FIELD_FIRST_ARG].rm_so) {
	    second_arg = pystr + match[FIELD_SECOND_ARG].rm_so;
	    if (position == POSITION_UNSET) {
	      position = (coords == COORDINATES_RECTANGULAR) ?
		POSITION_ABSOLUTE : POSITION_RELATIVE;
	    }
	  }
	}
      }
    }
  }

  if (first_arg && second_arg &&
      position != POSITION_UNSET && coords != COORDINATES_UNSET) {
    gdouble la;
    gdouble ra;

    la = NAN;
    ra = NAN;

    PyObject *eval = Py_CompileString(first_arg, "filename", Py_eval_input);
    if (eval) {
      PyObject *local_dict = sheet_pydict (sheet);
      PyObject *result = PyEval_EvalCode(eval, global_dict, local_dict);
      if (result) {
	la = get_double (result);
	Py_DECREF(eval);
	Py_DECREF(result);
	eval = Py_CompileString(second_arg, "filename", Py_eval_input);
	if (eval) {
	  result = PyEval_EvalCode(eval, global_dict, local_dict);
	  if (result) {
	    ra = get_double (result);
	    Py_DECREF(eval);
	    Py_DECREF(result);
	  }
	}
      }
    }
    if (!isnan (la) && !isnan (ra)) {
      gdouble xv, yv;
      if (coords == COORDINATES_POLAR) {
	gdouble ang = ra * G_PI / 180.0;
	xv = la * cos (ang);
	yv = la * sin (ang);
      }
      else {
	xv = la;
	yv = ra;
      }
      if (position == POSITION_RELATIVE) {
	point_s *current_point = &sheet_current_point (sheet);
	xv += point_x (current_point);
	yv += point_y (current_point);
      }
      if (xvp)  *xvp  = xv;
      if (yvp)  *yvp  = yv;
      frc = TRUE;
    }
  }

  return frc;
}

/********************* string and script evaluation **************/

void
evaluate_python (gchar *pystr, sheet_s *sheet)
{

  /**********
  https://docs.python.org/3/c-api/reflection.html
	     
    PyObject* PyEval_GetLocals()
    Return value: Borrowed reference.

    Return a dictionary of the local variables in the current execution
    frame, or NULL if no frame is currently executing.

    
    PyObject* PyLong_FromVoidPtr(void *p)
    Return value: New reference.

    Create a Python integer from the pointer p. The pointer value can
    be retrieved from the resulting value using PyLong_AsVoidPtr().



    void* PyLong_AsVoidPtr(PyObject *pylong)

    Convert a Python integer pylong to a C void pointer. If pylong cannot
    be converted, an OverflowError will be raised. This is only assured
    to produce a usable void pointer for values created with
    PyLong_FromVoidPtr().


   *********/

  PyObject *local_dict = sheet_pydict (sheet);

  PyObject *rc = 
    PyRun_String (pystr,  Py_single_input, global_dict, local_dict);
  if (rc == NULL) {                 // error
    PyObject* ex = PyErr_Occurred();
    if (ex) {
      PyErr_Print ();
    }
  }
}

void
execute_python (const gchar *pyfile,  sheet_s *sheet)
{

  /**********
  https://docs.python.org/3/c-api/reflection.html
	     
    PyObject* PyEval_GetLocals()
    Return value: Borrowed reference.

    Return a dictionary of the local variables in the current execution
    frame, or NULL if no frame is currently executing.

    
    PyObject* PyLong_FromVoidPtr(void *p)
    Return value: New reference.

    Create a Python integer from the pointer p. The pointer value can
    be retrieved from the resulting value using PyLong_AsVoidPtr().



    void* PyLong_AsVoidPtr(PyObject *pylong)

    Convert a Python integer pylong to a C void pointer. If pylong cannot
    be converted, an OverflowError will be raised. This is only assured
    to produce a usable void pointer for values created with
    PyLong_FromVoidPtr().


   *********/

  PyObject *local_dict = sheet_pydict (sheet);

  FILE *fp = fopen (pyfile, "r");
  if (!fp) {
    log_string (LOG_GFPY_ERROR, sheet, _ ("Python script not found."));
    return;
  }
  

  PyObject *rc = 
    PyRun_File (fp,  pyfile, Py_file_input, global_dict, local_dict);
  if (rc == NULL) {                 // error
    PyObject* ex = PyErr_Occurred();
    if (ex) {
      PyErr_Print ();
    }
  }
}

static gboolean
is_gfig_pen (PyObject *xobj)
{
  return (Py_TYPE (xobj) == &gfig_Pen);
}

static gboolean
is_gfig_point (PyObject *xobj)
{
  return (xobj && (Py_TYPE (xobj) == &gfig_Point));
}

static gboolean
is_gfig_transform (PyObject *xobj)
{
  return (xobj && ((Py_TYPE (xobj) == &gfig_Transform) ||
		   (Py_TYPE (xobj) == &gfig_Scale)     ||
		   (Py_TYPE (xobj) == &gfig_Translate) ||
		   (Py_TYPE (xobj) == &gfig_Rotate)));
}

static gboolean
is_gfig_entity (PyObject *xobj)
{
  return (xobj && ((Py_TYPE (xobj) == &gfig_Circle)      ||
		   (Py_TYPE (xobj) == &gfig_EllipseFFAE) ||
		   (Py_TYPE (xobj) == &gfig_EllipseCABT) ||
		   (Py_TYPE (xobj) == &gfig_Polyline)    ||
		   (Py_TYPE (xobj) == &gfig_Group)       ||
		   (Py_TYPE (xobj) == &gfig_Text)));
  // fixme add the rest when they happen
}

static gboolean
entry_key_press_cb (GtkWidget *widget,
		    GdkEvent  *event,
		    gpointer   user_data)
{
  set_button_tags sbt = user_data;
  //  derive_key (set_state_bit (event));
  (*sbt) (set_state_bit (event));
  return GDK_EVENT_PROPAGATE;
}

static gboolean
entry_key_release_cb (GtkWidget *widget,
		      GdkEvent  *event,
		      gpointer   user_data)
{
  set_button_tags sbt = user_data;
  //  derive_key (clear_state_bit (event));
  (*sbt) (clear_state_bit (event));
  return GDK_EVENT_PROPAGATE;
}

void
evaluate_point (sheet_s *sheet, gdouble *pxip, gdouble *pyip,
		set_button_tags sbt)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *entry;
  gint response;

  dialog =  gtk_dialog_new_with_buttons (_ ("Enter point"),
					 NULL,
					 GTK_DIALOG_MODAL |
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 _ ("_OK"), GTK_RESPONSE_ACCEPT,
					 _ ("_Cancel"), GTK_RESPONSE_CANCEL,
					 NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
				   GTK_RESPONSE_ACCEPT);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  entry = gtk_entry_new ();
  gtk_widget_add_events (entry, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
  g_signal_connect (entry, "key-press-event",
		    G_CALLBACK (entry_key_press_cb), sbt);
  g_signal_connect (entry, "key-release-event",
		    G_CALLBACK (entry_key_release_cb), sbt);
  gtk_entry_set_activates_default(GTK_ENTRY (entry), TRUE);
  gtk_container_add (GTK_CONTAINER (content), entry);
  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gdouble xv, yv;
  xv = yv = NAN;
  if (response == GTK_RESPONSE_ACCEPT) {
    const gchar *expr = gtk_entry_get_text (GTK_ENTRY (entry));
    gboolean rc = python_evaluate_point ((gchar *)expr, sheet,
					 &xv, &yv);
    if (rc && !isnan (xv) && !isnan (yv)) {
      if (pxip) *pxip = xv;
      if (pyip) *pyip = yv;
    }
  }
  gtk_widget_destroy (dialog); 
}
