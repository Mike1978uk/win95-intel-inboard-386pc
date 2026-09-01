#!/usr/bin/env python3
"""Dump the LE structures of a Win9x VxD/PDR: header, objects, page map,
entry table, resident/non-resident names, fixup page table and fixup records.

Written because every comparable LE *header* field of our PORT.PDR already
matched a driver that loads, so the fault had to be below the header.

  python tools/ledump.py <file> [--pages] [--fixups]
"""
import sys, struct

HDR = [
 (0x00, '2s', 'sig'), (0x02, 'B', 'byteorder'), (0x03, 'B', 'wordorder'), (0x04, 'I', 'level'),
 (0x08, 'H', 'cpu'), (0x0A, 'H', 'os'), (0x0C, 'I', 'modver'), (0x10, 'I', 'modflags'),
 (0x14, 'I', 'numpages'), (0x18, 'I', 'eip_obj'), (0x1C, 'I', 'eip'), (0x20, 'I', 'esp_obj'),
 (0x24, 'I', 'esp'), (0x28, 'I', 'pagesize'), (0x2C, 'I', 'lastpagesize'),
 (0x30, 'I', 'fixupsecsize'), (0x34, 'I', 'fixupsecchk'), (0x38, 'I', 'loadersecsize'),
 (0x3C, 'I', 'loadersecchk'), (0x40, 'I', 'objtab'), (0x44, 'I', 'numobj'),
 (0x48, 'I', 'objpagemap'), (0x4C, 'I', 'objiter'), (0x50, 'I', 'restab'), (0x54, 'I', 'numres'),
 (0x58, 'I', 'resnametab'), (0x5C, 'I', 'entrytab'), (0x60, 'I', 'moddir'), (0x64, 'I', 'nummoddir'),
 (0x68, 'I', 'fixuppagetab'), (0x6C, 'I', 'fixuprectab'), (0x70, 'I', 'impmodtab'),
 (0x74, 'I', 'numimpmod'), (0x78, 'I', 'impproctab'), (0x7C, 'I', 'perpagechk'),
 (0x80, 'I', 'datapages'), (0x84, 'I', 'numpreload'), (0x88, 'I', 'nonresnametab'),
 (0x8C, 'I', 'nonresnamelen'), (0x90, 'I', 'nonresnamechk'), (0x94, 'I', 'autods'),
 (0x98, 'I', 'debuginfo'), (0x9C, 'I', 'debuglen'), (0xA0, 'I', 'numinstpre'),
 (0xA4, 'I', 'numinstdem'), (0xA8, 'I', 'heapsize'),
]

OBJF = [(0x0001, 'READ'), (0x0002, 'WRITE'), (0x0004, 'EXEC'), (0x0008, 'RESOURCE'),
        (0x0010, 'DISCARD'), (0x0020, 'SHARED'), (0x0040, 'PRELOAD'), (0x0080, 'INVALID'),
        (0x0100, 'ZEROFILL'), (0x0200, 'RESIDENT'), (0x0400, 'CONTIGUOUS'), (0x0800, 'LOCKABLE'),
        (0x1000, 'ALIAS16'), (0x2000, 'BIG'), (0x4000, 'CONFORM'), (0x8000, 'IOPL')]

PAGEF = {0: 'LEGAL', 1: 'ITER', 2: 'INVALID', 3: 'ZEROFILL', 4: 'RANGE'}


def flags(v, table):
    return '|'.join(n for m, n in table if v & m) or '-'


def pstr(d, o):
    n = d[o]
    return d[o + 1:o + 1 + n].decode('latin1'), o + 1 + n


