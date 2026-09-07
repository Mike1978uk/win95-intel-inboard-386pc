#!/usr/bin/env python3
r"""Phase 2a: add the sector-read path to XTIDETR.ASM.

Every insertion asserts its anchor, so a moved anchor fails the run instead of
producing a file that looks patched and is not (Technique 28).

  python drivers/xtide_pdr/tools/phase2_readpath.py [--check]
"""
import sys, os

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'XTIDETR.ASM')
TAB = chr(9)


def T(s):
    """Write assembly with visible tab markers; the DDK MASM wants real tabs."""
    return s.replace('|', TAB)


CONSTS = T("""ATA_IDENTIFY|equ|0ECh
ATA_READ|equ|020h||; READ SECTOR(S), the 28-bit PIO command

;  IDENTIFY DEVICE word offsets, in bytes. Word N is at 2N.
ID_CYLS||equ|2||; word 1  - logical cylinders
ID_HEADS|equ|6||; word 3  - logical heads
ID_SPT||equ|12||; word 6  - logical sectors per track
ID_CAPS||equ|98||; word 49 - bit 9 set = LBA supported
ID_LBA28|equ|120||; words 60-61 - total addressable sectors

DRVHD_LBA|equ|0E0h||; LBA mode | master. CHS mode is 0A0h.
""")

DATA = T("""
;----------------------------------------------------------------------------
;  Phase 2 - geometry taken from IDENTIFY, and the sector-read request block.
;
;  These are module variables rather than registers on purpose. Phase 1 failed
;  on real hardware because a helper documented as "Uses: EDX" left EDX on the
;  alternate-status port and the caller then read Drive/Head through it
;  (technique 78). A read path has four more addressing values to lose that
;  way, so none of them live in a register across a call.
;----------------------------------------------------------------------------
public XTIDE_Cyls
XTIDE_Cyls|||dd|0
public XTIDE_Heads
XTIDE_Heads|||dd|0
public XTIDE_Spt
XTIDE_Spt|||dd|0
public XTIDE_TotalSec
XTIDE_TotalSec||dd|0||; LBA28 capacity, 0 if the drive is CHS-only
public XTIDE_LbaOk
XTIDE_LbaOk|||dd|0||; 1 = address in LBA, 0 = fall back to CHS

;  Request block for XTIDE_ReadSectors.
public XTIDE_ReqLba
XTIDE_ReqLba|||dd|0
public XTIDE_ReqCount
XTIDE_ReqCount||dd|0
public XTIDE_ReqBuf
XTIDE_ReqBuf|||dd|0

;  The four taskfile values, filled by either addressing mode and written out
;  by one common block, so LBA and CHS cannot diverge in how they program the
;  drive - only in how they compute these.
XTIDE_oSecNum|||dd|0
XTIDE_oCylLo|||dd|0
XTIDE_oCylHi|||dd|0
XTIDE_oDrvHd|||dd|0

;  One sector, for the phase 2a self-test read of LBA 0.
public XTIDE_SecBuf
XTIDE_SecBuf|||db|512 dup (0)
public XTIDE_PartType
XTIDE_PartType||dd|0||; MBR partition 1 type byte, read back through the log
""")

