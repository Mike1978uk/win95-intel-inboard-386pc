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
import sys

CRLF = "\r\n"
B, S, C, E = 0x378, 0x379, 0x37A, 0x77A

# How many nibble reads a status wait is allowed before it gives up.
#
# 0xFFFF was the original, and it makes an unattended run impossible: a nibble
# read is ~6 port accesses at 5.77 us (technique 109e), so one expired wait is
# ~2.3 s, WDRQ6 is six of them, and a command that finds no drive costs ~20 s.
# Eight commands is then minutes of staring at a static screen with no way to
# tell a slow probe from a wedged one - which is exactly what happened on
# 2026-09-14.
#
# Pass a smaller value to make a FAILING run fail fast.
#
# *** DO NOT GO BELOW ABOUT 0x8000. *** The same constant bounds the wait that
# settles the ATA SOFT RESET, and a drive coming out of SRST needs seconds, not
# milliseconds. Measured 2026-09-14: --spin 2000 (~0.29 s) walks on while the
# drive is still in reset, and EVERY command after it comes back status C1
# with error 04 (ABRT) - deterministically, byte-identical across runs, which
# is what proves it is not a spin-up race. At 0xFFFF (~2.3 s) the identical
# script reads LBA 0 correctly on the first attempt.
#
# The lesson for the driver: a reset settle is a LONG wait and a per-command
# status poll is a SHORT one, and they must not share a budget. DESIGN.md I3.
SPIN = 0xFFFF

# Spin the drive up before anything else. An LS-120 that has parked answers
# INQUIRY perfectly well and refuses every media command, so a probe that
# skips this can look like a transport fault when it is a parked motor.
START_STOP = False

# helpers, moved clear of the main block
CPP4, CPPP, FRAME, NIB, KICK, WR = 0x1800, 0x1810, 0x1820, 0x18A0, 0x18D0, 0x18F0
WBSY, WBSYL, WBSYE = 0x1920, 0x1930, 0x1950
WDRQ, WDRQL, WDRQE, WDRQ6 = 0x1960, 0x1970, 0x1990, 0x19A0
BR, BR2, BRH, BRB, BRJ, BRE, BRL = (
    0x1A00, 0x1A10, 0x1A30, 0x1A50, 0x1A80, 0x1AA0, 0x1AC0)
# Trampoline for the JCXZ guard below. JCXZ is short-only (-128..+127) and BRE
# is +158 from it, so `jcxz BRE` does not assemble: DEBUG rejects the line,
# re-prompts at the same address, and the guard is silently ABSENT from the
# code. That is what wedged the box three times on 2026-09-14 - the guard the
# setup comment calls load-bearing was never actually there.
BRG = 0x1A2A          # the 6-byte gap after BR2, +40 from the jcxz
XFER_MAX = 0x200          # raised by --sectors; see MEDIA_SECTORS

# Every command is another chance for something unforeseen, and a runaway
# inside a cli window leaves the box with interrupts off - Ctrl-Alt-Del dead,
# hard reset only. So the write test is OFF until a read is proven. Set it
# true once READ(10) returns a valid boot sector.
WRITE_TEST = False

# Write a sector to the MEDIA and read it straight back. Unlike WRITE BUFFER
# this really does touch the disk, so it is off by default and the target is
# deliberately far from anything: LBA 100000 is ~51 MB into a 120 MB disk,
# well past the FATs, the root directory and every file written this session.
MEDIA_TEST = False
# Read the media WITHOUT writing first. The point is cache: a READ(10) issued
# straight after a WRITE(10) can be served from the drive's own buffer, so the
# pair proves the data round-tripped, not that it reached the platter. This
# mode runs after a fresh SRST with no write at all - if the pattern is still
# there, it is on the disk.
MEDIA_READ_ONLY = False
MEDIA_LBA = 100000

# Sectors per media transfer. 1 uses the proven 512-byte buffers; anything
# larger switches to the big buffers below and raises the block-transfer clamp.
#
# The point of sweeping this: NEITHER this probe NOR the miniport loops data
# BURSTS. Both read the count the device offers and do one transfer. If a
# multi-sector command is answered in several bursts, the first burst moves and
# the rest is stranded - and the command still reports success. That is a
# silent short transfer, which is the one failure mode worse than an error.
# The offered count lands at slot+6, so this measures it directly.
# Issue FORMAT UNIT. The drive formats its own surface internally, so this is
# ONE command with no data phase and no parameter list (FmtData=0 - "default
# parameters" - which means there is no defect-list header to get wrong on
# real media). It holds BSY for minutes, far past this probe's 2.3 s waits, so
# the probe fires it, times out waiting, and exits; the drive carries on. Run
# an ordinary read probe afterwards to see whether it finished.
#
# *** DESTRUCTIVE. This erases the disk. ***
FORMAT_UNIT = False
CAPACITY = False
BUFSIZE = False          # --bufsize: ask the drive how big its buffer is

