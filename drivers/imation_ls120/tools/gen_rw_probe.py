#!/usr/bin/env python3
"""Read AND write the LS-120 from DOS, in one run, without risking the media.

Five ATAPI commands, each following Linux pf.c's pf_command / pf_completion
sequence exactly - the same sequence the miniport now uses, so a pass here is
evidence about the driver:

  1. INQUIRY       - the proven anchor. If this fails, nothing else means
                     anything and the fault is upstream of every other test.
  2. REQUEST SENSE - clears the contingent allegiance condition (UNIT
                     ATTENTION after reset/media change) and names the last
                     sense key / ASC / ASCQ.
  3. READ(10)      - LBA 0. The real read test.
  4. WRITE BUFFER  - 512 bytes of an incrementing pattern into the drive's
                     own buffer. **This does not touch the media.**
  5. READ BUFFER   - the same 512 bytes back.

Steps 4 and 5 are technique 109f: WRITE BUFFER / READ BUFFER exercise the
write data path end to end with nothing at stake, and the incrementing
pattern makes a byte-order fault unmissable (a swap reads 01 00 03 02). If
the drive does not implement them it answers ILLEGAL REQUEST, which is a
clean, harmless negative rather than a lost disk.

Result slots, four bytes plus a word each:
    +0 status after the data phase   +1 interrupt reason there
    +2 status at completion          +3 error register there
    +4 interrupt reason before the CDB (pf.c requires exactly 1)
    +6 the byte count the DEVICE offered, rounded up to a multiple of 4

    0600 INQUIRY   0610 REQUEST SENSE   0620 READ(10)
    0630 WRITE BUFFER                   0640 READ BUFFER

Buffers: 2000 INQUIRY, 2040 sense, 2200 sector, 2400 pattern, 2600 read-back.

Helper routines live at 1800+ so the main block has room for five commands;
an earlier version put them at 0300 and the main block grew past it.

    DEBUG < C:\\RW.SCR > C:\\RW.OUT
"""
import base64

CRLF = "\r\n"
B, S, C, E = 0x378, 0x379, 0x37A, 0x77A

# helpers, moved clear of the main block
CPP4, CPPP, FRAME, NIB, KICK, WR = 0x1800, 0x1810, 0x1820, 0x18A0, 0x18D0, 0x18F0
WBSY, WBSYL, WBSYE = 0x1920, 0x1930, 0x1950
WDRQ, WDRQL, WDRQE = 0x1960, 0x1970, 0x1990
BR, BRH, BRB, BRJ, BRE, BRL = 0x1A00, 0x1A30, 0x1A50, 0x1A80, 0x1AA0, 0x1AC0
BW, BWL, BWE = 0x1B00, 0x1B30, 0x1B50
DLY, DLYL, DLYE = 0x1B70, 0x1B80, 0x1B90
PAT, PATL = 0x1BA0, 0x1BC0

CDBS = 0x1C00   # five 12-byte CDBs, 0x20 apart
BUF_INQ, BUF_SENSE, BUF_SECTOR, BUF_PATTERN, BUF_BACK = (
    0x2000, 0x2040, 0x2200, 0x2400, 0x2600)


def blk(a, i):
    return ["a %04X" % a] + i + [""]


def packet(cdb, buf, ln, slot, out=False):
    """One ATAPI packet command, transliterated from pf_command/pf_completion."""
    return [
        "mov ax,A01E", "call %04X" % WR,               # select device 0
        "call %04X" % WBSY,                             # BSY and DRQ clear
        "mov ax,%02X1C" % (ln & 0xFF), "call %04X" % WR,
        "mov ax,%02X1D" % (ln >> 8), "call %04X" % WR,
        "mov ax,0019", "call %04X" % WR,                # features = 0
        "mov ax,A01F", "call %04X" % WR,                # ATA A0, PACKET
        "call %04X" % WDRQ,                             # DRQ: wants the CDB
        # pf_command: read_reg(2) must be 1 or it is a command phase error
        "mov al,1A", "call %04X" % NIB, "and al,03",
        "mov [%04X],al" % (slot + 4),
        "mov si,%04X" % cdb, "mov cx,000C", "call %04X" % BW,
        "call %04X" % DLY,                              # pf_atapi's mdelay(1)
        "call %04X" % WDRQ,
        "mov al,1F", "call %04X" % NIB, "mov [%04X],al" % slot,
        "mov al,1A", "call %04X" % NIB, "mov [%04X],al" % (slot + 1),
        # n = ((bclo + 256*bchi) + 3) & 0xfffc - what the DEVICE offers
        "mov al,1C", "call %04X" % NIB, "mov bl,al",
        "mov al,1D", "call %04X" % NIB, "mov bh,al",
        "mov cx,bx", "add cx,0003", "and cx,FFFC",
        "mov [%04X],cx" % (slot + 6),
        "mov si,%04X" % buf,
        "call %04X" % (BW if out else BR),
        "mov al,1F", "call %04X" % NIB, "mov [%04X],al" % (slot + 2),
        "mov al,19", "call %04X" % NIB, "mov [%04X],al" % (slot + 3),
    ]


