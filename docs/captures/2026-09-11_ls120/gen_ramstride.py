#!/usr/bin/env python3
"""E1, second attempt: a STRIDED read, because the sequential one is cached.

RAMTIME2 swept 32 KB linearly and found planar DRAM and Inboard-backfilled
RAM identical to the tick (seg 8000: 5290 before the switch change, 5290
after). The instrument was sound - the F000 reference came back byte-identical
across both runs, so the noise floor is +/-2 ticks - but a sequential sweep is
the one pattern a cache with line fill hides completely: one miss pulls a whole
line and the next several reads are hits, so a slow bus is amortised away.

This reads ONE byte per 32-byte stride across 64 KB. Every access should be a
fresh line, so the fill cost is paid in full and a bus crossing has nowhere to
hide. 2048 accesses per region.

If seg 8000 is now markedly faster than seg 1000, conventional memory really
did move onto the card and E1 is answered. If they are still identical, the
Inboard serves planar DRAM at local speed for reads and the only remaining
gain is bus occupancy - which needs a card-throughput test, not a memory one.

Run with the switches as Intel requires (SW1-3 and SW1-4 ON):

    DEBUG < C:\\RMTMST.SCR > C:\\RMTMST.OUT

Six words at 0600, PIT ticks of 0.8381 us:
    0600/0602  seg 1000 (64K)  - planar DRAM, pass 1 / pass 2
    0604/0606  seg 8000 (512K) - Inboard backfill, pass 1 / pass 2
    0608       B800 video      0060A  F000 ROM
Bigger = slower. Pass 2 much faster than pass 1 means the stride is still
smaller than the cache line and the result is void.
"""
import base64

CRLF = "\r\n"


def blk(a, i):
    return ["a %04X" % a] + i + [""]


def build():
    main = []
    for seg, slot in [("1000", "0600"), ("1000", "0602"),
                      ("8000", "0604"), ("8000", "0606"),
                      ("B800", "0608"), ("F000", "060A")]:
        main += ["mov ax,%s" % seg, "call 0300", "mov [%s],ax" % slot]
    main += ["int 3"]

    latch = ["mov al,00", "out 43,al",
             "in al,40", "mov bl,al", "in al,40", "mov bh,al"]

    enter = (["push ds", "push bp", "push cx", "push si", "push bx",
              "mov ds,ax", "cli"] + latch +
             ["mov bp,bx", "xor si,si", "mov cx,0800", "jmp 0340"])
    # 2048 reads x 32-byte stride = exactly 64 KB, SI wrapping once.
    loop = ["mov al,[si]", "add si,20", "dec cx", "jnz 0340", "jmp 0350"]
    leave = latch + ["sti", "mov ax,bp", "sub ax,bx",
                     "pop bx", "pop si", "pop cx", "pop bp", "pop ds", "ret"]

    l = blk(0x100, main) + blk(0x300, enter) + blk(0x340, loop) + blk(0x350, leave)
    l += ["u 300 367", "g=100", "d 600 60B", "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    import sys
    text = build()
    if "--raw" in sys.argv:
        sys.stdout.write(text)
    else:
        print(base64.b64encode(text.encode("ascii")).decode())