# Skip the ATA soft reset. Polling a FORMAT UNIT needs this: SRST ABORTS an
# in-progress format, so a check run that resets destroys the thing it is
# measuring. 2026-09-14: the first format attempt was fired, then checked 90 s
# later by an ordinary probe - whose SRST would have killed it. LBA 0 came back
# untouched and the run proved nothing either way.
NO_RESET = False
MEDIA_SECTORS = 1
BUF_BIG_W, BUF_BIG_R = 0x4000, 0x8000   # 16 KB each, clear of everything
BW, BWC, BWL, BWE = 0x1B00, 0x1B20, 0x1B40, 0x1B60
DLY, DLYL, DLYE = 0x1B80, 0x1B90, 0x1BA0
PAT, PATL = 0x1BB0, 0x1BD0
# Poison the media read-back buffer before reading into it. Without this a
# read that moves NOTHING leaves the previous run's data in place - DEBUG
# reloads at the same segment - and a stale buffer is indistinguishable from a
# successful read. EE is chosen because it is not in the 00..FF ramp position
# it would occupy, so a partial transfer shows its own stopping point.
POIS, POISL = 0x1BE0, 0x1BF0
# Poison-all: BUF_INQ/SENSE/SENSE2/SECTOR (2000-23FF) and BUF_BACK/
# SECTOR2 (2600-29FF). BUF_PATTERN (2400) is deliberately skipped - it
# carries the write ramp and must not be overwritten.
POISA, POISAL, POISA2, POISAL2 = 0x1740, 0x1750, 0x1760, 0x1770
# SLOTS too. A slot that is never written otherwise holds whatever the
# PREVIOUS run left at the same address - POSTSRST.OUT showed a write
# result in +90 on a run that issued no write. An unwritten slot must read
# EE, not last time's answer.
POISA3, POISAL3 = 0x16A0, 0x16C0
# The vendor's ECP REGISTER read, SD120PPD.SYS 3CCEh: register number in AL,
# ONE byte out, every phase gated on the ECR. This is what 'ECP Read' in the
# vendor's mode table is. Every earlier ECP test here drove a BLOCK STREAM
# the vendor never performs, so none of them answered whether this bridge
# talks ECP at all. --ecpreg does NOT switch the rest of the probe to ECP:
# the media path stays on proven nibble so a hang is attributable.
ECPR, ECPRF = 0x1780, 0x17D0
# IEEE-1284 TERMINATE. Entering 1284 puts the PERIPHERAL into ECP too; the
# host dropping back to SPP does not bring it with us. Measured 2026-09-18:
# negotiate -> ECP read completes -> every later nibble read returns F5.
# Same probe without negotiation: ECP times out (FF) but nibble still reads
# EB. So negotiation is REQUIRED and termination is MISSING.
ETERM, ETERMA, ETERMB = 0x1600, 0x1620, 0x1640
ETERMC, ETERMD = 0x1660, 0x1680
ECPREG = False
ECPREG_REG = 0x1D        # cyl-high: holds EB of the 14 EB ATAPI signature
#            after SRST. A distinctive value - 00 matching 00 proves nothing.
ECPREG_TERM = False      # --ecpterm: terminate the 1284 phase on the way out
ECPREG_NEG = False       # --ecpneg: negotiate 1284 first (we do; the VENDOR
#            DOES NOT - 3CCEh goes straight to control=04, ECR=74h. Leaving
#            the bridge in 1284 ECP with the host back in SPP is the best
#            explanation for nibble reading F5 after every ECP episode.
PATB, PATBL, POISB, POISBL = 0x1700, 0x1710, 0x1720, 0x1730

# ECP block transfer, transliterated from the driver's LS_BlockReadEcp and
# LS_BlockWriteEcp so that a pass here is evidence about the driver and not
# about a second implementation (IMPLEMENTATION.md section 9).
#
# Registers stay NIBBLE throughout - only the DATA phase goes over ECP. A
# per-register ECP read times out by design: a register read has no data
# phase, so the reverse FIFO never fills (IMPLEMENTATION.md section 5).
#
# Unlike the driver this does NOT fall back to SPP on a refusal. The probe
# exists to find out whether ECP works, and a silent fallback would report
# SPP's result as ECP's.
ENEG, ENEGW, ENEGD = 0x1000, 0x1040, 0x1060
EDIR = 0x1080
EWE, EWEL = 0x10B0, 0x10C0
EWD, EWDL = 0x10E0, 0x10F0
ECMD, ECMDX = 0x1110, 0x1140
ELV = 0x1160
EBR, EBRL, EBRF = 0x1190, 0x11F0, 0x11D0   # EBRF within jcxz reach of EBR
# Gated bulk reader. EBRL already calls EWD for the last byte; the BULK read
# was a bare rep insb, which is the defect SD120PPD.SYS does not have and
# the reason ECP moves nothing on real hardware. 0x1220 is clear of EBRL.
EBRD = 0x1220
EBW, EBWC, EBWS, EBWT = 0x1250, 0x1290, 0x12C0, 0x1288   # EBWT ditto
ECP_MODE, ECP_BYTE = 0x74, 0x34
ECP_FIFO, ECP_ECR = 0x400, 0x402
USE_ECP = False

# EPP block transfer - epat.c mode 3, which is what the vendor's block path
# actually is (TRANSPORT_SPEC 4f-corrected). ECP on this bridge is a
# PER-REGISTER mode, not a block one, which is why the ECP block attempt moved
# nothing.
#
#   read : w3(80h) w2(24h) <count-1 bytes from base+4> w2(04h)
#          w3(A0h) w2(24h) <last byte> w2(04h)
#   write: w3(C0h) <count bytes to base+4> w2(04h)
#
# base+3 is the EPP ADDRESS register and base+4 the EPP DATA register. They
# only decode as EPP while the ECR is in EPP mode (100b), so it is switched
# for the transfer and restored after.
#
# The 1284 negotiation differs too: epat_connect sends extensibility byte 40h
# to request EPP where ECP uses 10h.
PNEG, PNEGW, PNEGD = 0x1300, 0x1340, 0x1360
PBR, PBRL, PBRF = 0x1380, 0x13D0, 0x1400
PBW, PBWF = 0x1420, 0x1460
EPP_ADDR, EPP_DATA = 3, 4
ECR_EPP = 0x80
USE_EPP = False

# The vendor's REAL ECP block write, SD120PPD.SYS 0x4CA3 (TRANSPORT_SPEC 11).
# The descriptor goes in FIRST through the bridge's 0Eh/0Fh/0Bh indirect
# register pair, and only then does the C0h command cycle happen. Issuing C0h
# cold - which is what the first attempt did - moves nothing.
VBW, VBWX = 0x1500, 0x1580   # jcxz reaches +126; block needs 123
VBWC, VBWS, VBWE = 0x15A0, 0x15D0, 0x15F0
USE_VECP = False

CDBS = 0x1C00   # five 12-byte CDBs, 0x20 apart
BUF_INQ, BUF_SENSE, BUF_SECTOR, BUF_PATTERN, BUF_BACK = (
    0x2000, 0x2040, 0x2200, 0x2400, 0x2600)
BUF_SENSE2, BUF_SECTOR2 = 0x2060, 0x2800


def blk(a, i):
    return ["a %04X" % a] + i + [""]


# Progress marker and result slots. These MUST sit clear of every code block.
#
# They used to be at 0580 and 0600-067F, which was fine when the main block
# held five commands and ended around 04B0. The main loop then grew to eight
# packets, the block grew past them, and the probe began writing its marks and
# its results INTO ITS OWN INSTRUCTION STREAM. It hung both times it was run on
# 2026-09-14, and cutting the status-wait budget eightfold changed nothing -
# which is what proved it was never the waits.
#
# The generator's layout check could not catch this: it only compares each
# block against the NEXT block's start, and these are data addresses, not
# blocks. check_layout() below now tests them explicitly.
SLOTS = 0x1E00            # clear of CDBS..CDBS+0x120 (nine CDBs)
# Clear of SLOTS' FULL range. MARK sat at 1EA0 and the FORMAT UNIT slot is
# SLOTS+0xA0 = 1EA0, so the marker overwrote the one result that run existed
# to collect. The guard below only knew about slots 00-9F.
MARK = 0x1F00             # one byte: how far the probe got


