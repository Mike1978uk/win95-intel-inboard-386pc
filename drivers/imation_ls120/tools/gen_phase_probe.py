#!/usr/bin/env python3
"""Emit the DEBUG script that FOUND the LS-120 data-path bug (INQ4).

Same ATAPI packet command as gen_inquiry_probe.py, plus the two things that
made it decisive:

  * per-stage bail-outs and a step counter at [0602], so a failure says WHERE
  * it photographs the device's own phase registers after the CDB goes in:
    interrupt reason, byte count lo/hi, status, error

The byte count is the field that mattered. It read 0x0024 - the drive had 36
bytes of INQUIRY reply ready - while the data read returned 36 zeros. That
single value split "the device refused" from "we cannot collect it", after two
sessions of hypotheses that all assumed the former.

    DEBUG < C:\INQ4.SCR > C:\INQ4.OUT

  [0600] status  [0601] error  [0602] step reached  [0604] timeout flag
  [0605] last status polled
  [0610] interrupt reason  [0611] byte count lo  [0612] byte count hi
  [0613] status  [0614] error
  [0700..0723] the 36 data bytes

Two DEBUG rules encoded here, both learned by hard-wedging the machine:
  * conditional jumps are +/-127 bytes. Use jz <a few bytes> + near jmp, with
    every target in its own "a <addr>" block.
  * a bounded loop needs an explicit jmp on the TIMEOUT path; falling off the
    end of an "a" block runs into unassembled memory.
"""
import base64

CRLF = "\r\n"
B, S, C, E = 0x378, 0x379, 0x37A, 0x77A


def blk(a, i):
    return ["a %04X" % a] + i + [""]


def build():
    cpp4 = ["mov dx,%04X" % C, "mov al,04", "out dx,al", "jmp 0320"]
    cppp = ["mov dx,%04X" % C, "in al,dx", "and al,0F", "out dx,al", "jmp 0320"]
    fr = ["mov dx,%04X" % B]
    for b in ["22", "AA", "55", "00", "FF", "87", "78"]:
        fr += ["mov al,%s" % b, "out dx,al", "out dx,al"]
    fr += ["mov al,ah", "out dx,al", "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "out dx,al", "out dx,al",
           "in al,dx", "and al,10", "or al,05", "out dx,al", "out dx,al",
           "and al,FE", "out dx,al", "out dx,al",
           "mov dx,%04X" % B, "mov al,FF", "out dx,al", "out dx,al", "ret"]
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

    m = ["cli", "mov dx,%04X" % E, "in al,dx", "and al,34", "out dx,al", "call 03D0",
         "mov ah,30", "call 0300", "mov ah,40", "call 0300",
         "mov ah,50", "call 0300", "mov ah,00", "call 0300",
         "mov ah,E0", "call 0310",
         "mov ax,0416", "call 03F0", "mov ax,0016", "call 03F0", "call 0420",
         "mov ax,A01E", "call 03F0",
         "mov ax,241C", "call 03F0",
         "mov ax,001D", "call 03F0",
         "mov ax,0019", "call 03F0",
         "mov ax,A01F", "call 03F0",
         "call 0460"]
    for v in ["12", "00", "00", "00", "24", "00", "00", "00", "00", "00", "00", "00"]:
        m += ["mov ax,%s18" % v, "call 03F0"]
    m += ["call 0460", "call 0500",
          "mov al,1F", "call 03A0", "mov [0200],al",
          "mov al,19", "call 03A0", "mov [0201],al",
          "mov ah,40", "call 0300", "mov ah,30", "call 0300", "sti", "int 3"]

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
         + blk(0x500, ["mov si,0300", "mov cx,0024", "jmp 0510"])
         + blk(0x510, ["mov al,18", "call 03A0", "mov [si],al", "inc si",
                       "dec cx", "jnz 0510", "ret"]))
    l += ["g=100", "d 200 201", "d 300 323", "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    print(base64.b64encode(build().encode("ascii")).decode())
