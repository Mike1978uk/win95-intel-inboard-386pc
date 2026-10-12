#!/usr/bin/env python3
"""Sweep a tree of ATI binaries for Mach8 ports the programmer's guide does not name.

  python tools/m8_port_census2.py <guide.txt> <dir-or-file> [...]

A hit is `mov dx, imm16` (BA lo hi) with a Mach8-style port (low byte E8-EB / EE-EF, >= 0200h)
followed within 6 bytes by an `in`/`out` through DX (EC-EF) - so byte runs inside data or other
instructions mostly drop out. Ports that are the high byte(s) of a guide-named word register are
skipped. Compressed archives (.ZIP/.LZH, LHA SFX) are not looked inside: unpack them first.
"""
import re, sys, os
from collections import defaultdict

EXT = ('.EXE', '.COM', '.DRV', '.386', '.3EX', '.DLL', '.BIN', '.SYS', '.VXD', '.3GR', '.2GR', '.OVL', '.ROM')
guide = open(sys.argv[1], encoding='latin-1').read().upper()
named = set(int(m, 16) for m in re.findall(r'\b([0-9A-F]{2}E[89ABEF])H?\b', guide))

files = []
for a in sys.argv[2:]:
    if os.path.isdir(a):
        for r, _, fs in os.walk(a):
            files += [os.path.join(r, f) for f in fs if f.upper().endswith(EXT)]
    else:
        files.append(a)

hits = defaultdict(list)
for f in files:
    d = open(f, 'rb').read()
    for m in re.finditer(rb'\xBA(..)', d, re.S):
        v = m.group(1)[0] | (m.group(1)[1] << 8)
        if v < 0x200 or (v & 0xFF) not in (0xE8, 0xE9, 0xEA, 0xEB, 0xEE, 0xEF):
            continue
        if v in named or any((v - k) in named for k in (1, 2, 3)):
            continue
        nxt = d[m.end():m.end() + 6]
        if any(b in (0xEC, 0xED, 0xEE, 0xEF) for b in nxt):
            hits[v].append(f'{f}@{m.start():X}: {d[m.start():m.start()+9].hex(" ")}')

print(f'{len(files)} files scanned')
for v in sorted(hits):
    print(f'{v:04X}')
    for h in hits[v][:6]:
        print('   ', h)
print('candidates:', ' '.join(f'{v:04X}' for v in sorted(hits)) or 'none')
