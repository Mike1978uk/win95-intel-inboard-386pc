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
