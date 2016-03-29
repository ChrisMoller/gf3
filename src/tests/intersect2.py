
gf_spw(1.5)


fr0 = gf_ellcabt_t (5, 5, 4, 3, gf_deg(30))
fs0 = gf_ellcabt_t (5, 5, 3, 5, gf_deg(20))
fy0 = gf_intersect (fs0, fr0)
gf_spw(3.5)
if (len (fy0) >= 4) :
  fq0=gf_line_t(fy0, closed=True)
  fw0 = (fr0, fs0, fq0)
else:
  fw0 = (fr0, fs0)

gf_draw(fw0)


gf_spw(1.5)
er0 = gf_ellcabt_t (5, 11, 4, 3, gf_deg(30))
es0 = gf_ellcabt_t (5, 12, 3, 4, gf_deg(-40))
ey0 = gf_intersect (es0, er0)
gf_spw(3.5)
if (len (ey0) >= 2) :
  eq0=gf_line_t(ey0)
  ew0 = (er0, es0, eq0)
else:
  ew0 = (er0, es0)
gf_draw(ew0)

