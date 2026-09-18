#!/usr/bin/env python3
"""Generate ACKREV.SCR - does the EPAT bridge drive AckReverse* ?  (v2)

v1 was void. Two faults, both fixed here:

  1. `a E300` assembled to E351, not the E33F I asserted, so `a E340` overwrote
     the tail of the measurement block. The layout guard compared blocks against
     GUESSED extents, so it could not catch an error in its own input. v2
     computes a conservative extent from the instruction count.
  2. Every status sample was taken with the ECR in ECP mode (74h), where the
     status port reads 00 in every bit - measured, D8h at park and 00h at 74h.
     v2 parks the ECR before each sample.

Derived from VCONN2.SCR (the proven negotiate, status B8h). Blocks are ADDED;
nothing in the connect or the negotiate is touched.

  E2F0  negotiate status (expect B8)
  E2F1  status at forward idle, ECR parked
  E2F2  status in reverse, ECR parked     E2F3 polls remaining (word)
  E2F5  status after leaving reverse      E2F6 polls remaining (word)
  E2F8  status after the 1284 terminate

AN062: "AckReverse* - the peripheral drives this line to follow the level of
the ReverseRequest* line." AckReverse* is SPP PError, status bit 5 (20h).
"""
import re
import sys

VCONN2 = open(sys.argv[2], "r").read() if len(sys.argv) > 2 else None

# ---- blocks copied VERBATIM from VCONN2.SCR -------------------------------
BASE = """a E000
mov dx,077A
in al,dx
and al,34
out dx,al
call E1D0
mov ah,30
call E100
mov ah,40
call E100
mov ah,50
call E100
mov ah,00
call E100
mov ah,E0
call E110
mov dx,0378
xor al,al
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,04
out dx,al
call E200
int 3

a E100
mov dx,037A
mov al,04
out dx,al
jmp E120

a E110
mov dx,037A
in al,dx
and al,0F
out dx,al
jmp E120

a E120
cli
mov dx,0378
mov al,22
out dx,al
out dx,al
mov al,AA
out dx,al
out dx,al
mov al,55
out dx,al
out dx,al
mov al,00
out dx,al
out dx,al
mov al,FF
out dx,al
out dx,al
mov al,87
out dx,al
out dx,al
mov al,78
out dx,al
out dx,al
mov al,ah
out dx,al
out dx,al
mov dx,037A
mov al,04
out dx,al
out dx,al
in al,dx
and al,10
or al,05
out dx,al
out dx,al
and al,FE
out dx,al
out dx,al
mov dx,0378
mov al,FF
out dx,al
out dx,al
sti
ret

a E1D0
mov dx,037A
mov al,04
out dx,al
mov al,0C
out dx,al
mov al,0E
out dx,al
out dx,al
out dx,al
mov al,04
out dx,al
out dx,al
ret

a E200
mov dx,037A
mov al,0C
out dx,al
mov al,04
out dx,al
mov dx,0378
xor al,al
out dx,al
out dx,al
mov dx,037A
mov al,01
out dx,al
out dx,al
mov al,04
out dx,al
mov al,0C
out dx,al
mov dx,0378
mov al,10
out dx,al
mov dx,037A
mov al,06
out dx,al
out dx,al
out dx,al
mov dx,0379
mov cx,0100
jmp E240

a E240
in al,dx
test al,40
jz E260
dec cx
jnz E240
jmp E260

a E260
mov [E2F0],al
mov dx,037A
mov al,07
out dx,al
out dx,al
mov al,04
out dx,al
out dx,al
ret
"""

# ---- NEW: the measurement tail -------------------------------------------
# Helpers spaced 40h apart, far clear of the main block's real extent.
NEW = """
a E300
mov dx,0379
in al,dx
mov [E2F1],al
mov dx,077A
mov al,34
out dx,al
mov dx,037A
mov al,20
out dx,al
mov dx,077A
mov al,74
out dx,al
mov al,34
out dx,al
mov dx,0379
mov cx,0400
call E400
mov [E2F2],al
mov [E2F3],cx
mov dx,077A
mov al,74
out dx,al
mov dx,037A
mov al,04
out dx,al
mov dx,077A
in al,dx
and al,1F
out dx,al
mov dx,0379
mov cx,0400
call E440
mov [E2F5],al
mov [E2F6],cx
call E480
mov dx,0379
in al,dx
mov [E2F8],al
int 3

a E400
in al,dx
test al,20
jz E408
dec cx
jnz E400
ret

a E440
in al,dx
test al,20
jnz E448
dec cx
jnz E440
ret

a E480
mov dx,037A
mov al,0C
out dx,al
mov dx,0379
mov cx,FFFF
in al,dx
test al,40
jz E494
dec cx
jnz E48C
mov dx,037A
mov al,0E
out dx,al
mov dx,0379
mov cx,FFFF
in al,dx
test al,40
jnz E4A8
dec cx
jnz E4A0
mov dx,037A
mov al,0C
out dx,al
mov al,04
out dx,al
ret
"""

TAIL = """
f E2F0 E2FF EE
g=E000
d E2F0 E2F0
g=E300
d E2F0 E2FF
q
"""

# ---- GUARD 1: block extents, COMPUTED not asserted ------------------------
# No 16-bit instruction used here exceeds 4 bytes; 5 is the conservative bound.
# Over-estimating is safe - it can only reject a layout, never accept a bad one.
MAX_INSN = 5
blocks = []
cur = None
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
DATA = (0xE2F0, 0xE2FF)
spans = [("data", DATA[0], DATA[1])]
for addr, n in blocks:
    spans.append(("%04X" % addr, addr, addr + n * MAX_INSN - 1))
for i in range(len(spans) - 1):
    if spans[i][2] >= spans[i + 1][1]:
        raise SystemExit(
            "BLOCK OVERLAP: %s ends %04X (worst case), %s starts %04X"
            % (spans[i][0], spans[i][2], spans[i + 1][0], spans[i + 1][1]))

# ---- GUARD 2: short-jump range -------------------------------------------
JUMPS = [
    ("E400 jz  E408", 0xE405, 0xE408), ("E400 jnz E400", 0xE408, 0xE400),
    ("E440 jnz E448", 0xE445, 0xE448), ("E440 jnz E440", 0xE448, 0xE440),
    ("E480 jz  E494", 0xE491, 0xE494), ("E480 jnz E48C", 0xE494, 0xE48C),
    ("E480 jnz E4A8", 0xE4A5, 0xE4A8), ("E480 jnz E4A0", 0xE4A8, 0xE4A0),
]
bad = [(n, t - nxt) for n, nxt, t in JUMPS if not -128 <= (t - nxt) <= 127]
if bad:
    raise SystemExit("SHORT JUMP OUT OF RANGE: %r" % bad)

text = BASE + NEW + TAIL
lines = text.split("\n")
data = "\r\n".join(lines).encode("ascii")
open(sys.argv[1], "wb").write(data)

crlf = data.count(b"\r\n")
print("wrote %s" % sys.argv[1])
print("  bytes %d   lines %d   CRLF %d   bare LF %d   longest %d chars"
      % (len(data), len(lines), crlf, data.count(b"\n") - crlf,
         max(len(l) for l in lines)))
for name, lo, hi in spans:
    print("  block %-6s %04X-%04X (worst case)" % (name, lo, hi))
print("  %d short jumps checked, all in range" % len(JUMPS))
