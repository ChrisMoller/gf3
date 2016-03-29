gf_spw(1.0)

#for x in range (1, 9) :
#    gf_text(x,  x,   "M | hello",  txtsize = x/10.0, filled = 1, 
#      angle = gf_deg (x * 10.0))
 

aaa = "a string\nsecond line"
aab = "a several line string\nsecond line xxxx yyyy\nthird line wwww zzzz"

#gf_text(0, 11,   aaa,  txtsize = 0.8, filled = 1, angle = gf_deg(0), 
#   font = "garbage", align = PANGO_ALIGN_CENTRE, justify = True)

#gf_text(0, 13,   aab,  txtsize = 0.6, filled = 1, angle = gf_deg(0), 
#    justify = True, spread = 30, lead = 15)
gf_text(0, 13,   aab,  txtsize = 0.6, filled = 1, angle = gf_deg(0), 
    spread = 30)
gf_text(0, 8,   aab,  txtsize = 0.6, filled = 1, angle = gf_deg(0), 
    lead = 0)