def mark(n):
    return ["mov byte ptr [%04X],%02X" % (MARK, n)]


def isize(ins):
    if ins.replace(" ", "").startswith("movbyteptr["):
        return 5
    """Encoded length of the instruction forms this probe emits.

    Used only by the layout check. Unknown forms return 4, which is an upper
    bound for everything here, so a mistake makes the check stricter rather
    than letting an overlap through.
    """
    t = ins.replace(" ", "")
    if t in ("ret",): return 1
    if t in ("cli", "sti"): return 1
    if t.startswith(("push", "pop")): return 1
    if t.startswith(("incsi", "incdi", "deccx")): return 1
    if t in ("outdx,al", "inal,dx"): return 1
    if t.startswith(("call", "jmp")): return 3
    if t[:2] in ("jz", "jn", "jb") or t.startswith("jcxz"): return 2
    if t.startswith(("cmpcx,", "addcx,", "andcx,")): return 4
    if t.startswith("mov[") and ",cx" in t: return 4
    if t.startswith("movcx,["): return 4   # 8B 0E xx xx - was counted as 3
    if t.startswith("mov["): return 3
    if t.startswith("xorbh,0"): return 3
    for r in ("movdx,", "movax,", "movcx,", "movsi,", "movdi,", "movbx,"):
        if t.startswith(r): return 3
    return 2


def packet(cdb, buf, ln, slot, out=False):
    """One ATAPI packet command, transliterated from pf_command/pf_completion."""
    return [
        "mov ax,A01E", "call %04X" % WR,               # select device 0
        "call %04X" % WBSY,                             # BSY and DRQ clear
        "mov ax,%02X1C" % (ln & 0xFF), "call %04X" % WR,
        "mov ax,%02X1D" % (ln >> 8), "call %04X" % WR,
        "mov ax,0019", "call %04X" % WR,                # features = 0
        "mov ax,A01F", "call %04X" % WR,                # ATA A0, PACKET
        "mov byte ptr [%04X],%02X" % (MARK, (slot & 0xF0) | 1),
        "call %04X" % WDRQ,                             # DRQ: wants the CDB
        # pf_command: read_reg(2) must be 1 or it is a command phase error
        "mov al,1A", "call %04X" % NIB, "and al,03",
        "mov [%04X],al" % (slot + 4),
        "mov si,%04X" % cdb, "mov cx,000C", "call %04X" % BW,
        "mov byte ptr [%04X],%02X" % (MARK, (slot & 0xF0) | 2),
        "call %04X" % DLY,                              # pf_atapi's mdelay(1)
        "call %04X" % WDRQ6,
        "mov byte ptr [%04X],%02X" % (MARK, (slot & 0xF0) | 3),
        "mov al,1F", "call %04X" % NIB, "mov [%04X],al" % slot,
        "mov al,1A", "call %04X" % NIB, "mov [%04X],al" % (slot + 1),
        # n = ((bclo + 256*bchi) + 3) & 0xfffc - what the DEVICE offers
        "mov al,1C", "call %04X" % NIB, "mov [%04X],al" % (slot + 6),
        "mov al,1D", "call %04X" % NIB, "mov [%04X],al" % (slot + 7),
        "mov cx,[%04X]" % (slot + 6), "add cx,0003", "and cx,FFFC",
        "mov si,%04X" % buf,
        "mov byte ptr [%04X],%02X" % (MARK, (slot & 0xF0) | 4),
        "call %04X" % ((VBW if USE_VECP else PBW if USE_EPP else EBW if USE_ECP else BW) if out
                       else (PBR if USE_EPP else EBR if USE_ECP else BR)),
        "mov byte ptr [%04X],%02X" % (MARK, (slot & 0xF0) | 5),
        "call %04X" % WBSY,                             # pf's "data done"
        "mov al,1F", "call %04X" % NIB, "mov [%04X],al" % (slot + 2),
        "mov al,19", "call %04X" % NIB, "mov [%04X],al" % (slot + 3),
    ]


