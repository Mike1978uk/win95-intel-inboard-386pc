#!/usr/bin/env python3
r"""Phase 2c: per-unit geometry, device inquiry that claims a unit and fills the
DCB, and a synchronous request handler that services IOR_READ / IOR_WRITE.

  python drivers/xtide_pdr/tools/phase2c_requestpath.py [--check]
"""
import sys, os

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'XTIDETR.ASM')
TAB = chr(9)


def T(s):
    return s.replace('|', TAB)


INCLUDES = T("""|include vmm.inc
|include|ddb.inc
|include|portddb.inc
|include|dcb.inc
|include|ior.inc
|include|iop.inc
""")

OLD_INCLUDES = T("""|include vmm.inc
|include|ddb.inc
|include|portddb.inc
""")

DATA = T("""
;----------------------------------------------------------------------------
;  Per-unit geometry. Phase 2b kept one global set, which was fine while the
;  probe was the only caller - but the moment IOS asks about two units, the
;  slave's IDENTIFY overwrites the master's numbers and every later request
;  addresses the wrong shape of disk.
;
;  Index 0 = master, 1 = slave, matching DCB_unit_on_ctl.
;----------------------------------------------------------------------------
XTIDE_UnitPresent|dd|2 dup (0)
XTIDE_UnitCyls||dd|2 dup (0)
XTIDE_UnitHeads||dd|2 dup (0)
XTIDE_UnitSpt||dd|2 dup (0)
XTIDE_UnitTotal||dd|2 dup (0)
XTIDE_UnitLbaOk||dd|2 dup (0)

;  Which units this driver is willing to claim, one bit each. The default
;  claims both; the emulator loop builds with 2 so the first request-path run
;  is aimed at a scratch slave and not at the boot disk.
ifndef XT_CLAIM_MASK
XT_CLAIM_MASK|equ|3
endif

;  Sector-count register is 8 bits, and 0 there means 256. IOS can ask for far
;  more than one command can carry, so requests are chunked.
XT_MAX_CHUNK|equ|128||; 64 KB per command

XTIDE_ReqUnit||dd|0
""")

