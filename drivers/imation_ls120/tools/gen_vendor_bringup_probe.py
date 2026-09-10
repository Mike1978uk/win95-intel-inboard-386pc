#!/usr/bin/env python3
"""Emit a base64 DEBUG script running SD120PPD.SYS's own cold bring-up.

Transcribed from the vendor DOS driver, not from epat.c - see
../TRANSPORT_SPEC.md section 4i.  epat.c's connect assumes the bridge is
already awake; this is the sequence that wakes it.

    ECR (base+0x402) &= 0x34
    control kick:  04 0C 0E 0E 0E 04 04        (vendor 0x2C6D)
    CPP(0x30) CPP(0x40) CPP(0x50) CPP(0x00)    (vendor 0x2CE6)
    CPP(0xE0)                                  (vendor 0x2C16, control preserved)
    read ATA status at task file 0x18+7
    CPP(0x40) CPP(0x30)                        (vendor 0x2C23, disconnect)

Result byte lands at [0200]: 0x50 = bridge up, 0x00 = still dead.

Techniques 109b/109c: CRLF lines, and DEBUG's own assembler computes every
call/jmp offset - hand-computed rel16 has hard-wedged this machine once.

Usage:  python gen_vendor_bringup_probe.py > probe.b64
        (on the box)  DEBUG < C:\VBRINGUP.SCR
"""
import base64

CRLF = "\r\n"
BASE, STAT, CTRL, ECR = 0x378, 0x379, 0x37A, 0x77A


def blk(addr, ins):
    return ["a %04X" % addr] + ins + [""]


# 0300  CPP4  - cmd in AH, control forced to 0x04, falls through to FRAME
cpp4 = ["mov dx,%04X" % CTRL, "mov al,04", "out dx,al", "jmp 0320"]

# 0310  CPPP  - cmd in AH, control PRESERVED (vendor does this for E0/20/D0)
cppp = ["mov dx,%04X" % CTRL, "in al,dx", "and al,0F", "out dx,al", "jmp 0320"]

# 0320  FRAME - the magic, the command byte, then the trailing strobe
frame = ["mov dx,%04X" % BASE]
for b in ["22", "AA", "55", "00", "FF", "87", "78"]:
    frame += ["mov al,%s" % b, "out dx,al", "out dx,al"]
frame += ["mov al,ah", "out dx,al", "out dx,al",
          "mov dx,%04X" % CTRL, "mov al,04", "out dx,al", "out dx,al",
          "in al,dx", "and al,10", "or al,05", "out dx,al", "out dx,al",
          "and al,FE", "out dx,al", "out dx,al",
          "mov dx,%04X" % BASE, "mov al,FF", "out dx,al", "out dx,al", "ret"]

# 03A0  nibble read - AL = register offset in, AL = byte out
nib = ["mov dx,%04X" % BASE, "out dx,al",
       "mov dx,%04X" % CTRL, "mov al,01", "out dx,al", "mov al,03", "out dx,al",
       "mov dx,%04X" % STAT, "in al,dx", "and al,F0",
       "shr al,1", "shr al,1", "shr al,1", "shr al,1", "mov bl,al",
       "mov dx,%04X" % CTRL, "mov al,04", "out dx,al",
       "mov dx,%04X" % STAT, "in al,dx", "and al,F0", "or al,bl", "ret"]

# 03D0  the kick
kick = ["mov dx,%04X" % CTRL,
        "mov al,04", "out dx,al",
        "mov al,0C", "out dx,al",
        "mov al,0E", "out dx,al", "out dx,al", "out dx,al",
        "mov al,04", "out dx,al", "out dx,al", "ret"]

main = ["cli",
        "mov dx,%04X" % ECR, "in al,dx", "and al,34", "out dx,al",
        "call 03D0",
        "mov ah,30", "call 0300",
        "mov ah,40", "call 0300",
        "mov ah,50", "call 0300",
        "mov ah,00", "call 0300",
        "mov ah,E0", "call 0310",
        "mov al,1F", "call 03A0", "mov [0200],al",
        "mov ah,40", "call 0300",
        "mov ah,30", "call 0300",
        "sti", "int 3"]

lines = (blk(0x100, main) + blk(0x300, cpp4) + blk(0x310, cppp)
         + blk(0x320, frame) + blk(0x3A0, nib) + blk(0x3D0, kick))
lines += ["g=100", "d 200 200", "q"]
print(base64.b64encode((CRLF.join(lines) + CRLF).encode("ascii")).decode())
