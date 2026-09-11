#!/usr/bin/env python3
"""ATAPI INQUIRY over the EPAT bridge, with the STREAMING block read.

Supersedes gen_inquiry_probe.py, which differed in one routine and that one
routine was the bug: its data phase (0500) was a loop of single-register
reads of the ATA data register, so it returned 36 zeros from a drive that
had the reply ready (byte count 0x0024, status 50, error 00).

Technique 111: registers and bulk data are different protocols on the same
wire. The bridge must be put into block mode ONCE and then streamed, with a
phase bit alternating on the control port each byte and the last byte
announced. A register loop cannot express that and fails silently.

This script's block read mirrors LS_BlockRead in LS120TR.ASM instruction for
instruction, so a pass here is evidence about the DRIVER, not merely about
the bridge. It is technique 111b item 3 and it needs no Windows.

Run from real-mode DOS with the vendor driver REM'd out of CONFIG.SYS:

    DEBUG < C:\\INQ7.SCR > C:\\INQ7.OUT

[0600] = final ATA status   [0601] = error register
[0700..0723] = the 36-byte INQUIRY reply.

PASS looks like "MATSHITA" in ASCII at 0708 and "LS-120" at 0710.
36 zeros means the block read is still wrong.
All-FF means the bridge never connected - check the CPP checkpoints first.

Write the output with CRLF (technique 105): DEBUG cannot read an LF-only
script - it reads the whole file as one line, truncates at ~128 bytes and
rings the BEL continuously.
"""
import base64

CRLF = "\r\n"
B, S, C, E = 0x378, 0x379, 0x37A, 0x77A


def blk(a, i):
    return ["a %04X" % a] + i + [""]


