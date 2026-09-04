"""Locate a VxD's DRP and its AER, and report the file offset of the AER body.

The DRP is eyecatcher[8], LGN dd, aer dd, ilb dd, name[16], rev db, feature dd,
ifreq dw, bus db, result dw.  We find it by scanning for a single-bit LGN in the
load-group range followed by a printable 16-byte name, as elsewhere in this repo.

LE objects are page-mapped: object 1's pages start at the header's `datapages`,
so a linked address inside object 1 maps to datapages + (addr - object1_base).
Object bases are 0 in every VxD seen here, so the mapping is just +datapages.
"""
import struct
import sys


def le_hdr(d):
    off = struct.unpack_from('<I', d, 0x3c)[0]
    assert d[off:off + 2] == b'LE', 'not an LE image'
    return off, {
        'datapages': struct.unpack_from('<I', d, off + 0x80)[0],
        'objtab': off + struct.unpack_from('<I', d, off + 0x40)[0],
        'numobj': struct.unpack_from('<I', d, off + 0x44)[0],
        'pagesize': struct.unpack_from('<I', d, off + 0x28)[0],
    }


def find_drp(d):
    for off in range(0, len(d) - 48, 4):
        lgn = struct.unpack_from('<I', d, off)[0]
        if lgn and (lgn & (lgn - 1)) == 0 and 0x06 <= lgn.bit_length() - 1 <= 0x1b:
            nm = d[off + 12:off + 28].rstrip(b'\x00 ')
            if 3 <= len(nm) <= 16 and all(32 <= c < 127 for c in nm):
                return off, lgn, struct.unpack_from('<I', d, off + 4)[0], \
                    struct.unpack_from('<I', d, off + 8)[0], nm.decode('latin1')
    return None


for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    _, h = le_hdr(d)
    r = find_drp(d)
    name = path.replace('\\', '/').split('/')[-1]
    if not r:
        print('%-26s no DRP found' % name)
        continue
    off, lgn, aer, ilb, nm = r
    dp = h['datapages']
    print('%-26s name=%-18s LGN=%08X' % (name, nm, lgn))
    print('    DRP at file 0x%05X   aer=%08X -> file 0x%05X   ilb=%08X -> file 0x%05X'
          % (off - 8, aer, aer + dp, ilb, ilb + dp))
    print('    datapages=0x%X pagesize=0x%X objects=%d' % (dp, h['pagesize'], h['numobj']))
