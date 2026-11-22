"""Sum EIPSAMP.TXT samples by module, by nearest export, by 16-bit segment, and by caller.

    python eipsum.py EIPSAMP.TXT [--gheap GHEAP16.TXT] [--map DISPLAY=m8disp.map] [FILE ...]

FILE is a PE DLL (its exports split that module's 32-bit samples) or an NE file - EXE, DLL or
DRV (its entry table splits that module's 16-bit samples). The module a file serves is its base
name, so copy them from the card under their module names (USER.EXE, GDI.EXE, KRNL386.EXE).
--map gives a Watcom map for a 16-bit module, which names its code by function, not export.

16-bit samples are named through GHEAP16.TXT (selector -> module and segment). Each sample's
stack top is searched for a return address outside the module it was sampled in: 32-bit, a
dword inside another module; 16-bit, an offset:selector pair whose selector is a code segment.
That names who called into a lock wait or a thunk.
"""
import os
import struct
import sys
from collections import Counter, defaultdict


def pe_exports(d):
    """{rva: name} from a PE file's export directory."""
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    exp_rva = struct.unpack_from('<I', d, opt + 96)[0]
    secs = []
    sh = opt + struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(nsec):
        vsize, va, size, raw = struct.unpack_from('<IIII', d, sh + i * 40 + 8)
        secs.append((va, max(vsize, size), raw))

    def off(rva):
        for va, size, raw in secs:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError(hex(rva))

    if not exp_rva:
        return {}
    e = off(exp_rva)
    nfunc, nname, afunc, aname, aord = struct.unpack_from('<IIIII', d, e + 20)
    funcs = [struct.unpack_from('<I', d, off(afunc) + 4 * i)[0] for i in range(nfunc)]
    out = {}
    for i in range(nname):
        n_rva = struct.unpack_from('<I', d, off(aname) + 4 * i)[0]
        o = struct.unpack_from('<H', d, off(aord) + 2 * i)[0]
        s = off(n_rva)
        out[funcs[o]] = d[s:d.index(b'\0', s)].decode('ascii', 'replace')
    return out


def ne_entries(d):
    """{(segment, offset): name} from an NE file's entry table and name tables."""
    ne = struct.unpack_from('<I', d, 0x3C)[0]
    ent_off, ent_len = struct.unpack_from('<HH', d, ne + 4)
    res_off = struct.unpack_from('<H', d, ne + 0x26)[0]
    nres_off = struct.unpack_from('<I', d, ne + 0x2C)[0]
    nres_len = struct.unpack_from('<H', d, ne + 0x20)[0]
    names = {}

    def table(p, end):
        first = True
        while p < end:
            n = d[p]
            if not n:
                break
            name = d[p + 1:p + 1 + n].decode('ascii', 'replace')
            ordn = struct.unpack_from('<H', d, p + 1 + n)[0]
            if not first:
                names[ordn] = name
            first = False
            p += n + 3

    table(ne + res_off, len(d))
    table(nres_off, nres_off + nres_len)
    out = {}
    p, ordn, end = ne + ent_off, 1, ne + ent_off + ent_len
    while p < end:
        cnt, seg = d[p], d[p + 1]
        p += 2
        if cnt == 0:
            break
        for _ in range(cnt):
            if seg == 0:
                pass
            elif seg == 0xFF:
                s, o = d[p + 3], struct.unpack_from('<H', d, p + 4)[0]
                out[(s, o)] = names.get(ordn, '#%d' % ordn)
                p += 6
                ordn += 1
                continue
            else:
                o = struct.unpack_from('<H', d, p + 1)[0]
                out[(seg, o)] = names.get(ordn, '#%d' % ordn)
                p += 3
                ordn += 1
                continue
            ordn += 1
    return out


def watcom_map(path):
    """{(segment, offset): name} from a Watcom map's address lines."""
    out = {}
    for line in open(path, errors='replace'):
        p = line.split()
        if len(p) >= 2 and len(p[0]) == 9 and p[0][4] == ':':
            try:
                s, o = int(p[0][:4], 16), int(p[0][5:], 16)
            except ValueError:
                continue
            out[(s, o)] = p[-1]
    return out


def nearest(table, key):
    """The name at or below key in a sorted [(key, name)] list."""
    best = None
    for k, n in table:
        if k <= key:
            best = n
        else:
            break
    return best


