#!/usr/bin/env python3
"""E1: does conventional memory live on the Inboard, or on the 5160 planar?

Replaces RAMTIME.SCR, whose numbers were impossible (FFFF and 12FB ticks).
Two changes:

  * PIT channel 0 latch reads (0.838 us) instead of the BIOS tick (54.9 ms).
    The counter is only LATCHED, never reprogrammed, so the system clock is
    untouched and this is read-only.
  * Each region is timed TWICE, back to back. Pass 2 much faster than pass 1
    means the region fits in the Inboard's cache and the result says nothing
    about where the RAM is - the working set has to exceed the cache. That
    confound is almost certainly why the first attempt "directionally"
    reported conventional memory as local.

32 KB per pass keeps every region under the counter's 54.9 ms wrap.

Regions, and what each is for:

    1000  physical  64K  - planar DRAM whatever the switches say
    8000  physical 512K  - planar DRAM now; Inboard RAM once SW1-3/4 are ON
    B800  video          - known across the bus, reference
    F000  system ROM     - known across the bus, reference

Run it TWICE: once as the machine stands, once after SW1-3 and SW1-4 are set
ON. Before the change 1000 and 8000 should agree - if they do not, the method
is wrong and nothing else in the capture means anything. After the change,
the ratio between them IS E1's answer, measured inside one address space with
no video or ROM reference needed.

    DEBUG < C:\\RAMTIME2.SCR > C:\\RAMTIME2.OUT

Results are six words at 0600:
    0600/0602  segment 1000, pass 1 / pass 2
    0604/0606  segment 8000, pass 1 / pass 2
    0608       B800        0060A  F000
Bigger = slower. Values are PIT ticks at 0.8381 us.
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

    # Time one 32 KB linear sweep of DS:0000. Returns elapsed PIT ticks in AX.
    # cli only spans the sweep; channel 0 keeps counting with IF clear, so the
    # measurement is unaffected and interrupt jitter is kept out of it.
    timed = ["push ds", "push bp", "push cx", "push si", "push bx",
             "mov ds,ax", "cli",
             "mov al,00", "out 43,al",          # latch channel 0
             "in al,40", "mov bl,al",
             "in al,40", "mov bh,al",
             "mov bp,bx",                        # start count
             "xor si,si", "mov cx,4000", "cld",  # 16384 words = 32 KB
             "rep lodsw",
             "mov al,00", "out 43,al",
             "in al,40", "mov bl,al",
             "in al,40", "mov bh,al",
             "sti",
             "mov ax,bp", "sub ax,bx",           # counter counts DOWN
             "pop bx", "pop si", "pop cx", "pop bp", "pop ds", "ret"]

    l = blk(0x100, main) + blk(0x300, timed)
    l += ["u 300 335", "g=100", "d 600 60B", "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    import sys
    text = build()
    if "--raw" in sys.argv:
        sys.stdout.write(text)
    else:
        print(base64.b64encode(text.encode("ascii")).decode())
