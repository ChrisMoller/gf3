
gf_spw(1.5)

dd0 = gf_ellcabt_t (5, 8, 1,    2, gf_deg(-45), filled=0)
dd1 = gf_ellcabt_t (5, 8, 1.3, 2, gf_deg(-45), filled=0)
dd2 = gf_ellcabt_t (5, 8, 1.6, 2, gf_deg(-45), filled=0)

gf_draw (dd0, dd1, dd2, tf = gf_scale_t (1.6, 1.6))

gr = gf_group_t (dd0, dd1, dd2, centre = gf_point_t (5, 8))
gf_draw (gr)

#ee = gf_ellffae_t (4, 8, 5, 7, 3, .7, start = gf_deg(25), stop =  gf_deg(64))
#gf_draw(ee)
