#!/usr/bin/env python3
"""Stop SD120PPD.MPD registering itself as a Micro Channel adapter.

Issue #22, and a different mechanism from v1-v3 - this is not a port write.

`DriverEntry` calls ScsiPortInitialize TWICE. The HW_INITIALIZATION_DATA is
built once at [ebp-0x50] and reused; only AdapterInterfaceType changes:

    0037f7  mov  dword [ebp-0x4c], eax    ; eax = 1  -> Isa
    00381e  call ScsiPortInitialize
    003823  mov  edi, eax                 ; keep the Isa status
    00382e  mov  dword [ebp-0x4c], 3      ; 3 -> MicroChannel
    00383b  call ScsiPortInitialize
    003840  cmp  eax, edi
    003842  jb   0x3846
    003844  mov  eax, edi                 ; return the better of the two
    003846  pop edi/esi/ebx ; ret 8

The second registration is meaningless on an IBM 5160 - there is no Micro
Channel bus - and it is what drags in the whole PS/2 code path: the port 0x94
system-board setup register and the POS registers at 0x100-0x102. The DOS build
of this same driver exposes `/fp` and `/dp` to disable exactly that, and this
machine's CONFIG.SYS sets both.

It also matters because a registered adapter gets an interrupt: the driver
declares HwInterrupt in the same structure, so SCSIPORT will ask VPICD to
virtualise whatever IRQ HwFindAdapter reports for EACH registration. That is a
mechanism no amount of NOPing port writes can reach, which is consistent with
v1, v2 and v3 all failing while every system-board write was neutralised.

The patch is two bytes: at 0x3825, jump over the second setup and call, landing
on `mov eax, edi` at 0x3844 - the path already taken when the Isa result is the
better one. No stack imbalance (the pushes are skipped with the call), no
instruction boundary moves, and the Isa registration is untouched.

    0x3825   89 5d   mov dword [ebp-4], ebx   ->   eb 1d   jmp 0x3844

Round-trip: --revert must reproduce the input md5 exactly.
"""
import sys, hashlib, argparse

SITE = 0x3825
ORIG = bytes([0x89, 0x5D])          # mov dword ptr [ebp-4], ebx
PATCH = bytes([0xEB, 0x1D])         # jmp 0x3844
# guards: the two instructions the jump depends on must be where we think
GUARDS = {
    0x382e: bytes([0xC7, 0x45, 0xB4, 0x03, 0x00, 0x00, 0x00]),  # mov [ebp-0x4c],3
    0x3844: bytes([0x8B, 0xC7]),                                # mov eax, edi
}


def md5(b):
    return hashlib.md5(b).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('infile')
    ap.add_argument('outfile')
    ap.add_argument('--revert', action='store_true')
    a = ap.parse_args()

    d = bytearray(open(a.infile, 'rb').read())
    print(f'in : {a.infile}  {len(d)} bytes  md5={md5(d)}')

    for off, want in GUARDS.items():
        got = bytes(d[off:off + len(want)])
        if got != want:
            sys.exit(f'FAIL guard {off:#08x}: expected {want.hex()}, found {got.hex()}')
    print('  guards ok: MicroChannel store at 0x382e, landing site at 0x3844')

    frm, to = (PATCH, ORIG) if a.revert else (ORIG, PATCH)
    got = bytes(d[SITE:SITE + 2])
    if got != frm:
        sys.exit(f'FAIL {SITE:#08x}: expected {frm.hex()}, found {got.hex()}')
    d[SITE:SITE + 2] = to
    print(f'  {SITE:#08x}  {frm.hex()} -> {to.hex()}   '
          f'{"restore mov [ebp-4],ebx" if a.revert else "jmp past the MicroChannel registration"}')

    open(a.outfile, 'wb').write(d)
    print(f'out: {a.outfile}  md5={md5(d)}')
    print('Patched: 1')


if __name__ == '__main__':
    main()
