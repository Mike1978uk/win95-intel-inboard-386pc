#!/usr/bin/env python3
"""Stop SD120PPD.MPD writing port 0x94 - the PS/2 DMA arbitration register.

Issue #22, third and last set of writes. This one is not a guess: it is the
Windows equivalent of two switches the DOS build of the same driver documents
in its own help text, both of which this machine already sets.

    /fp  - Disable PS/2 Dma Arbitration
    /dp  - Skip PS/2 Dma Arbitration disable
    /ni  - Skip chipset initialization
    /de  - Disable Epp check
    /db  - Disables Eppbios check
    /sf  - Skips fast mode detection

The owner's working CONFIG.SYS line is

    SD120PPD.SYS /port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp

i.e. essentially every initialisation step disabled - and the drive still
works, under DOS *and* under Windows, with the keyboard intact. The DOS binary
contains the same 33 system-port writes as the miniport; it simply never
executes them. That is the whole difference between the two.

Patch v1 removed the chipset config writes (0x22/0x23/0x24/0x25 and the
immediate 0x94 sites); v2 removed every `out 0x21`. Neither restored the
keyboard. What both missed is that 13 of the 0x94 writes are DX-addressed -
`mov edx, 0x94` then `out dx, al` - and the audit tool declared DX-addressed
I/O unresolvable until it was fixed on 2026-08-29.

Each site is a single-byte `out dx, al` (0xEE) -> 0x90. No instruction
boundary moves. The `mov edx, 0x94` loads are left alone so the sites stay
identifiable, and any read of 0x94 is left alone as harmless.

Apply on top of v2. Round-trip: --revert must reproduce the input md5 exactly.
"""
import sys, hashlib, argparse

OUT_DX = 0xEE
NOP = 0x90

# out dx,al sites where edx was loaded with 0x94, verified by locating the
# `mov edx,0x94` (BA 94 00 00 00) two bytes earlier in each case.
SITES = [0x00cc8c, 0x00cca0, 0x00ccb4, 0x00cccf, 0x00cced, 0x00cd0d,
         0x00ce97, 0x00ceaf, 0x00ced9, 0x00ceef, 0x00cf2b, 0x00cf91,
         0x00cfa6]

LOAD = bytes([0xBA, 0x94, 0x00, 0x00, 0x00])


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

    frm, to = (NOP, OUT_DX) if a.revert else (OUT_DX, NOP)
    n = 0
    for off in SITES:
        # anchor each site on its own `mov edx,0x94`, whichever direction we go
        anchor = off - 7
        if bytes(d[anchor:anchor + 5]) != LOAD:
            sys.exit(f'FAIL {off:#08x}: no `mov edx,0x94` at {anchor:#08x}, '
                     f'found {bytes(d[anchor:anchor+5]).hex()}')
        if d[off] != frm:
            sys.exit(f'FAIL {off:#08x}: expected {frm:#04x}, found {d[off]:#04x}')
        d[off] = to
        n += 1
        print(f'  {off:#08x}  {frm:#04x} -> {to:#04x}   out dx,al with dx=0x94')

    if n != len(SITES):
        sys.exit(f'FAIL: changed {n}, expected {len(SITES)}')
    open(a.outfile, 'wb').write(d)
    print(f'out: {a.outfile}  md5={md5(d)}')
    print(f'Patched: {n}')


if __name__ == '__main__':
    main()
