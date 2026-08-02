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
"""
import capstone

SRC = "VDMAD.VXD"
OUT = "VDMAD_INBOARD.VXD"

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

dma2_ports = set(range(0xC0, 0xE0)) | {0x89, 0x8A, 0x8B, 0x8F}

candidates = []
for i in range(len(data) - 1):
    if data[i] == 0xE4 and data[i+1] in dma2_ports:
        candidates.append((i, "in", data[i+1]))
    if data[i] == 0xE6 and data[i+1] in dma2_ports:
        candidates.append((i, "out", data[i+1]))

print(f"Found {len(candidates)} raw candidates")

patched = 0
for off, kind, port in candidates:
    insn = realign(data, off)
    assert insn is not None, f"could not verify instruction at {off}"
    assert len(insn.bytes) == 2
    if kind == "out":
        data[off:off+2] = b"\x90\x90"
    else:
        data[off:off+2] = b"\xB0\x00"
    patched += 1
    print(f"  patched offset {off}: {kind} port 0x{port:02X}")

print(f"Patched: {patched}")

# 2026-08-02: same silent-no-op bug as patch_vpicd.py had - this script wrote OUT unconditionally
# even with patched == 0, which happened for real against this project's actual Windows 95 retail
# (4.00.950) stock VDMAD.VXD and went unnoticed for multiple sessions (VDMAD_INBOARD.VXD sat
# byte-identical to stock the whole time). Refuse to write a no-op "patch" - re-derive the
# raw-opcode search assumptions for whatever stock file triggers this before trusting it again.
if patched == 0:
    print("\nREFUSING TO WRITE OUTPUT: found 0 real I/O sites to patch. This almost certainly "
          "means this stock VDMAD.VXD's compiled code doesn't match this script's raw-opcode "
          "search assumptions - fix the search logic for this specific build before re-running.")
    raise SystemExit(1)

open(OUT, "wb").write(data)
print(f"Wrote {OUT}, {len(data)} bytes (original was {len(open(SRC,'rb').read())})")

remaining = 0
for i in range(len(data) - 1):
    if data[i] == 0xE4 and data[i+1] in dma2_ports:
        remaining += 1
    if data[i] == 0xE6 and data[i+1] in dma2_ports:
        remaining += 1
print(f"Remaining raw DMA2-port opcode matches: {remaining}")
