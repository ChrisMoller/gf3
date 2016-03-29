#for x in range(0, 3):
#  gf_circle(x,2,1)

#gf_circ(4, 3, .5, 1)
#gf_circ(8.3, .5, [1, 2.2])
  
#gf_pline([ (1,2), (3,4), (2.5,1) ])
#gf_pline([ (1,4), (3,6), (2.7,1) ], closed=1)
#gf_pline([ (5,4), (6,6), (5.7,1) ], filled=1)
#gf_pline([ (5,4), (6,6), (5.7,1) ])
gf_pline([ (5,4), (6,6), (5.7,1) ], intersection=GF_INTERSECT_ARC, radius = 0.1)

#r = gf_sic(1.5,.5,5)
#print(r)
#gf_circ(8.3, .5, r)


x = gf_sic(2.5,.5,5)
y = gf_sic(2.5,.5,5)
gf_ellcabt(x,y,2,1,.3)

