"""Time spent inside the Mach8 driver's entry points, from a trace build's
A0F2/A0F3 records (PIT channel 2 count, 2 bytes, then the BIOS tick's low byte).

Usage: m8_timing.py <log> [--drag]
--drag limits the totals to the span from the first to the last screen-to-memory
BitBlt (B00E), which is Solitaire's drag when that is all the run did.
Times are wall-clock ms inside the call; interrupts (cursor moves) count in.
"""
import os
import sys
import importlib.util

_s = importlib.util.spec_from_file_location(
    'tally', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'm8_fallback_tally.py'))
tally = importlib.util.module_from_spec(_s)
_s.loader.exec_module(tally)

PIT_HZ = 1193182.0
NAMES = dict(tally.NAMES)
ASYNC = (0xB0C0, 0xB0C1)             # cursor markers, raised at interrupt time


def counts(c1, t1, c2, t2):
    """Counter ticks between two readings: the counter runs down and wraps every
    65536; the BIOS tick (about the same period) says how many wraps."""
    fine = (c1 - c2) & 0xFFFF
    wraps = round((((t2 - t1) & 0xFF) * 65536 - fine) / 65536.0)
    return max(wraps, 0) * 65536 + fine


def main(path, drag):
    rows = {}
    pending = None          # (mark, [values])
    skip = 0
    t_in = None             # (count, tick, position)
    who = None
    pairs = []              # (start position, name, counts)
    pos = 0
    scr2mem = []
    readings = []           # (position, count, tick): every timer reading
    for l in tally.lines(path):
        p = l.split()
        if p[1] != '56EE':
            continue
        v = int(p[2], 16)
        pos += 1
        if skip:
            skip -= 1
            continue
        if v in (0xA100, 0xA101):
            skip = 2
            continue
        if pending is not None:
            if v > 0xFF:
                continue
            pending[1].append(v)
            if len(pending[1]) == 3:
                c = (pending[1][0] << 8) | pending[1][1]
                tick = pending[1][2]
                readings.append((pos, c, tick))
                if pending[0] == 0xA0F2:
                    t_in = (c, tick, pos)
                    who = None
                elif t_in is not None:
                    pairs.append((t_in[2], who or 'unmarked', counts(t_in[0], t_in[1], c, tick)))
                    t_in = None
                pending = None
            continue
        if v in (0xA0F2, 0xA0F3):
            pending = (v, [])
            continue
        if t_in is None or who is not None or v in ASYNC:
            if v == 0xB00E:
                scr2mem.append(pos)
            continue
        if (v & 0xFF00) == 0xB000:
            who = NAMES.get(v, hex(v))
            if v == 0xB00E:
                scr2mem.append(pos)
        elif (v & 0xF000) == 0xC000:
            who = f"BitBlt {('none', 'screen', 'DIB', 'BITMAP')[(v >> 8) & 3]} rop {v & 0xFF:02X}"
        elif v == 0xA0FC:
            who = 'BitBlt memory->memory'
    lo, hi = (scr2mem[0] - 50, scr2mem[-1] + 50) if drag and scr2mem else (0, pos)
    pairs = [x for x in pairs if lo <= x[0] <= hi]
    span = [r for r in readings if lo <= r[0] <= hi]
    wall = sum(counts(a[1], a[2], b[1], b[2]) for a, b in zip(span, span[1:]))
    for _, name, n in pairs:
        r = rows.setdefault(name, [0, 0, 0])
        r[0] += 1
        r[1] += n
        r[2] = max(r[2], n)
    total = sum(r[1] for r in rows.values())
    print(f"{'entry':32} {'calls':>7} {'ms':>9} {'ms/call':>8} {'max ms':>8}")
    for name, r in sorted(rows.items(), key=lambda x: -x[1][1]):
        print(f'{name:32} {r[0]:7} {r[1] / PIT_HZ * 1000:9.1f} {r[1] / r[0] / PIT_HZ * 1000:8.3f} '
              f'{r[2] / PIT_HZ * 1000:8.2f}')
    print(f"{'inside the driver':32} {'':7} {total / PIT_HZ * 1000:9.1f}")
    if wall:
        print(f"{'wall clock, same span':32} {'':7} {wall / PIT_HZ * 1000:9.1f}   "
              f"driver share {100.0 * total / wall:.0f}%")
    if drag and scr2mem:
        print(f'drag frames (screen->memory saves): {len(scr2mem)}; per frame: driver '
              f'{total / PIT_HZ * 1000 / len(scr2mem):.1f} ms, wall {wall / PIT_HZ * 1000 / len(scr2mem):.1f} ms')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1], '--drag' in sys.argv[2:])
