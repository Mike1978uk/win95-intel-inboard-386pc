"""Sum EIPSAMP.TXT samples by module, and by nearest export within a module.

    python eipsum.py EIPSAMP.TXT [DLL ...]

Each DLL given (copied from the card, e.g. D3DIM.DLL, M8DD32.DLL) is read for its export table,
and its samples are split by the nearest export at or below the address (the image base comes
from the module list, so a relocated DLL still resolves). Samples whose CS is not the flat code
selector are 16-bit code (display driver, USER, GDI) and are summed by selector.
"""
import os
import struct
import sys
from collections import Counter


def exports(path):
    """{rva: name} from a PE file's export directory."""
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    exp_rva = struct.unpack_from('<I', d, opt + 96)[0]
    secs = []
    sh = opt + struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(nsec):
        va, size, raw = struct.unpack_from('<III', d, sh + i * 40 + 12)[0:3]
        vsize = struct.unpack_from('<I', d, sh + i * 40 + 8)[0]
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
        name = d[s:d.index(b'\0', s)].decode('ascii', 'replace')
        out[funcs[o]] = name
    return out


def main(argv):
    mods, samples, head = [], [], ''
    for line in open(argv[1]):
        p = line.split()
        if not p:
            continue
        if p[0] == 'M':
            mods.append((int(p[1], 16), int(p[2], 16), ' '.join(p[3:])))
        elif p[0] == 'S':
            samples.append((int(p[1], 16), int(p[2], 16)))
        else:
            head = line.strip()
    syms = {}
    for path in argv[2:]:
        syms[os.path.basename(path).upper()] = sorted(exports(path).items())

    def module(eip):
        for base, size, name in mods:
            if base <= eip < base + size:
                return base, name
        return None, None

    flat = Counter(cs for cs, eip in samples if module(eip)[1]).most_common(1)
    flat = flat[0][0] if flat else None
    by_mod, by_sym = Counter(), Counter()
    for cs, eip in samples:
        if cs != flat:
            by_mod['16-bit CS %04X' % cs] += 1
            continue
        base, name = module(eip)
        if name is None:
            by_mod['flat, no module'] += 1
            continue
        by_mod[name] += 1
        table = syms.get(name.upper())
        if table:
            rva = eip - base
            best = '(before first export)'
            for r, n in table:
                if r <= rva:
                    best = n
                else:
                    break
            by_sym['%s!%s' % (name, best)] += 1
    n = len(samples) or 1
    print(head)
    print('flat code selector %s, %d samples' % ('%04X' % flat if flat is not None else '?', len(samples)))
    print('\nby module:')
    for k, v in by_mod.most_common():
        print('  %6d  %5.1f %%  %s' % (v, 100.0 * v / n, k))
    if by_sym:
        print('\nby nearest export (top 30):')
        for k, v in by_sym.most_common(30):
            print('  %6d  %5.1f %%  %s' % (v, 100.0 * v / n, k))


if __name__ == '__main__':
    main(sys.argv)
