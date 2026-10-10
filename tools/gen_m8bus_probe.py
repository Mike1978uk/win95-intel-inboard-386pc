#!/usr/bin/env python3
"""What the Graphics Ultra costs on the bus, against an undecoded-port control.

Before any wait-state or FIFO knob on the card is changed (MAX_WAITSTATES 6AEEh,
FIFO_OPT 36EEh, the 28800's), find out whether the card adds anything to the
Inboard's own per-access cost (technique 128: 5.695 us per I/O byte, 2.861 us
per memory byte read, set by the Inboard and not the card). See
docs/mach8_card_coverage.md, "Host-interface tuning across both chips".

Fourteen timed transfers of 512 bytes, PIT channel 0 latched either side:

    0  1  outsb / outsw   0278h          control: decodes to nothing here
    2  3  outsb / outsw   A6E8h          Mach8 FRGD_COLOR (write; harmless)
    4  5  insb  / insw    0278h          control, read
    6  7  insb  / insw    9AE8h          Mach8 GE_STAT (read; what a driver polls)
    8  9 10 stos b/w/d    BA00:0000      28800 memory write, text page 2 (unseen)
   11 12 13 movs b/w/d    C000:0000 ->   Mach8 option ROM read (ROM_SPEED)

No dword I/O: A6EAh/A6EBh are not Mach8 registers and their decode is unknown.
Text page 1 (B9000h) is the LS-120 trace page, so page 2 is used.

Emits a DEBUG script; machine code is disassembled back with capstone first.

    python tools/gen_m8bus_probe.py > M8BUS.SCR
    on the machine:   DEBUG < M8BUS.SCR > M8BUS.OUT
    python tools/gen_m8bus_probe.py --decode M8BUS.OUT
"""

import argparse
import re
import sys

ORG = 0x100
RES = 0x400           # 14 x 16-bit deltas
BUF = 0x2000          # 512-byte scratch in DEBUG's own segment
NBYTES = 512
TICK_US = 1e6 / 1193182

TESTS = [
    # (label, kind, width, port_or_seg)
    ("out 0278 control", "out", 1, 0x0278),
    ("out 0278 control", "out", 2, 0x0278),
    ("out A6E8 FRGD_COLOR", "out", 1, 0xA6E8),
    ("out A6E8 FRGD_COLOR", "out", 2, 0xA6E8),
    ("in  0278 control", "in", 1, 0x0278),
    ("in  0278 control", "in", 2, 0x0278),
    ("in  9AE8 GE_STAT", "in", 1, 0x9AE8),
    ("in  9AE8 GE_STAT", "in", 2, 0x9AE8),
    ("mem write BA000", "stos", 1, 0xBA00),
    ("mem write BA000", "stos", 2, 0xBA00),
    ("mem write BA000", "stos", 4, 0xBA00),
    ("mem read  C0000", "movs", 1, 0xC000),
    ("mem read  C0000", "movs", 2, 0xC000),
    ("mem read  C0000", "movs", 4, 0xC000),
]