PROCS = T("""
;----------------------------------------------------------------------------
; XTIDE_CaptureGeometry - pull addressing parameters out of the IDENTIFY data.
;
;   In:   ESI -> validated IDENTIFY buffer
;   Uses: EAX, ECX
;
;   LBA is preferred wherever the drive offers it, and CHS is kept as a real
;   fallback rather than a comment: XTIDE Universal BIOS drives plenty of
;   pre-LBA drives, and a driver that only speaks LBA is a driver for the
;   CompactFlash cards and nothing else.
;----------------------------------------------------------------------------
BeginProc XTIDE_CaptureGeometry

|movzx|eax, word ptr [esi+ID_CYLS]
|mov|XTIDE_Cyls, eax
|movzx|eax, word ptr [esi+ID_HEADS]
|mov|XTIDE_Heads, eax
|movzx|eax, word ptr [esi+ID_SPT]
|mov|XTIDE_Spt, eax
|mov|eax, [esi+ID_LBA28]
|mov|XTIDE_TotalSec, eax

|xor|ecx, ecx
|movzx|eax, word ptr [esi+ID_CAPS]
|test|ax, 0200h||; capability bit 9 - LBA supported
|jz|xcg_nolba
|cmp|XTIDE_TotalSec, 0|; a drive can claim LBA and report no
|je|xcg_nolba||; capacity; believe the capacity
|inc|ecx
xcg_nolba:
|mov|XTIDE_LbaOk, ecx
|ret

EndProc XTIDE_CaptureGeometry

;----------------------------------------------------------------------------
; XTIDE_ReadSectors - read XTIDE_ReqCount sectors from XTIDE_ReqLba into
;                     XTIDE_ReqBuf, through whichever transport autodetect
;                     settled on.
;
;   Out:  CF=0 on success; on failure CF=1 and XTIDE_FailCode set to 7, 8 or 9
;   Uses: EAX, ECX, EDX, EDI
;
;   Polled throughout. The card here is jumpered without an interrupt, and a
;   driver that assumes otherwise is a driver for one machine.
;----------------------------------------------------------------------------
BeginProc XTIDE_ReadSectors

|call|XTIDE_WaitNotBusy|; drive idle before we program anything
|jc|xrs_notready

|cmp|XTIDE_LbaOk, 0
|je|xrs_chs

;  LBA28: the taskfile carries the address a byte at a time, low first, with
;  the top four bits riding in Drive/Head alongside the LBA and master bits.
|mov|eax, XTIDE_ReqLba
|and|eax, 0FFh
|mov|XTIDE_oSecNum, eax
|mov|eax, XTIDE_ReqLba
|shr|eax, 8
|and|eax, 0FFh
|mov|XTIDE_oCylLo, eax
|mov|eax, XTIDE_ReqLba
|shr|eax, 16
|and|eax, 0FFh
|mov|XTIDE_oCylHi, eax
|mov|eax, XTIDE_ReqLba
|shr|eax, 24
|and|eax, 0Fh
|or|eax, DRVHD_LBA
|mov|XTIDE_oDrvHd, eax
|jmp|xrs_program

;  CHS: cyl = lba / (heads*spt), then head = rem / spt and sector = rem MOD spt
;  + 1. Sector numbers are one-based; heads and cylinders are not.
xrs_chs:
|mov|eax, XTIDE_Heads
|imul|eax, XTIDE_Spt
|or|eax, eax
|jz|xrs_geom||; no LBA and no usable geometry either
|mov|ecx, eax
|mov|eax, XTIDE_ReqLba
|xor|edx, edx
|div|ecx|||; eax = cylinder, edx = remainder
|mov|XTIDE_oCylHi, eax
|and|eax, 0FFh
|mov|XTIDE_oCylLo, eax
|mov|eax, XTIDE_oCylHi
|shr|eax, 8
|and|eax, 0FFh
|mov|XTIDE_oCylHi, eax

|mov|eax, edx
|xor|edx, edx
|div|XTIDE_Spt||; eax = head, edx = sector - 1
|and|eax, 0Fh
|or|eax, DRVHD_MASTER
|mov|XTIDE_oDrvHd, eax
|inc|edx
|mov|XTIDE_oSecNum, edx

xrs_program:
|mov|edx, XTIDE_pDrvHd
|mov|eax, XTIDE_oDrvHd
|out|dx, al
|call|XTIDE_WaitNotBusy|; let the drive honour the select
|jc|xrs_notready

|mov|edx, XTIDE_pSecCnt
|mov|eax, XTIDE_ReqCount
|out|dx, al
|mov|edx, XTIDE_pSecNum
|mov|eax, XTIDE_oSecNum
|out|dx, al
|mov|edx, XTIDE_pCylLo
|mov|eax, XTIDE_oCylLo
|out|dx, al
|mov|edx, XTIDE_pCylHi
|mov|eax, XTIDE_oCylHi
|out|dx, al

|mov|edx, XTIDE_pStatus|; status on read, command on write
|mov|al, ATA_READ
|out|dx, al

;  Read the transport autodetect settled on, not whatever the last probe
;  attempt happened to leave in XTIDE_TryTransport.
|mov|eax, XTIDE_ActiveTransport
|mov|XTIDE_TryTransport, eax

|mov|ecx, XTIDE_ReqCount
|mov|edi, XTIDE_ReqBuf
xrs_sector:
|push|ecx
|call|XTIDE_WaitDrq||; one DRQ per sector, not one per command
|jc|xrs_nodrq
|call|XTIDE_ReadData||; advances EDI by 512
|pop|ecx
|dec|ecx
|jnz|xrs_sector
|clc
|ret

xrs_nodrq:
|pop|ecx
|mov|XTIDE_FailCode, 8|; read gave no DRQ, or set ERR
|stc
|ret

xrs_notready:
|mov|XTIDE_FailCode, 7|; BSY never cleared around the read
|stc
|ret

xrs_geom:
|mov|XTIDE_FailCode, 9|; no LBA and heads*spt is zero
|stc
|ret

EndProc XTIDE_ReadSectors
""")

