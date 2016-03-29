
redpen = gf_pen_t(colour="red", width = 1.5)
gf_circle(8.3, .5, gf_sic(1.5, .5, 5), start = gf_deg(30), 
  stop = gf_deg(180), negative = False, pen=redpen)
gf_spw(1.5)

gf_ellcabt (5, 4, gf_sic(1, .3, 3), 2, gf_deg(45), start = gf_deg(25), 
  stop =  gf_deg(64), negative = True)

dd0 = gf_ellcabt_t (5, 8, 1,    2, gf_deg(-45), filled=1)
dd1 = gf_ellcabt_t (5, 8, 1.3, 2, gf_deg(-45), filled=1)
dd2 = gf_ellcabt_t (5, 8, 1.6, 2, gf_deg(-45), filled=1)
gf_draw(dd0, dd1, dd2, tf = gf_scale_t (.6, .6))
gf_draw(dd0, dd1, dd2)

ee = gf_ellffae_t (4, 8, 5, 7, 3, .7, start = gf_deg(25), stop =  gf_deg(64))
gf_draw(ee)

gf_line([ (1,2), (3,4), (2.5,1) ])

gf_line([ (1,4), (3,6), (2.5,3) ], filled=1, pen = redpen)

gf_line([ (3,2), (5,4), (4.5,1) ], closed=1)

#gf_spw(0.05)
gf_text(4, 14,   "a string of text",
  txtsize = 0.7, angle = gf_deg(60), pen = redpen)