def ecp_blocks():
    """The driver's ECP path, in DEBUG's 16-bit form."""
    F, R = B + ECP_FIFO, B + ECP_ECR
    neg = ["mov dx,%04X" % C, "mov al,0C", "out dx,al", "mov al,04", "out dx,al",
           "mov dx,%04X" % B, "xor al,al", "out dx,al", "out dx,al",
           "mov dx,%04X" % C, "mov al,01", "out dx,al", "out dx,al",
           "mov al,04", "out dx,al", "mov al,0C", "out dx,al",
           "mov dx,%04X" % B, "mov al,10", "out dx,al",
           "mov dx,%04X" % C, "mov al,06", "out dx,al", "out dx,al", "out dx,al",
           "mov dx,%04X" % S, "mov cx,0100", "jmp %04X" % ENEGW]
    negw = ["in al,dx", "test al,40", "jz %04X" % ENEGD,
            "dec cx", "jnz %04X" % ENEGW, "jmp %04X" % ENEGD]
    negd = ["mov [%04X],al" % (SLOTS + 0xA8),
            "mov dx,%04X" % C, "mov al,07", "out dx,al", "out dx,al",
            "mov al,04", "out dx,al", "out dx,al", "ret"]
    # AL = CTRL_FWD (04) or CTRL_REV (20)
    edir = ["mov bh,al", "mov dx,%04X" % R, "mov al,%02X" % ECP_BYTE, "out dx,al",
            "mov dx,%04X" % C, "in al,dx", "and al,10", "or al,bh", "out dx,al",
            "mov dx,%04X" % R, "mov al,%02X" % ECP_MODE, "out dx,al", "ret"]
    ewe = ["mov dx,%04X" % R, "mov cx,FFFF", "jmp %04X" % EWEL]
    ewel = ["in al,dx", "test al,01", "jnz %04X" % ECMDX,
            "dec cx", "jnz %04X" % EWEL, "stc", "ret"]
    ewd = ["mov dx,%04X" % R, "mov cx,8000", "jmp %04X" % EWDL]
    ewdl = ["in al,dx", "test al,01", "jz %04X" % ECMDX,
            "dec cx", "jnz %04X" % EWDL, "stc", "ret"]
    # BL = the ECP command byte
    ecmd = ["call %04X" % EWE, "jc %04X" % ECMDX,
            "mov dx,%04X" % B, "mov al,bl", "out dx,al",
            "call %04X" % EWE, "ret"]
    ecmdx = ["clc", "ret"]
    # EXACTLY the driver's LS_EcpLeave. Both stores are read-modify-write:
    # the control port keeps IRQEN, and the ECR keeps its low five bits while
    # the mode bits are cleared. An earlier transliteration here stored 34h,
    # a bare 04h and a guessed 15h instead - which left the port in a state
    # where nibble register reads returned F5 and the whole ECP run failed.
    # Technique 118: restore what you found, do not write what you assumed.
    elv = ["mov dx,%04X" % C, "in al,dx", "and al,10", "or al,04", "out dx,al",
           "mov dx,%04X" % R, "in al,dx", "and al,1F", "out dx,al", "ret"]
    # block read: DI = dest, CX = count
    ebr = ["jcxz %04X" % EBRF, "call %04X" % ENEG,
           "push cx", "mov al,04", "call %04X" % EDIR,
           "mov bl,80", "call %04X" % ECMD,
           "pop cx", "push cx", "dec cx", "jcxz %04X" % EBRL,
           "mov al,20", "call %04X" % EDIR, "cld",
           "call %04X" % EBRD, "jmp %04X" % EBRL]
    # EWD sets CX=8000 as its own budget, so the byte count is preserved.
    ebrd = ["push cx", "call %04X" % EWD, "pop cx",
            "mov dx,%04X" % F, "in al,dx", "mov [di],al", "inc di",
            "dec cx", "jnz %04X" % EBRD, "ret"]
    ebrl = ["mov al,04", "call %04X" % EDIR,
            "mov bl,A0", "call %04X" % ECMD,
            "mov al,20", "call %04X" % EDIR,
            "call %04X" % EWD,
            "mov dx,%04X" % F, "in al,dx", "mov [di],al", "inc di",
            "call %04X" % ELV, "pop cx", "clc", "ret"]
    ebrf = ["clc", "ret"]
    # block write: SI = source, CX = count
    ebw = ["jcxz %04X" % EBWT, "call %04X" % ENEG,
           "push cx", "mov al,04", "call %04X" % EDIR,
           "mov bl,C0", "call %04X" % ECMD,
           "pop cx", "push cx", "cld", "mov bx,cx", "jmp %04X" % EBWC]
    ebwc = ["or bx,bx", "jz %04X" % EBWS,
            "call %04X" % EWE,
            "mov cx,0010", "cmp bx,cx", "jae %04X" % (EBWC + 0x20),
            "mov cx,bx", "jmp %04X" % (EBWC + 0x20)]
    ebwc2 = ["sub bx,cx", "mov dx,%04X" % F, "rep outsb", "jmp %04X" % EBWC]
    ebws = ["call %04X" % EWE, "call %04X" % ELV,
            "pop cx", "clc", "ret"]
    ebwt = ["clc", "ret"]
    A, D2 = B + EPP_ADDR, B + EPP_DATA
    # same handshake as ENEG, extensibility byte 40h instead of 10h
    pneg = ["mov dx,%04X" % C, "mov al,0C", "out dx,al", "mov al,04", "out dx,al",
            "mov dx,%04X" % B, "xor al,al", "out dx,al", "out dx,al",
            "mov dx,%04X" % C, "mov al,01", "out dx,al", "out dx,al",
            "mov al,04", "out dx,al", "mov al,0C", "out dx,al",
            "mov dx,%04X" % B, "mov al,40", "out dx,al",
            "mov dx,%04X" % C, "mov al,06", "out dx,al", "out dx,al", "out dx,al",
            "mov dx,%04X" % S, "mov cx,0100", "jmp %04X" % PNEGW]
    pnegw = ["in al,dx", "test al,40", "jz %04X" % PNEGD,
             "dec cx", "jnz %04X" % PNEGW, "jmp %04X" % PNEGD]
    pnegd = ["mov [%04X],al" % (SLOTS + 0xA9),
             "mov dx,%04X" % C, "mov al,07", "out dx,al", "out dx,al",
             "mov al,04", "out dx,al", "out dx,al", "ret"]
    # DI = dest, CX = count
    pbr = ["jcxz %04X" % PBRF, "call %04X" % PNEG,
           "mov dx,%04X" % R, "mov al,%02X" % ECR_EPP, "out dx,al",
           "in al,dx", "mov [%04X],al" % (SLOTS + 0xAA),
           "push cx", "dec cx", "jcxz %04X" % PBRL,
           "mov dx,%04X" % A, "mov al,80", "out dx,al",
           "mov dx,%04X" % C, "mov al,24", "out dx,al",
           "cld", "mov dx,%04X" % D2, "rep insb",
           "mov dx,%04X" % C, "mov al,04", "out dx,al", "jmp %04X" % PBRL]
    pbrl = ["mov dx,%04X" % A, "mov al,A0", "out dx,al",
            "mov dx,%04X" % C, "mov al,24", "out dx,al",
            "mov dx,%04X" % D2, "in al,dx", "mov [di],al", "inc di",
            "mov dx,%04X" % C, "mov al,04", "out dx,al",
            "mov dx,%04X" % R, "in al,dx", "and al,1F", "out dx,al",
            "pop cx", "clc", "ret"]
    pbrf = ["clc", "ret"]
    # SI = source, CX = count
    pbw = ["jcxz %04X" % PBWF, "call %04X" % PNEG,
           "mov dx,%04X" % R, "mov al,%02X" % ECR_EPP, "out dx,al",
           "in al,dx", "mov [%04X],al" % (SLOTS + 0xAB),
           "mov dx,%04X" % A, "mov al,C0", "out dx,al",
           "cld", "mov dx,%04X" % D2, "rep outsb",
           "mov dx,%04X" % C, "mov al,04", "out dx,al",
           "mov dx,%04X" % R, "in al,dx", "and al,1F", "out dx,al",
           "clc", "ret"]
    pbwf = ["clc", "ret"]
    # one address cycle + one data byte, each ECR-gated, as the vendor does
    def ad(addr_val, data_ins):
        return (["mov dx,%04X" % B, "mov al,%02X" % addr_val, "out dx,al",
                 "call %04X" % EWE]
                + ["mov dx,%04X" % (B + ECP_FIFO)] + data_ins
                + ["out dx,al", "call %04X" % EWE])
    vbw = (["jcxz %04X" % VBWX, "call %04X" % ENEG,
            "mov dx,%04X" % R, "mov al,14", "out dx,al",
            "mov dx,%04X" % C, "mov al,04", "out dx,al",
            "mov dx,%04X" % R, "mov al,74", "out dx,al",
            "call %04X" % EWE, "push cx"]
           + ad(0x0E, ["mov al,0B"])
           + ad(0x0F, ["pop cx", "push cx", "mov al,ch"])
           + ad(0x0B, ["pop cx", "push cx", "mov al,cl"])
           + ["mov dx,%04X" % C, "mov al,04", "out dx,al",
              "mov dx,%04X" % R, "mov al,74", "out dx,al",
              "mov dx,%04X" % B, "mov al,C0", "out dx,al",
              "call %04X" % EWE,
              "pop cx", "cld", "mov bx,cx", "jmp %04X" % VBWC])
    # CHUNKED, waiting for FIFO-empty before each chunk - vendor 0x4DED/0x4E0C.
    # The ECP FIFO is 16 bytes deep; the first attempt pushed 3584 bytes at it
    # in one rep outsb, which is what wedged the port.
    vbwc = ["or bx,bx", "jz %04X" % VBWE,
            "call %04X" % EWE,
            "mov cx,0010", "cmp bx,cx", "jae %04X" % VBWS,
            "mov cx,bx", "jmp %04X" % VBWS]
    vbws = ["sub bx,cx", "mov dx,%04X" % (B + ECP_FIFO), "rep outsb",
            "jmp %04X" % VBWC]
    vbwe = ["call %04X" % EWE,
            "mov dx,%04X" % R, "mov al,34", "out dx,al",   # vendor's teardown
            "clc", "ret"]
    vbwx = ["clc", "ret"]
    return (blk(VBW, vbw) + blk(VBWX, vbwx)
            + blk(VBWC, vbwc) + blk(VBWS, vbws) + blk(VBWE, vbwe)
            + blk(PNEG, pneg) + blk(PNEGW, pnegw) + blk(PNEGD, pnegd)
            + blk(PBR, pbr) + blk(PBRL, pbrl) + blk(PBRF, pbrf)
            + blk(PBW, pbw) + blk(PBWF, pbwf)
            + blk(ENEG, neg) + blk(ENEGW, negw) + blk(ENEGD, negd)
            + blk(EDIR, edir)
         + blk(ECPR, ["mov ah,al"]
                     + (["call %04X" % ENEG, "jc %04X" % ECPRF] if ECPREG_NEG else [])
                     + [
                      "mov al,04", "call %04X" % EDIR,
                      "call %04X" % EWE, "jc %04X" % ECPRF,
                      "mov dx,%04X" % B, "mov al,ah", "out dx,al",
                      "call %04X" % EWE, "jc %04X" % ECPRF,
                      "mov al,20", "call %04X" % EDIR,
                      "call %04X" % EWD, "jc %04X" % ECPRF,
                      "mov dx,%04X" % (B + 0x400), "in al,dx", "mov bl,al",
                      "mov al,04", "call %04X" % EDIR,
                      "call %04X" % ELV]
                     + (["call %04X" % ETERM] if ECPREG_TERM else [])
                     + ["mov al,bl", "clc", "ret"])
         + blk(ECPRF, ["call %04X" % ELV, "mov al,FF", "stc", "ret"])
         + blk(ETERM, ["mov dx,%04X" % C, "mov al,0C", "out dx,al",
                       "mov dx,%04X" % S, "mov cx,FFFF", "jmp %04X" % ETERMA])
         + blk(ETERMA, ["in al,dx", "test al,40", "jz %04X" % ETERMB,
                        "dec cx", "jnz %04X" % ETERMA, "jmp %04X" % ETERMB])
         + blk(ETERMB, ["mov dx,%04X" % C, "mov al,0E", "out dx,al",
                        "mov dx,%04X" % S, "mov cx,FFFF", "jmp %04X" % ETERMC])
         + blk(ETERMC, ["in al,dx", "test al,40", "jnz %04X" % ETERMD,
                        "dec cx", "jnz %04X" % ETERMC, "jmp %04X" % ETERMD])
         + blk(ETERMD, ["mov dx,%04X" % C, "mov al,0C", "out dx,al",
                        "mov al,04", "out dx,al", "ret"])
            + blk(EWE, ewe) + blk(EWEL, ewel)
            + blk(EWD, ewd) + blk(EWDL, ewdl)
            + blk(ECMD, ecmd) + blk(ECMDX, ecmdx)
            + blk(ELV, elv)
            + blk(EBR, ebr) + blk(EBRL, ebrl) + blk(EBRF, ebrf)
            + blk(EBRD, ebrd)
            + blk(EBW, ebw) + blk(EBWC, ebwc) + blk(EBWC + 0x20, ebwc2)
            + blk(EBWS, ebws) + blk(EBWT, ebwt))