def build():
    cpp4 = ["mov dx,%04X" % C, "mov al,04", "out dx,al", "jmp %04X" % FRAME]
    cppp = ["mov dx,%04X" % C, "in al,dx", "and al,0F", "out dx,al",
            "jmp %04X" % FRAME]
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

    m = ["mov dx,%04X" % E, "in al,dx", "and al,34", "out dx,al",
         "call %04X" % KICK,
         "mov ah,30", "call %04X" % CPP4, "mov ah,40", "call %04X" % CPP4,
         "mov ah,50", "call %04X" % CPP4, "mov ah,00", "call %04X" % CPP4,
         "mov ah,E0", "call %04X" % CPPP,
         "mov dx,%04X" % B, "xor al,al", "out dx,al",
         "mov dx,%04X" % C, "mov al,01", "out dx,al", "mov al,04", "out dx,al",
         "mov ax,1008", "call %04X" % WR, "mov ax,140C", "call %04X" % WR,
         "mov ax,380A", "call %04X" % WR, "mov ax,1012", "call %04X" % WR,
         "mov ax,0416", "call %04X" % WR, "mov ax,0016", "call %04X" % WR,
         "call %04X" % WBSY,
         "call %04X" % PAT]                             # build the pattern
    m += packet(CDBS + 0x00, BUF_INQ, 36, 0x0600)
    m += packet(CDBS + 0x20, BUF_SENSE, 18, 0x0610)
    m += packet(CDBS + 0x40, BUF_SECTOR, 512, 0x0620)
    m += packet(CDBS + 0x60, BUF_PATTERN, 512, 0x0630, out=True)
    m += packet(CDBS + 0x80, BUF_BACK, 512, 0x0640)
    m += ["mov ah,40", "call %04X" % CPP4, "mov ah,30", "call %04X" % CPP4,
          "int 3"]

    # Block read. SI = destination, CX = count. JCXZ guard: a device that
    # offers nothing gives cx=0, and without this the loop runs 65536 times
    # and walks over the probe's own code. That wedged the box once.
    setup = ["jcxz %04X" % BRE, "cli",
             "mov dx,%04X" % B, "mov al,07", "out dx,al",
             "mov dx,%04X" % C, "mov al,01", "out dx,al",
             "mov al,03", "out dx,al",
             "mov dx,%04X" % B, "mov al,FF", "out dx,al",
             "xor bh,bh", "jmp %04X" % BRH]
    head = ["cmp cx,0001", "jnz %04X" % BRB, "jmp %04X" % BRL]
    body = ["mov dx,%04X" % C, "mov al,06", "add al,bh", "out dx,al",
            "mov dx,%04X" % S, "in al,dx", "mov ah,al",
            "test al,08", "jnz %04X" % BRJ,
            "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",
            "mov dx,%04X" % S, "in al,dx", "jmp %04X" % BRJ]
    join = ["and al,F0", "shr ah,1", "shr ah,1", "shr ah,1", "shr ah,1",
            "or al,ah", "mov [si],al", "inc si",
            "xor bh,01", "dec cx", "jnz %04X" % BRH, "jmp %04X" % BRE]
    leave = ["mov dx,%04X" % B, "xor al,al", "out dx,al",
             "mov dx,%04X" % C, "mov al,04", "out dx,al", "sti", "ret"]
    lastb = ["mov dx,%04X" % B, "mov al,FD", "out dx,al", "jmp %04X" % BRB]

    bw = ["jcxz %04X" % BWE, "cli",
          "mov dx,%04X" % B, "mov al,67", "out dx,al",
          "mov dx,%04X" % C, "mov al,01", "out dx,al",
          "mov al,05", "out dx,al", "xor bh,bh", "jmp %04X" % BWL]
    bwl = ["mov al,[si]", "inc si", "mov dx,%04X" % B, "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",
           "xor bh,01", "dec cx", "jnz %04X" % BWL, "jmp %04X" % BWE]
    bwe = ["mov dx,%04X" % C, "mov al,07", "out dx,al",
           "mov al,04", "out dx,al", "sti", "ret"]

    l = (blk(0x100, m)
         + blk(CPP4, cpp4) + blk(CPPP, cppp) + blk(FRAME, fr)
         + blk(NIB, nib) + blk(KICK, kick) + blk(WR, wr)
         + blk(WBSY, ["push cx", "mov cx,4000", "jmp %04X" % WBSYL])
         + blk(WBSYL, ["mov al,1F", "call %04X" % NIB, "and al,88",
                       "jz %04X" % WBSYE, "dec cx", "jnz %04X" % WBSYL,
                       "jmp %04X" % WBSYE])
         + blk(WBSYE, ["pop cx", "ret"])
         + blk(WDRQ, ["push cx", "mov cx,4000", "jmp %04X" % WDRQL])
         + blk(WDRQL, ["mov al,1F", "call %04X" % NIB, "and al,89",
                       "cmp al,08", "jz %04X" % WDRQE,
                       "and al,01", "jnz %04X" % WDRQE,
                       "dec cx", "jnz %04X" % WDRQL, "jmp %04X" % WDRQE])
         + blk(WDRQE, ["pop cx", "ret"])
         + blk(BR, setup) + blk(BRH, head) + blk(BRB, body) + blk(BRJ, join)
         + blk(BRE, leave) + blk(BRL, lastb)
         + blk(BW, bw) + blk(BWL, bwl) + blk(BWE, bwe)
         + blk(DLY, ["push cx", "push dx", "mov cx,00B4", "mov dx,0080",
                     "jmp %04X" % DLYL])
         + blk(DLYL, ["in al,dx", "dec cx", "jnz %04X" % DLYL,
                      "jmp %04X" % DLYE])
         + blk(DLYE, ["pop dx", "pop cx", "ret"])
         + blk(PAT, ["mov di,%04X" % BUF_PATTERN, "mov cx,0200", "xor al,al",
                     "jmp %04X" % PATL])
         + blk(PATL, ["mov [di],al", "inc di", "inc al", "dec cx",
                      "jnz %04X" % PATL, "ret"]))

    cdbs = [
        "12 00 00 00 24 00 00 00 00 00 00 00",   # INQUIRY, 36
        "03 00 00 00 12 00 00 00 00 00 00 00",   # REQUEST SENSE, 18
        "28 00 00 00 00 00 00 00 01 00 00 00",   # READ(10) LBA 0, 1 block
        "3B 02 00 00 00 00 00 02 00 00 00 00",   # WRITE BUFFER, mode 2, 512
        "3C 02 00 00 00 00 00 02 00 00 00 00",   # READ BUFFER,  mode 2, 512
    ]
    for i, c in enumerate(cdbs):
        l += ["e %04X %s" % (CDBS + i * 0x20, c), ""]

    l += ["g=100",
          "d 600 647",
          "d %04X %04X" % (BUF_INQ, BUF_INQ + 0x23),
          "d %04X %04X" % (BUF_SENSE, BUF_SENSE + 0x11),
          "d %04X %04X" % (BUF_SECTOR, BUF_SECTOR + 0x1F),
          "d %04X %04X" % (BUF_SECTOR + 0x1F0, BUF_SECTOR + 0x1FF),
          "d %04X %04X" % (BUF_BACK, BUF_BACK + 0x1F),
          "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    print(base64.b64encode(build().encode("ascii")).decode())
