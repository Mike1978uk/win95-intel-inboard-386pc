#!/usr/bin/env python3
r"""Disassemble a 16-bit Windows NE executable (.DRV, .EXE, .DLL) - the companion to pedis.py.

  python tools/nedis.py <file> segs                 segment table (number, file offset, size, flags)
  python tools/nedis.py <file> exports              exported names with segment:offset
  python tools/nedis.py <file> dis <seg> <off> <n>  disassemble n bytes of segment <seg> from <off> (hex)
  python tools/nedis.py <file> all                  every code segment, linear sweep with resync
  python tools/nedis.py <file> io                   every in/out with the port, when DX was loaded
                                                    by an immediate in the same basic run

Segment numbers are 1-based, as Windows and the NE format count them. Offsets are hex.
A linear sweep stops being trustworthy at data embedded in code (technique 16/44); use `dis`
on a known entry point (from `exports`) before trusting any single decode.
"""
import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

def load(path):
    d = open(path, 'rb').read()
    assert d[:2] == b'MZ', 'not an MZ executable'
    ne = struct.unpack_from('<I', d, 0x3C)[0]
    assert d[ne:ne + 2] == b'NE', 'not an NE executable'
    h = {}
    (h['entry_off'], h['entry_len']) = struct.unpack_from('<HH', d, ne + 0x04)
    h['nseg'] = struct.unpack_from('<H', d, ne + 0x1C)[0]
    h['nnonres'] = struct.unpack_from('<H', d, ne + 0x20)[0]
    h['segtab'] = ne + struct.unpack_from('<H', d, ne + 0x22)[0]
    h['restab'] = ne + struct.unpack_from('<H', d, ne + 0x26)[0]
    h['nonres'] = struct.unpack_from('<I', d, ne + 0x2C)[0]
    h['shift'] = struct.unpack_from('<H', d, ne + 0x32)[0] or 9
    h['entry'] = ne + h['entry_off']
    segs = []
    for i in range(h['nseg']):
        sector, length, flags, minalloc = struct.unpack_from('<HHHH', d, h['segtab'] + i * 8)
        off = sector << h['shift']
        size = length or (0x10000 if sector else 0)
        segs.append({'n': i + 1, 'off': off, 'size': size, 'flags': flags,
                     'data': bool(flags & 1), 'reloc': bool(flags & 0x100)})
    return d, h, segs

def entries(d, h):
    """Ordinal -> (segment, offset) from the entry table."""
    out, p, ordinal = {}, h['entry'], 1
    end = p + h['entry_len']
    while p < end:
        count, seg = d[p], d[p + 1]
        p += 2
        if count == 0:
            break
        for _ in range(count):
            if seg == 0:
                pass
            elif seg == 0xFF:          # movable: flags, INT 3Fh, seg, offset
                s, o = d[p + 3], struct.unpack_from('<H', d, p + 4)[0]
                out[ordinal] = (s, o)
                p += 6
                ordinal += 1
                continue
            else:                      # fixed: flags, offset
                out[ordinal] = (seg, struct.unpack_from('<H', d, p + 1)[0])
                p += 3
                ordinal += 1
                continue
            ordinal += 1
    return out

def names(d, start, end=None):
    out, p = [], start
    while True:
        n = d[p]
        if n == 0 or (end is not None and p >= end):
            break
        out.append((d[p + 1:p + 1 + n].decode('latin1'), struct.unpack_from('<H', d, p + 1 + n)[0]))
        p += 3 + n
    return out

def seg_bytes(d, s):
    return d[s['off']:s['off'] + s['size']]

def relocs(d, s):
    """Offset of each patched field in segment s -> 'seg:off' (internal) or 'ordinal N' (import).

    A far call to another segment is stored as `9A FFFF 0000` and fixed up at load time from the
    records that follow the segment's data; without them every such call reads `lcall 0, 0xffff`.
    Additive=0 records chain through the field: each patched slot holds the next one's offset."""
    out = {}
    if not s['reloc']:
        return out
    p = s['off'] + s['size']
    n = struct.unpack_from('<H', d, p)[0]
    p += 2
    code = seg_bytes(d, s)
    for _ in range(n):
        atype, flags, off, a, b = struct.unpack_from('<BBHHH', d, p)
        p += 8
        kind = flags & 3
        if kind == 0:                         # internal reference
            seg = a & 0xFF
            target = ('%d:%04X' % (seg, b)) if seg != 0xFF else ('entry %d' % b)
        elif kind == 1:
            target = 'import mod%d.ord%d' % (a, b)
        elif kind == 2:
            target = 'import mod%d.name@%04X' % (a, b)
        else:
            target = 'osfixup %d' % a
        additive = flags & 4
        o, seen = off, 0
        while o < len(code) and seen < 4096:
            out[o] = target
            if additive:
                break
            nxt = struct.unpack_from('<H', code, o)[0]
            if nxt == 0xFFFF or nxt == o:
                break
            o, seen = nxt, seen + 1
    return out

