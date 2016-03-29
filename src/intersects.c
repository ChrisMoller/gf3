#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gtk/gtk.h>
#include <math.h>

#include "gf.h"
#include "utilities.h"
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_poly.h>
#include <gsl/gsl_complex.h>
#include "quartic.h"

/***
            text circle ellipse line 
   text
   circle         done     done  done
   ellipse        done     done  done
   line           done     done  done
   
 ***/

// fixme -- check that line intersections are on the segments

static GList *circle_circle_intersections (entity_circle_s *c0,
					   entity_circle_s *c1);

static GList *
storexy (GList *points, gdouble xn, gdouble yn)
{
  point_s *p0 = gfig_try_malloc0 (sizeof(point_s));
  point_x (p0) = xn; point_y (p0) = yn;
  return g_list_append (points, p0);
}

static GList *
ellipse_ellipse_intersections (entity_ellipse_s *el0, entity_ellipse_s *el1)
{
  GList *points = NULL;

  if (entity_ellipse_a (el1) == entity_ellipse_b (el1)) {
    // fixes a bug in the quartic code
    entity_ellipse_s *ttt = el0;
    el0 = el1;
    el1 = ttt;
  }
  gdouble ax = entity_ellipse_x (el0);
  gdouble ay = entity_ellipse_y (el0);
  gdouble aa = entity_ellipse_a (el0);
  gdouble ab = entity_ellipse_b (el0);
  gdouble at = entity_ellipse_t (el0);
  //  g_print ("a: %g %g %g %g %g\n", ax, ay, aa, ab, at);
  
  gdouble bx = entity_ellipse_x (el1);
  gdouble by = entity_ellipse_y (el1);
  gdouble ba = entity_ellipse_a (el1);
  gdouble bb = entity_ellipse_b (el1);
  gdouble bt = entity_ellipse_t (el1);
  //  g_print ("b: %g %g %g %g %g\n", bx, by, ba, bb, bt);

  // degenerate
  if (aa == 0.0 && ab == 0.0 && ba == 0.0 && bb == 0.0) return NULL;

  if (aa == ab && ba == bb) { 			// two circles
    entity_circle_s *c0 = gfig_try_malloc0 (sizeof(entity_circle_s));
    entity_circle_x (c0) = entity_ellipse_x (el0);
    entity_circle_y (c0) = entity_ellipse_y (el0);
    entity_circle_r (c0) = entity_ellipse_a (el0);
    
    entity_circle_s *c1 = gfig_try_malloc0 (sizeof(entity_circle_s));
    entity_circle_x (c1) = entity_ellipse_x (el1);
    entity_circle_y (c1) = entity_ellipse_y (el1);
    entity_circle_r (c1) = entity_ellipse_a (el1);

    points = circle_circle_intersections (c0, c1);

    g_free (c0);
    g_free (c1);
    
    return points;
  }

  gdouble X = bx;
  gdouble Y = by;

  {
    // translate the system such that the centre of el0 is at the origin
    // and rotate so that el0 is ortho to the axes
    
    cairo_matrix_t matrix; 
    cairo_matrix_init_rotate (&matrix, at);
    cairo_matrix_translate (&matrix, -ax, -ay);
    cairo_matrix_transform_point (&matrix, &X, &Y);
  }

  /***
       origin ctrd, ortho, ellipse el0
					    
       x^2 / a^2   +   y^2 / b^2  =  1       Eq  1

       A = a^2
       B = b^2
          
  ****/

  gdouble A = pow (aa, 2.0);
  gdouble B = pow (ab, 2.0);

  /****
       x^2 / A  +  y^2 / B  = 1             Eq  2, from Eq 1
   
       B x^2  +  A y^2    =  AB             Eq  3, from Eq 2
           
       A y^2  =  AB  -  B x^2               Eq  4, from Eq 3
           
       y^2    =   B  -  (B/A) x^2           Eq  5, from Eq 4
           
       y      = sqrt(B  -  (B/A) x^2)       Eq  6, from Eq 5

  ****/

  /***
      [X Y] origin, non-ortho, ellipse el1
                                        
      v^2 / c^2   +   w^2 / d^2  =  1         Eq  7

      where v and w are vectors ortho to the ellipse axes
        
      C = c^2
      D = d^2
   
  ****/

  gdouble C = pow (ba, 2.0);
  gdouble D = pow (bb, 2.0);

   /***    

      v^2 / C  +  w^2 / D  =  1              Eq  8, from Eq 7
         
      D v^2   +  C w^2     =  CD             Eq  9, from Eq 8
   
      [v w] = ([x y] - [X Y]) rot theta  [v w] is a rotation of [x y] around
                                            the center of ellipse 2
         
      v = (x - X) cos T + (y - Y) sin T      Eq 10
      w = (x - X) sin T - (y - Y) cos T      Eq 11
   
      COS = cos T
      SIN = sin T
   
   ****/

  gdouble COS = cos (at - bt);
  gdouble SIN = sin (at - bt);

  /****
      v = x COS - X COS  +  y SIN - Y SIN          Eq 12, from Eq 10
      w = x SIN - X SIN  -  y COS + Y COS          Eq 13, from Eq 11
           
      v = (x COS + y SIN)  -  (X COS + Y SIN)      Eq 14, from Eq 12
      w = (x SIN - y COS)  -  (X SIN - Y COS)      Eq 15, from Eq 13
           
      KV = X COS + Y SIN
      KW = X SIN - Y COS

  ****/

  gdouble KV = (X * COS) + (Y * SIN);
  gdouble KW = (X * SIN) - (Y * COS);

  /****

   v = (x COS + y SIN) -  KV            Eq 16, from Eq 14
   w = (x SIN - y COS) -  KW            Eq 17, from Eq 15
   
   
   D (x COS + y SIN -  KV)^2   +
   C  (x SIN - y COS -  KW)^2  =  CD    Eq 18, from Eq  9
   
   D ( COS^2 x^2 + COS SIN xy - COS KV x        Eq 19, from Eq 18
                 + COS SIN xy             + SIN^2 y^2 - SIN KV y
                              - COS KV x              - SIN KV y    + KV^2) +
   C ( SIN^2 x^2 - COS SIN xy - SIN KW x
                 - COS SIN xy             + COS^2 y^2 + COS KW y
                              - SIN KW x              + COS KW y    + KW^2) = CD
   
    + (   D COS^2    +    C SIN^2   ) x^2       Eq 21, from Eq 20
    - ( 2 D COS KV   +  2 C SIN KW  )  x
    + ( 2 D COS SIN  -  2 C COS SIN ) xy
    - ( 2 D SIN KV   -  2 C COS KW  )  y
    + (   D SIN^2    +    C COS^2   ) y^2
    + (   D KV^2     +    C KW^2  - CD )  = 0
           
  ****/

  gdouble E =  (      D * pow(COS, 2.0) +       C * pow(SIN, 2.0) );
  gdouble F = -(2.0 * D * COS * KV      + 2.0 * C * SIN * KW      );
  gdouble G =  (2.0 * D * COS * SIN     - 2.0 * C * COS * SIN     );
  gdouble H = -(2.0 * D * SIN * KV      - 2.0 * C * COS * KW      );
  gdouble K =  (      D * pow(SIN, 2.0) +       C * pow(COS, 2.0) );
  gdouble L =  (    ( D * pow(KV, 2.0)  +       C * pow(KW, 2.0) ) - C * D);

  /****

  E x^2  +  F x  +  G xy  +  H y  +  K y^2  +  L  = 0   Eq 22, from Eq 21
   
   
         E x^2
      +  F x
      +  K (B - (B/A) x^2)
      +  L =  -  G x sqrt(B - (B/A) x^2)                Eq 23, from Eq 22
              -  H sqrt(B - (B/A) x^2)
   
   
   
       E x^2
    +  F x
    +  K B - K(B/A) x^2
    +  L =  -  (G x + H) sqrt(B - (B/A) x^2)            Eq 23, from Eq 22
   
   
   
       (E - KB/A) x^2
    +  F x
    +  (KB +  L) = -  (G x + H) sqrt(B - (B/A) x^2)     Eq 23, from Eq 22

  ***/

  gdouble M = E - (K * B / A);
  gdouble N = (K * B) + L;

  /****
  M x^2  +  F x  +  N  =  -(G x + H) sqrt(B - (B/A) x^2)   Eq 27, from Eq 26
   
  (M x^2  +  F x  +  N)^2  =  (G x + H)^2 (B - (B/A) x^2)  Eq 28, from Eq 27
   
   M^2 x^4    +     FM x^3         +    MN x^2             Eq 29, from Eq 28
              +     FM x^3         +   F^2 x^2  +   FN x          
                                   +    MN x^2  +   FN x  + N^2
                  = (G^2 x^2  + 2GH x  + H^2)  (B - (B/A) x^2)
   
                                                           Eq 30, from Eq 29
     M^2 x^4  +      2FM x^3       +  (2MN + F^2) x^2  +  2FN x  + N^2
                                   -  B G^2 x^2        - 2BGH x  - B H^2
  + (B/A) G^2 x^4  + 2(B/A)GH x^3  +  (B/A) H^2 x^2  =  0
  
  ****/

  gdouble P = pow(M, 2.0)  +  (B/A) * pow(G, 2.0);
  gdouble Q = 2.0 * F * M  +  2.0 * (B/A) * G * H;
  gdouble R = (2.0 * M * N  +  pow(F, 2.0) +
	       (B/A) * pow(H, 2.0)) - B * pow(G, 2.0);
  gdouble S = 2.0 * F * N  -  2.0 * B * G * H;
  gdouble T = pow(N, 2.0)  -  B * pow(H, 2.0);

  gsl_complex x[4];
  gint n =
    gsl_poly_complex_solve_quartic (Q/P, R/P, S/P, T/P,
				    &x[0], &x[1], &x[2], &x[3]);
#if 0
  g_print ("%d [%g %g] [%g %g] [%g %g] [%g %g]\n",
	   n,
	   GSL_REAL (x[0]), GSL_IMAG (x[0]),
	   GSL_REAL (x[1]), GSL_IMAG (x[1]),
	   GSL_REAL (x[2]), GSL_IMAG (x[2]),
	   GSL_REAL (x[3]), GSL_IMAG (x[3]));
#endif
  {
    cairo_matrix_t matrix;  // fixme -- put translate here too
    cairo_matrix_init_translate (&matrix, ax, ay);
    cairo_matrix_rotate (&matrix, -at);
    
    for (gint i = 0; i < n; i++) {
      // test to find real or near-real roots
      gdouble ang1 = fabs (atan2 (GSL_IMAG (x[i]), GSL_REAL (x[i])));
      if (ang1 <= 0.01 || fabs (ang1 - G_PI) <= 0.01) {
	gdouble txp = GSL_REAL (x[i]);
	gdouble txn = txp;
	gdouble typ = sqrt (B - (B/A) * pow (txp, 2.0));  // from Eq 6
	gdouble tyn = -typ;
	gdouble ang = at - bt;
	gdouble vp = (txp - X) * cos (ang) + (typ - Y) * sin (ang);
	gdouble wp = (txp - X) * sin (ang) - (typ - Y) * cos (ang);
	gdouble vn = (txn - X) * cos (ang) + (tyn - Y) * sin (ang);
	gdouble wn = (txn - X) * sin (ang) - (tyn - Y) * cos (ang);
	
	gdouble dp = (vp * vp) / C  +  (wp * wp) / D;
	gdouble dn = (vn * vn) / C  +  (wn * wn) / D;

	if (fabs (dp - 1.0) < 0.01) {
	  cairo_matrix_transform_point (&matrix, &txp, &typ);
	  points = storexy (points, txp, typ);
#if 0
#if 1
	  g_print ("point [%g %g]\n", txp, typ);
#else
	  g_print ("[%d] = [%g, %g] p = [%g, %g] (%g)\n",
		   i, GSL_REAL (x[i]), GSL_IMAG (x[i]),
		   txp, typ, dp);
#endif
#endif
	}
	if (fabs (dn - 1.0) < 0.01) {
	  cairo_matrix_transform_point (&matrix, &txn, &tyn);
	  points = storexy (points, txn, tyn);
#if 0
#if 1
	  g_print ("point [%g %g]\n", txn, tyn);
#else
	  g_print ("[%d] = [%g, %g] p = [%g, %g] (%g)\n",
		   i, GSL_REAL (x[i]), GSL_IMAG (x[i]),
		   txn, tyn, dn);
#endif
#endif
	}
      }
    }
  }

  

  
  return points;
}