def main(argv):
    args, files, gheap, maps = argv[1:], [], None, {}
    path = args.pop(0)
    while args:
        a = args.pop(0)
        if a == '--gheap':
            gheap = args.pop(0)
        elif a == '--map':
            m, f = args.pop(0).split('=', 1)
            maps[m.upper()] = f
        else:
            files.append(a)

    mods, sels, samples, head = [], {}, [], ''
    for line in open(path):
        p = line.split()
        if not p:
            continue
        if p[0] == 'M':
            mods.append((int(p[1], 16), int(p[2], 16), ' '.join(p[3:])))
        elif p[0] == 'C':
            sels[int(p[1], 16) & 0xFFF8] = int(p[2], 16)
        elif p[0] == 'S':
            samples.append((int(p[1], 16), int(p[2], 16), int(p[3], 16) if len(p) > 3 else 0,
                            [int(x, 16) for x in p[4:]]))
        else:
            head = line.strip()
    flat_cs = None
    if ' flat ' in head:
        flat_cs = int(head.split(' flat ')[1].split()[0], 16)

    segs = {}                       # selector & ~7 -> (module, segment number)
    if gheap:
        for line in open(gheap):
            p = line.split()
            if p and p[0] == 'G':
                segs[int(p[1], 16) & 0xFFF8] = (p[5].upper(), int(p[4]))

    pe_syms, ne_syms = {}, {}
    for f in files:
        d = open(f, 'rb').read()
        name = os.path.splitext(os.path.basename(f))[0].upper()
        sig = d[struct.unpack_from('<I', d, 0x3C)[0]:][:2]
        if sig == b'PE':
            pe_syms[os.path.basename(f).upper()] = sorted(pe_exports(d).items())
        elif sig == b'NE':
            ne_syms[name] = ne_entries(d)
    for m, f in maps.items():
        ne_syms[m] = watcom_map(f)

    def module(eip):
        for base, size, name in mods:
            if base <= eip < base + size:
                return base, name
        return None, None

    if flat_cs is None:
        c = Counter(cs for cs, eip, ss, st in samples if module(eip)[1]).most_common(1)
        flat_cs = c[0][0] if c else None

    def name16(cs, ip):
        seg = segs.get(cs & 0xFFF8)
        if not seg:
            return '16-bit CS %04X' % cs, None
        mod, n = seg
        table = ne_syms.get(mod)
        sym = None
        if table:
            sym = nearest(sorted((o, s) for (sg, o), s in table.items() if sg == n), ip)
        return '%s seg %d' % (mod, n), '%s!%s' % (mod, sym or '(seg %d +%04X)' % (n, ip))

    def name32(eip):
        base, name = module(eip)
        if name is None:
            return 'flat, no module', None
        table = pe_syms.get(name.upper())
        sym = nearest(table, eip - base) if table else None
        return name, '%s!%s' % (name, sym) if sym else '%s+%X' % (name, eip - base)

    def caller(cs, eip, st):
        """The first return address on the stack top outside the sampled module."""
        if cs == flat_cs:
            own = module(eip)[1]
            for v in st:
                n = module(v)[1]
                if n and n != own:
                    return name32(v)[1]
            return None
        own = segs.get(cs & 0xFFF8, (None,))[0]
        words = []
        for v in st:
            words += [v & 0xFFFF, v >> 16]
        for i in range(len(words) - 1):
            sel = words[i + 1] & 0xFFF8
            if sel in segs and segs[sel][0] != own:
                return name16(words[i + 1], words[i])[1]
        return None

    by_mod, by_sym, by_call, k32 = Counter(), Counter(), defaultdict(Counter), Counter()
    for cs, eip, ss, st in samples:
        if cs == flat_cs:
            m, s = name32(eip)
            if m == 'KERNEL32.DLL':
                k32[(eip - module(eip)[0]) & ~0x1F] += 1
        else:
            m, s = name16(cs, eip & 0xFFFF)
        by_mod[m] += 1
        key = s or m
        by_sym[key] += 1
        by_call[key][caller(cs, eip, st) or '-'] += 1
    n = len(samples) or 1
    print(head)
    print('flat code selector %s, %d samples, %d 16-bit selectors named of %d seen' % (
        '%04X' % flat_cs if flat_cs is not None else '?', len(samples),
        sum(1 for s in sels if s in segs), len(sels)))
    print('\nby module:')
    for k, v in by_mod.most_common(40):
        print('  %6d  %5.1f %%  %s' % (v, 100.0 * v / n, k))
    print('\nby function (top 40), with the top callers:')
    for k, v in by_sym.most_common(40):
        cs = ', '.join('%s %d' % (c, w) for c, w in by_call[k].most_common(3))
        print('  %6d  %5.1f %%  %s   <- %s' % (v, 100.0 * v / n, k, cs))
    print('\nKERNEL32 hot spots (rva, 32-byte buckets):')
    for k, v in k32.most_common(12):
        print('  %6d  %5.1f %%  +%05X' % (v, 100.0 * v / n, k))


if __name__ == '__main__':
    main(sys.argv)
