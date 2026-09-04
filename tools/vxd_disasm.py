"""Disassemble a Win9x VxD/PDR region, handling the two things that desync a
naive linear pass:

  * a VxD service call is CD 20 followed by an INLINE 4-byte service id
    (low word = ordinal, high word = device), so it is 6 bytes, not 2;
  * LE objects are page-mapped, so file offset != linked address.

  python vxddis.py <file> <file_offset> <count> [--bytes]
"""
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs

DEV = {0x0001: 'VMM', 0x0003: 'DEV0003', 0x0004: 'VDMAD', 0x0005: 'VTD',
       0x0010: 'IOS', 0x0017: 'SHELL', 0x0021: 'VXDLDR', 0x0027: 'IFSMGR',
       0x0033: 'DEV0033'}


def disasm(data, base, start, count, show_bytes=False):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False
    off = start
    n = 0
    while n < count and off < len(data):
        if data[off] == 0xCD and off + 6 <= len(data) and data[off + 1] == 0x20:
            svc, dev = struct.unpack_from('<HH', data, off + 2)
            print('%08X  %-22s VxDCall %s service %04X'
                  % (base + off, 'CD 20 ...' if show_bytes else '',
                     DEV.get(dev, '%04X' % dev), svc))
            off += 6
            n += 1
            continue
        chunk = data[off:off + 16]
        got = list(md.disasm(chunk, base + off))
        if not got:
            print('%08X  db %02X' % (base + off, data[off]))
            off += 1
            n += 1
            continue
        i = got[0]
        bs = ' '.join('%02X' % b for b in i.bytes) if show_bytes else ''
        print('%08X  %-22s %s %s' % (i.address, bs, i.mnemonic, i.op_str))
        off += i.size
        n += 1


if __name__ == '__main__':
    path = sys.argv[1]
    start = int(sys.argv[2], 0)
    count = int(sys.argv[3], 0)
    sb = '--bytes' in sys.argv
    d = open(path, 'rb').read()
    disasm(d, 0, start, count, sb)