static GList *
ellipse_line_intersections (entity_ellipse_s *c0, entity_polyline_s *c1)
{
  GList *points = NULL;
  
  gdouble X = entity_ellipse_x (c0);
  gdouble Y = entity_ellipse_y (c0);
  gdouble A = entity_ellipse_a (c0);
  gdouble B = entity_ellipse_b (c0);
  gdouble T = entity_ellipse_t (c0);
  GList *p1 = entity_polyline_verts (c1);

  if (A == 0.0 || B == 0.0) return NULL;

/***
  x^2 / A^2  +  y^2 / B^2  = 1
  K = 1 / A^2
  L = 1 / B^2

  K x^2 + L y^2 = 1

  ax + by = c

    y = (c - ax) /  b                   x = (c - by) / a

    K x^2 + L (c - ax)^2 / b^2 = 1     	K (c - by)^2 / a^2 + L y^2 = 1

    K b^2 x^2 + L (c - ax)^2 = b^2      K (c - by)^2 + L a^2 y^2 = a^2

    K b^2 x^2 + L c^2 - 2acLx + L a^2 x^2 = b^2

                K c^2 - 2bcKy + K b^2 y^2 + L a^2 y^2 = a^2

    (K b^2 + L a^2) x^2 - 2acLx + (L c^2 - b^2) = 0
    M = K b^2 + L a^2
    N = -2acL
    P = L c^2 - b^2

            (K b^2 + L a^2) y^2 - 2bcKy + (K c^2 - a^2) = 0
            M = K b^2 + L a^2
            N = -2bcK
            P = K c^2 - a^2

***/

  cairo_matrix_t matrix;
  cairo_matrix_t rmatrix;
  cairo_matrix_init_rotate (&matrix, T);
  cairo_matrix_translate (&matrix, -X, -Y);
  cairo_matrix_init_translate (&rmatrix, X, Y);
  cairo_matrix_rotate (&rmatrix, -T);
  
  if (p1 && g_list_length (p1) >= 2) {
    gdouble K = 1.0 / (A * A);
    gdouble L = 1.0 / (B * B);
  
    for (int i1 = 1; i1 < g_list_length (p1); i1++) {
      gdouble xp, yp, xn, yn;
      point_s *pt1 = g_list_nth_data (p1, i1 - 1);
      gdouble x1 = point_x (pt1); gdouble y1 = point_y (pt1);
      point_s *pt2 = g_list_nth_data (p1, i1);
      gdouble x2 = point_x (pt2); gdouble y2 = point_y (pt2);
      
      cairo_matrix_transform_point (&matrix, &x1, &y1);
      cairo_matrix_transform_point (&matrix, &x2, &y2);

      gdouble a = y2 - y1;
      gdouble b = x1 - x2;
      gdouble c = x1 * y2 - x2 * y1;
      gboolean doit;

      doit = FALSE;
      gdouble M = K * b * b + L * a * a;
      
      if (M != 0.0) {
	gdouble N, P, D;
#if 1		// either of these should work
	N = -2.0 * a * c * L;
	P = (L * c * c) - (b * b);

	D = (N * N) - (4.0 * M * P);
	if (D >= 0.0) {
          D = sqrt (D);
	  xp = (-N + D) / (2.0 * M);
	  xn = (-N - D) / (2.0 * M);
	  yp = (c - (a * xp)) /  b;
	  yn = (c - (a * xn)) /  b;
	  doit = TRUE;
        }
#else
	N = -2.0 * b * c * K;
	P = (K * c * c) - (a * a);

	D = (N * N) - (4.0 * M * P);
	if (D >= 0.0) {
	  D = sqrt (D);
	  yp = (-N + D) / (2.0 * M);
	  yn = (-N - D) / (2.0 * M);
	  xp = (c - (b * yp)) / a;
	  xn = (c - (b * yn)) / a;
	  doit = TRUE;
	}
#endif
	if (doit) {
	  cairo_matrix_transform_point (&rmatrix, &xp, &yp);
	  cairo_matrix_transform_point (&rmatrix, &xn, &yn);
	  points = storexy (points, xp, yp);
	  points = storexy (points, xn, yn);
	}
      }
    }
  }
  
  return points;
}