def build():
    # --- unchanged from probe 1: these are VERIFIED on hardware 2026-09-08 ---
    cpp4 = ["mov dx,%04X" % C, "mov al,04", "out dx,al", "jmp 0320"]
    cppp = ["mov dx,%04X" % C, "in al,dx", "and al,0F", "out dx,al", "jmp 0320"]
    fr = ["cli", "mov dx,%04X" % B]
    for b in ["22", "AA", "55", "00", "FF", "87", "78"]:
        fr += ["mov al,%s" % b, "out dx,al", "out dx,al"]
    fr += ["mov al,ah", "out dx,al", "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "out dx,al", "out dx,al",
           "in al,dx", "and al,10", "or al,05", "out dx,al", "out dx,al",
           "and al,FE", "out dx,al", "out dx,al",
           "mov dx,%04X" % B, "mov al,FF", "out dx,al", "out dx,al",
           "sti", "ret"]
    nib = ["mov dx,%04X" % B, "out dx,al",
           "mov dx,%04X" % C, "mov al,01", "out dx,al", "mov al,03", "out dx,al",
           "mov dx,%04X" % S, "in al,dx", "and al,F0",
           "shr al,1", "shr al,1", "shr al,1", "shr al,1", "mov bl,al",
           "mov dx,%04X" % C, "mov al,04", "out dx,al",
           "mov dx,%04X" % S, "in al,dx", "and al,F0", "or al,bl", "ret"]
    kick = ["mov dx,%04X" % C, "mov al,04", "out dx,al", "mov al,0C", "out dx,al",
            "mov al,0E", "out dx,al", "out dx,al", "out dx,al",
            "mov al,04", "out dx,al", "out dx,al", "ret"]
    wr = ["mov bl,ah", "or al,60", "mov dx,%04X" % B, "out dx,al",
          "mov dx,%04X" % C, "mov al,01", "out dx,al",
          "mov dx,%04X" % B, "mov al,bl", "out dx,al",
          "mov dx,%04X" % C, "mov al,04", "out dx,al", "ret"]

    m = ["mov dx,%04X" % E, "in al,dx", "and al,34", "out dx,al", "call 03D0",
         "mov ah,30", "call 0300", "mov ah,40", "call 0300",
         "mov ah,50", "call 0300", "mov ah,00", "call 0300",
         "mov ah,E0", "call 0310",
         # epat_connect's tail, which probe 5 omitted entirely:
         #   w0(0); w2(1); w2(4)
         # then four BRIDGE registers - cont 2, whose cont_map offset is 0, so
         # these are raw register numbers with no task-file 0x18 added.
         #   WR(8,0x10)  WR(0xc,0x14)  WR(0xa,0x38)  WR(0x12,0x10)
         "mov dx,%04X" % B, "xor al,al", "out dx,al",
         "mov dx,%04X" % C, "mov al,01", "out dx,al", "mov al,04", "out dx,al",
         "mov ax,1008", "call 03F0",
         "mov ax,140C", "call 03F0",
         "mov ax,380A", "call 03F0",
         "mov ax,1012", "call 03F0",
         "mov ax,0416", "call 03F0", "mov ax,0016", "call 03F0", "call 0420",
         "mov al,1F", "call 03A0", "mov [0602],al",
         "mov ax,A01E", "call 03F0",
         "mov ax,241C", "call 03F0",
         "mov ax,001D", "call 03F0",
         "mov ax,0019", "call 03F0",
         "mov ax,A01F", "call 03F0",
         "call 0460",
         "mov al,1F", "call 03A0", "mov [0603],al"]
    for v in ["12", "00", "00", "00", "24", "00", "00", "00", "00", "00", "00", "00"]:
        m += ["mov ax,%s18" % v, "call 03F0"]
    m += ["call 0460",
          "mov al,1F", "call 03A0", "mov [0604],al",
          "mov al,1A", "call 03A0", "mov [0605],al",
          "mov al,1C", "call 03A0", "mov [0606],al",
          "mov al,1D", "call 03A0", "mov [0607],al",
          "call 0500",
          "mov al,1F", "call 03A0", "mov [0600],al",
          "mov al,19", "call 03A0", "mov [0601],al",
          "mov ah,40", "call 0300", "mov ah,30", "call 0300", "int 3"]

    # --- THE CHANGED ROUTINE: streaming block read, epat.c mode 0 ---
    # Mirrors LS_BlockRead in LS120TR.ASM. SI=buffer, CX=count, BH=phase.
    # Every block ends in an explicit jmp or ret so no block relies on
    # falling through into the next one's address (DEBUG leaves the gap
    # between blocks as whatever was already in memory).
    setup = ["cli", "mov si,0700", "mov cx,0024",
             "mov dx,%04X" % B, "mov al,07", "out dx,al",      # w0(7)
             "mov dx,%04X" % C, "mov al,01", "out dx,al",      # w2(1)
             "mov al,03", "out dx,al",                          # w2(3)
             "mov dx,%04X" % B, "mov al,FF", "out dx,al",      # w0(0xff)
             "xor bh,bh", "jmp 0530"]
    head = ["cmp cx,0001", "jnz 0550", "jmp 05C0"]
    body = ["mov dx,%04X" % C, "mov al,06", "add al,bh", "out dx,al",  # w2(6+ph)
            "mov dx,%04X" % S, "in al,dx", "mov ah,al",                 # a = r1()
            "test al,08", "jnz 0580",          # bit 3: both nibbles in one cycle
            "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",  # w2(4+ph)
            "mov dx,%04X" % S, "in al,dx",                              # b = r1()
            "jmp 0580"]
    join = ["and al,F0",                                   # b & 0xF0
            "shr ah,1", "shr ah,1", "shr ah,1", "shr ah,1",  # a >> 4
            "or al,ah", "mov [si],al", "inc si",
            "xor bh,01", "dec cx", "jnz 0530", "jmp 05A0"]
    leave = ["mov dx,%04X" % B, "xor al,al", "out dx,al",   # w0(0)
             "mov dx,%04X" % C, "mov al,04", "out dx,al",   # w2(4)
             "sti", "ret"]
    last = ["mov dx,%04X" % B, "mov al,FD", "out dx,al", "jmp 0550"]   # w0(0xfd)

    l = (blk(0x100, m) + blk(0x300, cpp4) + blk(0x310, cppp) + blk(0x320, fr)
         + blk(0x3A0, nib) + blk(0x3D0, kick) + blk(0x3F0, wr)
         + blk(0x420, ["push cx", "mov cx,FFFF", "jmp 0430"])
         + blk(0x430, ["mov al,1F", "call 03A0", "test al,80", "jz 0450",
                       "dec cx", "jnz 0430"])
         + blk(0x450, ["pop cx", "ret"])
         + blk(0x460, ["push cx", "mov cx,FFFF", "jmp 0470"])
         + blk(0x470, ["mov al,1F", "call 03A0", "and al,88", "cmp al,08",
                       "jz 0490", "dec cx", "jnz 0470"])
         + blk(0x490, ["pop cx", "ret"])
         + blk(0x500, setup) + blk(0x530, head) + blk(0x550, body)
         + blk(0x580, join) + blk(0x5A0, leave) + blk(0x5C0, last))
    # Unassemble the block read too, so the capture proves what actually ran
    # rather than what the generator meant to emit.
    l += ["u 500 5CF", "g=100", "d 600 607", "d 700 723", "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    import sys
    text = build()
    if "--raw" in sys.argv:
        sys.stdout.write(text)
    else:
        print(base64.b64encode(text.encode("ascii")).decode())
