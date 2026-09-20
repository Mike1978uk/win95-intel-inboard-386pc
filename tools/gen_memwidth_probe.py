#!/usr/bin/env python3
"""Is an ISA MEMORY crossing cheaper than an ISA I/O crossing on the Inboard?

`gen_iowidth_probe.py` measured port access: 5.695 / 3.819 / 3.183 us per byte
for byte / word / dword. An XT I/O bus cycle carries a mandatory wait state
that a memory cycle does not - 5 clocks against 4, 1.048 us against 0.838 - but
nobody has measured whether the Inboard's ~3.75 us synchronisation is the same
for both. If memory syncs cheaper, then any card with a memory aperture should
move data through it rather than through ports, and that changes how the XT-IDE
and Trantor drivers should be written.

512 bytes are copied out of an option ROM into RAM three ways - 512 x movsb,
256 x movsw, 128 x movsd - with the PIT latched either side. Reading a ROM has
no side effects, which is why the source is a ROM and not a card's data window.

⚠ The result also says whether the ROM is SHADOWED. The Inboard has its own
local RAM, and a shadowed ROM never reaches the ISA bus. Per-byte costs near
the port numbers mean it is crossing; costs an order of magnitude lower mean
the read never left the card and the measurement says nothing about ISA.

    python tools/gen_memwidth_probe.py --seg d800   > memwidth.scr

On the machine:  DEBUG < MEMWIDTH.SCR
Read the three 16-bit deltas from the dump at 0200h.
"""

import argparse
import sys

ORG = 0x100
DST = 0x2000
RES = 0x200
NBYTES = 512


def build(seg):
    b = bytearray()

    def emit(*v):
        b.extend(v)

    def read_pit():
        # Latch counter 0 and read it LSB then MSB. Touches no segment register.
        emit(0xB0, 0x00)            # mov al, 0
        emit(0xE6, 0x43)            # out 43h, al
        emit(0xE4, 0x40)            # in  al, 40h
        emit(0x88, 0xC3)            # mov bl, al
        emit(0xE4, 0x40)            # in  al, 40h
        emit(0x88, 0xC4)            # mov ah, al
        emit(0x88, 0xD8)            # mov al, bl

    emit(0xFA)                      # cli
    emit(0xFC)                      # cld
    emit(0x1E)                      # push ds
    # DS addresses the ROM for the whole run, so every result store needs an ES
    # override - ES stays on the .COM's own segment.
    emit(0xB8, seg & 0xFF, seg >> 8)  # mov ax, seg
    emit(0x8E, 0xD8)                # mov ds, ax

    def run(slot, count, opcode):
        read_pit()
        emit(0x89, 0xC5)                        # mov bp, ax
        emit(0xBE, 0x00, 0x00)                  # mov si, 0      (ROM offset 0)
        emit(0xBF, DST & 0xFF, DST >> 8)        # mov di, DST
        emit(0xB9, count & 0xFF, count >> 8)    # mov cx, count
        emit(*opcode)                           # rep movs{b,w,d}
        read_pit()
        emit(0x29, 0xC5)                        # sub bp, ax   (PIT counts down)
        off = RES + slot * 2
        emit(0x26, 0x89, 0x2E, off & 0xFF, off >> 8)   # mov es:[off], bp

    run(0, NBYTES,      (0xF3, 0xA4))           # rep movsb
    run(1, NBYTES // 2, (0xF3, 0xA5))           # rep movsw
    run(2, NBYTES // 4, (0x66, 0xF3, 0xA5))     # rep movsd

    emit(0x1F)                      # pop ds
    emit(0xFB)                      # sti
    done = ORG + len(b)
    emit(0xCC)                      # int3
    return bytes(b), done


def verify(code, seg):
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
    for want in ("movsb", "movsw", "movsd"):
        if not any(want in m for m in seen):
            sys.exit(f"FAILED: no {want} in the generated code")
    if sum("rep" in m for m in seen) < 3:
        sys.exit("FAILED: fewer than three rep-prefixed copies")
    print(f"; verified: three widths, source segment {seg:#06x}", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seg", default="d800",
                    help="hex source segment, default d800 (the XT-CF option ROM)")
    args = ap.parse_args()
    seg = int(args.seg, 16)

    code, done = build(seg)
    verify(code, seg)

    out = []
    for off in range(0, len(code), 16):
        chunk = code[off:off + 16]
        out.append(f"e {ORG + off:x} " + " ".join(f"{x:02x}" for x in chunk))
    out.append(f"e {RES:x} 00 00 00 00 00 00")
    out.append(f"g={ORG:x} {done:x}")
    out.append(f"d {RES:x} l 6")
    out.append("q")
    sys.stdout.buffer.write(("\r\n".join(out) + "\r\n").encode("ascii"))


if __name__ == "__main__":
    main()
