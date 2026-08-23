#!/usr/bin/env python3
"""
Neuter phantom DMA-controller-2 (channels 4-7, AT-only) access in stock
VDMAD.VXD. The 5160+Inboard ISA bus is 8-bit only -- every card on it,
including the Trantor T130B SCSI, runs in 8-bit mode, so any real DMA
usage on this system is confined to channels 0-3 (the single physical
8237). Ports 0xC0-0xDE / 0x89/0x8A/0x8B/0x8F are unclaimed by anything on
this board (unlike port 0xA0, which the Inboard repurposes) -- reads there
float, writes go nowhere. Same patch shape as VPICD: OUT -> NOP/NOP,
IN -> MOV AL,0 (matching "no channel 4-7 activity", the true answer for
hardware that doesn't exist).

2026-08-23 -- THE BUG THIS SCRIPT USED TO HAVE, and why the verifier is
now external. The old realign() accepted any start alignment that decoded
an IN/OUT at the target offset. Stock VDMAD.VXD has

    OBJ1:0x1660   80 E4 C0      AND AH, 0C0h

and the raw-opcode scan matched the `E4 C0` in the middle of it as
`IN AL, 0C0h`. realign() dutifully "confirmed" it by disassembling from a
start that made it true, and the patch wrote B0 00 over those two bytes:

    OBJ1:0x1660   80 B0 00 A8 20 74 06   XOR byte ptr [EAX+7420A800h], 6

a wild write to an unmapped linear address. That file went into the card's
VMM32.VXD and is the exact, byte-for-byte cause of

    A fatal exception 0E has occurred at 0028:C002F330
    in VXD VDMAD(01) + 00001660

Sites are now verified against the object's REAL instruction stream
(vxdstream.py), decoded with the VxD INT 20h + inline-DWORD calling
convention so it doesn't desync. Any candidate not sitting on a true
2-byte IN/OUT boundary is refused, not patched.
"""
import sys
from vxdstream import verify_sites

SRC = "VDMAD.VXD"
OUT = "VDMAD_INBOARD.VXD"

data = bytearray(open(SRC, "rb").read())

dma2_ports = set(range(0xC0, 0xE0)) | {0x89, 0x8A, 0x8B, 0x8F}

candidates = []
for i in range(len(data) - 1):
    if data[i] in (0xE4, 0xE6) and data[i + 1] in dma2_ports:
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

# 2026-08-02: same silent-no-op bug as patch_vpicd.py had - this script wrote OUT unconditionally
# even with patched == 0, which happened for real against this project's actual Windows 95 retail
# (4.00.950) stock VDMAD.VXD and went unnoticed for multiple sessions (VDMAD_INBOARD.VXD sat
# byte-identical to stock the whole time). Refuse to write a no-op "patch" - re-derive the
# raw-opcode search assumptions for whatever stock file triggers this before trusting it again.
if patched == 0:
    print("\nREFUSING TO WRITE OUTPUT: found 0 verified I/O sites to patch. This almost certainly "
          "means this stock VDMAD.VXD's compiled code doesn't match this script's raw-opcode "
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