static GList *
circle_line_intersections (entity_circle_s *c0, entity_polyline_s *c1)
{
  GList *points = NULL;
  
  gdouble X0 = entity_circle_x (c0);
  gdouble Y0 = entity_circle_y (c0);
  gdouble R0 = entity_circle_r (c0);
  GList *p1 = entity_polyline_verts (c1);

  if (p1 && g_list_length (p1) >= 2) {
    for (int i1 = 1; i1 < g_list_length (p1); i1++) {
      point_s *pt1 = g_list_nth_data (p1, i1 - 1);
      gdouble x1 = point_x (pt1); gdouble y1 = point_y (pt1);
      point_s *pt2 = g_list_nth_data (p1, i1);
      gdouble x2 = point_x (pt2); gdouble y2 = point_y (pt2);

      x1 -= X0; y1 -= Y0;
      x2 -= X0; y2 -= Y0;

      gdouble a = y2 - y1;
      gdouble b = x1 - x2;
      gdouble c = x1 * y2 - x2 * y1;
      gdouble D = a * a + b * b;
      if (D != 0.0) {
	gdouble K = D * R0 * R0 - c * c;
	if (K >= 0.0) {
	  gdouble xp = X0 + (a * c + b * sqrt (K)) / D;
	  gdouble xn = X0 + (a * c - b * sqrt (K)) / D;
	  gdouble yp = Y0 + (b * c - a * sqrt (K)) / D;
	  gdouble yn = Y0 + (b * c + a * sqrt (K)) / D;

	  points = storexy (points, xp, yp);
	  points = storexy (points, xn, yn);
	}
      }
    }
  }
  return points;
}

