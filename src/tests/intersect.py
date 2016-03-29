c0 = gf_circle_t(3, 4, 3)
c1 = gf_circle_t(1, 3, 3)
i0 = gf_intersect(c0, c1)
if (len (i0) >= 2) :
  l0=gf_line_t(i0)
  o0 = (c0, c1, l0)
else:
  o0 = (c0, c1)
gf_draw(o0)

xx = gf_translate_t (4, 8, gf_scale_t (1, -1))
gf_draw(o0, xx)


v0 = gf_line_t ( [ (1,8), (4,9), (7,8) ] )
v1 = gf_line_t ( [ (1,9), (4,8), (7,9) ] )
x0 = gf_intersect(v0, v1)
if (len (x0) >= 2) :
  m0=gf_line_t(x0)
  n0 = (v0, v1, m0)
else:
  n0 = (v0, v1)
gf_draw(n0)

r0 = gf_circle_t (3, 12, 2)
s0 = gf_line_t ( [ (0.5,11.5), (5, 13.5) ] )
y0 = gf_intersect (r0, s0)
gf_spw(3.5)
if (len (y0) >= 2) :
  q0=gf_line_t(y0)
  w0 = (r0, s0, q0)
else:
  w0 = (r0, s0)
gf_draw(w0)

gf_spw(0.5)
er0 = gf_ellcabt_t (8, 12, 1.6, 2, gf_deg(130))
#er0 = gf_ellcabt_t (8, 12, 1.6, 2, gf_deg(0))
es0 = gf_line_t ( [ (5.5,11.5), (10, 13.5) ] )
ey0 = gf_intersect (es0, er0)
gf_spw(3.5)
if (len (ey0) >= 2) :
  eq0=gf_line_t(ey0)
  ew0 = (er0, es0, eq0)
else:
  ew0 = (er0, es0)
gf_draw(ew0)

