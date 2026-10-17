"""Disassemble a 16-bit Windows NE module (a .DRV or .DLL) for reading: every code
segment through objdump (i8086, so 66h/67h prefixes decode as 386), with the
module's exports as labels, relocations resolved to names (imports by ordinal are
named from the DLLs given with --dll), and port I/O marked "<<IO".

Usage: ne_disasm.py <module> <out dir> [--dll FILE ...]
Writes <out dir>/segNN.asm and <out dir>/map.txt. Listings of vendor code stay out of
the repository: write them under references/ (ignored) or a scratch folder.
"""
import os
import re
import struct
import subprocess
import sys

OBJDUMP = r'C:\msys64\mingw64\bin\objdump.exe'


class NE:
    def __init__(self, path):
        self.d = d = open(path, 'rb').read()
        self.ne = ne = struct.unpack_from('<I', d, 0x3c)[0]
        h = lambda o: struct.unpack_from('<H', d, ne + o)[0]
        self.nseg, self.shift = h(0x1c), h(0x32)
        segtab, restab, modtab, imptab = ne + h(0x22), ne + h(0x26), ne + h(0x28), ne + h(0x2a)
        self.segs = []
        for i in range(self.nseg):
            off, ln, fl, mn = struct.unpack_from('<HHHH', d, segtab + i * 8)
            self.segs.append((off << self.shift, ln or 0x10000, fl))
        self.names = {}
        self._names(restab)
        self.module = self.names.pop(0, os.path.basename(path))
        self._names(struct.unpack_from('<I', d, ne + 0x2c)[0])
        self.names.pop(0, None)         # the non-resident table's first entry: the description
        self.mods = []
        for i in range(h(0x1e)):
            o = imptab + struct.unpack_from('<H', d, modtab + i * 2)[0]
            self.mods.append(d[o + 1:o + 1 + d[o]].decode('latin1'))
        self.imptab = imptab
        self.entries = {}
        o, ordn, end = ne + h(0x04), 1, ne + h(0x04) + h(0x06)
        while o < end:
            cnt, typ = d[o], d[o + 1]
            o += 2
            if cnt == 0:
                break
            for _ in range(cnt):
                if typ == 0:
                    ordn += 1
                    continue
                if typ == 0xFF:
                    _, _, seg, off = struct.unpack_from('<BHBH', d, o)
                    o += 6
                else:
                    _, off = struct.unpack_from('<BH', d, o)
                    seg = typ
                    o += 3
                self.entries[ordn] = (seg, off)
                ordn += 1

    def _names(self, o):
        d = self.d
        while d[o]:
            n = d[o]
            self.names[struct.unpack_from('<H', d, o + 1 + n)[0]] = d[o + 1:o + 1 + n].decode('latin1')
            o += 3 + n

    def relocs(self, i):
        start, ln, fl = self.segs[i]
        if not fl & 0x100:
            return []
        d = self.d
        o = start + ln
        out = []
        for _ in range(struct.unpack_from('<H', d, o)[0]):
            src, flags, off, a, b = struct.unpack_from('<BBHHH', d, o + 2)
            o += 8
            out.append((src, flags & 3, off, a, b))
        return out


def main():
    mod, outdir = sys.argv[1], sys.argv[2]
    dlls = {}
    args = sys.argv[3:]
    while args:
        if args[0] == '--dll':
            n = NE(args[1])
            dlls[n.module.upper()] = n.names
            args = args[2:]
        else:
            sys.exit(__doc__)
    ne = NE(mod)
    os.makedirs(outdir, exist_ok=True)
    labels = {}
    for ordn, (seg, off) in ne.entries.items():
        labels.setdefault(seg, {})[off] = ne.names.get(ordn, f'ord{ordn}')
    with open(os.path.join(outdir, 'map.txt'), 'w') as m:
        m.write(f'{ne.module}: {ne.nseg} segments; imports {", ".join(ne.mods)}\n')
        for i, (start, ln, fl) in enumerate(ne.segs):
            m.write(f'seg{i + 1:02}: {"DATA" if fl & 1 else "CODE"} {ln:#06x} bytes, flags {fl:#06x}\n')
        for ordn in sorted(ne.entries):
            seg, off = ne.entries[ordn]
            m.write(f'  {ne.names.get(ordn, "?"):24} ord {ordn:4}  seg{seg:02}:{off:04x}\n')
    for i, (start, ln, fl) in enumerate(ne.segs):
        if fl & 1:
            continue
        notes = {}
        for src, kind, off, a, b in ne.relocs(i):
            if kind == 0:
                what = f'seg{a:02}:{b:04x}' + (f' {labels.get(a, {}).get(b, "")}' if a != 0xFF else '')
            elif kind == 1:
                modn = ne.mods[a - 1] if 0 < a <= len(ne.mods) else f'mod{a}'
                what = f'{modn}.{dlls.get(modn.upper(), {}).get(b, f"#{b}")}'
            elif kind == 2:
                modn = ne.mods[a - 1] if 0 < a <= len(ne.mods) else f'mod{a}'
                o = ne.imptab + b
                what = f'{modn}.{ne.d[o + 1:o + 1 + ne.d[o]].decode("latin1")}'
            else:
                what = f'osfixup {a}'
            notes[off] = what
        raw = os.path.join(outdir, f'seg{i + 1:02}.bin')
        open(raw, 'wb').write(ne.d[start:start + ln])
        dis = subprocess.run([OBJDUMP, '-D', '-b', 'binary', '-m', 'i8086', raw],
                             capture_output=True, text=True).stdout.splitlines()
        lab = labels.get(i + 1, {})
        out = []
        for line in dis:
            mt = re.match(r'\s+([0-9a-f]+):\t([0-9a-f ]+)\t?(.*)', line)
            if not mt:
                continue
            addr = int(mt.group(1), 16)
            nbytes = len(mt.group(2).split())
            if addr in lab:
                out.append(f'\n{lab[addr]}:')
            text = f'{addr:04x}  {mt.group(2):<24} {mt.group(3)}'
            tags = [notes[a] for a in range(addr, addr + nbytes) if a in notes]
            if tags:
                text += '   ; -> ' + ', '.join(tags)
            if re.search(r'\b(in|out|ins[bwl]?|outs[bwl]?)\b', mt.group(3)):
                text += '   <<IO'
            out.append(text)
        os.remove(raw)
        open(os.path.join(outdir, f'seg{i + 1:02}.asm'), 'w').write('\n'.join(out) + '\n')
    print(f'{ne.module}: wrote {outdir}')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main()
