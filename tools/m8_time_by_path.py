"""Split timed entry calls by the path taken inside each (cache hit, save, restore,
write-back...). Usage: m8_time_by_path.py <log> tools/m8_timing.py"""
import sys, importlib.util, collections
sp = importlib.util.spec_from_file_location('tm', sys.argv[2]); tm = importlib.util.module_from_spec(sp); sp.loader.exec_module(tm)
EV = {0xA0F7:'save', 0xA0F8:'restore', 0xA0FF:'m2m', 0xA0F1:'fill', 0xA0F9:'WB', 0xA0FA:'lost', 0xA0F0:'declined',
      0xB0F3:'bc-hit', 0xB0F4:'bc-store', 0xA0F6:'first-sight', 0xA0F5:'evict', 0xB00E:'S2M', 0xA0FC:'M2M'}
pending=None; skip=0; t_in=None; ev=[]; who=None; out=collections.defaultdict(lambda:[0,0])
for l in tm.tally.lines(sys.argv[1]):
    p=l.split()
    if p[1]!='56EE': continue
    v=int(p[2],16)
    if skip: skip-=1; continue
    if v in (0xA100,0xA101): skip=2; continue
    if pending is not None:
        if v>0xFF: continue
        pending[1].append(v)
        if len(pending[1])==3:
            c=(pending[1][0]<<8)|pending[1][1]; tick=pending[1][2]
            if pending[0]==0xA0F2: t_in=(c,tick); ev=[]; who=None
            elif t_in:
                k=(who,' '.join(dict.fromkeys(ev)))
                out[k][0]+=1; out[k][1]+=tm.counts(t_in[0],t_in[1],c,tick); t_in=None
            pending=None
        continue
    if v in (0xA0F2,0xA0F3): pending=(v,[]); continue
    if t_in is None: continue
    if (v&0xF000)==0xC000 and who is None: who=f'C{v&0xFFF:03X}'
    if v in EV: ev.append(EV[v])
for k,(n,c) in sorted(out.items(), key=lambda x:-x[1][1])[:16]:
    print(f'{str(k[0]):6} {k[1]:40} {n:5} calls {c/1193.182:9.1f} ms  {c/n/1193.182:7.2f} ms/call')