PROCS = T("""
;----------------------------------------------------------------------------
; XTIDE_StashGeometry / XTIDE_SelectUnit - move geometry between the working
; globals and the per-unit arrays.
;
;   In:   EAX = unit index (0 master, 1 slave)
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_StashGeometry

|mov|ecx, eax
|shl|ecx, 2|||; dword index
|mov|edx, XTIDE_Cyls
|mov|XTIDE_UnitCyls[ecx], edx
|mov|edx, XTIDE_Heads
|mov|XTIDE_UnitHeads[ecx], edx
|mov|edx, XTIDE_Spt
|mov|XTIDE_UnitSpt[ecx], edx
|mov|edx, XTIDE_TotalSec
|mov|XTIDE_UnitTotal[ecx], edx
|mov|edx, XTIDE_LbaOk
|mov|XTIDE_UnitLbaOk[ecx], edx
|mov|XTIDE_UnitPresent[ecx], 1
|ret

EndProc XTIDE_StashGeometry

BeginProc XTIDE_SelectUnit

|mov|ecx, eax
|shl|ecx, 2
|mov|edx, XTIDE_UnitCyls[ecx]
|mov|XTIDE_Cyls, edx
|mov|edx, XTIDE_UnitHeads[ecx]
|mov|XTIDE_Heads, edx
|mov|edx, XTIDE_UnitSpt[ecx]
|mov|XTIDE_Spt, edx
|mov|edx, XTIDE_UnitTotal[ecx]
|mov|XTIDE_TotalSec, edx
|mov|edx, XTIDE_UnitLbaOk[ecx]
|mov|XTIDE_LbaOk, edx

;  Drive/Head base for this unit. Getting this wrong is how phase 2b wrote to
;  the boot disk while reporting success (technique 79).
|mov|edx, DRVHD_MASTER
|or|eax, eax
|jz|xsu_master
|mov|edx, DRVHD_SLAVE
xsu_master:
|mov|XTIDE_TryUnit, edx
|ret

EndProc XTIDE_SelectUnit

;----------------------------------------------------------------------------
; XTIDE_Inquiry - PUBLIC. Does a device exist at this unit, and if so describe
;                 it in the DCB.
;
;   In:   [esp+4] = unit number, [esp+8] -> DCB
;   Out:  EAX = 0 present, 1 absent
;   Uses: EAX, ECX, EDX, ESI
;----------------------------------------------------------------------------
BeginProc XTIDE_Inquiry

|push|ebx
|push|edi
|mov|eax, [esp+12]|| ; unit
|mov|esi, [esp+16]|| ; DCB

|cmp|eax, 1
|ja|xin_absent

;  Claim only what this build is configured to claim.
|mov|ecx, 1
|push|eax
|mov|edx, eax
|and|edx, 1Fh
xin_shift:
|or|edx, edx
|jz|xin_shifted
|shl|ecx, 1
|dec|edx
|jmp|xin_shift
xin_shifted:
|test|ecx, XT_CLAIM_MASK
|pop|eax
|jz|xin_absent

|mov|ecx, eax
|shl|ecx, 2
|cmp|XTIDE_UnitPresent[ecx], 0
|je|xin_absent

|call|XTIDE_SelectUnit||; geometry for THIS unit

;  Describe the device. Block size is 512 everywhere here - the CF is in
;  512-byte mode and ATA has no other sector size for a disk.
|mov|byte ptr [esi].DCB_device_type, DCB_type_disk
|mov|byte ptr [esi].DCB_bus_type, DCB_BUS_ESDI
|mov|byte ptr [esi].DCB_apparent_blk_shift, 9
|or|[esi].DCB_device_flags, DCB_DEV_PHYSICAL
|mov|[esi].DCB_apparent_blk_size, 512
|mov|[esi].DCB_actual_blk_size, 512

;  Capacity. A CHS-only drive reports no LBA total, so derive one - IOS wants a
;  sector count whatever the drive can address.
|mov|eax, XTIDE_TotalSec
|or|eax, eax
|jnz|xin_havetotal
|mov|eax, XTIDE_Cyls
|imul|eax, XTIDE_Heads
|imul|eax, XTIDE_Spt
xin_havetotal:
|mov|[esi].DCB_apparent_sector_cnt, eax
|mov|dword ptr [esi].DCB_apparent_sector_cnt+4, 0
|mov|[esi].DCB_actual_sector_cnt, eax
|mov|dword ptr [esi].DCB_actual_sector_cnt+4, 0

|mov|eax, XTIDE_Heads
|mov|[esi].DCB_apparent_head_cnt, eax
|mov|[esi].DCB_actual_head_cnt, eax
|mov|eax, XTIDE_Cyls
|mov|[esi].DCB_apparent_cyl_cnt, eax
|mov|[esi].DCB_actual_cyl_cnt, eax
|mov|eax, XTIDE_Spt
|mov|[esi].DCB_apparent_spt, eax
|mov|[esi].DCB_actual_spt, eax

|xor|eax, eax|||; present
|pop|edi
|pop|ebx
|ret

xin_absent:
|mov|eax, 1
|pop|edi
|pop|ebx
|ret

EndProc XTIDE_Inquiry

;----------------------------------------------------------------------------
; XTIDE_StartRequest - PUBLIC. Service one IOP to completion, synchronously.
;
;   In:   [esp+4] -> IOP, [esp+8] -> DCB
;   Uses: everything bar EBP
;
;   Synchronous on purpose. The card is jumpered without an interrupt, so there
;   is no completion ISR to come back through; the transfer happens here and
;   the IOP is completed before returning. That is what a polled port driver
;   does, and it is why PORTISR.ASM is not in this path at all.
;----------------------------------------------------------------------------
BeginProc XTIDE_StartRequest

|push|ebx
|push|esi
|push|edi
|mov|ebx, [esp+16]|| ; IOP
|mov|esi, [esp+20]|| ; DCB

|movzx|eax, byte ptr [esi].DCB_unit_on_ctl
|mov|XTIDE_ReqUnit, eax
|call|XTIDE_SelectUnit

|movzx|eax, [ebx].IOP_ior.IOR_func
|cmp|ax, IOR_VERIFY
|je|xsr_ok||; verify does not have to move data
|cmp|ax, IOR_READ
|je|xsr_io
|cmp|ax, IOR_WRITE
|je|xsr_io
|mov|word ptr [ebx].IOP_ior.IOR_status, IORS_INVALID_COMMAND
|jmp|xsr_complete

xsr_io:
;  A 64-bit start address with anything in the high dword is past LBA28 and
;  past this card, so refuse it rather than truncate it into a wild write.
|cmp|dword ptr [ebx].IOP_ior.IOR_start_addr+4, 0
|jne|xsr_badsector

|mov|eax, dword ptr [ebx].IOP_ior.IOR_start_addr
|mov|XTIDE_ReqLba, eax
|mov|eax, [ebx].IOP_ior.IOR_xfer_count
|mov|XTIDE_ReqCount, eax
|mov|eax, [ebx].IOP_ior.IOR_buffer_ptr
|mov|XTIDE_ReqBuf, eax

;  Bounds check against the drive's own capacity, before touching hardware.
|mov|eax, XTIDE_TotalSec
|or|eax, eax
|jz|xsr_chunk|||; CHS-only drive, no total to check against
|mov|ecx, XTIDE_ReqLba
|add|ecx, XTIDE_ReqCount
|jc|xsr_badsector
|cmp|ecx, eax
|ja|xsr_badsector

;  Chunk the request. The sector-count register is one byte and 0 means 256,
;  so no single command can carry an arbitrary IOS request.
xsr_chunk:
|mov|ecx, XTIDE_ReqCount
|or|ecx, ecx
|jz|xsr_ok
|cmp|ecx, XT_MAX_CHUNK
|jbe|xsr_have
|mov|ecx, XT_MAX_CHUNK
xsr_have:
|push|ecx|||; this chunk's sector count
|mov|eax, XTIDE_ReqCount
|sub|eax, ecx
|push|eax|||; sectors still to do after it
|mov|XTIDE_ReqCount, ecx

|movzx|eax, [ebx].IOP_ior.IOR_func
|cmp|ax, IOR_WRITE
|je|xsr_write
|call|XTIDE_ReadSectors
|jmp|xsr_done_chunk
xsr_write:
|call|XTIDE_WriteSectors
xsr_done_chunk:
|pop|eax|||; remaining
|pop|ecx|||; this chunk
|jc|xsr_ioerror

|mov|XTIDE_ReqCount, eax
|or|eax, eax
|jz|xsr_ok

;  Advance by the chunk actually transferred: LBA by sectors, buffer by bytes.
|add|XTIDE_ReqLba, ecx
|shl|ecx, 9
|add|XTIDE_ReqBuf, ecx
|jmp|xsr_chunk

xsr_ok:
|mov|word ptr [ebx].IOP_ior.IOR_status, IORS_SUCCESS
|jmp|xsr_complete

xsr_badsector:
|mov|word ptr [ebx].IOP_ior.IOR_status, IORS_INVALID_SECTOR
|jmp|xsr_complete

xsr_ioerror:
|mov|word ptr [ebx].IOP_ior.IOR_status, IORS_DEVICE_ERROR

;  Hand the IOP back up the callback stack. Same sequence the sample uses for
;  a command it cannot service.
xsr_complete:
|mov|eax, [ebx].IOP_callback_ptr
|sub|eax, size IOP_CallBack_Entry
|mov|[ebx].IOP_callback_ptr, eax
|push|ebx
|call|dword ptr [eax]
|add|esp, 4

|pop|edi
|pop|esi
|pop|ebx
|ret

EndProc XTIDE_StartRequest
""")

