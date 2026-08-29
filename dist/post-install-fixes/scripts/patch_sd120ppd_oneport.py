#!/usr/bin/env python3
"""Stop SD120PPD.MPD probing LPT bases that do not exist on this machine.

Issue #22, aimed at a measurement rather than a theory.

A logged boot (F8 -> Logged) shows this driver's initialisation is the second
slowest operation in the entire boot, and by far the slowest inside the Windows
load:

    [0012AE31] Initing hsflop.pdr
    [0012AE38] Init Success hsflop.pdr        7 ticks
    [0012AE3A] Initing sd120ppd.mpd
    [0012B1B9] Init Success sd120ppd.mpd    895 ticks

Every other driver in the boot initialises in 1-8 ticks. The whole Windows load
is roughly 1790, so this one driver accounts for about half of it. And it
happens in the window after VKD has hooked IRQ 1 at DEVICEINIT (0012AD8D) but
before VKD reaches INITCOMPLETE (0012B279).

HwFindAdapter opens by hardcoding three candidate parallel-port bases and
probing each in turn:

    00385d  mov dword [ebp-0x54], 0x378     <- the card that is actually here
    003864  mov dword [ebp-0x50], 0x3bc     <- nothing responds
    00386b  mov dword [ebp-0x4c], 0x278     <- nothing responds

On this machine two of the three are empty addresses. Probing them means EPP
and ECP handshakes against a floating bus, which is where the bulk of those 895
ticks goes, and it is exactly the work the DOS build of the same driver skips
with /de /db /sf. That build drives the hardware correctly with essentially no
initialisation at all, which is the evidence that none of this is required.

This points all three candidates at 0x378 so the probe only ever touches real
hardware. Four bytes, two immediates, no instruction boundary moves.

Note what this does NOT claim: the mechanism by which the keyboard dies is still
unknown. Patches v1-v4 neutralised every system-board port write, the Micro
Channel double-registration, and polling, and none of them helped. This targets
the one measured anomaly left.

Round-trip: --revert must reproduce the input md5 exactly.
"""
import sys, hashlib, argparse

# offset -> (original immediate, patched immediate, description)
SITES = {
    0x3867: (bytes([0xBC, 0x03]), bytes([0x78, 0x03]), 'candidate 2: 0x3bc -> 0x378'),
    0x386e: (bytes([0x78, 0x02]), bytes([0x78, 0x03]), 'candidate 3: 0x278 -> 0x378'),
}
# the mov opcodes these immediates belong to, checked before touching anything
GUARDS = {
    0x385d: bytes([0xC7, 0x45, 0xAC, 0x78, 0x03, 0x00, 0x00]),
    0x3864: bytes([0xC7, 0x45, 0xB0]),
    0x386b: bytes([0xC7, 0x45, 0xB4]),
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
    print('  guards ok: three `mov dword [ebp-N], imm32` at 0x385d/0x3864/0x386b')

    n = 0
    for off, (orig, patched, what) in sorted(SITES.items()):
        frm, to = (patched, orig) if a.revert else (orig, patched)
        got = bytes(d[off:off + 2])
        if got != frm:
            sys.exit(f'FAIL {off:#08x}: expected {frm.hex()}, found {got.hex()}')
        d[off:off + 2] = to
        n += 1
        print(f'  {off:#08x}  {frm.hex()} -> {to.hex()}   {what}')

    if n != len(SITES):
        sys.exit(f'FAIL: changed {n}, expected {len(SITES)}')
    open(a.outfile, 'wb').write(d)
    print(f'out: {a.outfile}  md5={md5(d)}')
    print(f'Patched: {n}')


if __name__ == '__main__':
    main()
