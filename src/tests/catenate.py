tt=gf_translate_t(1,2)
ss=gf_scale_t(3,4)
rr=gf_rotate_t(gf_deg(45))
ts=gf_catenate(tt,ss)
st=gf_catenate(ss,tt)
sr=gf_catenate(ss,rr)
rs=gf_catenate(rr,ss)
rst=gf_catenate(rr,ss,tt)
