"""Emit a .COM that times byte/word/dword reads out of a ROM segment.

Same measurement as gen_memwidth_probe.py, but as a program rather than a
DEBUG script. A DEBUG script wedged this machine twice on 2026-09-20 - once
on an escaped shell metacharacter, once on LF-only line endings - and once
wedged there is no remote recovery, because DEBUG's stdin is the file so no
keystroke reaches it. A .COM takes no stdin and exits on its own.

Results are left in the BIOS Intra-Applications Communications Area at
0040:00F0 (16 bytes, reserved for exactly this), so they can be read back
with a plain memory read after the program has exited.

    python tools/gen_memwidth_com.py --seg c000 --out PROBE.COM
"""
import argparse
import struct
import sys

DST_OFF = 0x2000          # destination inside our own 64K segment
IACA    = 0x00F0          # 0040:00F0, BIOS scratch


def pit_read():
    """Latch PIT channel 0 and leave the 16-bit count in BX."""
    return bytes([0xB0, 0x00,        # mov al,0      (latch ch0)
                  0xE6, 0x43,        # out 43h,al
                  0xE4, 0x40,        # in  al,40h
                  0x88, 0xC3,        # mov bl,al
                  0xE4, 0x40,        # in  al,40h
                  0x88, 0xC7])       # mov bh,al   -> BX = count


def one_pass(count, rep_op):
    b = pit_read()
    b += bytes([0x89, 0xDD])                       # mov bp,bx      (t0)
    b += bytes([0xBE, 0x00, 0x00])                 # mov si,0
    b += bytes([0xBF]) + struct.pack("<H", DST_OFF)  # mov di,DST
    b += bytes([0xB9]) + struct.pack("<H", count)  # mov cx,count
    b += rep_op                                    # rep movs*
    b += pit_read()                                # BX = t1
    b += bytes([0x29, 0xDD])                       # sub bp,bx  (down-counter)
    b += bytes([0x55])                             # push bp
    return b


def build(seg):
    c = bytes([0xFA, 0xFC])                        # cli ; cld
    c += bytes([0xB8]) + struct.pack("<H", seg)    # mov ax,seg
    c += bytes([0x8E, 0xD8])                       # mov ds,ax   (source)
    # ES is already CS on .COM entry, so ES:DST is our own segment.
    c += one_pass(512, bytes([0xF3, 0xA4]))            # rep movsb
    c += one_pass(256, bytes([0xF3, 0xA5]))            # rep movsw
    c += one_pass(128, bytes([0x66, 0xF3, 0xA5]))      # rep movsd
    c += bytes([0xFB])                             # sti
    c += bytes([0xB8, 0x40, 0x00, 0x8E, 0xD8])     # mov ax,40h ; mov ds,ax
    for off in (IACA + 4, IACA + 2, IACA):         # pops come back reversed
        c += bytes([0x5D])                         # pop bp
        c += bytes([0x89, 0x2E]) + struct.pack("<H", off)   # mov [off],bp
    c += bytes([0xB4, 0x4C, 0xCD, 0x21])           # mov ah,4Ch ; int 21h
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seg", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    code = build(int(a.seg, 16))

    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_16
        md = Cs(CS_ARCH_X86, CS_MODE_16)
        text = [i.mnemonic + " " + i.op_str for i in md.disasm(code, 0x100)]
        reps = [t for t in text if t.startswith("rep movs")]
        if len(reps) != 3:
            sys.exit("FAILED: %d rep-prefixed copies, expected 3: %r" % (len(reps), reps))
        if not any("int 0x21" in t for t in text):
            sys.exit("FAILED: no int 21h exit")
        if any("int3" in t for t in text):
            sys.exit("FAILED: an INT 3 survived - this must not need DEBUG")
        print("verified: %s" % ", ".join(reps))
    except ImportError:
        print("WARNING: capstone missing, code NOT verified")

    open(a.out, "wb").write(code)
    print("wrote %s: %d bytes, source segment %04X, results at 0040:%04X"
          % (a.out, len(code), int(a.seg, 16), IACA))


if __name__ == "__main__":
    main()