def main():
    path = sys.argv[1]
    want_pages = '--pages' in sys.argv
    want_fix = '--fixups' in sys.argv
    d = open(path, 'rb').read()
    le = struct.unpack_from('<I', d, 0x3c)[0] if d[:2] == b'MZ' else 0
    h = {}
    for off, fmt, name in HDR:
        h[name] = struct.unpack_from('<' + fmt, d, le + off)[0]
    print('== %s  (%d bytes, LE @ 0x%x) ==' % (path, len(d), le))
    for off, fmt, name in HDR:
        v = h[name]
        print('  %-14s %s' % (name, v if isinstance(v, bytes) else '0x%08X (%d)' % (v, v)))

    print('')
    print('-- objects (%d) --' % h['numobj'])
    for i in range(h['numobj']):
        o = le + h['objtab'] + i * 24
        vsize, base, of, pmi, pmc, _r = struct.unpack_from('<6I', d, o)
        print('  #%d vsize=0x%06X base=0x%08X flags=0x%04X %-38s pageidx=%d count=%d'
              % (i + 1, vsize, base, of, flags(of, OBJF), pmi, pmc))

    print('')
    print('-- object page map (%d pages) --' % h['numpages'])
    pm = []
    for i in range(h['numpages']):
        o = le + h['objpagemap'] + i * 4
        hi, lo, fl = struct.unpack_from('>HBB', d, o)
        pm.append((hi << 8 | lo, fl))
    if want_pages:
        for i, (n, fl) in enumerate(pm):
            print('  page %3d -> phys %3d  flags %d %s' % (i + 1, n, fl, PAGEF.get(fl, '?')))
    else:
        seq = all(n == i + 1 for i, (n, fl) in enumerate(pm))
        fls = sorted(set(fl for _, fl in pm))
        print('  physical numbers sequential 1..N: %s ; flag values: %s'
              % (seq, [PAGEF.get(f, f) for f in fls]))

    print('')
    print('-- resident name table @0x%x --' % h['resnametab'])
    o = le + h['resnametab']
    while d[o]:
        name, o = pstr(d, o)
        ordv = struct.unpack_from('<H', d, o)[0]
        o += 2
        print('  %-20s ord %d' % (name, ordv))

    print('')
    print('-- entry table @0x%x --' % h['entrytab'])
    o = le + h['entrytab']
    ordinal = 1
    while d[o]:
        cnt = d[o]
        typ = d[o + 1]
        o += 2
        if typ == 0:
            print('  %d unused entries' % cnt)
            ordinal += cnt
            continue
        objn = struct.unpack_from('<H', d, o)[0]
        o += 2
        for _ in range(cnt):
            fl = d[o]
            o += 1
            if typ == 1:
                off32 = struct.unpack_from('<H', d, o)[0]
                o += 2
            elif typ == 3:
                off32 = struct.unpack_from('<I', d, o)[0]
                o += 4
            else:
                print('  unhandled bundle type %d' % typ)
                return
            print('  ord %d: obj %d off 0x%X flags 0x%02X %s'
                  % (ordinal, objn, off32, fl, 'EXPORTED' if fl & 1 else ''))
            ordinal += 1

    print('')
    print('-- non-resident name table @0x%x len %d --' % (h['nonresnametab'], h['nonresnamelen']))
    if h['nonresnamelen']:
        o = h['nonresnametab']            # from top of FILE, not the LE header
        while d[o]:
            name, o = pstr(d, o)
            ordv = struct.unpack_from('<H', d, o)[0]
            o += 2
            print('  %-20s ord %d' % (name, ordv))

    print('')
    print('-- imported module name table @0x%x (%d) --' % (h['impmodtab'], h['numimpmod']))
    o = le + h['impmodtab']
    for _ in range(h['numimpmod']):
        name, o = pstr(d, o)
        print('  %s' % name)

    print('')
    print('-- fixup page table @0x%x --' % h['fixuppagetab'])
    fpt = [struct.unpack_from('<I', d, le + h['fixuppagetab'] + i * 4)[0]
           for i in range(h['numpages'] + 1)]
    nz = [(i + 1, fpt[i + 1] - fpt[i]) for i in range(h['numpages']) if fpt[i + 1] != fpt[i]]
    print('  total fixup record bytes: %d ; pages with fixups: %d of %d'
          % (fpt[-1], len(nz), h['numpages']))
    print('  ' + ', '.join('p%d:%dB' % x for x in nz))
    if want_fix:
        print('  fixup record table head: '
              + d[le + h['fixuprectab']:le + h['fixuprectab'] + 64].hex())

    print('')
    print('-- geometry --')
    print('  data pages start 0x%08X ; file size 0x%X ; numpages*pagesize = 0x%X'
          % (h['datapages'], len(d), h['numpages'] * h['pagesize']))
    print('  last page size %d ; implied end 0x%X'
          % (h['lastpagesize'],
             h['datapages'] + (h['numpages'] - 1) * h['pagesize'] + h['lastpagesize']))


main()