def build():
    b = bytearray()
    e = b.extend

    def pc():
        return ORG + len(b)

    def read_pit():
        e([0xB0, 0x00, 0xE6, 0x43])                 # mov al,0 / out 43h,al  latch ch0
        e([0xE4, 0x40, 0x88, 0xC3])                 # in al,40h / mov bl,al
        e([0xE4, 0x40, 0x88, 0xC4, 0x88, 0xD8])     # in al,40h / mov ah,al / mov al,bl

    e([0xFA, 0xFC])                                 # cli / cld
    for slot, (_, kind, width, arg) in enumerate(TESTS):
        count = NBYTES // width
        e([0x1E, 0x06])                             # push ds / push es
        read_pit()
        e([0x89, 0xC5])                             # mov bp, ax
        e([0xB9, count & 0xFF, count >> 8])         # mov cx, count
        op32 = [0x66] if width == 4 else []
        if kind in ("out", "in"):
            e([0xBA, arg & 0xFF, arg >> 8])         # mov dx, port
            if kind == "out":
                e([0xBE, BUF & 0xFF, BUF >> 8])     # mov si, BUF  (ds = DEBUG seg)
                e([0xF3, 0x6E if width == 1 else 0x6F])   # rep outsb/outsw
            else:
                e([0xBF, BUF & 0xFF, BUF >> 8])     # mov di, BUF  (es = DEBUG seg)
                e([0xF3, 0x6C if width == 1 else 0x6D])   # rep insb/insw
        elif kind == "stos":
            e([0xB8, arg & 0xFF, arg >> 8])         # mov ax, seg
            e([0x8E, 0xC0])                         # mov es, ax
            e([0x31, 0xFF])                         # xor di, di
            e([0x66, 0xB8, 0x20, 0x07, 0x20, 0x07]) # mov eax, 07200720h (spaces)
            e(op32 + [0xF3, 0xAA if width == 1 else 0xAB])  # rep stos
        elif kind == "movs":
            e([0xBF, BUF & 0xFF, BUF >> 8])         # mov di, BUF  (es = DEBUG seg)
            e([0xB8, arg & 0xFF, arg >> 8])         # mov ax, seg
            e([0x8E, 0xD8])                         # mov ds, ax
            e([0x31, 0xF6])                         # xor si, si
            e(op32 + [0xF3, 0xA4 if width == 1 else 0xA5])  # rep movs
        read_pit()
        e([0x07, 0x1F])                             # pop es / pop ds
        e([0x29, 0xC5])                             # sub bp, ax  (PIT counts down)
        off = RES + slot * 2
        e([0x89, 0x2E, off & 0xFF, off >> 8])       # mov [RES+2*slot], bp
    e([0xFB])                                       # sti
    done = pc()
    e([0xCC])                                       # int3 -> DEBUG
    assert pc() < RES, "code overlaps the result area"
    return bytes(b), done


def verify(code):
    try:
        import capstone
    except ImportError:
        sys.exit("capstone not installed - refusing to emit an unverified probe")
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    ins = list(md.disasm(code, ORG))
    if sum(i.size for i in ins) != len(code):
        sys.exit("FAILED: disassembly does not cover the code")
    for i in ins:
        print(f"; {i.address:#06x}  {i.bytes.hex():<14} {i.mnemonic} {i.op_str}", file=sys.stderr)
    want = ["rep outsb", "rep outsw", "rep outsb", "rep outsw", "rep insb", "rep insw",
            "rep insb", "rep insw", "rep stosb", "rep stosw", "rep stosd",
            "rep movsb", "rep movsw", "rep movsd"]
    got = [i.mnemonic for i in ins if i.mnemonic.startswith("rep ")]
    if got != want:
        sys.exit(f"FAILED: transfers {got}")
    print(f"; verified: {len(got)} transfers in order, {len(code)} bytes", file=sys.stderr)


def decode(path):
    text = open(path, "rb").read().decode("ascii", "replace")
    words = []
    for m in re.finditer(r"^[0-9A-Fa-f]{4}:(04[0-9A-Fa-f]0)\s+((?:[0-9A-Fa-f]{2}[ -]){1,16})", text, re.M):
        bs = [int(x, 16) for x in re.findall(r"[0-9A-Fa-f]{2}", m.group(2))]
        words.extend(bs)
    if len(words) < 2 * len(TESTS):
        sys.exit(f"only {len(words)} result bytes found in {path}")
    print(f"{'#':>2}  {'test':<22}{'w':>2}  {'ticks':>6}  {'us/access':>9}  {'us/byte':>7}")
    for slot, (label, kind, width, _) in enumerate(TESTS):
        t = words[2 * slot] | (words[2 * slot + 1] << 8)
        us = t * TICK_US
        print(f"{slot:>2}  {label:<22}{width:>2}  {t:>6}  {us / (NBYTES // width):>9.3f}  {us / NBYTES:>7.3f}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decode", metavar="OUT", help="decode a captured DEBUG output file")
    a = ap.parse_args()
    if a.decode:
        decode(a.decode)
        return
    code, done = build()
    verify(code)
    out = []
    for off in range(0, len(code), 16):
        out.append(f"e {ORG + off:x} " + " ".join(f"{x:02x}" for x in code[off:off + 16]))
    out.append(f"f {RES:x} l {2 * len(TESTS):x} 0")
    out.append(f"g={ORG:x} {done:x}")
    out.append(f"d {RES:x} l {2 * len(TESTS):x}")
    out.append("q")
    sys.stdout.buffer.write(("\r\n".join(out) + "\r\n").encode("ascii"))


if __name__ == "__main__":
    main()
