#!/usr/bin/env python3
"""Retarget a VxD's _PageAllocate DMA buffer allocations to the XT's 20-bit reach.

An Intel Inboard 386/PC in an IBM 5160 keeps the XT motherboard's 4-bit DMA page
latch, so DMA reach is 20-bit (1 MB). Every ISA-era driver assumes 24-bit (16 MB)
and passes maxPhys = 0xFFF to _PageAllocate. VMM honours that and allocates from
the top of RAM down; the 8237's page register then drops the high bits and the
transfer runs against a different physical address entirely. No fault, no crash -
just wrong data. For MSSBLST.VXD that is the distorted Sound Blaster Pro audio in
issue #5, measured 2026-08-23:

    intended 0x4E0000 (4.875 MB)  ->  actual 0x0E0000 (896 KB, adapter ROM space)

The change is maxPhys 0xFFF -> 0xFF. `push 0xfff` and `push 0xff` both encode as a
5-byte `68 imm32`, so exactly one byte moves per site and no instruction boundary
shifts - the failure mode that corrupted VDMAD (see patch_vdmad.py) is structurally
impossible here.

Graceful degradation: contiguous low memory can be scarce. MSSBLST's first call
site already halves nPages and retries on failure (OBJ4:0x78 `shr eax,1`), so a
refused allocation shrinks the buffer rather than breaking playback.

Usage: python patch_vxd_dma_maxphys.py <in.vxd> <out.vxd>
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vxdstream import le_objects, file_off, sweep

PAGEALLOCATE = 0x00010053
XT_MAX_PAGE  = 0xFF

def find_sites(path):
    """Return file offsets of the 5-byte `push imm32` that supplies maxPhys."""
    d, objs, opt, dp, ps = le_objects(path)
    sites = []
    for o in objs:
        if not (o['flags'] & 0x4):
            continue
        buf = bytearray(); fmap = []
        for off in range(o['vsize']):
            fo = file_off(d, opt, dp, ps, o, off); buf.append(d[fo]); fmap.append(fo)
        stream = list(sweep(buf))
        for i, (off, size, mn, ops) in enumerate(stream):
            if mn != "VxDCall" or int(ops, 16) != PAGEALLOCATE:
                continue
            pushes = []
            j = i - 1
            while j >= 0 and len(pushes) < 8 and stream[j][2] == "push":
                pushes.append(stream[j]); j -= 1
            if len(pushes) < 6:
                print(f"  SKIP OBJ{o['n']}:0x{off:04x}: only {len(pushes)} pushes before the call")
                continue
            poff, psize, pmn, pops = pushes[5]          # maxPhys
            if not pops.startswith("0x"):
                print(f"  SKIP OBJ{o['n']}:0x{off:04x}: maxPhys is {pops}, not an immediate")
                continue
            val = int(pops, 16)
            if val <= XT_MAX_PAGE:
                print(f"  OK   OBJ{o['n']}:0x{off:04x}: maxPhys already {pops}")
                continue
            if psize != 5 or buf[poff] != 0x68:
                print(f"  SKIP OBJ{o['n']}:0x{off:04x}: maxPhys push is {psize} bytes, not push imm32")
                continue
            sites.append((fmap[poff], o['n'], off, val))
    return sites

def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, 'rb').read())
    sites = find_sites(src)
    if not sites:
        print("\nREFUSING TO WRITE OUTPUT: no _PageAllocate site needed changing. Either this VxD "
              "is already XT-safe, or it does not allocate its DMA buffer this way - do not "
              "assume a silent no-op is a patch (see patch_vdmad.py's header).")
        raise SystemExit(1)
    for fo, objn, coff, val in sites:
        data[fo + 1:fo + 5] = XT_MAX_PAGE.to_bytes(4, 'little')
        print(f"  patched file 0x{fo:x} (OBJ{objn}, call at 0x{coff:04x}): "
              f"maxPhys {val:#x} -> {XT_MAX_PAGE:#x}  ({((val+1)*4096)//(1024*1024)} MB -> 1 MB)")
    open(out, 'wb').write(data)

    stock = open(src, 'rb').read()
    changed = [i for i in range(len(stock)) if stock[i] != data[i]]
    expected = sorted(i for fo, _, _, _ in sites for i in range(fo + 1, fo + 5)
                      if stock[i] != data[i])
    if changed != expected:
        print(f"\nPOST-CHECK FAILED: changed {[hex(c) for c in changed]}, expected {[hex(e) for e in expected]}")
        raise SystemExit(1)
    print(f"\nWrote {out}, {len(data)} bytes. Post-check OK: {len(changed)} byte(s) changed, "
          f"all inside a maxPhys push imm32; no instruction boundary moved.")
    print("Re-audit with: python tools/vxd_dma_audit.py " + out)

main()
