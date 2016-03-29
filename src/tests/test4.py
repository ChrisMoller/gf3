def yyy (scale) :
  gf_line([ (5,4), (6*scale,6*scale), (5.7,1) ], filled=1)

def xxx (scale) :
  global yyy
  gf_circ (1,2,3*scale)
  yyy (scale)

xxx (2)
