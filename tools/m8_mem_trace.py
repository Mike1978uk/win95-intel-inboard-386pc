"""Decode the trace build's memory-to-memory (A0FC) and screen-to-memory (A0FD)
BitBlt records. Usage: m8_mem_trace.py <log> tools/m8_fallback_tally.py"""
import sys, importlib.util
spec = importlib.util.spec_from_file_location('t', sys.argv[2]); t = importlib.util.module_from_spec(spec); spec.loader.exec_module(t)
seq=[]; cur=None; buf=[]; skip=0
for l in t.lines(sys.argv[1]):
    p=l.split(); port=p[1]; v=int(p[2],16)
    if port!='56EE': continue
    if skip: skip-=1; continue
    if v in (0xA100,0xA101): skip=2; continue
    if cur is not None:
        if v > 0xFF: continue
        buf.append(v)
        if len(buf)==14:
            w=[(buf[i]<<8)|buf[i+1] for i in range(0,14,2)]
            seq.append(('M2M' if cur==0xA0FC else 'S2M', f'dst {w[0]:04X}:{w[1]:04X}', f'src {w[2]:04X}:{w[3]:04X}', f'rop {w[4]:02X}', f'{w[5]}x{w[6]}'))
            cur=None
        continue
    if v in (0xA0FC,0xA0FD): cur=v; buf=[]; continue
    if (v&0xF000)==0xC000: seq.append(('to screen', f'kind {(v>>8)&3}', f'rop {v&0xFF:02X}'))
idx=[i for i,s in enumerate(seq) if s[0]=='S2M']
print('S2M total', len(idx), 'M2M total', sum(1 for s in seq if s[0]=='M2M'))
i=idx[min(40,len(idx)-1)]
for s in seq[i-1:i+16]: print(*s)
