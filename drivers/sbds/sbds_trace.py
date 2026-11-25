#!/usr/bin/env python3
"""Decode SBDS.VXD's call trace from a REGEDIT4 export of HKLM\\Software\\SBDS.

  regedit /e C:\\SBDS.REG HKEY_LOCAL_MACHINE\\Software\\SBDS     (in the guest)
  python drivers/sbds/sbds_trace.py SBDS.REG

The value is dwTraceN then a ring of entries (id, a1, a2, result), its size
taken from the data. A -Trace build rewrites it on every call except
GetPosition and the reference counts. Channel entries carry the channel number
in bits 8-15 of the id.
"""
import re
import struct
import sys

NAMES = {1: 'Drv.QueryInterface', 2: 'Drv.AddRef', 3: 'Drv.Release', 4: 'Drv.GetDriverDesc',
         5: 'Drv.Open', 6: 'Drv.Close', 7: 'Drv.GetCaps', 8: 'Drv.CreateSoundBuffer',
         9: 'Drv.DuplicateSoundBuffer', 10: 'Buf.QueryInterface', 11: 'Buf.AddRef',
         12: 'Buf.Release', 13: 'Buf.Lock', 14: 'Buf.Unlock', 15: 'Buf.SetFormat',
         16: 'Buf.SetFrequency', 17: 'Buf.SetVolumePan', 18: 'Buf.SetPosition',
         19: 'Buf.GetPosition', 20: 'Buf.Play', 21: 'Buf.Stop',
         # 22-28: the diagnostic build kept beside its captures,
         # docs/captures/2026-11-24_sbds_dxdiag/SBDS_diag.ASM
         22: 'Probe', 23: 'DS.Set_Async_Time_Out', 24: 'DS.Set_Global_Time_Out',
         25: 'DS.ioctl', 26: 'DS.ioctl total',
         27: 'PAGEFILE version', 28: 'VMCPD version',
         # 30-41: channel (hardware secondary) methods, decoded as 10-21
         42: 'Ch.Created', 43: 'Ch.Duplicated'}
for _i in range(30, 42):
    NAMES[_i] = 'Ch.' + NAMES[_i - 20].split('.', 1)[1]
RESULTS = {0: 'ok', 0x80004001: 'UNSUPPORTED', 0x80004002: 'NOINTERFACE',
           0x8878000A: 'ALLOCATED', 0x8878001E: 'CONTROLUNAVAIL', 0x88780064: 'BADFORMAT',
           0x88780078: 'NODRIVER', 0x88780032: 'INVALIDCALL', 0x80070057: 'INVALIDPARAM'}


def fmt(v):
    if v == 0:
        return 'no format'
    return '%d Hz %d-bit %s' % (v & 0xFFFF, (v >> 16) & 0xFF, {1: 'mono', 2: 'stereo'}.get(v >> 24, '%d ch' % (v >> 24)))


T0 = [None]


def at(ms):
    return '  t+%d ms' % (ms - T0[0]) if T0[0] is not None else ''


def describe(i, a1, a2, r):
    res = RESULTS.get(r, '%08X' % r)
    if i == 42:
        return 'memory %08X (%s), %d bytes' % (a1, 'shared arena' if a1 >= 0x80000000 else 'PRIVATE', a2)
    if i == 43:
        return 'from ch %d, memory %08X' % (a2, a1)
    if 30 <= i <= 41:
        i -= 20
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
    if i == 22:
        return 'time-outs set system-wide %d, our ticks %d%s' % (a1, r, at(a2))
    if i == 27:
        return 'EAX %08X ECX %08X BL %d' % (a1, a2, r)
    if i == 28:
        return 'EAX %08X (AH %d AL %d) ECX %08X (coprocessor type)' % (a1, (a1>>8)&0xFF, a1&0xFF, a2)
    if i == 25:
        return 'code %d (call %d)%s' % (a1, r, at(a2))
    if i == 26:
        return 'code %d x %d' % (a1, r)
    if i in (23, 24):
        return '%d ms  from +%X  callback +%X%s' % (a1, r & 0xFFFF, r >> 16, at(a2))
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
    N = (len(data) - 4) // 16
    first = max(0, total - N)
    print('%d calls traced; showing %d' % (total, total - first))
    for k in range(first, total):
        raw, a1, a2, r = struct.unpack_from('<4I', data, 4 + (k % N) * 16)
        i, ch = raw & 0xFF, raw >> 8
        name = NAMES.get(i, 'id %d' % i) + (' %d' % ch if i >= 30 else '')
        print('%5d  %-26s %s' % (k, name, describe(i, a1, a2, r)))


if __name__ == '__main__':
    main()
