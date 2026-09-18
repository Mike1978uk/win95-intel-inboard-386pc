#!/usr/bin/env python3
"""BYTEMODE.SCR - stage 1: can we read the task file in PS/2 byte mode?

Transliterates epat.c mode 2 (reference_gpl/epat.c, read_regr):

    case 2: w0(0x20+r); w2(1); w2(0x25);
            a = r0(); w2(4);
            return a;

r0() is the DATA port, so one access per byte instead of the two status reads
nibble mode needs. That requires the port bidirectional, so the ECR goes to
34h (mode 001) first - which is where the VENDOR parks (SD120PPD.SYS 0x482D)
and where we do not.

No 1284 negotiation: epat.c negotiates only for EPP (modes 3-5, the w0(0x40)
sequence). Byte mode is entered by the CPP connect alone, so this probe cannot
strand the peripheral in ECP the way the ACKREV runs could.

CONTROL ARM IN THE SAME RUN: the same register is read first by NIBBLE
(epat.c mode 0), which is the path the shipping driver uses and is known to
work. If the two disagree, byte mode is wrong. If the nibble arm itself fails,
the run is void and says nothing about byte mode.

  E2E0/E2E1  nibble halves of ATA status   -> j44 = ((a>>4)&0x0F)|(b&0xF0)
  E2E2       byte-mode ATA status          expect 50h  (DRDY|DSC)
  E2E3       byte-mode cylinder low        expect 14h  ] ATAPI
  E2E4       byte-mode cylinder high       expect EBh  ] signature

Task file is bridge container offset 18h, so status=1Fh, cyl lo=1Ch, cyl hi=1Dh.
"""
import re
import sys

BASE = open(sys.argv[2]).read()

NEW = """
a E600
mov dx,0378
mov al,1F
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,03
out dx,al
mov dx,0379
in al,dx
mov [E2E0],al
mov dx,037A
mov al,04
out dx,al
mov dx,0379
in al,dx
mov [E2E1],al
mov dx,077A
mov al,34
out dx,al
mov dx,0378
mov al,3F
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,25
out dx,al
mov dx,0378
in al,dx
mov [E2E2],al
mov dx,037A
mov al,04
out dx,al
mov dx,0378
mov al,3C
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,25
out dx,al
mov dx,0378
in al,dx
mov [E2E3],al
mov dx,037A
mov al,04
out dx,al
mov dx,0378
mov al,3D
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,25
out dx,al
mov dx,0378
in al,dx
mov [E2E4],al
mov dx,037A
mov al,04
out dx,al
mov dx,077A
in al,dx
and al,1F
out dx,al
int 3
"""

TAIL = """
f E2E0 E2EF EE
g=E000
g=E600
d E2E0 E2EF
q
"""

# GUARD: computed extents. No instruction here exceeds 4 bytes; 5 is conservative.
MAX_INSN = 5
blocks, cur = [], None
for line in NEW.split("\n"):
    s = line.strip()
    m = re.match(r"^a ([0-9A-F]{4})$", s)
    if m:
        cur = [int(m.group(1), 16), 0]
        blocks.append(cur)
    elif s == "":
        cur = None
    elif cur is not None:
        cur[1] += 1
blocks.sort()
spans = [("data", 0xE2E0, 0xE2EF)]
for addr, n in blocks:
    spans.append(("%04X" % addr, addr, addr + n * MAX_INSN - 1))
for i in range(len(spans) - 1):
    if spans[i][2] >= spans[i + 1][1]:
        raise SystemExit("BLOCK OVERLAP: %s ends %04X, %s starts %04X"
                         % (spans[i][0], spans[i][2], spans[i+1][0], spans[i+1][1]))

# GUARD: no short jumps at all in the new code - assert that, do not assume it.
for line in NEW.split("\n"):
    if re.match(r"^\s*(j[a-z]+|loop[a-z]*)\s", line):
        raise SystemExit("UNEXPECTED BRANCH in a block that should have none: %r" % line)

text = BASE + NEW + TAIL
lines = text.split("\n")

# GUARD: every `a XXXX` must be preceded by a BLANK line, or DEBUG never leaves
# assembly mode - it takes the literal text "a E600" as a failed instruction,
# re-prompts, and assembles the whole next block into the PREVIOUS block's
# address space. The target address is then empty and `g=` runs garbage.
# This is what void'd the first BYTEMODE run: split('\n\n') drops the separator,
# so the last base block had no trailing newline and the join added none.
for i, ln in enumerate(lines):
    if re.match(r"^a [0-9A-F]{4}$", ln.strip()) and i > 0:
        if lines[i - 1].strip() != "":
            raise SystemExit(
                "NO BLANK LINE BEFORE %r (line %d) - preceded by %r. DEBUG would "
                "stay in assembly mode and leave that block EMPTY."
                % (ln.strip(), i + 1, lines[i - 1].strip()))
# GUARD: the run must end by quitting, or DEBUG holds the console with the
# output file unflushed and Ctrl+C cannot reach it (its stdin is the file).
if [l for l in lines if l.strip()][-1].strip() != "q":
    raise SystemExit("script does not end in `q`")
data = "\r\n".join(lines).encode("ascii")
open(sys.argv[1], "wb").write(data)
crlf = data.count(b"\r\n")
print("wrote %s" % sys.argv[1])
print("  bytes %d  lines %d  CRLF %d  bare LF %d  longest %d"
      % (len(data), len(lines), crlf, data.count(b"\n") - crlf, max(len(l) for l in lines)))
for n, lo, hi in spans:
    print("  block %-6s %04X-%04X (worst case)" % (n, lo, hi))
print("  no branches in the new block - jump-range class cannot arise")
