#!/usr/bin/env python3
"""
Neuter all real port 0xA0/0xA1 ("slave 8259") access in stock VPICD.VXD.

On the 5160+Inboard there is no physical second 8259 -- port 0xA0 is the
Inboard's own control register / XT NMI latch (bit 7 = NMI enable, default
0x80), per this project's own reverse-engineering. Stock VPICD blindly does
a full ICW1-4 dual-PIC init/reinit against 0x20/0x21 (real master, left
untouched) AND 0xA0/0xA1 (phantom slave). Disassembly confirmed no compare/
branch is gated on the 0xA0/0xA1 readback in the routines examined, so it's
safe to fully virtualize the "slave" in software: OUT writes become no-ops
(2-byte E6 xx -> 2-byte NOP NOP), IN reads become a fixed idle-PIC value
(2-byte E4 xx -> 2-byte MOV AL,0), matching what a real, quiescent slave
8259 would read back on a normal AT anyway.

2026-08-23: shares vxdstream.py's verifier with patch_vdmad.py, after that
script's old realign() was found to have corrupted VDMAD_INBOARD.VXD by
"confirming" an IN/OUT that was really the tail of a 3-byte AND. See
patch_vdmad.py's header for the full account. All 32 of this script's sites
re-verified clean against the real instruction stream on 2026-08-23 -- the
one that first looked corrupt (file 0x616a) was a false alarm from a
verifier that didn't yet understand the VxD INT 20h + inline-DWORD calling
convention and desynced on it.
"""
import sys
from vxdstream import verify_sites

SRC = "VPICD.VXD"
OUT = "VPICD_INBOARD.VXD"

data = bytearray(open(SRC, "rb").read())

slave_ports = {0xA0, 0xA1}

candidates = []
for i in range(len(data) - 1):
    if data[i] in (0xE4, 0xE6) and data[i + 1] in slave_ports:
        candidates.append(i)

print(f"Found {len(candidates)} raw opcode candidates")

good, bad = verify_sites(SRC, candidates)

for off, reason in bad:
    print(f"  REFUSED offset 0x{off:x}: {reason}")

patched = 0
for off, mn, ops, size in good:
    if mn == "out":
        data[off:off + 2] = b"\x90\x90"          # NOP NOP
    else:
        data[off:off + 2] = b"\xB0\x00"          # MOV AL, 0
    print(f"  patched offset 0x{off:x}: {mn} {ops}")
    patched += 1

print(f"Patched: {patched}   Refused: {len(bad)}")

# 2026-08-02: this script used to write OUT unconditionally, even with patched == 0 - which
# happened silently for this project's actual Windows 95 retail (4.00.950) stock VPICD.VXD,
# producing a "patched" file that was byte-identical to stock and went unnoticed for multiple
# sessions. Refuse to write a no-op "patch".
if patched == 0:
    print("\nREFUSING TO WRITE OUTPUT: found 0 verified I/O sites to patch. This almost certainly "
          "means this stock VPICD.VXD's compiled code doesn't match this script's raw-opcode "
          "search assumptions - fix the search logic for this specific build before re-running.")
    raise SystemExit(1)

open(OUT, "wb").write(data)
print(f"Wrote {OUT}, {len(data)} bytes")

# Re-verify the OUTPUT: every byte we changed must still sit on a boundary of the
# stock stream, and nothing else may have moved.
stock = open(SRC, "rb").read()
changed = [i for i in range(len(stock)) if stock[i] != data[i]]
starts = sorted({off for off, _, _, _ in good})
expected = sorted({i for s in starts for i in (s, s + 1)})
if changed != expected:
    print(f"\nPOST-CHECK FAILED: changed bytes {[hex(c) for c in changed]} != "
          f"expected {[hex(e) for e in expected]}")
    raise SystemExit(1)
print(f"Post-check OK: exactly {len(changed)} bytes changed, all on verified IN/OUT boundaries")
