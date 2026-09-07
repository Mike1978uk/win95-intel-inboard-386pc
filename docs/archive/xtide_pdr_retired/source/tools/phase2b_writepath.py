#!/usr/bin/env python3
r"""Phase 2b-1: split the taskfile programmer out of XTIDE_ReadSectors and add
XTIDE_WriteSectors, plus a slave probe and a write/read-back self-test.

Read and write must not diverge in HOW they address the drive - only in what
they do once the command is issued - so the addressing lives in one routine both
call, the same reason the four taskfile values are module variables.

Every insertion asserts its anchor (Technique 28).

  python drivers/xtide_pdr/tools/phase2b_writepath.py [--check]
"""
import sys, os

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'XTIDETR.ASM')
TAB = chr(9)


def T(s):
    return s.replace('|', TAB)


CONSTS = T("""ATA_READ|equ|020h||; READ SECTOR(S), the 28-bit PIO command
ATA_WRITE|equ|030h||; WRITE SECTOR(S)
""")

DATA = T("""
;  Command byte for XTIDE_ProgramTaskfile, so read and write share one
;  addressing path and differ only in what they do after the command lands.
XTIDE_Cmd|||dd|0

;  Write/read-back self-test buffers. 2 x 512 so a mismatch can be seen as a
;  mismatch rather than inferred from a checksum.
public XTIDE_WrBuf
XTIDE_WrBuf|||db|512 dup (0)
public XTIDE_RbBuf
XTIDE_RbBuf|||db|512 dup (0)
""")

# --- 1. rename the head of ReadSectors into the shared programmer -----------

OLD_HEAD = T(""";----------------------------------------------------------------------------
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
""")

NEW_HEAD = T(""";----------------------------------------------------------------------------
; XTIDE_ProgramTaskfile - select the unit, program the address, issue the
;                         command in XTIDE_Cmd.
;
;   In:   XTIDE_ReqLba, XTIDE_ReqCount, XTIDE_Cmd
;   Out:  CF=0 command issued; CF=1 and XTIDE_FailCode set to 7 or 9
;   Uses: EAX, ECX, EDX
;
;   Read and write call this same routine. The addressing arithmetic is the
;   easiest thing in a disk driver to get subtly different between the two
;   paths, and the difference shows up as data written to the wrong sector -
;   silent, and only visible much later.
;
;   Polled throughout. The card here is jumpered without an interrupt, and a
;   driver that assumes otherwise is a driver for one machine.
;----------------------------------------------------------------------------
BeginProc XTIDE_ProgramTaskfile
""")

# --- 2. replace the command issue + read loop + error tails ----------------

