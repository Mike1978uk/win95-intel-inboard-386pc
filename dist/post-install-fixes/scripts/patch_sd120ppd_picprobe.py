#!/usr/bin/env python3
"""Stop SD120PPD.MPD writing the 8259 interrupt mask at all.

Issue #22. The first patch (patch_sd120ppd_chipset.py) NOPed the driver's
*configuration* writes to 0x22/0x23/0x24/0x25/0x94 and deliberately spared the
`out 21h` sites, reasoning they were paired save/restore and therefore neutral.
Deployed on the real 5160, it did not restore the keyboard.

Decoding .text linearly (this is a PE, not an LE - vxdstream.py will not parse
it) shows what those sites actually are:

    00ea98  cli
    00ea9a  in   al, 0x21      ; save the REAL interrupt mask
    00ea9c  push eax
    00eaa1  mov  al, 0xA0      ; test pattern
    00eaa3  out  0x21, al      ; <-- writes it to the INTERRUPT MASK
    00eaa9  in   al, 0x23      ; read it back through the ALIAS
    00eaab  cmp  al, 0xA0      ; matched -> "a chipset is present"
    00eab2  out  0x21, al      ; restore

That is the aliasing bug in its purest form: on an XT, 0x23 *is* 0x21, so the
read-back always matches and the probe always succeeds. Two more sites at
0xde17 and 0xe695 do the same with an inverted mask, read back via 0x2F and DX.

Those two carry NO `cli`. That is an unprotected read-modify-write of the
interrupt mask. Under Windows, VPICD virtualises that register and changes it
constantly; if it moves inside that window, the driver's restore writes back a
stale value and the mask stays wrong permanently. It fits the symptom: fine
under DOS where the mask is static, dead under Windows, and selective - the
serial mouse on IRQ 4 keeps working while the keyboard on IRQ 1 does not.

So this NOPs every `out 0x21, al` in the file, probes and restores alike. The
driver has no business writing the interrupt mask. The `in al, 0x21` reads and
the push/pop pairs are untouched, so nothing goes out of balance; with the probe
write gone the read-back returns an unchanged mask, the compare fails, and the
driver concludes no chipset is present - which is what `/ni` does in the DOS
build of the same driver.

Round-trip: --revert must reproduce the input md5 exactly.
"""
import sys, hashlib, argparse

OUT21 = bytes([0xE6, 0x21])
NOP = bytes([0x90, 0x90])

SITES = {
    0x00de17: 'probe write, inverted mask, read back via 0x2F',
    0x00de24: 'restore after that probe  - UNPROTECTED, no cli',
    0x00e695: 'probe write, inverted mask, read back via DX',
    0x00e6a1: 'restore after that probe  - UNPROTECTED, no cli',
    0x00eaa3: 'probe write 0xA0, read back via 0x23',
    0x00eab2: 'restore after that probe',
    0x00eac3: 'probe write 0x0A, read back via 0x23',
    0x00ead2: 'restore after that probe',
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

    frm, to = (NOP, OUT21) if a.revert else (OUT21, NOP)
    n = 0
    for off in sorted(SITES):
        cur = bytes(d[off:off + 2])
        if cur != frm:
            sys.exit(f'FAIL {off:#08x}: expected {frm.hex()}, found {cur.hex()} ({SITES[off]})')
        d[off:off + 2] = to
        n += 1
        print(f'  {off:#08x}  {frm.hex()} -> {to.hex()}   {SITES[off]}')

    if n != len(SITES):
        sys.exit(f'FAIL: changed {n}, expected {len(SITES)}')
    open(a.outfile, 'wb').write(d)
    print(f'out: {a.outfile}  md5={md5(d)}')
    print(f'Patched: {n}')


if __name__ == '__main__':
    main()
