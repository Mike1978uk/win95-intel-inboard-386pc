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
"""
import capstone

SRC = "VPICD.VXD"
OUT = "VPICD_INBOARD.VXD"

data = bytearray(open(SRC, "rb").read())
md32 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

def realign(data, target, search_back=40):
    for start in range(max(0, target - search_back), target + 1):
        chunk = bytes(data[start:target+16])
        insns = list(md32.disasm(chunk, start))
        for insn in insns:
            if insn.address == target and insn.mnemonic in ("out", "in"):
                return insn
    return None

candidates = []
for i in range(len(data) - 1):
    if data[i] == 0xE4 and data[i+1] in (0xA0, 0xA1):
        candidates.append((i, "in", data[i+1]))
    if data[i] == 0xE6 and data[i+1] in (0xA0, 0xA1):
        candidates.append((i, "out", data[i+1]))

print(f"Found {len(candidates)} raw candidates")

patched = 0
skipped = []
for off, kind, port in candidates:
    insn = realign(data, off)
    if insn is None:
        skipped.append((off, kind, port))
        continue
    assert len(insn.bytes) == 2, f"unexpected instruction length at {off}: {insn.bytes}"
    if kind == "out":
        data[off:off+2] = b"\x90\x90"  # NOP NOP
    else:
        data[off:off+2] = b"\xB0\x00"  # MOV AL, 0
    patched += 1

print(f"Patched: {patched}  Skipped (unverified): {len(skipped)}")
if skipped:
    print("SKIPPED (left untouched):", skipped)

# 2026-08-02: this script used to write OUT unconditionally, even with patched == 0 - which
# happened silently for this project's actual Windows 95 retail (4.00.950) stock VPICD.VXD
# (different build than whatever stock this patch was originally validated against), producing
# a "patched" file that was actually byte-identical to stock. Confirmed the hard way: this
# repo's VPICD_INBOARD.VXD sat unnoticed as an unpatched copy through multiple investigation
# sessions. Refuse to write a no-op "patch" - if this fires, the raw-opcode search assumptions
# in this script need re-deriving for whatever stock file was just fed to it (check for
# DX-indirect IN/OUT forms, a different compiled layout, etc.) before trusting its output again.
if patched == 0:
    print("\nREFUSING TO WRITE OUTPUT: found 0 real I/O sites to patch. This almost certainly "
          "means this stock VPICD.VXD's compiled code doesn't match this script's raw-opcode "
          "search assumptions (see WIN95_PLAN.md search for build-mismatch precedent) - fix the "
          "search logic for this specific build before re-running, don't trust a silent no-op.")
    raise SystemExit(1)

open(OUT, "wb").write(data)
print(f"Wrote {OUT}, {len(data)} bytes")

# verify: rescan the patched file for any remaining raw opcode matches
remaining = 0
for i in range(len(data) - 1):
    if data[i] == 0xE4 and data[i+1] in (0xA0, 0xA1):
        remaining += 1
    if data[i] == 0xE6 and data[i+1] in (0xA0, 0xA1):
        remaining += 1
print(f"Remaining raw A0/A1 IN/OUT opcode matches after patch: {remaining}")
