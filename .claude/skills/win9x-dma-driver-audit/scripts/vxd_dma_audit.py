#!/usr/bin/env python3
"""Audit any Windows 9x VxD for DMA buffer allocations that assume 24-bit ISA reach.

WHY THIS EXISTS
An Intel Inboard 386/PC in an IBM 5160 has the XT's 4-bit DMA page latch, so DMA
reach is 20-bit (1 MB) - not the 24-bit (16 MB) every ISA-era driver assumes. A
driver that allocates its DMA buffer anywhere above 1 MB does not crash: the page
register silently drops the high bits and the 8237 transfers from a completely
different physical address. Measured on 2026-08-23 with MSSBLST.VXD:

    [dmapage] ch=1 val=4E -> page=0E *** TRUNCATED, buffer is above 1MB ***

    intended 0x4E0000 = 4.875 MB   (top of installed RAM)
    actual   0x0E0000 = 896 KB     (adapter ROM space)

which is why Sound Blaster Pro playback is distorted rather than silent or fatal.

WHAT IT CHECKS
_PageAllocate (VMM service 0x00010053) takes eight dword arguments, pushed right
to left, and the caller balances with `add esp, 0x20`:

    nPages, pType, hVM, AlignMask, minPhys, maxPhys, PhysAddrPtr, flags

`maxPhys` is a maximum PAGE NUMBER. 0xFFF means "anywhere below 16 MB". On this
machine anything above 0xFF is unreachable by DMA and will be silently truncated.

Usage:  python tools/vxd_dma_audit.py <file.vxd> [more.vxd ...]
Read-only. Reports; never modifies.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vxdstream import le_objects, file_off, sweep

PAGEALLOCATE = 0x00010053
# Arguments are pushed RIGHT TO LEFT, so walking backwards from the call yields them
# in prototype order: _PageAllocate(nPages, pType, hVM, AlignMask, minPhys, maxPhys,
#                                   PhysAddrPtr, flags)
ARGS = ["nPages", "pType", "hVM", "AlignMask", "minPhys", "maxPhys", "PhysAddrPtr", "flags"]
XT_MAX_PAGE = 0xFF          # 1 MB - the 20-bit limit of a 4-bit page latch

def audit(path):
    d, objs, opt, dp, ps = le_objects(path)
    findings = []
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
            # Walk back over the eight pushes that set up the call.
            #
            # The pushes are NOT always contiguous. Compilers interleave instructions that
            # do not touch the stack - HSFLOP.PDR has `shl eax,12` and `mov [x],eax`
            # between its last push and the VxDCall, and `xor edx,edx` between two pushes.
            # Breaking at the first non-push made this function return ZERO arguments, and
            # a missing maxPhys then reported as "register - not statically decidable".
            # That is a FALSE NEGATIVE on a plainly visible `push 0x1000`, and it cleared a
            # driver that was genuinely broken. Skip stack-neutral instructions instead.
            SKIPPABLE = {"mov", "movzx", "movsx", "xor", "shl", "shr", "sar", "lea", "add",
                         "sub", "and", "or", "test", "cmp", "inc", "dec", "nop", "cdq", "xchg"}
            pushes = []
            j = i - 1
            skipped = 0
            while j >= 0 and len(pushes) < 8:
                poff, psize, pmn, pops = stream[j]
                if pmn == "push":
                    pushes.append((poff, pops, psize))
                elif pmn in SKIPPABLE and skipped < 12:
                    skipped += 1          # stack-neutral filler, keep looking
                else:
                    break                 # call/jmp/ret/pop/int - stop, the frame ends here
                j -= 1
            args = {}
            for k, (poff, pops, psize) in enumerate(pushes):
                if k < len(ARGS):
                    args[ARGS[k]] = (pops, fmap[poff], psize)
            findings.append((o['n'], off, fmap[off], args))
    return findings

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    bad_total = 0
    for path in sys.argv[1:]:
        print(f"\n=== {path}")
        try:
            fs = audit(path)
        except Exception as e:
            print(f"  not a readable LE VxD: {e}"); continue
        if not fs:
            print("  no _PageAllocate calls found")
            continue
        for objn, off, fo, args in fs:
            mp = args.get("maxPhys", ("?", 0, 0))
            verdict = ""
            try:
                v = int(mp[0], 16) if mp[0].startswith("0x") else int(mp[0])
                mb = ((v + 1) * 4096) / (1024 * 1024)
                if v <= XT_MAX_PAGE:
                    verdict = "  ok - within the XT's 20-bit reach"
                elif v >= 0xFFFF:
                    # 0xFFFFF and friends mean "anywhere in the address space" - an ordinary
                    # allocation that never touches DMA. Forcing those into low memory would
                    # waste the only DMA-capable RAM the machine has. Report, do not patch.
                    verdict = (f"  NOTE maxPhys = {v:#x} ({mb:g} MB) = 'anywhere' - an ordinary "
                               f"allocation, not an ISA DMA buffer. Do NOT patch.")
                else:
                    # ~16 MB is a deliberate ISA-DMA constraint: the driver is saying "this
                    # buffer must be reachable by an ISA DMA controller". On this machine that
                    # ceiling is 1 MB, not 16 MB.
                    verdict = (f"  *** maxPhys = {v:#x} pages = {mb:g} MB - a deliberate ISA DMA "
                               f"ceiling, and WRONG here: a 4-bit page latch reaches {XT_MAX_PAGE:#x} "
                               f"(1 MB) ***")
                    bad_total += 1
            except ValueError:
                verdict = "  maxPhys is a register - not statically decidable"
            print(f"  _PageAllocate at OBJ{objn}:0x{off:04x} (file 0x{fo:x}){verdict}")
            for name in ARGS:
                if name in args:
                    val, afo, asz = args[name]
                    mark = " <-- patch here" if (name == "maxPhys" and "***" in verdict) else ""
                    print(f"      {name:<12} = {val:<12} (push at file 0x{afo:x}, {asz} bytes){mark}")
    print(f"\n{bad_total} allocation(s) exceed the XT's 20-bit DMA reach.")
    return 1 if bad_total else 0

sys.exit(main())
