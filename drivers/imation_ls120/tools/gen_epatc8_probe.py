#!/usr/bin/env python3
"""Emit a base64 DEBUG script that runs epat.c's epatc8 (Shuttle EP1284) connect.

Background: docs live in ../TRANSPORT_SPEC.md section 4h.  epat_connect() has two
branches selected by the build-time CONFIG_PARIDE_EPATC8.  Every hardware test to
date has run the !epatc8 branch; this is the other one.

Usage:
    python gen_epatc8_probe.py > probe.b64
    # then, on the DOS box (COMrade), decode to EPATC8.SCR and:  DEBUG < EPATC8.SCR
    # result byte lands at [0200]:  0x50 = bridge up, 0x00 = still dead.

Two rules this file exists to encode (inboard-hw-debug techniques 109b/109c):
  * lines are CRLF - DOS DEBUG reads an LF-only file as one line and truncates.
  * the script uses DEBUG's own assembler, so DEBUG computes every call offset.
    Hand-computed rel16 offsets have already hard-wedged the machine once.

Port 0x378 data / 0x379 status / 0x37A control.
"""
import base64

CRLF = "\r\n"


def blk(addr, ins):
    return ["a %s" % addr] + ins + [""]


# --- CPP(mode) at 0300: magic frame, then pulse control 04->05->04 -------------
cpp = ["mov ah,al", "mov dx,037A", "mov al,04", "out dx,al", "mov dx,0378"]
for b in ["22", "AA", "55", "00", "FF", "87", "78"]:
    cpp += ["mov al,%s" % b, "call 350"]
cpp += ["mov al,ah", "call 350",
        "mov dx,037A", "mov al,04", "out dx,al",
        "mov al,05", "out dx,al",
        "mov al,04", "out dx,al",
        "mov dx,0378", "mov al,FF", "call 350", "ret"]

# --- w0 twice at 0350 (each magic byte is written two times) -------------------
w0x2 = ["out dx,al", "out dx,al", "ret"]

# --- nibble read at 0360: AL = register offset, returns AL = byte -------------
nib = ["mov dx,0378", "out dx,al",
       "mov dx,037A", "mov al,01", "out dx,al", "mov al,03", "out dx,al",
       "mov dx,0379", "in al,dx", "and al,F0",
       "shr al,1", "shr al,1", "shr al,1", "shr al,1", "mov bl,al",
       "mov dx,037A", "mov al,04", "out dx,al",
       "mov dx,0379", "in al,dx", "and al,F0", "or al,bl", "ret"]

# --- register write at 03A0: AL = reg (tagged 0x60), AH = value ---------------
wr = ["mov bl,ah", "or al,60", "mov dx,0378", "out dx,al",
      "mov dx,037A", "mov al,01", "out dx,al",
      "mov dx,0378", "mov al,bl", "out dx,al",
      "mov dx,037A", "mov al,04", "out dx,al", "ret"]

# --- w0(0) w2(1) w2(4) at 03C0 ------------------------------------------------
idle = ["mov dx,0378", "mov al,00", "out dx,al",
        "mov dx,037A", "mov al,01", "out dx,al", "mov al,04", "out dx,al", "ret"]

main = ["cli",
        "mov al,00", "call 300",        # CPP(0)
        "mov al,40", "call 300",        # CPP(0x40)   <- epatc8 only, never tried
        "mov al,E0", "call 300",        # CPP(0xE0)
        "call 3C0",
        "mov ax,1208", "call 3A0",      # WR(0x8, 0x12)
        "mov ax,140C", "call 3A0",      # WR(0xc, 0x14)
        "mov ax,1012", "call 3A0",      # WR(0x12,0x10)
        "mov ax,0F0E", "call 3A0",      # WR(0xe, 0x0f)
        "mov ax,040F", "call 3A0",      # WR(0xf, 0x04)
        "mov ax,0D0E", "call 3A0",      # WR(0xe, 0x0d)
        "mov ax,000F", "call 3A0",      # WR(0xf, 0x00)
        "mov al,E0", "call 300",        # CPP(0xE0)  - the shared tail
        "call 3C0",
        "mov al,1F", "call 360",        # task file 0x18 + 7 = ATA status
        "mov [0200],al",
        "mov al,30", "call 300",        # CPP(0x30) - disconnect
        "sti", "int 3"]

lines = (blk("100", main) + blk("300", cpp) + blk("350", w0x2)
         + blk("360", nib) + blk("3A0", wr) + blk("3C0", idle))
lines += ["g=100", "d 200 200", "q"]

print(base64.b64encode((CRLF.join(lines) + CRLF).encode("ascii")).decode())