static GList *
line_line_intersections (entity_polyline_s *c0, entity_polyline_s *c1)
{
  GList *points = NULL;
  
  GList *p0 = entity_polyline_verts (c0);
  GList *p1 = entity_polyline_verts (c1);

  // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection#Given_two_points_on_each_line
  
  if (p0 && g_list_length (p0) >= 2 && p1 && g_list_length (p1) >= 2) {

    for (int i0 = 1; i0 < g_list_length (p0); i0++) {
      point_s *pt1 = g_list_nth_data (p0, i0 - 1);
      gdouble x1 = point_x (pt1); gdouble y1 = point_y (pt1);
      point_s *pt2 = g_list_nth_data (p0, i0);
      gdouble x2 = point_x (pt2); gdouble y2 = point_y (pt2);

      for (int i1 = 1; i1 < g_list_length (p1); i1++) {
	point_s *pt3 = g_list_nth_data (p1, i1 - 1);
	gdouble x3 = point_x (pt3); gdouble y3 = point_y (pt3);
	point_s *pt4 = g_list_nth_data (p1, i1);
	gdouble x4 = point_x (pt4); gdouble y4 = point_y (pt4);

	gdouble D = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

	if (D != 0) {
	  gdouble Px =
	    (x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4);
	  gdouble Py =
	    (x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4);
	  Px /= D;
	  Py /= D;
	  points = storexy (points, Px, Py);
	}
      }
    }
  }
  return points;
}

