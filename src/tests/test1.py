for x in range(0, 3):
  gfig.Circle(x,2,1)

gfig.Circle(4, 3, .5, 1)
gfig.Circle(8.3, .5, [1, 2.2])
  
gfig.Line([ (1,2), (3,4), (2.5,1) ])
gfig.Line([ (1,4), (3,6), (2.7,1) ], closed=1)
gfig.Line([ (5,4), (6,6), (5.7,1) ], filled=1)

#gfig.IterSIC(1,2,3)
r = gfig.IterSIC(1.5,.5,5)
print(r)
gfig.Circle(8.3, .5, r)

gfig.IterSIC( (1,2), (3,4), 3)

