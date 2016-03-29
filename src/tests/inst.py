tt0 = gf_circle_t (0, 0, 1)
tt1 = gf_circle_t (2.5, 0, 1)
tt2 = gf_circle_t (0, 2.5, 1)
txt = gf_text_t (1,1, "text", txtsize = 0.6)
tt = (tt0, tt1, tt2, txt)

xx = gf_scale_t(2, 2, gf_rotate_t (gf_deg(90), gf_translate_t (8, 5)))
gf_draw (tt, tf = xx)

yy = gf_scale_t(2, 2, gf_rotate_t (gf_deg(45), gf_translate_t (2, 5)))
gf_draw (tt0, transform = yy)

