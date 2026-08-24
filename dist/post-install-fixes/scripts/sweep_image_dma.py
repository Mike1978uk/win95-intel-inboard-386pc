#!/usr/bin/env python3
r"""Sweep every VxD on a raw disk image for DMA buffers that assume 24-bit ISA reach.

The single-file companion to tools/vxd_dma_audit.py: same decoding, same judgment,
applied to a whole install at once. Written after the MSSBLST.VXD fix was confirmed
on real hardware (2026-08-24) - if one stock Microsoft driver got the machine class
wrong, the question "which others" deserves a measured answer rather than a guess.

The verdicts matter more than the count. `~16 MB` is a driver DELIBERATELY declaring
an ISA DMA constraint, and on a 4-bit page latch that ceiling is wrong. `0xFFFFF` is
"anywhere" - an ordinary allocation that never goes near the 8237, and forcing it low
would spend the only DMA-capable RAM the machine has. A blanket patch gets the second
kind wrong, so this tool never patches; it tells you which files to hand to
vxd-patches/patch_vxd_dma_maxphys.py.

Blind spot, stated rather than hidden: VxDs bundled inside VMM32.VXD cannot be read -
that file is W4 compressed. Audit their pre-monolith staging copies instead (this
install keeps them in C:\PREPATCH and C:\PATCHES, and they are swept here).

  python tools/sweep_image_dma.py <image> [--extract-to DIR]
"""
import sys, os, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from fatls import Fat

# vxd_dma_audit.py calls sys.exit() at import time, so import audit() from it by hand.
_spec = importlib.util.spec_from_file_location("_vda", os.path.join(HERE, "vxd_dma_audit.py"))
_vda = importlib.util.module_from_spec(_spec)
_argv = sys.argv
sys.argv = [_argv[0]]        # so its main() prints usage to nothing rather than auditing our image
try:
    _spec.loader.exec_module(_vda)
except SystemExit:
    pass
finally:
    sys.argv = _argv
audit, XT_MAX_PAGE = _vda.audit, _vda.XT_MAX_PAGE

EXTS = ('.VXD', '.386', '.PDR', '.MPD')


def classify(args):
    """-> (verdict, maxPhys value or None). Mirrors vxd_dma_audit.py's judgment."""
    mp = args.get("maxPhys", ("?", 0, 0))[0]
    if not mp.startswith("0x"):
        return "REGISTER", None
    v = int(mp, 16)
    if v <= XT_MAX_PAGE:
        return "OK", v
    if v >= 0xFFFF:
        return "ANYWHERE", v
    return "PATCH", v


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    img = sys.argv[1]
    out = None
    if "--extract-to" in sys.argv:
        out = sys.argv[sys.argv.index("--extract-to") + 1]
        os.makedirs(out, exist_ok=True)

    fat = Fat(img)
    files = sorted((p, e) for p, e in fat.walk()
                   if not e['dir'] and e['name'].upper().endswith(EXTS))

    rows, patch_list = [], []
    scratch = out or os.path.join(os.environ.get('TEMP', '.'), '_dmasweep')
    os.makedirs(scratch, exist_ok=True)

    for path, ent in files:
        local = os.path.join(scratch, path.replace(chr(92), '_').replace(':', ''))
        with open(local, 'wb') as f:
            f.write(fat.read(ent))
        try:
            fs = audit(local)
        except Exception as e:
            rows.append((path, "UNREADABLE", str(e)[:48], 0))
            continue
        if not fs:
            rows.append((path, "-", "no _PageAllocate", 0))
            continue
        worst, detail, n = "OK", [], 0
        for objn, off, fo, args in fs:
            verdict, v = classify(args)
            detail.append(f"OBJ{objn}:0x{off:04x} maxPhys="
                          + (f"{v:#x}" if v is not None else "reg") + f" [{verdict}]")
            if verdict == "PATCH":
                worst, n = "PATCH", n + 1
            elif verdict == "ANYWHERE" and worst != "PATCH":
                worst = "ANYWHERE"
            elif verdict == "REGISTER" and worst == "OK":
                worst = "REGISTER"
        rows.append((path, worst, "; ".join(detail), n))
        if worst == "PATCH":
            patch_list.append((path, n))

    print(f"Swept {len(files)} VxD-class files in {img}\n")
    order = {"PATCH": 0, "ANYWHERE": 1, "REGISTER": 2, "OK": 3, "UNREADABLE": 4, "-": 5}
    for path, verdict, detail, n in sorted(rows, key=lambda r: (order.get(r[1], 9), r[0])):
        if verdict == "-":
            continue
        print(f"  {verdict:<10} {path}")
        print(f"             {detail}")

    skipped = sum(1 for r in rows if r[1] == "-")
    print(f"\n  ({skipped} files carry no _PageAllocate call at all)")
    print("\n" + "=" * 72)
    if patch_list:
        print("NEEDS PATCHING - a deliberate ISA DMA ceiling that is wrong on a 4-bit latch:")
        for p, n in patch_list:
            print(f"  {p}   ({n} call site{'s' if n != 1 else ''})")
    else:
        print("Nothing needs patching.")
    print("\nANYWHERE = maxPhys 'don't care'. NOT a DMA buffer. Leave alone - patching it")
    print("would force an ordinary allocation into the machine's only DMA-capable RAM.")
    print("\nNot covered: VxDs bundled inside VMM32.VXD (W4 compressed). Sweep a"
          r" pre-monolith image, or the staging copies in \\PREPATCH, instead.")


main()
