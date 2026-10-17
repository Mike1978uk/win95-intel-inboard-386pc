"""Decode the trace build's memory-to-memory (A0FC) and screen-to-memory (A0FD)
BitBlt records with the proxy events around them, from the first drag onwards.
Usage: m8_mem_trace.py <log> [first S2M index, default 40; -1 = from the start] [count, default 30]"""
import os, sys, importlib.util
_s = importlib.util.spec_from_file_location('tally', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'm8_fallback_tally.py'))
tally = importlib.util.module_from_spec(_s); _s.loader.exec_module(tally)

EVENT = {0xA0F7: '[save on card]', 0xA0F8: '[restore from card]', 0xA0FF: '[m2m on card]', 0xA0FA: '[proxy lost]', 0xA0F1: '[fill on card]'}
WHY_BACK = {1: 'entry', 2: 'mem->mem', 3: 'forwarded', 4: 'evicted', 5: 'suspend', 6: 'overlap'}
WHY_NO = {1: 'no dest proxy', 2: 'source not 8-bit', 3: 'not cached', 4: 'dest evicted'}
seq = []; cur = None; buf = []; skip = 0; follow = None
for l in tally.lines(sys.argv[1]):
    p = l.split(); port = p[1]; v = int(p[2], 16)
    if port != '56EE': continue
    if skip: skip -= 1; continue
    if v in (0xA100, 0xA101): skip = 2; continue
    if follow:
        seq.append((f'  {follow[0]} {follow[1].get(v, v)}',)); follow = None; continue
    if cur is not None:
        if v > 0xFF: continue
        buf.append(v)
        if len(buf) == 22:
            w = [(buf[i] << 8) | buf[i + 1] for i in range(0, 22, 2)]
            s = lambda x: x - 65536 if x > 32767 else x
            seq.append(('M2M' if cur == 0xA0FC else 'S2M', f'dst {w[0]:04X}:{w[1]:04X} at ({s(w[7])},{s(w[8])})',
                        f'src {w[2]:04X}:{w[3]:04X} at ({s(w[9])},{s(w[10])})', f'rop {w[4]:02X}', f'{w[5]}x{w[6]}'))
            cur = None
        continue
    if v in (0xA0FC, 0xA0FD): cur = v; buf = []; continue
    if v in EVENT: seq.append(('  ' + EVENT[v],)); continue
    if v == 0xA0F9: follow = ('[WRITE-BACK]', WHY_BACK); continue
    if v == 0xA0F0: follow = ('[m2m declined]', WHY_NO); continue
    if (v & 0xF000) == 0xC000: seq.append(('to screen', f'kind {(v >> 8) & 3}', f'rop {v & 0xFF:02X}'))
idx = [i for i, x in enumerate(seq) if x[0] == 'S2M']
print('S2M total', len(idx), 'M2M total', sum(1 for x in seq if x[0] == 'M2M'))
if idx:
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    i = idx[min(first, len(idx) - 1)] if first >= 0 else 1
    for x in seq[i - 1:i + (int(sys.argv[3]) if len(sys.argv) > 3 else 30)]: print(*x)