# The probe's success tail: capture geometry, then read LBA 0 as a self-test.
OLD_TAIL = T("""xp_ok:
|mov|eax, XTIDE_TryTransport
|mov|XTIDE_ActiveTransport, eax
|mov|XTIDE_MasterPresent, 1
|mov|eax, XTIDE_ModelChars|; success delay = 8 + model length, so
|add|eax, 8|||; the log reports WHICH string was read,
|mov|XTIDE_FailCode, eax|; not merely that one validated. A zero
|||||; delay still means the probe never ran.
|jmp|xp_exit
""")

NEW_TAIL = T("""xp_ok:
|mov|eax, XTIDE_TryTransport
|mov|XTIDE_ActiveTransport, eax
|mov|XTIDE_MasterPresent, 1

|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry

;  PHASE 2a - prove the read path on the safest target there is: LBA 0, one
;  sector, read-only, on a drive this routine has just established is idle.
;  Wiring reads into the IOS request path means claiming the device, and the
;  only disk in the test bed is the boot disk - that comes after this works.
|mov|XTIDE_ReqLba, 0
|mov|XTIDE_ReqCount, 1
|mov|XTIDE_ReqBuf, OFFSET32 XTIDE_SecBuf
|call|XTIDE_ReadSectors
|jc|xp_exit|||; 7, 8 or 9 already in XTIDE_FailCode

;  A sector that came back with an MBR signature is a sector that came back.
|cmp|word ptr XTIDE_SecBuf+510, 0AA55h
|jne|xp_nosig

;  Report the partition type through the delay, so the log says WHICH sector
;  was read rather than merely that 512 bytes arrived. Codes 1-9 are failures,
;  so success starts clear of them.
|movzx|eax, byte ptr XTIDE_SecBuf+01BEh+4
|mov|XTIDE_PartType, eax
|and|eax, 0Fh
|add|eax, 24
|mov|XTIDE_FailCode, eax
|jmp|xp_exit

xp_nosig:
|mov|XTIDE_FailCode, 10|; 512 bytes read, no 55AA at the end of them
|jmp|xp_exit
""")


def main():
    p = os.path.normpath(SRC)
    d = open(p, encoding='latin1').read()
    check = '--check' in sys.argv

    if 'XTIDE_ReadSectors' in d:
        print('already applied')
        return

    edits = 0

    a = T('ATA_IDENTIFY|equ|0ECh\n')
    assert a in d, 'anchor missing: ATA_IDENTIFY equate'
    d = d.replace(a, CONSTS, 1); edits += 1

    a = T('public XTIDE_IdBuf\nXTIDE_IdBuf||db|512 dup (0)\n')
    assert a in d, 'anchor missing: XTIDE_IdBuf declaration'
    d = d.replace(a, a + DATA, 1); edits += 1

    a = ';----------------------------------------------------------------------------\n' \
        '; XTIDE_Probe - PUBLIC.'
    assert a in d, 'anchor missing: XTIDE_Probe comment banner'
    d = d.replace(a, PROCS + '\n' + a, 1); edits += 1

    assert OLD_TAIL in d, 'anchor missing: xp_ok success tail'
    d = d.replace(OLD_TAIL, NEW_TAIL, 1); edits += 1

    print('Patched: %d' % edits)
    assert edits == 4, 'expected 4 edits'
    if check:
        print('--check: not written')
        return
    open(p, 'w', encoding='latin1', newline='').write(d)
    print('wrote ' + p)


main()
