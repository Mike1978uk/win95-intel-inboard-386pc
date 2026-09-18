#!/usr/bin/env python3
"""INQBYTE.SCR - stage 1b: a full ATAPI INQUIRY with BYTE-MODE register reads.

Derived from INQ9.SCR, the proven capture, by replacing exactly ONE block:
03A0, the register read. Everything else - connect, reset, phase waits, the
CDB, the data-in loop, the dumps - is byte-identical to the script that is
known to return MATSHITA on this machine.

  old 03A0 = epat.c mode 0  : w0(r); w2(1); w2(3); r1(); w2(4); r1(); j44
  new 03A0 = epat.c mode 2  : w0(0x20+r); w2(1); w2(0x25); r0(); w2(4)

The register WRITE at 03F0 is left alone: it is ALREADY epat.c mode 2
(`or al,60` = w0(0x60+r)), so only the read side is nibble today.

The ECR is set to 34h (byte mode) inside 03A0 rather than once at connect,
because where the connect leaves the ECR in INQ9 is not known and guessing it
would be a second variable. That costs one extra I/O access per read, so this
probe does NOT demonstrate the speed win - only that the path returns correct
data. The driver sets it once.

PASS = the dump at 700 contains "MATSHITA" in ASCII.
FAIL = anything else, and the run is then evidence about byte mode only if the
       unchanged INQ9.SCR still passes on the same boot.
"""
import re
import sys

src = open(sys.argv[2]).read().replace("\r\n", "\n")

# 03A0's slot is only 0x30 bytes and the byte-mode read does not provably fit,
# so 03A0 becomes a 3-byte jump and the real code goes to free space at 0A00.
# A jmp does not push, so the ret at 0A00 returns to 03A0's caller unchanged.
NEW_03A0 = """a 03A0
jmp 0A00
"""

BYTE_READ = """
a 0A00
mov bl,al
mov dx,077A
mov al,34
out dx,al
mov al,bl
or al,20
mov dx,0378
out dx,al
mov dx,037A
mov al,01
out dx,al
mov al,25
out dx,al
mov dx,0378
in al,dx
mov bl,al
mov dx,037A
mov al,04
out dx,al
mov al,bl
ret
"""

# Replace only the 03A0 block, bounded by the next blank line.
m = re.search(r"^a 03A0\n(?:.*?\n)*?(?=\n)", src, re.M)
if not m:
    raise SystemExit("could not locate the 03A0 block in the source script")
old = m.group(0)
if "shr al,1" not in old:
    raise SystemExit("03A0 does not look like the nibble read - refusing to patch")
out = src[:m.start()] + NEW_03A0 + src[m.end():]
if out == src:
    raise SystemExit("patch was a no-op")

# Append the relocated read as its own block, before the trailing commands.
tail_at = out.index("\ng=100")
out = out[:tail_at] + "\n" + BYTE_READ + out[tail_at:]

# GUARD: every block must fit before whatever block starts next.
addrs = sorted(int(a, 16) for a in re.findall(r"^a ([0-9A-F]{4})$", out, re.M))
counts = {}
cur = None
for line in out.split("\n"):
    s = line.strip()
    mm = re.match(r"^a ([0-9A-F]{4})$", s)
    if mm:
        cur = int(mm.group(1), 16)
        counts[cur] = 0
    elif s == "":
        cur = None
    elif cur is not None:
        counts[cur] += 1
# Only the blocks THIS script creates or changes. The untouched ones are proven
# by INQ9.SCR working on hardware; applying a 5-byte-per-instruction worst case
# to them produces false positives (0300 packs 4 instructions into 16 bytes).
for a in (0x03A0, 0x0A00):
    later = [x for x in addrs if x > a]
    if not later:
        continue
    worst = a + counts[a] * 5
    if worst > min(later):
        raise SystemExit("block %04X worst case ends %04X, next block at %04X"
                         % (a, worst, min(later)))
n_insn = counts[0x0A00]
worst = 0x0A00 + n_insn * 5

# GUARD: blank line before every `a` directive (the fault that void'd BYTEMODE v1).
lines = out.split("\n")
for i, ln in enumerate(lines):
    if re.match(r"^a [0-9A-F]{4}$", ln.strip()) and i > 0 and lines[i - 1].strip() != "":
        raise SystemExit("NO BLANK LINE BEFORE %r (line %d)" % (ln.strip(), i + 1))
if [l for l in lines if l.strip()][-1].strip() != "q":
    raise SystemExit("script does not end in `q`")

data = "\r\n".join(lines).encode("ascii")
open(sys.argv[1], "wb").write(data)
crlf = data.count(b"\r\n")
print("wrote %s" % sys.argv[1])
print("  bytes %d  lines %d  CRLF %d  bare LF %d  longest %d"
      % (len(data), len(lines), crlf, data.count(b"\n") - crlf, max(len(l) for l in lines)))
print("  03A0 -> jmp 0A00; read relocated, worst case ends %04X" % worst)
print("  blocks unchanged: %d" % (len(addrs) - 1))
