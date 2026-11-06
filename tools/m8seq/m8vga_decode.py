#!/usr/bin/env python3
"""Decode the probes' .BIN files: M8GRAD, M8DACX, M8BNDRY, M8LATCH, M8VFLIP, M8W128.

  python m8vga_decode.py <folder holding the .BIN files>

Prints what each one found and what the bytes mean. A probe whose control arm fails says so.
"""
import os
import struct
import sys

US = 1000000.0 / 1193182.0


def load(folder, name, magic):
    path = os.path.join(folder, name)
    if not os.path.exists(path):
        print(f'{name}: not there')
        return None
    data = open(path, 'rb').read()
    if data[:4] != magic:
        print(f'{name}: bad header {data[:4]!r}')
        return None
    return data[4:]


def dacx(d):
    want = [(0x11, 0x22, 0x33), (0x14, 0x25, 0x36), (1, 2, 3), (0x2A, 0x15, 0x0B)]
    got = [tuple(d[i:i + 3]) for i in range(0, 12, 3)]
    for w, g, label in zip(want, got, ('VGA>8514', 'VGA>8514', '8514>VGA', '8514>VGA')):
        print(f'  {label}: wrote {w} read {g} {"same" if w == g else "DIFFERENT"}')
    print('  one shared table' if want == got else '  NOT shown to be one table')


def bndry(d):
    arms = [struct.unpack_from('<HBBBB', d, i * 6) for i in range(3)]
    want = [(0x21, 0x22), (0x43, 0x44), (0x65, 0x66)]
    for n, (a, w) in enumerate(zip(arms, want)):
        print(f'  arm {n}: 42EE reads {a[0]:04x}  A000:0 {a[1]:02x}->{a[4]:02x}  '
              f'row100 {a[2]:02x} (wrote {w[0]:02x})  row300 {a[3]:02x} (wrote {w[1]:02x})')
    ctl = all((arms[n][2], arms[n][3]) == want[n] for n in (0, 2))
    if not ctl:
        print('  CONTROL FAILED: engine readback is not trustworthy, no conclusion')
        return
    acted = (arms[1][2], arms[1][3]) != want[1] or arms[1][1] != arms[1][4]
    print('  the register ACTS on this card' if acted else '  no effect seen on the engine or on A000:0')


def latch(d):
    w = struct.unpack('<13H', d[:26])
    mode, fill, lat, plane, ram, m0, m1 = w[0], w[1], w[2], w[3:7], w[7:11], w[11], w[12]
    plane_ms, ram_ms = sum(plane) * US / 1000, sum(ram) * US / 1000
    print(f'  mode {mode:02x}; 16000 pixels each:')
    print(f'  plane fill {fill * US / 1000:8.2f} ms')
    print(f'  latch copy {lat * US / 1000:8.2f} ms')
    print(f'  CPU VGA>VGA{plane_ms:8.2f} ms')
    print(f'  CPU RAM>VGA{ram_ms:8.2f} ms')
    print(f'  mismatches before {m0} (must be high), after {m1} (must be 0)')
    if m0 == 0 or m1 != 0:
        print('  CHECK FAILED: the timings are not of a working copy')
    else:
        print(f'  latch copy is {plane_ms / (lat * US / 1000):.1f}x cheaper than CPU VGA>VGA, '
              f'{ram_ms / (lat * US / 1000):.1f}x than CPU RAM>VGA')


def vflip(d):
    ticks, tout = struct.unpack('<HH', d[:4])
    ms = ticks * 54.925
    print(f'  360 frames in {ticks} BIOS ticks: about {360 / (ms / 1000):.1f} Hz' if ticks else '  no ticks counted')
    print(f'  retrace timeouts {tout} (0 expected). The answer is what the owner saw.')


def w128(d):
    bd, flip = d[0], d[1]
    a0, a1 = d[2:5], d[5:8]
    print(f'  BDh as found {bd:02x}; flipped reads {flip:02x}')
    for label, a in (('as found', a0), ('bit 2 flipped', a1)):
        kind = 'mirror' if a[0] == a[1] else ('128 KB, linear' if a[2] == 0x22 else 'distinct, not the next bank')
        print(f'  {label}: A000 {a[0]:02x} B000 {a[1]:02x} bank1 {a[2]:02x}: {kind}')


def grad(d):
    tout, rows = d[:5], d[5:]
    names = ('2213 control FRGD', '6213 VRAM colour', 'A213 colour pattern', 'E213 source 7', '6271 plane 7 mask')
    for c, name in enumerate(names):
        print(f'  case {c} {name}{"  NEVER WENT IDLE" if tout[c] else ""}')
        for r in range(2):
            row = rows[(2 * c + r) * 64:(2 * c + r + 1) * 64]
            print('    ' + ' '.join(f'{b:02x}' for b in row[:64]))
    ctl = rows[:64]
    lit = [x for x, b in enumerate(ctl) if b == 0x77]
    if not lit or tout[0] or any(b not in (0, 0x77) for b in ctl):
        print('  CONTROL FAILED: read the other cases with care')
    else:
        print(f'  control fill spans x {lit[0]}-{lit[-1]}')


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else '.'
    for name, magic, fn in (('M8GRAD.BIN', b'M8GR', grad),
                            ('M8DACX.BIN', b'M8DX', dacx), ('M8BNDRY.BIN', b'M8BD', bndry),
                            ('M8LATCH.BIN', b'M8LA', latch), ('M8VFLIP.BIN', b'M8VF', vflip),
                            ('M8W128.BIN', b'M8WT', w128)):
        d = load(folder, name, magic)
        if d is not None:
            print(name)
            fn(d)


if __name__ == '__main__':
    main()
