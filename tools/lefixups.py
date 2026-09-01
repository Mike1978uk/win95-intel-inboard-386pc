#!/usr/bin/env python3
"""Decode an LE image's fixup records, page by page.

  python tools/lefixups.py <file> [page] [--at obj:off]

--at prints only the fixup whose source is that object-relative address, which is
how you answer "does DDB_Control_Proc actually get relocated?".
"""
import sys, struct

SRC = {0: 'byte', 2: 'sel16', 3: 'ptr16:16', 5: 'off16', 6: 'ptr16:32', 7: 'off32', 8: 'rel32'}


def main():
    path = sys.argv[1]
    only_page = None
    at = None
    args = sys.argv[2:]
    for i, a in enumerate(args):
        if a == '--at':
            o, s = args[i + 1].split(':')
            at = (int(o), int(s, 0))
        elif a.isdigit():
            only_page = int(a)
    d = open(path, 'rb').read()
    le = struct.unpack_from('<I', d, 0x3c)[0] if d[:2] == b'MZ' else 0
    g = lambda o: struct.unpack_from('<I', d, le + o)[0]
    numpages, pagesize = g(0x14), g(0x28)
    objtab, numobj = g(0x40), g(0x44)
    fpt, frt = g(0x68), g(0x6C)

    # page index -> owning object and the page's base offset within that object
    owner = {}
    for i in range(numobj):
        vsize, base, fl, pmi, pmc, _ = struct.unpack_from('<6I', d, le + objtab + i * 24)
        for k in range(pmc):
            owner[pmi + k] = (i + 1, k * pagesize)

    for p in range(1, numpages + 1):
        if only_page and p != only_page:
            continue
        start = struct.unpack_from('<I', d, le + fpt + (p - 1) * 4)[0]
        end = struct.unpack_from('<I', d, le + fpt + p * 4)[0]
        o = le + frt + start
        stop = le + frt + end
        obj, pbase = owner.get(p, (0, 0))
        if not at:
            print('-- page %d (object %d, +0x%X) : %d bytes of records --'
                  % (p, obj, pbase, end - start))
        while o < stop:
            st = d[o]
            tf = d[o + 1]
            o += 2
            srcs = []
            if st & 0x20:                       # source list
                n = d[o]; o += 1
            else:
                n = 1
                srcs.append(struct.unpack_from('<h', d, o)[0]); o += 2
            # target
            kind = tf & 3
            if kind == 0:                       # internal reference
                if tf & 0x40:
                    tobj = struct.unpack_from('<H', d, o)[0]; o += 2
                else:
                    tobj = d[o]; o += 1
                toff = 0
                if (st & 0x0F) != 2:
                    if tf & 0x10:
                        toff = struct.unpack_from('<I', d, o)[0]; o += 4
                    else:
                        toff = struct.unpack_from('<H', d, o)[0]; o += 2
                tgt = 'obj %d off 0x%X' % (tobj, toff)
            elif kind == 1:                     # import by ordinal
                mod = struct.unpack_from('<H', d, o)[0] if tf & 0x40 else d[o]
                o += 2 if tf & 0x40 else 1
                if tf & 0x80:
                    ordv = d[o]; o += 1
                elif tf & 0x10:
                    ordv = struct.unpack_from('<I', d, o)[0]; o += 4
                else:
                    ordv = struct.unpack_from('<H', d, o)[0]; o += 2
                tgt = 'import mod %d ord 0x%X (VxD service)' % (mod, ordv)
            elif kind == 2:                     # import by name
                mod = struct.unpack_from('<H', d, o)[0] if tf & 0x40 else d[o]
                o += 2 if tf & 0x40 else 1
                if tf & 0x10:
                    nameoff = struct.unpack_from('<I', d, o)[0]; o += 4
                else:
                    nameoff = struct.unpack_from('<H', d, o)[0]; o += 2
                tgt = 'import mod %d name@0x%X' % (mod, nameoff)
            else:
                ordv = struct.unpack_from('<H', d, o)[0]; o += 2
                tgt = 'entry ord %d' % ordv
            if tf & 0x04:                       # additive
                if tf & 0x20:
                    add = struct.unpack_from('<I', d, o)[0]; o += 4
                else:
                    add = struct.unpack_from('<H', d, o)[0]; o += 2
                tgt += ' +0x%X' % add
            if st & 0x20:
                for _ in range(n):
                    srcs.append(struct.unpack_from('<h', d, o)[0]); o += 2
            for s in srcs:
                if at:
                    if at[0] == obj and pbase + s == at[1]:
                        print('page %d src %s obj%d:0x%X -> %s'
                              % (p, SRC.get(st & 0xF, st & 0xF), obj, pbase + s, tgt))
                else:
                    print('  %-8s obj%d:0x%04X -> %s'
                          % (SRC.get(st & 0xF, st & 0xF), obj, pbase + s, tgt))


main()
