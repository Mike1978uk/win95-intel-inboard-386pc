#!/usr/bin/env python3
"""Third point on the Inboard's I/O cost curve: byte vs word vs dword.

Technique 109e fitted `sync + n x byte_cycle` from two points, `rep insb` and
`rep insw` on one 8-bit port, and got sync ~3.90 us, byte ~1.87 us. Every width
argument in this project extrapolates that fit to four bytes. This takes the
third point instead of assuming it.

512 bytes are written to one port three ways - 512 x outsb, 256 x outsw,
128 x outsd - with the PIT latched either side of each loop. Same port, same
byte count, one run, so nothing but the access width differs.

The port is written, never read: a read of an undecoded port returns float and
a read of a live one can have side effects, while a write to the LPT data
register only drives the data lines. The EPAT bridge latches on nStrobe, which
this never touches.

Emits a DEBUG script. Machine code is generated as bytes and disassembled back
with capstone before it is written out, because a hand-assembled loop that is
one prefix wrong still runs and still produces a plausible number.

    python tools/gen_iowidth_probe.py            > iowidth.scr
    python tools/gen_iowidth_probe.py --port 278 > iowidth.scr

On the machine:  DEBUG < IOWIDTH.SCR
Read the three 16-bit deltas from the dump at 0200h.
"""

import argparse
import sys

ORG = 0x100
BUF = 0x2000          # source buffer; contents are irrelevant, only the count is
RES = 0x200           # three 16-bit deltas land here
NBYTES = 512


class Asm:
    def __init__(self, org):
        self.org = org
        self.b = bytearray()
        self.fix = []          # (offset, label) 16-bit absolute patches
        self.lbl = {}

    @property
    def pc(self):
        return self.org + len(self.b)

    def emit(self, *vals):
        self.b.extend(vals)

    def label(self, name):
        self.lbl[name] = self.pc

    def call(self, name):
        self.emit(0xE8)
        self.fix.append((len(self.b), name, "rel16"))
        self.emit(0x00, 0x00)

    def link(self):
        for off, name, kind in self.fix:
            assert kind == "rel16"
            target = self.lbl[name]
            rel = target - (self.org + off + 2)
            self.b[off] = rel & 0xFF
            self.b[off + 1] = (rel >> 8) & 0xFF
        return bytes(self.b)


def build(port):
    a = Asm(ORG)

    # PIT channel 0 counts down at 1.193182 MHz and free-runs, wrapping every
    # 54.9 ms. 512 bytes at ~5.8 us is ~3000 us, ~3550 ticks, so no wrap.
    # Interrupts off for the whole run: one timer tick inside a loop is 54.9 ms
    # of noise on a 3 ms measurement.
    a.emit(0xFA)                                    # cli
    a.emit(0xFC)                                    # cld

    def read_pit():
        a.emit(0xB0, 0x00)                          # mov al, 0    latch ch0
        a.emit(0xE6, 0x43)                          # out 43h, al
        a.emit(0xE4, 0x40)                          # in  al, 40h  LSB
        a.emit(0x88, 0xC3)                          # mov bl, al
        a.emit(0xE4, 0x40)                          # in  al, 40h  MSB
        a.emit(0x88, 0xC4)                          # mov ah, al
        a.emit(0x88, 0xD8)                          # mov al, bl   ax = MSB:LSB

    def run(slot, count, opcode_bytes):
        # before
        read_pit()
        a.emit(0x89, 0xC5)                          # mov bp, ax
        a.emit(0xBE, BUF & 0xFF, BUF >> 8)          # mov si, BUF
        a.emit(0xB9, count & 0xFF, count >> 8)      # mov cx, count
        a.emit(0xBA, port & 0xFF, port >> 8)        # mov dx, port
        a.emit(*opcode_bytes)                       # rep outs{b,w,d}
        read_pit()
        # PIT counts DOWN, so delta = before - after
        a.emit(0x29, 0xC5)                          # sub bp, ax
        off = RES + slot * 2
        a.emit(0x89, 0x2E, off & 0xFF, off >> 8)    # mov [RES+slot*2], bp

    run(0, NBYTES,      (0xF3, 0x6E))               # rep outsb
    run(1, NBYTES // 2, (0xF3, 0x6F))               # rep outsw
    run(2, NBYTES // 4, (0x66, 0xF3, 0x6F))         # rep outsd

    a.emit(0xFB)                                    # sti
    a.label("done")
    a.emit(0xCC)                                    # int3 -> back to DEBUG
    a.label("pit")                                  # (unused; kept for clarity)
    return a.link(), a.lbl["done"]


def verify(code, port):
    """Disassemble what we built. A wrong prefix still runs and still lies."""
    try:
        import capstone
    except ImportError:
        print("capstone not installed - NOT verified", file=sys.stderr)
        return
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    seen = []
    for i in md.disasm(code, ORG):
        seen.append(i.mnemonic)
        print(f"; {i.address:#06x}  {i.bytes.hex():<10} {i.mnemonic} {i.op_str}",
              file=sys.stderr)
    for want in ("outsb", "outsw", "outsd"):
        if not any(want in m for m in seen):
            sys.exit(f"FAILED: no {want} in the generated code")
    if seen.count("rep") < 3 and sum("rep" in m for m in seen) < 3:
        sys.exit("FAILED: fewer than three rep-prefixed transfers")
    print(f"; verified: three widths, port {port:#05x}", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="378",
                    help="hex port to write, default 378 (LPT1 data register)")
    args = ap.parse_args()
    port = int(args.port, 16)

    code, done = build(port)
    verify(code, port)

    out = []
    out.append(f"e {ORG:x} " + " ".join(f"{b:02x}" for b in code))
    out.append(f"e {RES:x} 00 00 00 00 00 00")
    out.append(f"g={ORG:x} {done:x}")
    out.append(f"d {RES:x} l 6")
    out.append("q")
    print("\r\n".join(out) + "\r\n", end="")


if __name__ == "__main__":
    main()
