#!/usr/bin/env python3
"""Decode SBDS.VXD's call trace from a REGEDIT4 export of HKLM\\Software\\SBDS.

  regedit /e C:\\SBDS.REG HKEY_LOCAL_MACHINE\\Software\\SBDS     (in the guest)
  python drivers/sbds/sbds_trace.py SBDS.REG

The value is dwTraceN then a ring of 128 entries (id, a1, a2, result); the
driver writes it when DirectSound closes the device.
"""
import re
import struct
import sys

NAMES = {1: 'Drv.QueryInterface', 2: 'Drv.AddRef', 3: 'Drv.Release', 4: 'Drv.GetDriverDesc',
         5: 'Drv.Open', 6: 'Drv.Close', 7: 'Drv.GetCaps', 8: 'Drv.CreateSoundBuffer',
         9: 'Drv.DuplicateSoundBuffer', 10: 'Buf.QueryInterface', 11: 'Buf.AddRef',
         12: 'Buf.Release', 13: 'Buf.Lock', 14: 'Buf.Unlock', 15: 'Buf.SetFormat',
         16: 'Buf.SetFrequency', 17: 'Buf.SetVolumePan', 18: 'Buf.SetPosition',
         19: 'Buf.GetPosition', 20: 'Buf.Play', 21: 'Buf.Stop'}
RESULTS = {0: 'ok', 0x80004001: 'UNSUPPORTED', 0x80004002: 'NOINTERFACE',
           0x8878000A: 'ALLOCATED', 0x8878001E: 'CONTROLUNAVAIL', 0x88780064: 'BADFORMAT',
           0x88780078: 'NODRIVER'}
N = 128


def fmt(v):
    if v == 0:
        return 'no format'
    return '%d Hz %d-bit %s' % (v & 0xFFFF, (v >> 16) & 0xFF, {1: 'mono', 2: 'stereo'}.get(v >> 24, '%d ch' % (v >> 24)))


T0 = [None]


def at(ms):
    return '  t+%d ms' % (ms - T0[0]) if T0[0] is not None else ''


def describe(i, a1, a2, r):
    res = RESULTS.get(r, '%08X' % r)
    if i == 20:
        T0[0] = a2
    if i in (1, 10):
        return 'IID %08X -> %s' % (a1, res)
    if i == 8:
        return '%s, flags %08X -> %s' % (fmt(a1), a2, res)
    if i == 15:
        return '%s -> %s' % (fmt(a1), res)
    if i == 16:
        return '%d Hz -> %s' % (a1, res)
    if i == 17:
        return 'amp L %08X R %08X -> %s' % (a1, a2, res)
    if i == 19:
        return 'play %d%s' % (a1, at(a2))
    if i == 20:
        return 'flags %08X -> %s%s' % (a1, res, at(a2))
    if i == 21:
        return res + at(a2)
    if i in (2, 3, 11, 12):
        return 'count %d' % r
    return res


def main():
    text = open(sys.argv[1], 'rb').read().decode('latin-1').replace('\\\r\n', '').replace('\\\n', '')
    m = re.search(r'"Trace"=hex:([0-9a-fA-F,\s]+)', text)
    if not m:
        raise SystemExit('no "Trace" value in the export')
    data = bytes(int(x, 16) for x in m.group(1).replace(' ', '').split(',') if x)
    total = struct.unpack_from('<I', data, 0)[0]
    first = max(0, total - N)
    print('%d calls traced; showing %d' % (total, total - first))
    for k in range(first, total):
        i, a1, a2, r = struct.unpack_from('<4I', data, 4 + (k % N) * 16)
        print('%5d  %-26s %s' % (k, NAMES.get(i, 'id %d' % i), describe(i, a1, a2, r)))


if __name__ == '__main__':
    main()
