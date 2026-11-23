import sys
S=[l.split() for l in open(sys.argv[1]) if l.startswith('S ')]
ms=int(open(sys.argv[1]).readline().split()[6]); n=len(S); B=int(sys.argv[2])
def k(r):
    if r[1]!='0137': return '16bit'
    e=r[2]
    if e=='bff74269': return 'lockwait'
    if e.startswith('b0c'): return 'ours32'
    if e.startswith('566'): return 'd3dim'
    if e.startswith('baa') or e.startswith('bab'): return 'ddraw'
    if e.startswith('bff'): return 'kernel'
    return 'other'
cols=['16bit','lockwait','ours32','d3dim','ddraw','kernel','other']
print('secs      '+' '.join(f'{c:>8}' for c in cols))
for b in range(B):
    rows=S[b*n//B:(b+1)*n//B]
    c={x:0 for x in cols}
    for r in rows: c[k(r)]+=1
    print(f'{b*ms/B/1000:5.0f}-{(b+1)*ms/B/1000:3.0f} '+' '.join(f'{100*c[x]/len(rows):7.0f}%' for x in cols))