# Probe: stash each unit's geometry as it is identified.
OLD_MASTER = T("""|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry
""")
NEW_MASTER = T("""|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry
|xor|eax, eax
|call|XTIDE_StashGeometry|; unit 0 = master
""")

OLD_SLAVE = T("""|mov|XTIDE_SlavePresent, 1
|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry
|call|XTIDE_WriteReadBack
""")
NEW_SLAVE = T("""|mov|XTIDE_SlavePresent, 1
|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry
|mov|eax, 1
|call|XTIDE_StashGeometry|; unit 1 = slave
|call|XTIDE_WriteReadBack
""")


def main():
    p = os.path.normpath(SRC)
    d = open(p, encoding='latin1').read()
    if 'XTIDE_StartRequest' in d:
        print('already applied')
        return
    edits = 0

    assert OLD_INCLUDES in d, 'anchor missing: include block'
    d = d.replace(OLD_INCLUDES, INCLUDES, 1); edits += 1

    a = T('XTIDE_RbBuf|||db|512 dup (0)\n')
    assert a in d, 'anchor missing: XTIDE_RbBuf'
    d = d.replace(a, a + DATA, 1); edits += 1

    a = ';----------------------------------------------------------------------------\n' \
        '; XTIDE_Probe - PUBLIC.'
    assert a in d, 'anchor missing: XTIDE_Probe banner'
    d = d.replace(a, PROCS + '\n' + a, 1); edits += 1

    assert OLD_MASTER in d, 'anchor missing: master CaptureGeometry'
    d = d.replace(OLD_MASTER, NEW_MASTER, 1); edits += 1

    assert OLD_SLAVE in d, 'anchor missing: slave CaptureGeometry'
    d = d.replace(OLD_SLAVE, NEW_SLAVE, 1); edits += 1

    print('Patched: %d' % edits)
    assert edits == 5, 'expected 5 edits'
    if '--check' in sys.argv:
        print('--check: not written')
        return
    open(p, 'w', encoding='latin1', newline='').write(d)
    print('wrote ' + p)


main()
