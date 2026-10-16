#!/usr/bin/env python3
"""Charge M8DISP's bus traffic to the GDI entry that caused it.

Reads an 86Box log taken with MACH8_WLOG=1 while a -Trace build of M8DISP.DRV
ran. The driver writes B0xx to SCRATCH_PAD_1 (56EEh) as each screen entry
starts (m8.h, M8_ENTRY); every engine command after it is charged to that
entry until the next marker. BitBlt adds Cksr: k = source kind (0 none,
1 screen, 2 DIB Engine bitmap, 3 plain BITMAP), rr = ROP3 byte. A pattern
BitBlt adds Dffs: ff = brush flags, s = brush style (listed, not charged).

  python tools/m8_fallback_tally.py <86box.log>

Bus estimates use the card's measured costs: 3.84 us per pixel for a host
write (M8TIME raw), 5.7 us per pixel for a read-back (two word reads per two
pixels: data-ready poll and PIX_TRANS, ~5.7 us per byte-wide access).
Every engine operation also costs ~35 us of register set-up (US_OP).
Engine text (Mach8 extended blit, started by a DEST_Y_END write) is charged
one 16-bit write per two bytes of mono data, 7.6 us each.
pclog folds identical consecutive lines into "*** N repeats ***"; those are
expanded (inboard-hw-debug technique 148).
"""
import re
import sys

NAMES = {
    0xB000: 'BitBlt', 0xB001: 'Output', 0xB002: 'Pixel', 0xB003: 'StrBlt',
    0xB004: 'ExtTextOut', 0xB005: 'ScanLR', 0xB006: 'StretchBlt',
    0xB007: 'StretchDIBits', 0xB008: 'DibToDevice', 0xB009: 'FastBorder',
    0xB00A: 'UpdateColors', 0xB00E: 'BitBlt screen->memory',
    0xB0C0: 'cursor move', 0xB0C1: 'cursor shape', 0xB0E0: 'ExtTextOut fallback', 0xB0D0: 'SaveScreenBitmap save', 0xB0D1: 'SaveScreenBitmap restore',
}
US_WRITE = 3.84
US_READ = 5.7
US_WORD = 7.6     # one 16-bit write, M8TIME raw
US_OP = 35.0      # register set-up per engine operation (~6 writes at ~5.7 us)


def lines(path):
    prev = None
    for raw in open(path, errors='replace'):
        m = re.match(r'\*\*\* (\d+) repeats \*\*\*', raw.strip())
        if m:
            if prev is not None:
                for _ in range(int(m.group(1))):
                    yield prev
            continue
        if raw.startswith('M8W '):
            prev = raw
            yield raw
        else:
            prev = None


def main(path):
    reg = {}
    who = 'before any marker'
    rows = {}
    brushes = {}

    def row(name):
        return rows.setdefault(name, {'calls': 0, 'fill': 0, 'copy': 0, 'mono': 0, 'mono_w': 0,
                                      'rd_ops': 0, 'rd_px': 0, 'wr_ops': 0, 'wr_px': 0})

    for l in lines(path):
        port, val = l.split()[1], int(l.split()[2], 16)
        if port == '56EE' and (val & 0xFF00) == 0xB000:
            who = NAMES.get(val, hex(val))
            row(who)['calls'] += 1
            continue
        if port == '56EE' and (val & 0xF000) == 0xD000:
            key = f'flags {(val >> 4) & 0xFF:02X} style {val & 0xF}'
            brushes[key] = brushes.get(key, 0) + 1
            continue
        if port == '56EE' and (val & 0xF000) == 0xC000:
            kind = ('none', 'screen', 'DIB', 'BITMAP')[(val >> 8) & 3]
            who = f'BitBlt {kind} rop {val & 0xFF:02X}'
            row(who)['calls'] += 1
            continue
        if port in ('96E8', '86E8', '82E8', 'A6EE', 'AAEE'):
            reg[port] = val
        elif port == 'BEE8' and val < 0x1000:
            reg['min'] = val
        elif port == 'AEEE':
            r = row(who)
            nbytes = (reg.get('AAEE', 0) - reg.get('A6EE', 0)) // 8 * (val - reg.get('82E8', 0))
            r['mono'] += 1
            r['mono_w'] += (max(nbytes, 0) + 1) // 2
        elif port == '9AE8':
            px = (reg.get('96E8', 0) + 1) * (reg.get('min', 0) + 1)
            r = row(who)
            if val == 0x40F3:
                r['fill'] += 1
            elif (val & 0xFF00) == 0xC000:
                r['copy'] += 1
            elif val == 0x53B0:
                r['rd_ops'] += 1
                r['rd_px'] += px
            elif val == 0x53B1:
                r['wr_ops'] += 1
                r['wr_px'] += px

    total = 0.0
    out = []
    for name, r in rows.items():
        ops = r['fill'] + r['copy'] + r['mono'] + r['rd_ops'] + r['wr_ops']
        ms = (r['rd_px'] * US_READ + r['wr_px'] * US_WRITE + r['mono_w'] * US_WORD + ops * US_OP) / 1000.0
        total += ms
        out.append((ms, name, r))
    print(f"{'entry':24} {'calls':>6} {'fills':>6} {'copies':>6} {'mono':>6} {'reads':>6} {'read px':>9} "
          f"{'writes':>6} {'write px':>9} {'bus ms':>8}")
    for ms, name, r in sorted(out, key=lambda x: -x[0]):
        print(f"{name:24} {r['calls']:6} {r['fill']:6} {r['copy']:6} {r['mono']:6} {r['rd_ops']:6} {r['rd_px']:9} "
              f"{r['wr_ops']:6} {r['wr_px']:9} {ms:8.0f}")
    for key, n in sorted(brushes.items(), key=lambda x: -x[1]):
        print(f'brush {key}: {n}')
    print(f"{'total':24} {'':6} {'':6} {'':6} {'':6} {'':6} {'':9} {'':6} {'':9} {total:8.0f}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
