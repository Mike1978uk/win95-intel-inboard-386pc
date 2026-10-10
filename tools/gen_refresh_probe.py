#!/usr/bin/env python3
"""Does a slower DRAM refresh buy the Inboard any bus time?

The XT refreshes its planar DRAM by a DMA cycle on channel 0, paced by PIT
channel 1: IBM's divisor 18 gives one every ~15 us. Speeder, FastV20 and GLaBIOS
lengthen it (divisor 64 or more) to free the bus. The Inboard runs from its own
RAM, so it meets refresh only on a bus access - this measures how much that costs.

Four arms, interleaved 18, 64, 18, 64, each timing three 512-byte transfers:
    out 0278h byte (I/O), write BA000h dword (memory), read C0000h byte (memory)
Divisor 18 is written back before the script returns. The planar's 64 KB is
under-refreshed for well under a second.

    python tools/gen_refresh_probe.py > RFSH.SCR
    on the machine:   DEBUG < RFSH.SCR > RFSH.OUT
    python tools/gen_refresh_probe.py --decode RFSH.OUT
"""

import argparse
import re
import sys

ORG = 0x100
RES = 0x400
BUF = 0x2000
NBYTES = 512
TICK_US = 1e6 / 1193182
ARMS = [18, 64, 18, 64]
TESTS = [("out 0278 byte", "out", 1), ("write BA000 dword", "stos", 4),
         ("read C0000 byte", "movs", 1)]


def build():
    b = bytearray()
    e = b.extend

    def set_refresh(div):
        e([0xB0, 0x54, 0xE6, 0x43])                 # mov al,54h / out 43h,al  ch1 LSB mode 2
        e([0xB0, div, 0xE6, 0x41])                  # mov al,div / out 41h,al

    def read_pit():
        e([0xB0, 0x00, 0xE6, 0x43, 0xE4, 0x40, 0x88, 0xC3, 0xE4, 0x40, 0x88, 0xC4, 0x88, 0xD8])

    e([0xFA, 0xFC])                                 # cli / cld
    slot = 0
    for div in ARMS:
        set_refresh(div)
        for _, kind, width in TESTS:
            count = NBYTES // width
            e([0x1E, 0x06])                         # push ds / push es
            read_pit()
            e([0x89, 0xC5, 0xB9, count & 0xFF, count >> 8])   # mov bp,ax / mov cx,count
            if kind == "out":
                e([0xBA, 0x78, 0x02, 0xBE, BUF & 0xFF, BUF >> 8, 0xF3, 0x6E])
            elif kind == "stos":
                e([0xB8, 0x00, 0xBA, 0x8E, 0xC0, 0x31, 0xFF,
                   0x66, 0xB8, 0x20, 0x07, 0x20, 0x07, 0x66, 0xF3, 0xAB])
            else:
                e([0xBF, BUF & 0xFF, BUF >> 8, 0xB8, 0x00, 0xC0, 0x8E, 0xD8,
                   0x31, 0xF6, 0xF3, 0xA4])
            read_pit()
            e([0x07, 0x1F, 0x29, 0xC5])             # pop es / pop ds / sub bp,ax
            off = RES + 2 * slot
            e([0x89, 0x2E, off & 0xFF, off >> 8])
            slot += 1
    set_refresh(18)                                 # IBM's rate back, always
    e([0xFB])                                       # sti
    done = ORG + len(b)
    e([0xCC])
    assert ORG + len(b) < RES
    return bytes(b), done, slot


def verify(code, slots):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    ins = list(md.disasm(code, ORG))
    if sum(i.size for i in ins) != len(code):
        sys.exit("FAILED: disassembly does not cover the code")
    reps = [i.mnemonic for i in ins if i.mnemonic.startswith("rep ")]
    if reps != ["rep outsb", "rep stosd", "rep movsb"] * len(ARMS):
        sys.exit(f"FAILED: transfers {reps}")
    outs41 = [i for i in ins if i.mnemonic == "out" and i.op_str.startswith("0x41")]
    if len(outs41) != len(ARMS) + 1:
        sys.exit("FAILED: refresh writes")
    tail = [i for i in ins][-6:]
    if not (tail[2].op_str == "al, 0x12" and tail[3].op_str.startswith("0x41")):
        sys.exit("FAILED: divisor 18 is not restored last")
    print(f"; verified: {slots} timings, {len(ARMS)} arms, 18 restored, {len(code)} bytes",
          file=sys.stderr)


def decode(path):
    text = open(path, "rb").read().decode("ascii", "replace")
    bs = []
    for m in re.finditer(r"^[0-9A-Fa-f]{4}:(04[0-9A-Fa-f]0)\s+((?:[0-9A-Fa-f]{2}[ -]){1,16})", text, re.M):
        bs += [int(x, 16) for x in re.findall(r"[0-9A-Fa-f]{2}", m.group(2))]
    n = len(ARMS) * len(TESTS)
    if len(bs) < 2 * n:
        sys.exit(f"only {len(bs)} result bytes")
    print(f"{'divisor':>7}  " + "  ".join(f"{t[0]:>18}" for t in TESTS) + "   (us per byte)")
    for a, div in enumerate(ARMS):
        row = []
        for t in range(len(TESTS)):
            s = a * len(TESTS) + t
            ticks = bs[2 * s] | (bs[2 * s + 1] << 8)
            row.append(f"{ticks * TICK_US / NBYTES:>18.3f}")
        print(f"{div:>7}  " + "  ".join(row))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decode")
    a = ap.parse_args()
    if a.decode:
        decode(a.decode)
        return
    code, done, slots = build()
    verify(code, slots)
    out = [f"e {ORG + o:x} " + " ".join(f"{x:02x}" for x in code[o:o + 16])
           for o in range(0, len(code), 16)]
    out += [f"f {RES:x} l {2 * slots:x} 0", f"g={ORG:x} {done:x}", f"d {RES:x} l {2 * slots:x}", "q"]
    sys.stdout.buffer.write(("\r\n".join(out) + "\r\n").encode("ascii"))


if __name__ == "__main__":
    main()
