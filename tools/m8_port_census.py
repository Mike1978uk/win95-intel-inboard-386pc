#!/usr/bin/env python3
"""Census of the Mach8 ports ATI's own binaries name, against the ports the programmer's guide names.

  python tools/m8_port_census.py <guide.txt> <binary> [<binary> ...]

Finds `mov dx, imm16` (BA lo hi; in 32-bit code also 66 BA lo hi) whose value is a Mach8 port - low
byte E8/E9/EA/EB/EE/EF - plus the 28800's 01CE/01CF. A raw byte scan: a hit can be data, so a port
the guide does not name is a lead to disassemble, not a finding. Prints per port: which files use it,
how often, and whether the guide names it.
"""
import re, sys, os
from collections import defaultdict

guide = open(sys.argv[1], encoding='latin-1').read().upper()
named = set(int(m, 16) for m in re.findall(r'\b([0-9A-F]{2}E[89ABEF])H?\b', guide))
named |= {0x1CE, 0x1CF}

use = defaultdict(lambda: defaultdict(int))
for path in sys.argv[2:]:
    d = open(path, 'rb').read()
    name = os.path.basename(path)
    for m in re.finditer(rb'\xBA(..)', d, re.S):
        v = m.group(1)[0] | (m.group(1)[1] << 8)
        if (v & 0xFF) in (0xE8, 0xE9, 0xEA, 0xEB, 0xEE, 0xEF) and v >= 0x0200 or v in (0x1CE, 0x1CF):
            use[v][name] += 1

print('port   guide  files (count)')
for v in sorted(use):
    tag = 'yes' if v in named else '**NO**'
    files = ', '.join(f'{n} ({c})' for n, c in sorted(use[v].items()))
    print(f'{v:04X}   {tag:6s} {files}')
print()
print('ports ATI code names that the guide does not:',
      ' '.join(f'{v:04X}' for v in sorted(use) if v not in named) or 'none')
