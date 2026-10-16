# ATI port census: which Mach8 ports and 28800 extended indices each ATI binary touches.
#   python tools/ati_port_census.py <file-list> references/ati_mach32/ports.txt <out.tsv>
# A byte-pattern scan (docs/mach8_levers.md): single hits can be data.
import sys,re,hashlib,os,collections
files=[l.strip() for l in open(sys.argv[1]) if l.strip()]
guide={}
for l in open(sys.argv[2],encoding='latin1'):
    m=re.match(r'([0-9A-F]{4})\s+\((\S+)\)\s*(.*)',l)
    if m: guide.setdefault(int(m.group(1),16),[]).append(m.group(3).strip())
seen={}; uniq=[]
for f in files:
    try: d=open(f,'rb').read()
    except Exception as e: continue
    h=hashlib.md5(d).hexdigest()
    if h in seen: continue
    seen[h]=f; uniq.append((f,d))
def group(f):
    n=os.path.basename(f).upper(); p=f.upper()
    if n=='TEST.COM': return 'TEST.COM'
    if n=='VVESA.COM': return 'VVESA'
    if n.endswith('.BIN'): return 'ROM'
    if 'MACHW3' in n: return 'MACHW3'
    if n.startswith('MACH') or n.startswith('W31-'): return 'MACH(W31)'
    if n.startswith('ULTRA'): return 'ULTRA'
    if 'ATIM32' in n: return 'ATIM32'
    if 'ATIM8' in n or (n.startswith('ATI') and n.endswith('.VXD')): return 'ATIM8/VXD'
    if 'ATIDESK' in p or 'FLEXDESK' in n: return 'ATIDESK'
    return 'tools'
ports=collections.defaultdict(set); idx=collections.defaultdict(set)
for f,d in uniq:
    g=group(f)
    for m in re.finditer(rb'(?:\xba|\x68|\xb8|\xbb|\xb9|\xbe|\xbf|\x3d)(.)(.)',d,re.S):
        lo,hi=m.group(1)[0],m.group(2)[0]
        if lo in (0xe8,0xee) and 0x02<=hi<=0xfe and (hi&3)==2:
            ports[(hi<<8)|lo].add(g)
    for m in re.finditer(rb'\xba\xce\x01',d):
        w=d[max(0,m.start()-8):m.end()+10]
        for mm in re.finditer(rb'(?:\xb0|\xb8)([\xa0-\xbf])',w):
            idx[mm.group(1)[0]].add(g)
G=['TEST.COM','ROM','VVESA','tools','MACH(W31)','ULTRA','MACHW3','ATIM8/VXD','ATIM32','ATIDESK']
out=open(sys.argv[3],'w')
print('unique files',len(uniq), collections.Counter(group(f) for f,_ in uniq))
out.write('port\t'+'\t'.join(G)+'\tguide\n')
for p in sorted(set(ports)|set(guide)):
    out.write(f'{p:04X}\t'+'\t'.join('x' if g in ports[p] else '.' for g in G)+'\t'+'; '.join(guide.get(p,[]))[:46]+'\n')
for i in sorted(idx):
    out.write(f'1CE:{i:02X}\t'+'\t'.join('x' if g in idx[i] else '.' for g in G)+'\t28800 ext\n')
