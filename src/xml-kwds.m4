dnl generator for xml-kwds.m4
define(`RQ',`changequote(<,>)dnl`
'changequote`'')
dnl start at 1000 just so we don't have to with a null pointer
define(`offset', 1000)dnl
define(`cnt',    offset)dnl
define(`xinc', `define(`$1',incr($1))')dnl
define(`upcase', `translit($1, `a-z', `A-Z')')dnl
define(`entry', ``#'define upcase($1) "$1"
`#'define `KWD_'upcase($1) cnt xinc(`cnt')
divert(1)  `{' upcase($1), `KWD_'upcase($1) `},'
divert(0)'
)dnl

`/********* DON'RQ()`T MODIFY THIS FILE! ********/'
`/**** Make all changes in xml-kwds.m4. ****/'

#ifndef XML_KWDS_H
#define XML_KWDS_H

entry(aaxis)
entry(alignment)
entry(alpha)
entry(angle)
entry(baxis)
entry(blue)
entry(centre)
entry(circle)
entry(closed)
entry(colour)
entry(drawing)
entry(drawing_unit)
entry(ellipse)
entry(environment)
entry(filled)
entry(font)
entry(green)
entry(grid)
entry(inch)
entry(justify)
entry(landscape)
entry(lead)
entry(left)
entry(linestyle)
entry(linewidth)
entry(lwidx)
entry(millimetre)
entry(name)
entry(negative)
entry(no)
entry(orientation)
entry(paper)
entry(papersize)
entry(parent)
entry(pen)
entry(point)
entry(polyline)
entry(portrait)
entry(project)
entry(radius)
entry(red)
entry(right)
entry(sheet)
entry(show)
entry(snap)
entry(size)
entry(spline)
entry(spread)
entry(start)
entry(stop)
entry(string)
entry(text)
entry(transform)
entry(unit)
entry(unset)
entry(value)
entry(width)
entry(x)
entry(xx)
entry(xy)
entry(x0)
entry(y)
entry(yy)
entry(yx)
entry(y0)
entry(yes)

`typedef struct {
  gchar *keyword;
  gint   keyvalue;
} keyword_s;

keyword_s keywords[] = {'
undivert
`};
gint nr_keys =' eval(cnt - offset)`;'
#endif /* XML_KWDS_H*/
