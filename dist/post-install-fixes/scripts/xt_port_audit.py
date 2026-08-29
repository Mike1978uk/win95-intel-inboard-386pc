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
import argparse
import re, os, sys

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

def text_range(d):
    """(lo, hi) of the executable text, or the whole file if not a PE."""
    lo, hi = 0, len(d)
    if d[:2] == b'MZ':
        import struct
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        if d[pe:pe + 2] == b'PE':
            nsec = struct.unpack_from('<H', d, pe + 6)[0]
            optsz = struct.unpack_from('<H', d, pe + 20)[0]
            off = pe + 24 + optsz
            for _ in range(nsec):
                nm = d[off:off + 8].rstrip(b'\x00')
                _vs, _va, rs, ro = struct.unpack_from('<IIII', d, off + 8)
                if nm == b'.text':
                    lo, hi = ro, ro + rs
                off += 40
    return lo, hi


def report_dx(path):
    """Resolve DX-addressed I/O whose port comes from an immediate.

    This tool used to declare all EC-EF I/O unresolvable. That is only true in
    general: `mov edx, imm32` (BA imm32) followed by an I/O opcode IS decidable.
    On SD120PPD.MPD that pattern hid 13 writes to port 0x94 the immediate-port
    scan never saw, and the gap survived two rounds of patching (issue #22).

    Also applies the XT wrap: the 5160's I/O channel carries only A0-A9, so the
    effective port is `value & 0x3FF`. A driver writing to a PS/2 or ECP
    register above 0x3FF can land on the system board without naming one.

    Decodes forward from each load with capstone, stopping when EDX is
    reassigned or control flow leaves the straight line. A first version just
    byte-scanned for the next I/O opcode within a window; on an ALREADY-PATCHED
    file it walked straight past the NOP and reported an unrelated later byte
    as a surviving write. Three false positives, and they looked authoritative.
    Without capstone this falls back to that scan and says so.
    """
    data = open(path, 'rb').read()
    lo, hi = text_range(data)
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        exact = True
    except ImportError:
        md = None
        exact = False

    STOP_MN = ('mov', 'xchg', 'pop', 'add', 'sub', 'inc', 'dec',
               'and', 'or', 'xor', 'lea', 'movzx', 'movsx')
    hits = {}
    for m in re.finditer(b'\xba(....)', data[lo:hi], re.S):
        off = lo + m.start()
        val = int.from_bytes(m.group(1), 'little')
        if val > 0xFFFF:
            continue
        key = lambda k: (val, val & 0x3FF, k)
        if md is None:
            for b in data[off + 5:off + 45]:
                if b in (0xEC, 0xED, 0xEE, 0xEF):
                    k = {0xEC: 'in  al,dx', 0xED: 'in  eax,dx',
                         0xEE: 'out dx,al', 0xEF: 'out dx,eax'}[b]
                    hits.setdefault(key(k), []).append(off)
                    break
            continue
        start = off + 5
        for i in md.disasm(data[start:start + 120], start):
            mn, ops = i.mnemonic, i.op_str
            if mn == 'out' and ops.startswith('dx,'):
                hits.setdefault(key('out dx,al'), []).append(i.address)
                continue
            if mn == 'in' and ', dx' in ops:
                hits.setdefault(key('in  al,dx'), []).append(i.address)
                continue
            if mn[:1] == 'j' or mn in ('call', 'ret', 'retf', 'jmp', 'loop', 'int'):
                break
            if mn in STOP_MN and re.match(r'\s*(edx|dx|dl|dh)\b', ops):
                break

    print('')
    print('  DX-addressed I/O with a resolvable immediate port%s:'
          % ('' if exact else '   [HEURISTIC - capstone not installed]'))
    if not hits:
        print('    none found')
        return 0
    nw = 0
    for (val, eff, kind), offs in sorted(hits.items()):
        note = ''
        if eff <= 0xFF:
            note = '   <<== system board: %s' % classify(eff)
            if kind.startswith('out'):
                nw += len(offs)
        wrap = ' (wraps from %#06x)' % val if eff != val else ''
        print('    port %#06x -> %#05x%s  %-11s x%d%s'
              % (val, eff, wrap, kind, len(offs), note))
    if nw:
        print('')
        print('    %d DX-addressed WRITE(s) land on the system board.' % nw)
    return nw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="also list reads and low-confidence candidates")
    a = ap.parse_args()
    total = sum(scan(f, a.verbose) + report_dx(f) for f in a.files)
    print("=" * 74)
    print("TOTAL destructive-write candidates: %d" % total)
    print("\nConfirm every hit by reading its context before believing it, and note")
    print("DX-addressed I/O is resolved where the port comes from an immediate; it")
    print("still cannot be resolved when the port is computed at runtime.")
    return 1 if total else 0

if __name__ == "__main__":
    sys.exit(main())