static GList *
circle_circle_intersections (entity_circle_s *c0, entity_circle_s *c1)
{
  /***
  D = hypot((Y1 - Y0), (X1 - X))
  T = atan2((Y1 - Y0), (X1 - X))

x^2        + y^2 = R^2
             y^2 = R^2 - x^2
             y   = +/- sqrt(R^2 - x^2)

(x - D)^2  + y^2 = P^2
(x - D)^2  + (R^2 - x^2) = P^2
x^2 - 2Dx + D^2 + R^2 - x^2 = P^2
-2Dx = (P^2 - R^2 - D^2)
   x = -(P^2 - R^2 -D^2) / 2D
   y = +/- sqrt(R^2 - x^2)

  ***/

  GList *points = NULL;
  gdouble X0 = entity_circle_x (c0);
  gdouble Y0 = entity_circle_y (c0);
  gdouble R0 = entity_circle_r (c0);
  gdouble X1 = entity_circle_x (c1);
  gdouble Y1 = entity_circle_y (c1);
  gdouble R1 = entity_circle_r (c1);

  gdouble D = hypot ((Y1 - Y0), (X1 - X0));

  if (fabs (D) > (R0 + R1) || D == 0.0) return NULL;

  gdouble xp = -(R1 * R1 -(R0 * R0 + D * D)) / (2.0 * D);
  gdouble xn = xp;

  gdouble yy = R0 * R0 - xp * xp;

  if (yy < 0.0) return NULL;

  gdouble yp = sqrt (yy);
  gdouble yn = -yp;
  
  gdouble T = atan2 ((Y1 - Y0), (X1 - X0));

  cairo_matrix_t matrix;
  cairo_matrix_init_translate (&matrix, X0, Y0);
  cairo_matrix_rotate (&matrix, T);
  cairo_matrix_transform_point (&matrix, &xp, &yp);
  cairo_matrix_transform_point (&matrix, &xn, &yn);

  points = storexy (points, xp, yp);
  points = storexy (points, xn, yn);
  return points;
}