def note(ins, rel):
    """The fix-up that lands inside this instruction, if any."""
    for k in range(ins.address, ins.address + ins.size):
        if k in rel:
            return '   ; -> ' + rel[k]
    return ''

def sweep(code, base=0):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    pos = 0
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], base + pos):
            got = True
            yield ins
            pos = ins.address - base + ins.size
        if not got or pos < len(code):
            pos += 1                     # resync past an undecodable byte

def main():
    if len(sys.argv) < 3:
        print(__doc__); return
    path, cmd = sys.argv[1], sys.argv[2]
    d, h, segs = load(path)
    if cmd == 'segs':
        for s in segs:
            print('%2d  file %06X  size %04X  %s%s' % (s['n'], s['off'], s['size'],
                  'DATA' if s['data'] else 'CODE', ' reloc' if s['reloc'] else ''))
    elif cmd == 'exports':
        ent = entries(d, h)
        rows = names(d, h['restab'])[1:] + names(d, h['nonres'])[1:]
        for nm, ordl in sorted(rows, key=lambda r: r[1]):
            s, o = ent.get(ordl, (0, 0))
            print('%4d  %-24s %2d:%04X' % (ordl, nm, s, o))
    elif cmd == 'imports':
        ne = struct.unpack_from('<I', d, 0x3C)[0]
        nmod = struct.unpack_from('<H', d, ne + 0x1E)[0]
        modtab = ne + struct.unpack_from('<H', d, ne + 0x28)[0]
        imptab = ne + struct.unpack_from('<H', d, ne + 0x2A)[0]
        for i in range(nmod):
            o = struct.unpack_from('<H', d, modtab + i * 2)[0]
            n = d[imptab + o]
            print('mod%d  %s' % (i + 1, d[imptab + o + 1:imptab + o + 1 + n].decode('latin1')))
        seen = {}
        for s in segs:
            for off, t in relocs(d, s).items():
                if t.startswith('import'):
                    seen.setdefault(t, []).append('%d:%04X' % (s['n'], off))
        for t in sorted(seen):
            print('%-28s used at %d site(s), first %s' % (t, len(seen[t]), seen[t][0]))
    elif cmd == 'dis':
        s = segs[int(sys.argv[3]) - 1]
        off, n = int(sys.argv[4], 16), int(sys.argv[5], 16)
        code = seg_bytes(d, s)[off:off + n]
        rel = relocs(d, s)
        for ins in Cs(CS_ARCH_X86, CS_MODE_16).disasm(code, off):
            print('%2d:%04X  %-20s %s %s%s' % (s['n'], ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str, note(ins, rel)))
    elif cmd == 'all':
        for s in segs:
            if s['data']:
                continue
            print('; ===== segment %d, %d bytes' % (s['n'], s['size']))
            rel = relocs(d, s)
            for ins in sweep(seg_bytes(d, s)):
                print('%2d:%04X  %-20s %s %s%s' % (s['n'], ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str, note(ins, rel)))
    elif cmd == 'io':
        for s in segs:
            if s['data']:
                continue
            dx = None
            for ins in sweep(seg_bytes(d, s)):
                m, ops = ins.mnemonic, ins.op_str
                if m == 'mov' and ops.startswith('dx, ') and ops[4:].startswith('0x'):
                    dx = int(ops[4:], 16)
                elif m in ('in', 'out', 'insb', 'insw', 'outsb', 'outsw') or m.startswith('rep'):
                    if 'dx' in ops or m.startswith(('ins', 'outs', 'rep')):
                        port = '%04X' % dx if dx is not None else '????'
                        print('%2d:%04X  %-6s %-14s port %s' % (s['n'], ins.address, m, ops, port))
                    elif ops:
                        print('%2d:%04X  %-6s %s' % (s['n'], ins.address, m, ops))
                if m in ('jmp', 'call', 'ret', 'retf', 'iret') or m.startswith('j'):
                    if m in ('jmp', 'ret', 'retf', 'iret'):
                        dx = None
                elif 'dx' in ops.split(',')[0] and not (m == 'mov' and ops.startswith('dx, 0x')) and m not in ('in', 'out'):
                    dx = None if m not in ('add', 'sub', 'inc', 'dec', 'or', 'and', 'xor') else dx
    else:
        print(__doc__)

if __name__ == '__main__':
    main()
