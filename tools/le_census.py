#!/usr/bin/env python3
"""List LE/LX objects of VxD/MPD/PDR/386 files with page usage.

A resident object is charged whole 4 KB pages, so a few bytes over a
boundary cost a full page of locked RAM.

usage: le_census.py [--tsv] FILE...
"""
import struct, sys, hashlib

PAGE = 4096
FLAGS = [(0x1, 'R'), (0x2, 'W'), (0x4, 'X'), (0x8, 'res'), (0x10, 'DISC'),
         (0x20, 'shr'), (0x40, 'PRELOAD'), (0x80, 'inval'), (0x100, 'zero'),
         (0x200, 'RESIDENT'), (0x400, 'contig'), (0x800, 'LOCK'),
         (0x2000, 'alias16'), (0x4000, 'big'), (0x8000, 'conf'),
         (0x10000, 'iopl')]


def decode(f, pe=False):
    if pe:
        return ','.join(n for b, n in PE_FLAGS if f & b) or '-'
    return ','.join(n for b, n in FLAGS if f & b) or '-'


PE_FLAGS = [(0x20, 'code'), (0x40, 'idata'), (0x80, 'bss'),
            (0x02000000, 'DISC'), (0x10000000, 'shr'), (0x20000000, 'X'),
            (0x40000000, 'R'), (0x80000000, 'W')]


def parse_pe(d, off):
    """MPD/PDR miniports are PE; sections are charged like LE objects."""
    ns, = struct.unpack_from('<H', d, off + 6)
    opt, = struct.unpack_from('<H', d, off + 20)
    t = off + 24 + opt
    salign, = struct.unpack_from('<I', d, off + 24 + 32)
    objs = []
    for i in range(ns):
        nm, vs, va, rs, rp, _, _, _, _, fl = struct.unpack_from('<8sIIIIIIHHI', d, t + 40 * i)
        nm = nm.rstrip(b'\0').decode('latin1')
        if not vs:
            vs = rs
        # INIT/.reloc are dropped after load
        disc = bool(fl & 0x02000000) or nm.upper().startswith(('INIT', '.RELOC'))
        fl2 = (fl & ~0x02000000) | (0x02000000 if disc else 0)
        pages = -(-vs // PAGE)
        objs.append(dict(idx=i + 1, size=vs, flags=fl2, pages=pages,
                         slack=pages * PAGE - vs, over=vs % PAGE, name=nm, pe=True, va=va,
                         align=salign))
    return objs


def pe_extent(objs):
    """Bytes from image base to the end of the last non-discardable
    section; PE sections are packed at SectionAlignment (often < 4 KB),
    so the locked cost is this extent, not a page per section."""
    r = [o for o in objs if not o['flags'] & 0x02000000]
    ext = max(o['va'] + o['size'] for o in r)
    pages = -(-ext // PAGE)
    return ext, pages, ext % PAGE


def parse(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ' or len(d) < 0x40:
        raise ValueError('no MZ header')
    off = struct.unpack_from('<I', d, 0x3C)[0]
    if d[off:off + 2] == b'PE':
        return d, parse_pe(d, off)
    if d[off:off + 2] not in (b'LE', b'LX'):
        raise ValueError('not LE/LX (%r at 0x%x)' % (d[off:off + 2], off))
    nobj, = struct.unpack_from('<I', d, off + 0x44)
    otab, = struct.unpack_from('<I', d, off + 0x40)
    objs = []
    for i in range(nobj):
        vs, base, fl, pmi, pmn, _ = struct.unpack_from('<6I', d, off + otab + 24 * i)
        pages = -(-vs // PAGE)
        slack = pages * PAGE - vs
        objs.append(dict(idx=i + 1, size=vs, flags=fl, pages=pages,
                         slack=slack, over=vs % PAGE if vs % PAGE else 0))
    return d, objs


def main(argv):
    tsv = '--tsv' in argv
    files = [a for a in argv if a != '--tsv']
    for p in files:
        try:
            d, objs = parse(p)
        except Exception as e:
            print('SKIP %s: %s' % (p, e))
            continue
        print('%s  md5=%s  %d bytes' % (p, hashlib.md5(d).hexdigest()[:8], len(d)))
        res = disc = 0
        for o in objs:
            kind = 'disc' if o['flags'] & (0x02000000 if o.get('pe') else 0x10) else 'resident'
            if kind == 'disc':
                disc += o['pages']
            else:
                res += o['pages']
            print('  %s%d  size %6d  pages %3d  slack %4d  over %4d  %-8s 0x%05x %s'
                  % ('sec ' if o.get('pe') else 'obj ', o['idx'], o['size'], o['pages'], o['slack'], o['over'],
                     kind, o['flags'], decode(o['flags'], o.get('pe')) + (' ' + o['name'] if o.get('pe') else '')))
        if objs and objs[0].get('pe'):
            ext, pg, ov = pe_extent(objs)
            print('  PE section alignment 0x%x; non-disc image extent %d bytes = %d pages, over %d'
                  % (objs[0]['align'], ext, pg, ov))
        print('  total: resident-type %d pages (%d KB), discardable %d pages'
              % (res, res * 4, disc))


if __name__ == '__main__':
    main(sys.argv[1:])