static GList *
line_intersections (entity_polyline_s *c0, gpointer e1)
{
  GList *points = NULL;
  entity_type_e t1 = entity_type (e1);
  
  switch (t1) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_GROUP:
    break;
  case ENTITY_TYPE_TEXT:
    break;
  case ENTITY_TYPE_CIRCLE:
    points = circle_line_intersections ((entity_circle_s *)e1, c0);
    break;
  case ENTITY_TYPE_ELLIPSE:
    points = ellipse_line_intersections ((entity_ellipse_s *)e1, c0);
    break;
  case ENTITY_TYPE_POLYLINE:
    points = line_line_intersections (c0, (entity_polyline_s *)e1);
    break;
  case ENTITY_TYPE_TRANSFORM:
    break;
  }
  return points;
}

static GList *
circle_intersections (entity_circle_s *c0, gpointer e1)
{
  GList *points = NULL;
  entity_type_e t1 = entity_type (e1);
  
  switch (t1) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_GROUP:
    break;
  case ENTITY_TYPE_TEXT:
    break;
  case ENTITY_TYPE_CIRCLE:
    points = circle_circle_intersections (c0, (entity_circle_s *)e1);
    break;
  case ENTITY_TYPE_ELLIPSE:
    {
      entity_ellipse_s *eo = gfig_try_malloc0 (sizeof(entity_ellipse_s));
      entity_ellipse_x (eo) = entity_circle_x (c0);
      entity_ellipse_y (eo) = entity_circle_y (c0);
      entity_ellipse_a (eo) = entity_circle_r (c0);
      entity_ellipse_b (eo) = entity_circle_r (c0);
      entity_ellipse_t (eo) = 0.0;

      points = ellipse_ellipse_intersections (eo, (entity_ellipse_s *)e1);

      g_free (eo);
    }
    break;
  case ENTITY_TYPE_POLYLINE:
    points = circle_line_intersections (c0, (entity_polyline_s *)e1);
    break;
  case ENTITY_TYPE_TRANSFORM:
    break;
  }
  return points;
}

static GList *
ellipse_intersections (entity_ellipse_s *c0, gpointer e1)
{
  GList *points = NULL;
  entity_type_e t1 = entity_type (e1);
  
  switch (t1) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_GROUP:
    break;
  case ENTITY_TYPE_TEXT:
    break;
  case ENTITY_TYPE_CIRCLE:
    {
      entity_circle_s *ci = e1;
      entity_ellipse_s *eo = gfig_try_malloc0 (sizeof(entity_ellipse_s));
      entity_ellipse_x (eo) = entity_circle_x (ci);
      entity_ellipse_y (eo) = entity_circle_y (ci);
      entity_ellipse_a (eo) = entity_circle_r (ci);
      entity_ellipse_b (eo) = entity_circle_r (ci);
      entity_ellipse_t (eo) = 0.0;

      points = ellipse_ellipse_intersections (c0, eo);

      g_free (eo);
    }
    break;
  case ENTITY_TYPE_ELLIPSE:
    points = ellipse_ellipse_intersections (c0, (entity_ellipse_s *)e1);
    break;
  case ENTITY_TYPE_POLYLINE:
    points = ellipse_line_intersections (c0, (entity_polyline_s *)e1);
    break;
  case ENTITY_TYPE_TRANSFORM:
    break;
  }
  return points;
}

GList *
do_intersect (gpointer e0, gpointer e1)
{
  GList *points = NULL;
  entity_type_e t0 = entity_type (e0);

  switch (t0) {
  case ENTITY_TYPE_NONE:
    break;
  case ENTITY_TYPE_GROUP:
    break;
  case ENTITY_TYPE_TEXT:
    break;
  case ENTITY_TYPE_CIRCLE:
    points = circle_intersections ((entity_circle_s *)e0, e1);
    break;
  case ENTITY_TYPE_ELLIPSE:
    points = ellipse_intersections ((entity_ellipse_s *)e0, e1);
    break;
  case ENTITY_TYPE_POLYLINE:
    points = line_intersections ((entity_polyline_s *)e0, e1);
    break;
  case ENTITY_TYPE_TRANSFORM:
    break;
  }
  return points;
}
