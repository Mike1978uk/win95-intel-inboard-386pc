"""Split a Windows 9x VxD into locked, pageable and init-only bytes.

    python tools/le_locked.py <file.vxd|.pdr|.386> [...]

Reads the LE object table. A preloaded, non-discardable object is locked
(LCODE/LDATA), a discardable one is freed after init (ICODE), anything else
is pageable. A W3 monolith (VMM32.VXD) is split into its member VxDs; convert
a W4 file first with `patcher9x -force-w3 --vxd-convert VMM32.VXD`.

Header flags, not a measurement: a VxD can still lock pages at run time.
PE miniports (.MPD) are not LE and are reported as such.
"""
import os
import struct
import sys


def le_objects(data, off):
    if data[off:off + 2] not in (b'LE', b'LX'):
        return None
    objtab, nobj = struct.unpack_from('<II', data, off + 0x40)
    return [struct.unpack_from('<IIIIII', data, off + objtab + i * 24)[:3:2]
            for i in range(nobj)]


def split(objs):
    t = {'locked': 0, 'page': 0, 'init': 0}
    for vsize, flags in objs:
        if flags & 0x0010:
            t['init'] += vsize
        elif flags & 0x0040:
            t['locked'] += vsize
        else:
            t['page'] += vsize
    return t


def emit(name, objs):
    t = split(objs)
    print(f'{name:14} locked {t["locked"]:7} page {t["page"]:7} init {t["init"]:7}')


def report(path):
    data = open(path, 'rb').read()
    hdr = struct.unpack_from('<I', data, 0x3C)[0] if data[:2] == b'MZ' else 0
    if data[hdr:hdr + 2] == b'W4':
        print(f'{os.path.basename(path):14} W4 - convert to W3 first')
    elif data[hdr:hdr + 2] == b'W3':
        for i in range(struct.unpack_from('<H', data, hdr + 4)[0]):
            name, off, size = struct.unpack_from('<8sII', data, hdr + 16 + i * 16)
            sub = data[off:off + size]
            objs = le_objects(sub, struct.unpack_from('<I', sub, 0x3C)[0])
            if objs:
                emit(name.rstrip(b'\0 ').decode(), objs)
    else:
        objs = le_objects(data, hdr)
        if objs:
            emit(os.path.basename(path), objs)
        else:
            print(f'{os.path.basename(path):14} not LE ({data[hdr:hdr + 2]!r})')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        report(p)
