"""List poll loops in a Windows 9x LE VxD or PDR.

    python tools/le_poll_scan.py <file.vxd|.pdr|.386>

A poll loop here is a backward jump of at most 0x60 bytes whose body contains
an IN. Each executable LE object is decoded end to end; a VxD service call
(CD 20 plus an inline 4-byte id) is taken as 6 bytes, or the sweep desyncs.
The undecodable-byte count per object shows how much the sweep skipped.

Not for drivers that reach ports through import thunks (T130.MPD calls
ScsiPortReadPortUchar): count those call sites instead - this reports zero.
"""
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

MAX_BACK = 0x60


def decode(buf):
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    out, off = [], 0
    while off < len(buf):
        if buf[off] == 0xCD and off + 6 <= len(buf) and buf[off + 1] == 0x20:
            svc, dev = struct.unpack_from('<HH', buf, off + 2)
            out.append((off, 'vxdcall', '%04X:%04X' % (dev, svc)))
            off += 6
            continue
        got = list(md.disasm(buf[off:off + 16], off, 1))
        if not got:
            out.append((off, 'db', '%02X' % buf[off]))
            off += 1
            continue
        out.append((off, got[0].mnemonic, got[0].op_str))
        off += got[0].size
    return out


def main():
    data = open(sys.argv[1], 'rb').read()
    le = struct.unpack_from('<I', data, 0x3C)[0]
    if data[le:le + 2] != b'LE':
        sys.exit('not an LE file')
    npages = struct.unpack_from('<I', data, le + 0x14)[0]
    pagesize, lastpage = struct.unpack_from('<II', data, le + 0x28)
    objtab = le + struct.unpack_from('<I', data, le + 0x40)[0]
    nobj = struct.unpack_from('<I', data, le + 0x44)[0]
    datapages = struct.unpack_from('<I', data, le + 0x80)[0]

    for o in range(nobj):
        vsize, _, flags, first, count, _ = struct.unpack_from('<6I', data, objtab + o * 24)
        if not flags & 4:
            continue
        buf = b''
        for p in range(first - 1, first - 1 + count):
            size = lastpage if p == npages - 1 else pagesize
            buf += data[datapages + p * pagesize:][:size]
        fbase = datapages + (first - 1) * pagesize
        ins = decode(buf)
        at = {a: k for k, (a, _, _) in enumerate(ins)}
        print('object %d: file %X, %d insns, %d undecodable, IN %d, OUT %d'
              % (o + 1, fbase, len(ins), sum(m == 'db' for _, m, _ in ins),
                 sum(m == 'in' for _, m, _ in ins), sum(m == 'out' for _, m, _ in ins)))
        for k, (a, m, op) in enumerate(ins):
            if not (m.startswith('j') or m.startswith('loop')):
                continue
            try:
                t = int(op, 16)
            except ValueError:
                continue
            if t < a and a - t <= MAX_BACK and t in at:
                body = ins[at[t]:k + 1]
                if any(bm == 'in' for _, bm, _ in body):
                    print('  loop at file %X:' % (fbase + t))
                    for ba, bm, bop in body:
                        print('    %06X  %s %s' % (fbase + ba, bm, bop))


if __name__ == '__main__':
    main()
