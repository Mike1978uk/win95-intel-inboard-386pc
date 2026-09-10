#!/usr/bin/env python3
"""Whole-file I/O sweep of a 16-bit .SYS image.

Linear-sweeps the entire image (with resync past undecodable bytes - capstone's
disasm() stops silently at the first bad byte) and reports every IN/OUT, with the
last immediate loaded into DX and into AL.  A .SYS is loaded at offset 0 of its
own segment, so file offset == CS offset.

The point is to see the whole shape at once rather than walking one routine at a
time - doing that cost a session on the LS-120 bridge bring-up.

Usage: python io_sweep.py [file] [--ctrl]
       --ctrl  only show accesses whose tracked DX is a parallel control/ECR port
"""
import sys, os
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

d = open(sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-")
         else os.path.join(os.path.dirname(__file__), "..", "SD120PPD.SYS.orig"), "rb").read()
only_ctrl = "--ctrl" in sys.argv

md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = False

dx = al = None
pos = 0
rows = []
while pos < len(d):
    n = 0
    for ins in md.disasm(d[pos:], pos):
        n += ins.size
        m, o = ins.mnemonic, ins.op_str
        if m == "mov" and o.startswith("dx, 0x"):
            dx = int(o.split("0x")[1], 16)
        elif m == "mov" and o.startswith("al, 0x"):
            al = int(o.split("0x")[1], 16)
        elif m in ("out", "in"):
            port = o.split(",")[0].strip() if m == "out" else o.split(",")[1].strip()
            if port == "dx":
                p = dx
            elif port.startswith("0x"):
                p = int(port, 16)
            else:
                p = None
            rows.append((ins.address, m, o, p, al))
        elif m in ("call", "jmp", "ret", "retf", "iret"):
            dx = al = None          # tracking is only valid within a straight run
    pos += n + 1 if n == 0 else n

for addr, m, o, p, al in rows:
    if p is None:
        tag = "port=?"
    else:
        tag = "port=%04X" % p
    if only_ctrl and (p is None or (p & 0xFFF) not in (0x37A, 0x77A, 0x77B, 0x7A, 0x2FA, 0x77C)):
        continue
    print("%05X  %-4s %-14s %-10s al=%s" % (addr, m, o, tag,
                                            "%02X" % al if al is not None else "?"))
print("\n%d I/O sites" % len(rows), file=sys.stderr)