OLD_TAIL = T("""|mov|edx, XTIDE_pStatus|; status on read, command on write
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

NEW_TAIL = T("""|mov|edx, XTIDE_pStatus|; status on read, command on write
|mov|eax, XTIDE_Cmd
|out|dx, al

;  Use the transport autodetect settled on, not whatever the last probe
;  attempt happened to leave in XTIDE_TryTransport.
|mov|eax, XTIDE_ActiveTransport
|mov|XTIDE_TryTransport, eax
|clc
|ret

xrs_notready:
|mov|XTIDE_FailCode, 7|; BSY never cleared around the transfer
|stc
|ret

xrs_geom:
|mov|XTIDE_FailCode, 9|; no LBA and heads*spt is zero
|stc
|ret

EndProc XTIDE_ProgramTaskfile

;----------------------------------------------------------------------------
; XTIDE_ReadSectors - read XTIDE_ReqCount sectors from XTIDE_ReqLba into
;                     XTIDE_ReqBuf.
;
;   Out:  CF=0 on success; CF=1 with XTIDE_FailCode 7, 8 or 9
;   Uses: EAX, ECX, EDX, EDI
;----------------------------------------------------------------------------
BeginProc XTIDE_ReadSectors

|mov|XTIDE_Cmd, ATA_READ
|call|XTIDE_ProgramTaskfile
|jc|xrd_fail

|mov|ecx, XTIDE_ReqCount
|mov|edi, XTIDE_ReqBuf
xrd_sector:
|push|ecx
|call|XTIDE_WaitDrq||; one DRQ per sector, not one per command
|jc|xrd_nodrq
|call|XTIDE_ReadData||; advances EDI by 512
|pop|ecx
|dec|ecx
|jnz|xrd_sector
|clc
|ret

xrd_nodrq:
|pop|ecx
|mov|XTIDE_FailCode, 8|; read gave no DRQ, or set ERR
xrd_fail:
|stc
|ret

EndProc XTIDE_ReadSectors

;----------------------------------------------------------------------------
; XTIDE_WriteSectors - write XTIDE_ReqCount sectors from XTIDE_ReqBuf to
;                      XTIDE_ReqLba.
;
;   Out:  CF=0 on success; CF=1 with XTIDE_FailCode 7, 9, 12 or 13
;   Uses: EAX, ECX, EDX, ESI
;
;   The drive asserts DRQ to ASK for each sector, so the wait is the same shape
;   as a read - but the completion check is not. A write is not finished when
;   the last byte has gone out; the drive is still committing it. Poll BSY down
;   and test ERR afterwards, or a failed write looks exactly like a good one.
;----------------------------------------------------------------------------
BeginProc XTIDE_WriteSectors

|mov|XTIDE_Cmd, ATA_WRITE
|call|XTIDE_ProgramTaskfile
|jc|xwr_fail

|mov|ecx, XTIDE_ReqCount
|mov|esi, XTIDE_ReqBuf
xwr_sector:
|push|ecx
|call|XTIDE_WaitDrq||; drive asking for the next sector
|jc|xwr_nodrq
|call|XTIDE_WriteData||; advances ESI by 512
|pop|ecx
|dec|ecx
|jnz|xwr_sector

|call|XTIDE_WaitNotBusy|; the drive is still committing the last sector
|jc|xwr_notready
|test|al, ST_ERR
|jnz|xwr_err
|clc
|ret

xwr_nodrq:
|pop|ecx
|mov|XTIDE_FailCode, 13|; drive never asked for the data
|stc
|ret

xwr_notready:
|mov|XTIDE_FailCode, 12|; BSY never cleared after the last sector
|stc
|ret

xwr_err:
|movzx|eax, al
|mov|XTIDE_LastStatus, eax
|mov|XTIDE_FailCode, 14|; drive reported ERR on completion
|stc
|ret

EndProc XTIDE_WriteSectors

;----------------------------------------------------------------------------
; XTIDE_WriteData - push 512 bytes at the data register.
;
;   In:   ESI -> source buffer, XTIDE_TryTransport selects how
;   Uses: EAX, ECX, EDX, ESI
;
;   LATCH ORDER IS THE OPPOSITE OF A READ, and getting it backwards writes
;   every byte pair swapped - which reads back fine through the same wrong
;   code, so a round-trip self-test will NOT catch it. On a read the data port
;   is read first and latches the high byte; on a write the high byte is
;   staged in the latch first and the data-port write commits the word.
;   Confirmed against OBattler's XT-IDE port in drivers/xtide_cdrom/, whose
;   register map and write ordering are sound even though its transfer loops
;   are not.
;----------------------------------------------------------------------------
BeginProc XTIDE_WriteData

|cmp|XTIDE_TryTransport, XT_TR_LATCH
|je|xwd8_latch

|mov|edx, XTIDE_pData
|mov|ecx, 512|||; bytes
xwd8_pio8_loop:
|mov|al, [esi]
|out|dx, al
|inc|esi
|dec|ecx
|jnz|xwd8_pio8_loop
|ret

xwd8_latch:
|mov|ecx, 256|||; words
xwd8_latch_loop:
|mov|al, [esi+1]||; high byte STAGES in the latch
|mov|edx, XTIDE_pHiLatch
|out|dx, al
|mov|al, [esi]||; low byte COMMITS the word
|mov|edx, XTIDE_pData
|out|dx, al
|add|esi, 2
|dec|ecx
|jnz|xwd8_latch_loop
|ret

EndProc XTIDE_WriteData
""")


def main():
    p = os.path.normpath(SRC)
    d = open(p, encoding='latin1').read()

    if 'XTIDE_WriteSectors' in d:
        print('already applied')
        return

    edits = 0

    a = T('ATA_READ|equ|020h||; READ SECTOR(S), the 28-bit PIO command\n')
    assert a in d, 'anchor missing: ATA_READ equate'
    d = d.replace(a, CONSTS, 1); edits += 1

    a = T('XTIDE_PartType||dd|0||; MBR partition 1 type byte, read back through the log\n')
    assert a in d, 'anchor missing: XTIDE_PartType declaration'
    d = d.replace(a, a + DATA, 1); edits += 1

    assert OLD_HEAD in d, 'anchor missing: ReadSectors header'
    d = d.replace(OLD_HEAD, NEW_HEAD, 1); edits += 1

    assert OLD_TAIL in d, 'anchor missing: ReadSectors command issue and loop'
    d = d.replace(OLD_TAIL, NEW_TAIL, 1); edits += 1

    print('Patched: %d' % edits)
    assert edits == 4, 'expected 4 edits'
    if '--check' in sys.argv:
        print('--check: not written')
        return
    open(p, 'w', encoding='latin1', newline='').write(d)
    print('wrote ' + p)


main()