def build():
    cpp4 = ["mov dx,%04X" % C, "mov al,04", "out dx,al", "jmp %04X" % FRAME]
    cppp = ["mov dx,%04X" % C, "in al,dx", "and al,0F", "out dx,al",
            "jmp %04X" % FRAME]
    fr = ["mov dx,%04X" % B]
    for b in ["22", "AA", "55", "00", "FF", "87", "78"]:
        fr += ["mov al,%s" % b, "out dx,al", "out dx,al"]
    fr += ["mov al,ah", "out dx,al", "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "out dx,al", "out dx,al",
           "in al,dx", "and al,10", "or al,05", "out dx,al", "out dx,al",
           "and al,FE", "out dx,al", "out dx,al",
           "mov dx,%04X" % B, "mov al,FF", "out dx,al", "out dx,al",
           "ret"]
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
         ] + ([] if NO_RESET else
         ["mov ax,0416", "call %04X" % WR, "mov ax,0016", "call %04X" % WR]) + [
         "call %04X" % WBSY,
         "call %04X" % PAT]                             # build the pattern
    # POISON EVERY READ BUFFER FIRST, not just BUF_SECTOR2. On 2026-09-17 an
    # ECP run "matched" an SPP run across five regions and was read as a pass;
    # only BUF_SECTOR2 was poisoned, and it was the one region that showed the
    # truth - ECP had delivered nothing and the rest was the previous run's
    # data still sitting at the same addresses. A buffer that cannot show
    # "untouched" cannot fail, so it is not evidence.
    m = mark(0x01) + ["call %04X" % POISA] + m[:5] + mark(0x02) + m[5:] + mark(0x03)

    # Read ONE register three ways in one run: nibble, then the vendor's ECP
    # sequence, then nibble again - the last one says whether the ECP episode
    # left the port usable. ECR captured either side. Slots B0-B4 are already
    # inside the SLOTS..SLOTS+BF dump.
    if ECPREG:
        m += (mark(0xE0)
              + ["mov al,%02X" % ECPREG_REG, "call %04X" % NIB,
                 "mov [%04X],al" % (SLOTS + 0xB0)]
              + mark(0xE1)
              + ["mov dx,%04X" % E, "in al,dx",
                 "mov [%04X],al" % (SLOTS + 0xB2)]
              + mark(0xE2)
              + ["mov al,%02X" % ECPREG_REG, "call %04X" % ECPR,
                 "mov [%04X],al" % (SLOTS + 0xB1)]
              + mark(0xE3)
              + ["mov dx,%04X" % E, "in al,dx",
                 "mov [%04X],al" % (SLOTS + 0xB3)]
              + mark(0xE4)
              + ["mov al,%02X" % ECPREG_REG, "call %04X" % NIB,
                 "mov [%04X],al" % (SLOTS + 0xB4)]
              + mark(0xE5))
    # REQUEST SENSE consumes the unit-attention condition SRST just raised,
    # then two reads: the first may still meet a spinning drive, the second
    # should not. INQUIRY is dropped - it is proven and costs four more waits.
    # pf_atapi's actual shape: issue the command, and ON ERROR call
    # pf_req_sense before doing anything else. A CHECK CONDITION is a
    # contingent allegiance condition - it is cleared by READING THE SENSE,
    # not by the next command - so repeating a read without a REQUEST SENSE
    # in between returns the identical error for ever. RD8 showed exactly
    # that: two reads, both 51/64, byte for byte.
    # pf.c retries up to PF_MAX_RETRIES (5). Each read that fails on a unit
    # attention exits its wait immediately - the wait breaks on ERR - so the
    # extra attempts are nearly free, and only a read that actually starts
    # moving data spends the long timeout.
    if START_STOP:
        # No data phase: ln=0, and the device goes straight to completion.
        m += mark(0x10) + packet(CDBS + 0xA0, BUF_SECTOR, 0, SLOTS + 0x70)
    for i in range(4):
        m += mark(0x20 + i * 4) + packet(
            CDBS + 0x20, BUF_SENSE + i * 0x20, 18, SLOTS + 0x50 + i * 8)
        m += mark(0x22 + i * 4) + packet(
            CDBS + 0x40, BUF_SECTOR, 512, SLOTS + 0x10 + i * 8)
    if WRITE_TEST:
        m += packet(CDBS + 0x60, BUF_PATTERN, 512, SLOTS + 0x30, out=True)
        m += packet(CDBS + 0x80, BUF_BACK, 512, SLOTS + 0x40)
    nbytes = MEDIA_SECTORS * 512
    wbuf = BUF_BIG_W if MEDIA_SECTORS > 1 else BUF_PATTERN
    rbuf = BUF_BIG_R if MEDIA_SECTORS > 1 else BUF_SECTOR2
    if MEDIA_TEST:
        # The real thing: the pattern onto the disk, then back off it into a
        # DIFFERENT buffer so a stale read cannot look like a successful write.
        if MEDIA_SECTORS > 1:
            m += ["call %04X" % PATB, "call %04X" % POISB]
        else:
            m += mark(0x97) + ["call %04X" % POIS]
        m += mark(0x90) + packet(CDBS + 0xC0, wbuf, nbytes,
                                 SLOTS + 0x90, out=True)
        m += mark(0x98) + packet(CDBS + 0xE0, rbuf, nbytes, SLOTS + 0x98)
    if CAPACITY:
        # Where does the disk end? Every media test so far has touched LBA 0
        # and LBA 100000 - two points on a ~234,000 sector disk. Nothing has
        # shown the far end is reachable at all.
        #
        # POISON FIRST. An earlier media read leaves a 00,01,02.. ramp in this
        # buffer, and the first eight bytes of that ramp are indistinguishable
        # from a capacity reply. Third time in one session that an unpoisoned
        # buffer made a failure look like a result.
        m += mark(0xB0) + ["call %04X" % POIS]
        m += packet(CDBS + 0x120, BUF_SENSE2, 8, SLOTS + 0xB0)

    # READ BUFFER mode 3: the DRIVE's own buffer capacity, straight from the
    # drive. Distinct from the 3584-byte BRIDGE burst ceiling - that is what
    # the EPAT will carry in one go, this is what the LS-120 will absorb
    # before it must commit to the platter. The two together decide whether
    # throughput or bus-occupancy is the lever worth pulling first.
    #   byte 0   offset boundary (log2 alignment)
    #   byte 1-3 BUFFER CAPACITY, big-endian
    if BUFSIZE:
        m += mark(0xC0) + packet(CDBS + 0x140, BUF_INQ, 4, SLOTS + 0xB8)
    elif FORMAT_UNIT:
        # No data phase. Fire and leave - the drive keeps formatting after
        # DEBUG exits, so a later read probe is what reports the outcome.
        m += mark(0xA0) + packet(CDBS + 0x100, BUF_SECTOR, 0, SLOTS + 0xA0)
    elif MEDIA_READ_ONLY:
        m += mark(0x97) + ["call %04X" % (POISB if MEDIA_SECTORS > 1 else POIS)]
        m += mark(0x98) + packet(CDBS + 0xE0, rbuf, nbytes, SLOTS + 0x98)
    m += ["mov ah,40", "call %04X" % CPP4, "mov ah,30", "call %04X" % CPP4,
          "int 3"]

    # Block read. SI = destination, CX = count. JCXZ guard: a device that
    # offers nothing gives cx=0, and without this the loop runs 65536 times
    # and walks over the probe's own code. That wedged the box once.
    setup = ["jcxz %04X" % BRG,
             "cmp cx,%04X" % XFER_MAX, "jbe %04X" % BR2,
             "mov cx,%04X" % XFER_MAX, "jmp %04X" % BR2]
    setup2 = [
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
             "mov dx,%04X" % C, "mov al,04", "out dx,al", "ret"]
    lastb = ["mov dx,%04X" % B, "mov al,FD", "out dx,al", "jmp %04X" % BRB]

    bw = ["jcxz %04X" % BWE,
          "cmp cx,%04X" % XFER_MAX, "jbe %04X" % BWC,
          "mov cx,%04X" % XFER_MAX, "jmp %04X" % BWC]
    bwc = [
          "mov dx,%04X" % B, "mov al,67", "out dx,al",
          "mov dx,%04X" % C, "mov al,01", "out dx,al",
          "mov al,05", "out dx,al", "xor bh,bh", "jmp %04X" % BWL]
    bwl = ["mov al,[si]", "inc si", "mov dx,%04X" % B, "out dx,al",
           "mov dx,%04X" % C, "mov al,04", "add al,bh", "out dx,al",
           "xor bh,01", "dec cx", "jnz %04X" % BWL, "jmp %04X" % BWE]
    bwe = ["mov dx,%04X" % C, "mov al,07", "out dx,al",
           "mov al,04", "out dx,al", "ret"]

    l = (blk(0x100, m)
         + blk(CPP4, cpp4) + blk(CPPP, cppp) + blk(FRAME, fr)
         + blk(NIB, nib) + blk(KICK, kick) + blk(WR, wr)
         + blk(WBSY, ["push cx", "mov cx,%04X" % SPIN, "jmp %04X" % WBSYL])
         + blk(WBSYL, ["mov al,1F", "call %04X" % NIB, "and al,88",
                       "jz %04X" % WBSYE, "dec cx", "jnz %04X" % WBSYL,
                       "jmp %04X" % WBSYE])
         + blk(WBSYE, ["pop cx", "ret"])
         + blk(WDRQ, ["push cx", "mov cx,%04X" % SPIN, "jmp %04X" % WDRQL])
         + blk(WDRQL, ["mov al,1F", "call %04X" % NIB, "and al,89",
                       "cmp al,08", "jz %04X" % WDRQE,
                       "and al,01", "jnz %04X" % WDRQE,
                       "dec cx", "jnz %04X" % WDRQL, "jmp %04X" % WDRQE])
         + blk(WDRQE, ["pop cx", "ret"])
         + blk(WDRQ6, ["call %04X" % WDRQ] * 6 + ["ret"])
         + blk(BR, setup) + blk(BR2, setup2) + blk(BRG, ["jmp %04X" % BRE])
         + blk(BRH, head) + blk(BRB, body) + blk(BRJ, join)
         + blk(BRE, leave) + blk(BRL, lastb)
         + blk(BW, bw) + blk(BWC, bwc) + blk(BWL, bwl) + blk(BWE, bwe)
         + blk(DLY, ["push cx", "push dx", "mov cx,00B4", "mov dx,0080",
                     "jmp %04X" % DLYL])
         + blk(DLYL, ["in al,dx", "dec cx", "jnz %04X" % DLYL,
                      "jmp %04X" % DLYE])
         + blk(DLYE, ["pop dx", "pop cx", "ret"])
         + blk(PAT, ["mov di,%04X" % BUF_PATTERN, "mov cx,0200", "xor al,al",
                     "jmp %04X" % PATL])
         + blk(PATL, ["mov [di],al", "inc di", "inc al", "dec cx",
                      "jnz %04X" % PATL, "ret"])
         + blk(POIS, ["mov di,%04X" % BUF_SECTOR2, "mov cx,0200",
                      "mov al,EE", "jmp %04X" % POISL])
         + blk(POISL, ["mov [di],al", "inc di", "dec cx",
                       "jnz %04X" % POISL, "ret"])
         + blk(POISA, ["mov di,2000", "mov cx,0400", "mov al,EE",
                       "jmp %04X" % POISAL])
         + blk(POISAL, ["mov [di],al", "inc di", "dec cx",
                        "jnz %04X" % POISAL, "jmp %04X" % POISA2])
         + blk(POISA2, ["mov di,2600", "mov cx,0400", "mov al,EE",
                        "jmp %04X" % POISAL2])
         + blk(POISAL2, ["mov [di],al", "inc di", "dec cx",
                         "jnz %04X" % POISAL2, "jmp %04X" % POISA3])
         + blk(POISA3, ["mov di,%04X" % SLOTS, "mov cx,00C0",
                        "mov al,EE", "jmp %04X" % POISAL3])
         + blk(POISAL3, ["mov [di],al", "inc di", "dec cx",
                         "jnz %04X" % POISAL3, "ret"])
         + blk(PATB, ["mov di,%04X" % BUF_BIG_W,
                      "mov cx,%04X" % (MEDIA_SECTORS * 512),
                      "xor al,al", "jmp %04X" % PATBL])
         + blk(PATBL, ["mov [di],al", "inc di", "inc al", "dec cx",
                       "jnz %04X" % PATBL, "ret"])
         + blk(POISB, ["mov di,%04X" % BUF_BIG_R,
                       "mov cx,%04X" % (MEDIA_SECTORS * 512),
                       "mov al,EE", "jmp %04X" % POISBL])
         + blk(POISBL, ["mov [di],al", "inc di", "dec cx",
                        "jnz %04X" % POISBL, "ret"])
         + ecp_blocks())

    cdbs = [
        "12 00 00 00 24 00 00 00 00 00 00 00",   # INQUIRY, 36
        "03 00 00 00 12 00 00 00 00 00 00 00",   # REQUEST SENSE, 18
        "28 00 00 00 00 00 00 00 01 00 00 00",   # READ(10) LBA 0, 1 block
        "3B 02 00 00 00 00 00 02 00 00 00 00",   # WRITE BUFFER, mode 2, 512
        "3C 02 00 00 00 00 00 02 00 00 00 00",   # READ BUFFER,  mode 2, 512
        "1B 00 00 00 01 00 00 00 00 00 00 00",   # START STOP UNIT, start=1
        "2A 00 %02X %02X %02X %02X 00 %02X %02X 00 00 00"  # WRITE(10)
        % ((MEDIA_LBA >> 24) & 0xFF, (MEDIA_LBA >> 16) & 0xFF,
           (MEDIA_LBA >> 8) & 0xFF, MEDIA_LBA & 0xFF,
           (MEDIA_SECTORS >> 8) & 0xFF, MEDIA_SECTORS & 0xFF),
        "28 00 %02X %02X %02X %02X 00 %02X %02X 00 00 00"  # READ(10)
        % ((MEDIA_LBA >> 24) & 0xFF, (MEDIA_LBA >> 16) & 0xFF,
           (MEDIA_LBA >> 8) & 0xFF, MEDIA_LBA & 0xFF,
           (MEDIA_SECTORS >> 8) & 0xFF, MEDIA_SECTORS & 0xFF),
        "04 00 00 00 00 00 00 00 00 00 00 00",   # FORMAT UNIT, default params
        "25 00 00 00 00 00 00 00 00 00 00 00",   # READ CAPACITY, 8 bytes
        "3C 03 00 00 00 00 00 00 04 00 00 00",   # READ BUFFER mode 3, descriptor
    ]
    for i, c in enumerate(cdbs):
        l += ["e %04X %s" % (CDBS + i * 0x20, c), ""]

    # Layout check - refuse to emit a script whose blocks overlap.
    # Sorted by ADDRESS, not by position in the list: blocks are appended in
    # whatever order reads well, so list order says nothing about layout.
    starts = sorted((int(x[2:], 16), n)
                    for n, x in enumerate(l) if x.startswith("a "))
    for i, (addr, n) in enumerate(starts):
        count = 0
        for x in l[n + 1:]:
            if not x or x.startswith("a ") or x.startswith("e "):
                break
            count += 1
        limit = starts[i + 1][0] if i + 1 < len(starts) else CDBS
        need = sum(isize(x) for x in l[n + 1:n + 1 + count])
        if addr + need > limit:
            raise SystemExit(
                "block %04X: %d instructions need up to %d bytes, "
                "next block starts at %04X" % (addr, count, need, limit))
        # Data must not land inside code. The check above only compares a block
        # against the NEXT block, so it cannot see a marker or a result slot
        # sitting in the middle of one - which is exactly how MARK at 0580 and
        # the slots at 0600 ended up inside a main block that had grown to 0717.
        # An out-of-range short jump is not a build error here: DEBUG prints
        # its own message, re-prompts at the same address and carries on
        # WITHOUT the instruction, so the jump silently deletes itself.
        pc = addr
        for x in l[n + 1:n + 1 + count]:
            nxt = pc + isize(x)
            t = x.replace(" ", "")
            if t[:2] in ("jz", "jn", "jb") or t.startswith("jcxz"):
                disp = int(x.split()[-1], 16) - nxt
                if not -128 <= disp <= 127:
                    raise SystemExit(
                        "%04X: %s is %+d away - short jumps reach -128..+127, "
                        "and DEBUG DROPS the instruction rather than failing"
                        % (pc, x, disp))
            pc = nxt
        if SLOTS <= MARK <= SLOTS + 0xBF:
            raise SystemExit(
                "MARK (%04X) is inside SLOTS (%04X-%04X) - it will overwrite "
                "a result slot" % (MARK, SLOTS, SLOTS + 0xAF))
        cdb_end = CDBS + len(cdbs) * 0x20 - 1
        for name, lo, hi in (("MARK", MARK, MARK),
                             ("SLOTS", SLOTS, SLOTS + 0xBF)):
            if lo <= cdb_end and hi >= CDBS:
                raise SystemExit(
                    "%s (%04X-%04X) overlaps the CDB table at %04X-%04X"
                    % (name, lo, hi, CDBS, cdb_end))
        for name, lo, hi in (("MARK", MARK, MARK),
                             ("SLOTS", SLOTS, SLOTS + 0x7F)):
            if lo < addr + need and hi >= addr:
                raise SystemExit(
                    "%s (%04X-%04X) lands inside the code block at %04X-%04X"
                    % (name, lo, hi, addr, addr + need - 1))

    l += ["g=100",
          "d %04X %04X" % (MARK, MARK),
          "d %04X %04X" % (SLOTS, SLOTS + 0xBF),
          "d %04X %04X" % (BUF_SENSE2, BUF_SENSE2 + 0x0F),
          "d %04X %04X" % (BUF_INQ, BUF_INQ + 0x23),
          "d %04X %04X" % (BUF_SENSE, BUF_SENSE + 0x11),
          "d %04X %04X" % (BUF_SENSE + 0x20, BUF_SENSE + 0x31),
          "d %04X %04X" % (BUF_SENSE + 0x40, BUF_SENSE + 0x51),
          "d %04X %04X" % (BUF_SENSE + 0x60, BUF_SENSE + 0x71),
          "d %04X %04X" % (BUF_SECTOR, BUF_SECTOR + 0x1F),
          "d %04X %04X" % (BUF_SECTOR + 0x1F0, BUF_SECTOR + 0x1FF),
          "d %04X %04X" % (BUF_BACK, BUF_BACK + 0x1F),
          # media read-back: head and tail, so a partial write shows up
          "d %04X %04X" % (rbuf, rbuf + 0x1F),
          "d %04X %04X" % (rbuf + nbytes - 0x20, rbuf + nbytes - 1),
          "q"]
    return CRLF.join(l) + CRLF


