#!/usr/bin/env python3
"""Two ATAPI commands in one DOS run: REQUEST SENSE, then READ(10) of LBA 0.

RD10 showed a READ(10) refused with sense key 6, UNIT ATTENTION - the normal
first response after a media change. The miniport now reports CHECK CONDITION
and fetches sense, and Windows STILL says the media is not formatted, so the
open question is simply: does a read work at all once the condition is
cleared? REQUEST SENSE is the command that clears it, so issuing the two back
to back answers that with no Windows involved.

    REQUEST SENSE -> clears the contingent allegiance condition, and its own
                     reply names the sense key and ASC/ASCQ
    READ(10)      -> the actual test

Read it as:
    0600 status   0601 IR    after REQUEST SENSE's data phase
    0602 status   0603 error after it completed
    0610 status   0611 IR    after READ(10)'s data phase
    0612 status   0613 error after it completed
    0A00..0A11  the 18-byte sense reply. Byte 2 low nibble = sense key,
                byte 12 = ASC, byte 13 = ASCQ.
    1000..      the sector. A valid boot sector ends 55 AA at 11FE.

PASS = 0610/0611 read 08 / 02 (DRQ set, data to host) and the sector is not
all zeros. If the READ still shows 51 / 03 the drive is refusing it for a
reason the sense data will name.

Generalised from gen_inquiry_probe9.py, which proved this machinery: the
block routines now take SI and CX from the caller so a command can be issued
more than once per run.

    DEBUG < C:\\ATAPI.SCR > C:\\ATAPI.OUT
"""
import base64

CRLF = "\r\n"
B, S, C, E = 0x378, 0x379, 0x37A, 0x77A

CDB_SENSE = 0x0800      # REQUEST SENSE, 18 bytes
CDB_READ = 0x0820       # READ(10), LBA 0, one block
BUF_SENSE, LEN_SENSE = 0x0A00, 18
BUF_READ, LEN_READ = 0x1000, 512


def blk(a, i):
    return ["a %04X" % a] + i + [""]


def packet(cdb, buf, ln, slot):
    """One ATAPI packet command, data-in. Results land at slot..slot+3."""
    return [
        "mov ax,A01E", "call 03F0",                   # select device 0
        "call 0420",                                   # wait BSY clear
        "mov ax,%02X1C" % (ln & 0xFF), "call 03F0",   # byte count low
        "mov ax,%02X1D" % (ln >> 8), "call 03F0",     # byte count high
        "mov ax,0019", "call 03F0",                    # features = 0, PIO
        "mov ax,A01F", "call 03F0",                    # ATA command A0, PACKET
        "call 0460",                                   # DRQ: drive wants the CDB
        # pf_command: interrupt reason must read 1 (C/D set, I/O clear) or
        # this is a command phase error - sense 0Bh / ASC 4Ah.
        "mov al,1A", "call 03A0", "and al,03", "mov [%04X],al" % (slot + 4),
        "mov si,%04X" % cdb, "mov cx,000C", "call 0900",
        "call 0970",                                   # pf_atapi's mdelay(1)
        "call 0460",                                   # DRQ: data phase
        "mov al,1F", "call 03A0", "mov [%04X],al" % slot,
        "mov al,1A", "call 03A0", "mov [%04X],al" % (slot + 1),
        # n = ((bclo + 256*bchi) + 3) & 0xfffc - the count the DEVICE offers.
        "mov al,1C", "call 03A0", "mov bl,al",
        "mov al,1D", "call 03A0", "mov bh,al",
        "mov cx,bx", "add cx,0003", "and cx,FFFC",
        "mov [%04X],cx" % (slot + 6),
        "mov si,%04X" % buf, "call 0500",
        "mov al,1F", "call 03A0", "mov [%04X],al" % (slot + 2),
        "mov al,19", "call 03A0", "mov [%04X],al" % (slot + 3),
    ]


