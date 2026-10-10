#!/usr/bin/env python3
"""Do the 28800's zero-wait-state bits make VGA memory cheaper on the 5160?

1CEh index ABh (vgadoc ATI.TXT): bit 0 video memory zero-wait-state writes, bit 1 BIOS ROM
zero-wait-state reads, bit 5 (28800-6) zero wait state. Our card runs with ABh = 80h, all three
off (M8REGS). VGA memory writes are already the card's cheapest path, 2.0 us per byte as dwords
(docs/mach8_card_coverage.md, "Host-interface tuning").

Six arms, ABh = saved value OR 00h, 01h, 20h, 21h, 02h, 00h, each timing four 512-byte transfers:
    write BA000h byte, word, dword (text page 2, unseen); read C0000h byte (video BIOS ROM)
The saved ABh is written back before the script returns. Interrupts are off throughout, so no
video BIOS code runs while bit 1 is set.

    python tools/gen_zws_probe.py > ZWS.SCR
    on the machine:   DEBUG < ZWS.SCR > ZWS.OUT
    python tools/gen_zws_probe.py --decode ZWS.OUT
"""

import argparse
import re
import sys

ORG = 0x100
RES = 0x800
SAVE = 0x7F0
BUF = 0x2000
NBYTES = 512
TICK_US = 1e6 / 1193182
ARMS = [0x00, 0x01, 0x20, 0x21, 0x02, 0x00]
TESTS = [("write BA000 byte", "stos", 1), ("write BA000 word", "stos", 2),
         ("write BA000 dword", "stos", 4), ("read C0000 byte", "movs", 1)]


def build():
    b = bytearray()
    e = b.extend

    def select_ab():
        e([0xBA, 0xCE, 0x01, 0xB0, 0xAB, 0xEE, 0x42])     # mov dx,1CEh / mov al,ABh / out / inc dx

    def set_ab(mask):
        select_ab()
        e([0xA0, SAVE & 0xFF, SAVE >> 8, 0x0C, mask, 0xEE])  # mov al,[SAVE] / or al,mask / out dx,al

    def read_pit():
        e([0xB0, 0x00, 0xE6, 0x43, 0xE4, 0x40, 0x88, 0xC3, 0xE4, 0x40, 0x88, 0xC4, 0x88, 0xD8])

    e([0xFA, 0xFC])                                       # cli / cld
    select_ab()
    e([0xEC, 0xA2, SAVE & 0xFF, SAVE >> 8])               # in al,dx / mov [SAVE],al
    slot = 0
    for mask in ARMS:
        set_ab(mask)
        for _, kind, width in TESTS:
            count = NBYTES // width
            e([0x1E, 0x06])                               # push ds / push es
            read_pit()
            e([0x89, 0xC5, 0xB9, count & 0xFF, count >> 8])   # mov bp,ax / mov cx,count
            if kind == "stos":
                e([0xB8, 0x00, 0xBA, 0x8E, 0xC0, 0x31, 0xFF])  # es = BA00h, di = 0
                if width == 1:
                    e([0xB0, 0x20, 0xF3, 0xAA])
                elif width == 2:
                    e([0xB8, 0x20, 0x07, 0xF3, 0xAB])
                else:
                    e([0x66, 0xB8, 0x20, 0x07, 0x20, 0x07, 0x66, 0xF3, 0xAB])
            else:
                e([0xBF, BUF & 0xFF, BUF >> 8, 0xB8, 0x00, 0xC0, 0x8E, 0xD8,
                   0x31, 0xF6, 0xF3, 0xA4])
            read_pit()
            e([0x07, 0x1F, 0x29, 0xC5])                   # pop es / pop ds / sub bp,ax
            off = RES + 2 * slot
            e([0x89, 0x2E, off & 0xFF, off >> 8])
            slot += 1
    set_ab(0x00)                                          # the card's own value back, always
    e([0xFB])                                             # sti
    done = ORG + len(b)
    e([0xCC])
    assert ORG + len(b) < SAVE
    return bytes(b), done, slot


def verify(code, slots):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    ins = list(md.disasm(code, ORG))
    if sum(i.size for i in ins) != len(code):
        sys.exit("FAILED: disassembly does not cover the code")
    reps = [i.mnemonic for i in ins if i.mnemonic.startswith("rep ")]
    if reps != ["rep stosb", "rep stosw", "rep stosd", "rep movsb"] * len(ARMS):
        sys.exit(f"FAILED: transfers {reps}")
    ors = [i.op_str for i in ins if i.mnemonic == "or" and i.op_str.startswith("al,")]
    want = [f"al, {m:#x}" if m > 9 else f"al, {m}" for m in ARMS + [0]]   # capstone prints 0-9 in decimal
    if ors != want:
        sys.exit(f"FAILED: arm masks {ors}")
    tail = ins[-4:]
    if not (tail[0].op_str == "al, 0" and tail[1].mnemonic == "out" and tail[2].mnemonic == "sti"):
        sys.exit("FAILED: the saved ABh is not restored last")
    print(f"; verified: {slots} timings, {len(ARMS)} arms, ABh restored, {len(code)} bytes", file=sys.stderr)


def decode(path):
    text = open(path, "rb").read().decode("ascii", "replace")
    bs = []
    for m in re.finditer(r"^[0-9A-Fa-f]{4}:(08[0-9A-Fa-f]0)\s+((?:[0-9A-Fa-f]{2}[ -]){1,16})", text, re.M):
        bs += [int(x, 16) for x in re.findall(r"[0-9A-Fa-f]{2}", m.group(2))]
    n = len(ARMS) * len(TESTS)
    if len(bs) < 2 * n:
        sys.exit(f"only {len(bs)} result bytes")
    print(f"{'AB |':>5}  " + "  ".join(f"{t[0]:>18}" for t in TESTS) + "   (us per byte)")
    for a, mask in enumerate(ARMS):
        row = []
        for t in range(len(TESTS)):
            s = a * len(TESTS) + t
            ticks = bs[2 * s] | (bs[2 * s + 1] << 8)
            row.append(f"{ticks * TICK_US / NBYTES:>18.3f}")
        print(f"{mask:>5X}  " + "  ".join(row))


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
