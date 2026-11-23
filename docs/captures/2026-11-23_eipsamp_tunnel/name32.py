import re,collections,bisect,sys
lstdir,mapf,samp,frac=sys.argv[1],sys.argv[2],sys.argv[3],float(sys.argv[4])
pub={}
for l in open(mapf):
    m=re.match(r'^(b0c[0-9a-f]{5})[+*]?\s+(\S+)',l)
    if m: pub[m.group(2)]=int(m.group(1),16)
def labels(f):
    r=[];seg=None
    for l in open(f,errors='replace'):
        m=re.match(r'^Segment: (\S+)',l)
        if m: seg=m.group(1); continue
        if seg!='_TEXT': continue
        m=re.match(r'^([0-9A-F]{4,5})\s+([A-Za-z_][\w@$]*):\s*$',l)
        if m and not m.group(2).startswith('L$'): r.append((int(m.group(1),16),m.group(2)))
    return r
A=labels(lstdir+'/m8dd32.lst'); B=labels(lstdir+'/m8d3d.lst')
ba=[pub[n]-o for o,n in A if n in pub][0]; bb=[pub[n]-o for o,n in B if n in pub][0]
syms=sorted([(ba+o,n) for o,n in A]+[(bb+o,n) for o,n in B]); keys=[k for k,_ in syms]
S=[l.split() for l in open(samp) if l.startswith('S ')]
S=S[int(len(S)*frac):]
c=collections.Counter()
for r in S:
    e=int(r[2],16)
    if 0xb0c00000<=e<0xb0d00000: c[syms[bisect.bisect_right(keys,e)-1][1]]+=1
tot=sum(c.values()); print('samples',len(S),'ours32',tot, f'{100*tot/len(S):.0f}%')
for n,k in c.most_common(int(sys.argv[5]) if len(sys.argv)>5 else 30): print(f'{k:6d} {100*k/len(S):5.1f} % {n}')
