import sys,re
runs={};cur=None
for l in open(sys.argv[1]):
    m=re.match(r'== (modex\w*) (t4b_\w+)',l)
    if m: cur=m.group(2); runs[cur]={}; continue
    m=re.match(r'(mx_\w+)\s+([\d.]+) ops/s',l)
    if m and cur: runs[cur][m.group(1)]=float(m.group(2))
tests=['mx_fill','mx_frame','mx_box','mx_same','mx_lock','mx_flip','mx_flipblt']
order=['t4b_x1','t4b_x2','t4b_s1','t4b_s2','t4b_d1','t4b_d2']
print('test        '+' '.join(f'{r[4:]:>7}' for r in order)+'   s/x    d/x')
for t in tests:
    v=[runs.get(r,{}).get(t,0) for r in order]
    x=(v[0]+v[1])/2; s=(v[2]+v[3])/2; d=(v[4]+v[5])/2
    print(f'{t:11} '+' '.join(f'{a:7.2f}' for a in v)+f'  {s/x:5.2f}  {d/x:5.2f}')
