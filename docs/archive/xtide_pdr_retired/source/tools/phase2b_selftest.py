#!/usr/bin/env python3
r"""Phase 2b-2: probe the slave, and prove the write path by writing a sector
and reading it back - on the SLAVE only, never the master.

The master is the boot disk in both the test bed and the 5160. A write
self-test belongs on a disk nothing else owns.

  python drivers/xtide_pdr/tools/phase2b_selftest.py [--check]
"""
import sys, os

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'XTIDETR.ASM')
TAB = chr(9)


def T(s):
    return s.replace('|', TAB)


# A comment written in the previous patch is wrong and would mislead the next
# reader: a byte-swap in WriteData IS caught by a round trip, because the read
# path is separate code. Correct it rather than leave it.
BAD_COMMENT = T(""";   LATCH ORDER IS THE OPPOSITE OF A READ, and getting it backwards writes
;   every byte pair swapped - which reads back fine through the same wrong
;   code, so a round-trip self-test will NOT catch it. On a read the data port
;   is read first and latches the high byte; on a write the high byte is
;   staged in the latch first and the data-port write commits the word.
""")

GOOD_COMMENT = T(""";   LATCH ORDER IS THE OPPOSITE OF A READ. On a read the data port is read
;   first and that latches the high byte; on a write the high byte is staged in
;   the latch first and the data-port write commits the word. Get it backwards
;   and every byte pair is stored swapped.
;
;   A write/read-back self-test DOES catch that, because the read path is
;   separate code that is not making the same mistake - which is exactly why
;   the test pattern below gives each word two different bytes.
""")

PROC = T("""
;----------------------------------------------------------------------------
; XTIDE_WriteReadBack - write one sector of a known pattern and read it back.
;
;   Out:  CF=0 if the sector came back identical
;         CF=1 with XTIDE_FailCode 7, 9, 12, 13, 14 (transfer) or 15 (mismatch)
;   Uses: EAX, ECX, EDX, ESI, EDI
;
;   The pattern gives every 16-bit word two DIFFERENT bytes - n and its
;   complement - so a high/low swap in the latch path shows up as a mismatch
;   rather than passing. All-zeroes or a constant fill would prove nothing:
;   blank media returns those too.
;
;   LBA 100 is arbitrary but deliberately clear of sector 0 and of anything a
;   partition table or boot sector lives in, so a run against the wrong disk
;   damages nothing structural.
;----------------------------------------------------------------------------
XTIDE_TEST_LBA|equ|100

BeginProc XTIDE_WriteReadBack

|mov|edi, OFFSET32 XTIDE_WrBuf
|mov|ecx, 256|||; words
|xor|eax, eax
xwrb_fill:
|mov|[edi], al|||; low byte counts up
|not|al
|mov|[edi+1], al||; high byte is its complement
|not|al
|inc|al
|add|edi, 2
|dec|ecx
|jnz|xwrb_fill

|mov|XTIDE_ReqLba, XTIDE_TEST_LBA
|mov|XTIDE_ReqCount, 1
|mov|XTIDE_ReqBuf, OFFSET32 XTIDE_WrBuf
|call|XTIDE_WriteSectors
|jc|xwrb_out|||; code already set by the write path

|mov|XTIDE_ReqLba, XTIDE_TEST_LBA
|mov|XTIDE_ReqCount, 1
|mov|XTIDE_ReqBuf, OFFSET32 XTIDE_RbBuf
|call|XTIDE_ReadSectors
|jc|xwrb_out

|mov|esi, OFFSET32 XTIDE_WrBuf
|mov|edi, OFFSET32 XTIDE_RbBuf
|mov|ecx, 512
xwrb_cmp:
|mov|al, [esi]
|cmp|al, [edi]
|jne|xwrb_mismatch
|inc|esi
|inc|edi
|dec|ecx
|jnz|xwrb_cmp
|clc
|ret

xwrb_mismatch:
|mov|XTIDE_FailCode, 15|; round trip returned different bytes
xwrb_out:
|stc
|ret

EndProc XTIDE_WriteReadBack
""")

OLD = T(""";  Report the partition type through the delay, so the log says WHICH sector
;  was read rather than merely that 512 bytes arrived. Codes 1-9 are failures,
;  so success starts clear of them.
|movzx|eax, byte ptr XTIDE_SecBuf+01BEh+4
|mov|XTIDE_PartType, eax
|and|eax, 0Fh
|add|eax, 24
|mov|XTIDE_FailCode, eax
|jmp|xp_exit
""")

NEW = T(""";  Remember which sector was read, so the delay can report WHICH one rather
;  than merely that 512 bytes arrived.
|movzx|eax, byte ptr XTIDE_SecBuf+01BEh+4
|mov|XTIDE_PartType, eax

;  PHASE 2b - the write path, on a SLAVE if one is fitted, never on the master.
;  The master is the boot disk both here and on the 5160.
;
;  NOTE: XTIDE_CaptureGeometry overwrites one global set of geometry, so the
;  slave's values replace the master's from here on. That is fine while this is
;  a self-test and nothing addresses the master afterwards; per-unit geometry
;  arrives with the DCB work, which needs it anyway.
|mov|XTIDE_TryUnit, DRVHD_SLAVE
|call|XTIDE_TryIdentify
|jc|xp_noslave

|mov|XTIDE_SlavePresent, 1
|mov|esi, OFFSET32 XTIDE_IdBuf
|call|XTIDE_CaptureGeometry
|call|XTIDE_WriteReadBack
|jc|xp_exit|||; 7, 9, 12, 13, 14 or 15 already set

|mov|eax, XTIDE_PartType|; read AND write verified
|and|eax, 0Fh
|add|eax, 40
|mov|XTIDE_FailCode, eax
|jmp|xp_exit

;  No slave is not a failure - the real card has one drive. Report the phase 2a
;  result: the read verified, there was nothing safe to write to.
xp_noslave:
|mov|XTIDE_TryUnit, DRVHD_MASTER
|mov|eax, XTIDE_PartType
|and|eax, 0Fh
|add|eax, 24
|mov|XTIDE_FailCode, eax
|jmp|xp_exit
""")


def main():
    p = os.path.normpath(SRC)
    d = open(p, encoding='latin1').read()

    if 'XTIDE_WriteReadBack' in d:
        print('already applied')
        return

    edits = 0

    assert BAD_COMMENT in d, 'anchor missing: WriteData latch comment'
    d = d.replace(BAD_COMMENT, GOOD_COMMENT, 1); edits += 1

    a = ';----------------------------------------------------------------------------\n' \
        '; XTIDE_Probe - PUBLIC.'
    assert a in d, 'anchor missing: XTIDE_Probe banner'
    d = d.replace(a, PROC + '\n' + a, 1); edits += 1

    assert OLD in d, 'anchor missing: phase 2a success tail'
    d = d.replace(OLD, NEW, 1); edits += 1

    print('Patched: %d' % edits)
    assert edits == 3, 'expected 3 edits'
    if '--check' in sys.argv:
        print('--check: not written')
        return
    open(p, 'w', encoding='latin1', newline='').write(d)
    print('wrote ' + p)


main()
