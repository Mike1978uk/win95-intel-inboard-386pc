#!/usr/bin/env python3
"""Audit a Windows 9x driver for fixed-port I/O that is destructive on an XT.

The bug class
-------------
PC/AT-era drivers probe for host chipsets by writing a known value to a
configuration port - most often the 0x22/0x23 index/data pair - and reading it
back.  Harmless on a machine that has such a chipset.

An IBM 5160 decodes I/O incompletely.  Measured on real hardware 2026-08-28:
ports 0x21, 0x23, 0x25, 0x31 and 0x3F ALL returned 0xAC, the 8259 interrupt
mask.  The PIC answers across the whole 0x20-0x3F range.  So:

  * the probe's read-back SUCCEEDS, because the alias returns what was written,
    and the driver concludes a chipset is present that is not;
  * its follow-up configuration writes land on the interrupt controller.

SD120PPD.MPD (LS-120) does exactly this and leaves the mask at 0x06 - IRQ 1
(keyboard) and IRQ 2 masked, with no restore.  See issue #22 and
docs/ls120_keyboard_root_cause.md.

This is the same family as the 20-bit DMA bug: a driver assuming AT-class
hardware on a machine that decodes fewer address lines.  Expect it from any
stock driver.

Usage:  xt_port_audit.py DRIVER.MPD [DRIVER.VXD ...] [-v]
"""
import argparse, os, sys

# XT device blocks.  base, span-of-alias, name.  On a 5160 each block answers
# across its whole aliased range, so any port inside these is a live register.
BLOCKS = [
    (0x00, 0x20, "8237 DMA controller"),
    (0x20, 0x20, "8259 PIC  (0x21 = INTERRUPT MASK)"),
    (0x40, 0x20, "8253 PIT  (0x43 = mode/control)"),
    (0x60, 0x20, "8255 PPI  (keyboard / config switches)"),
    (0x80, 0x20, "DMA page registers"),
    (0xA0, 0x20, "NMI mask register"),
]

def classify(port):
    for base, span, name in BLOCKS:
        if base <= port < base + span:
            return name
    return None

def confidence(d, i):
    """Cheap plausibility check.  A raw byte scan is NOT evidence - E4..E7 occur
    constantly inside data and mid-instruction.  A genuine port access sits in a
    recognisable idiom."""
    pre = d[max(0, i - 8):i]
    score = 0
    if len(pre) >= 2 and pre[-2] == 0xB0:          # mov al,imm8
        score += 2
    if pre[-1:] in (b'\xFA', b'\xFB'):             # cli / sti
        score += 2
    if b'\xEB\x00' in pre:                         # jmp $+2  I/O delay idiom
        score += 2
    if pre[-1:] in (b'\x50', b'\x58'):             # push/pop eax around it
        score += 1
    if len(pre) >= 2 and pre[-2:] in (b'\x86\xE0', b'\x8A\xE0'):  # xchg/mov ah,al
        score += 1
    nxt = d[i + 2:i + 6]
    if nxt[:2] == b'\xEB\x00':
        score += 1
    return score

def scan(path, verbose=False):
    d = open(path, 'rb').read()
    lo, hi = 0, len(d)
    if d[:2] == b'MZ':                              # PE - limit to .text
        import struct
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        if d[pe:pe + 2] == b'PE':
            nsec = struct.unpack_from('<H', d, pe + 6)[0]
            optsz = struct.unpack_from('<H', d, pe + 20)[0]
            off = pe + 24 + optsz
            for _ in range(nsec):
                nm = d[off:off + 8].rstrip(b'\0')
                _vs, _va, rs, ro = struct.unpack_from('<IIII', d, off + 8)
                if nm == b'.text':
                    lo, hi = ro, ro + rs
                off += 40

    hits = []
    for i in range(lo, min(hi, len(d)) - 1):
        op = d[i]
        if op not in (0xE4, 0xE5, 0xE6, 0xE7):
            continue
        port = d[i + 1]
        blk = classify(port)
        if not blk:
            continue
        write = op in (0xE6, 0xE7)
        hits.append((i, port, write, blk, confidence(d, i)))

    strong = [h for h in hits if h[4] >= 3]
    writes = [h for h in strong if h[2]]
    print("=" * 74)
    print("%s  (%d bytes)" % (os.path.basename(path), len(d)))
    if lo:
        print("  .text %06x-%06x" % (lo, hi))
    print("  candidates in XT system ports : %d" % len(hits))
    print("  plausible (confidence >= 3)   : %d" % len(strong))
    print("  of which WRITES               : %d   <-- these are the dangerous ones"
          % len(writes))
    if writes:
        print("\n  DESTRUCTIVE WRITES:")
        for i, port, _w, blk, c in writes:
            print("    %06x  out %02Xh   conf %d   %s" % (i, port, c, blk))
    if verbose:
        for i, port, w, blk, c in hits:
            if (i, port, w, blk, c) not in writes:
                print("    %06x  %s %02Xh  conf %d  %s"
                      % (i, "out" if w else "in ", port, c, blk))
    if not writes:
        print("\n  No plausible writes to XT system ports.")
    return len(writes)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="also list reads and low-confidence candidates")
    a = ap.parse_args()
    total = sum(scan(f, a.verbose) for f in a.files)
    print("=" * 74)
    print("TOTAL destructive-write candidates: %d" % total)
    print("\nConfirm every hit by reading its context before believing it, and note")
    print("that DX-addressed I/O (EC-EF) cannot be resolved statically - this tool")
    print("only sees immediate-port instructions.")
    return 1 if total else 0

if __name__ == "__main__":
    sys.exit(main())