def build():
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
         # epat_connect's tail
         "mov dx,%04X" % B, "xor al,al", "out dx,al",
         "mov dx,%04X" % C, "mov al,01", "out dx,al", "mov al,04", "out dx,al",
         "mov ax,1008", "call 03F0", "mov ax,140C", "call 03F0",
         "mov ax,380A", "call 03F0", "mov ax,1012", "call 03F0",
         # ATA soft reset - the cold bring-up
         "mov ax,0416", "call 03F0", "mov ax,0016", "call 03F0", "call 0420"]
    m += packet(CDB_SENSE, BUF_SENSE, LEN_SENSE, 0x0600)
    m += packet(CDB_READ, BUF_READ, LEN_READ, 0x0610)
    m += ["mov ah,40", "call 0300", "mov ah,30", "call 0300", "int 3"]

    # Block read: SI = destination, CX = count (caller sets both).
    setup = ["cli",
             "mov dx,%04X" % B, "mov al,07", "out dx,al",
             "mov dx,%04X" % C, "mov al,01", "out dx,al",
             "mov al,03", "out dx,al",
             "mov dx,%04X" % B, "mov al,FF", "out dx,al",
             "xor bh,bh", "jmp 0530"]
    head = ["cmp cx,0001", "jnz 0550", "jmp 05C0"]
    body = ["mov dx,%04X" % C, "mov al,06", "add al,bh", "out dx,al",
            "mov dx,%04X" % S, "in al,dx", "mov ah,al",
            "test al,08", "jnz 0580",
            "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",
            "mov dx,%04X" % S, "in al,dx", "jmp 0580"]
    join = ["and al,F0", "shr ah,1", "shr ah,1", "shr ah,1", "shr ah,1",
            "or al,ah", "mov [si],al", "inc si",
            "xor bh,01", "dec cx", "jnz 0530", "jmp 05A0"]
    leave = ["mov dx,%04X" % B, "xor al,al", "out dx,al",
             "mov dx,%04X" % C, "mov al,04", "out dx,al", "sti", "ret"]
    last = ["mov dx,%04X" % B, "mov al,FD", "out dx,al", "jmp 0550"]

    # Block write: SI = source, CX = count (caller sets both).
    bw = ["cli",
          "mov dx,%04X" % B, "mov al,67", "out dx,al",
          "mov dx,%04X" % C, "mov al,01", "out dx,al",
          "mov al,05", "out dx,al", "xor bh,bh", "jmp 0930"]
    bwl = ["mov al,[si]", "inc si", "mov dx,%04X" % B, "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",
           "xor bh,01", "dec cx", "jnz 0930", "jmp 0950"]
    bwe = ["mov dx,%04X" % C, "mov al,07", "out dx,al",
           "mov al,04", "out dx,al", "sti", "ret"]

    l = (blk(0x100, m) + blk(0x300, cpp4) + blk(0x310, cppp) + blk(0x320, fr)
         + blk(0x3A0, nib) + blk(0x3D0, kick) + blk(0x3F0, wr)
         # Every bounded loop exits by an EXPLICIT jump on timeout. Without it
         # the loop runs off the end of its block into whatever bytes precede
         # the next one, and DEBUG leaves those as it found them.
         + blk(0x420, ["push cx", "mov cx,4000", "jmp 0430"])
         + blk(0x430, ["mov al,1F", "call 03A0", "test al,80", "jz 0450",
                       "dec cx", "jnz 0430", "jmp 0450"])
         + blk(0x450, ["pop cx", "ret"])
         + blk(0x460, ["push cx", "mov cx,4000", "jmp 0470"])
         + blk(0x470, ["mov al,1F", "call 03A0", "and al,88", "cmp al,08",
                       "jz 0490", "dec cx", "jnz 0470", "jmp 0490"])
         + blk(0x490, ["pop cx", "ret"])
         + blk(0x500, setup) + blk(0x530, head) + blk(0x550, body)
         + blk(0x580, join) + blk(0x5A0, leave) + blk(0x5C0, last)
         + blk(0x900, bw) + blk(0x930, bwl) + blk(0x950, bwe)
         + blk(0x970, ["push cx", "push dx", "mov cx,00B4", "mov dx,0080",
                       "jmp 0980"])
         + blk(0x980, ["in al,dx", "dec cx", "jnz 0980", "jmp 0990"])
         + blk(0x990, ["pop dx", "pop cx", "ret"])
         + ["e %04X 03 00 00 00 12 00 00 00 00 00 00 00" % CDB_SENSE, "",
            "e %04X 28 00 00 00 00 00 00 00 01 00 00 00" % CDB_READ, ""])
    l += ["g=100", "d 600 61F", "d 0A00 0A11",
          "d 1000 102F", "d 11F0 11FF", "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    print(base64.b64encode(build().encode("ascii")).decode())