if __name__ == "__main__":
    if "--spin" in sys.argv:
        SPIN = int(sys.argv[sys.argv.index("--spin") + 1], 16)
    if "--startstop" in sys.argv:
        START_STOP = True
    if "--write" in sys.argv:
        WRITE_TEST = True
    if "--media" in sys.argv:
        MEDIA_TEST = True
    if "--mediaread" in sys.argv:
        MEDIA_READ_ONLY = True
    if "--noreset" in sys.argv:
        NO_RESET = True
    if "--ecpreg" in sys.argv:
        ECPREG = True
    if "--ecpneg" in sys.argv:
        ECPREG_NEG = True
    if "--ecpterm" in sys.argv:
        ECPREG_TERM = True
    if "--ecp" in sys.argv:
        USE_ECP = True
    if "--epp" in sys.argv:
        USE_EPP = True
    if "--vecp" in sys.argv:
        USE_VECP = True
    if "--lba" in sys.argv:
        MEDIA_LBA = int(sys.argv[sys.argv.index("--lba") + 1])
    if "--capacity" in sys.argv:
        CAPACITY = True
    if "--bufsize" in sys.argv:
        BUFSIZE = True
        CAPACITY = True
    if "--formatunit" in sys.argv:
        FORMAT_UNIT = True
    if "--sectors" in sys.argv:
        MEDIA_SECTORS = int(sys.argv[sys.argv.index("--sectors") + 1])
        if MEDIA_SECTORS > 1:
            XFER_MAX = MEDIA_SECTORS * 512
    print(base64.b64encode(build().encode("ascii")).decode())
