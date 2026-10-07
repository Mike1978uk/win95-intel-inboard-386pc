#!/usr/bin/env python3
"""Write M8PRE.DAT for M8TSX: the accelerator writes ATI TEST.COM makes before Test Sequence 1, taken
from an 86Box log made with MACH8_WLOG=1 (fork diagnostic) while TEST.COM ran.

    python m8pre_from_log.py <86box.log> <M8PRE.DAT>

TEST.COM's run is found by its first writes (8514 display on and off, 32EEh reset, 42E8h reset,
6AEEh, 36EEh); it ends at the last 32EEh write before Test Sequence 1's table (MULTIFUNC_CNTL 17FEh),
which is TEST.COM's own engine reset and setup (1E21h) - M8TSX plays that itself.
Output: entry count (word), then per write: port (word), value (word), width (byte).
"""
import struct
import sys

CRT = {0x02E8, 0x06E8, 0x0AE8, 0x0EE8, 0x12E8, 0x16E8, 0x1AE8, 0x1EE8, 0x22E8}
SIG = [(0x4AE8, 7), (0x4AE8, 2), (0x32EE, 0), (0x42E8, 0x900F), (0x42E8, 0x400F), (0x6AEE, 0x049A),
       (0x36EE, 1)]

w = []
for line in open(sys.argv[1], errors='replace'):
    if line.startswith('M8W '):
        a = line.split()
        w.append((int(a[1], 16), int(a[2], 16), int(a[3])))
idx = [i for i, x in enumerate(w) if x[0] not in CRT]
f = [w[i] for i in idx]
starts = [j for j in range(len(f) - len(SIG)) if [(p, v) for p, v, _ in f[j:j + len(SIG)]] == SIG]
if not starts:
    raise SystemExit('no TEST.COM run in the log')
s = idx[starts[-1]]
t1 = next(i for i in range(s, len(w)) if w[i][:2] == (0xBEE8, 0x17FE))
r = max(i for i in range(s, t1) if w[i][0] == 0x32EE)
pre = w[s:r]
with open(sys.argv[2], 'wb') as o:
    o.write(struct.pack('<H', len(pre)))
    for p, v, n in pre:
        o.write(struct.pack('<HHB', p, v, n))
print('%d writes' % len(pre))
