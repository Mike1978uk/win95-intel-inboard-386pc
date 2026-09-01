#!/usr/bin/env python3
"""Read the request marker XT_REQ_MARKER leaves on disk.

  python pdr_reqmarker.py <image> [<image> ...]

Checks EVERY image given, not just the one aimed at: absence from a disk the
driver was not meant to touch is half the result (technique 79). Prints nothing
found rather than guessing, so a stale marker from a previous run is visible as
an unchanged count.
"""
import os, sys, struct

LBA = 16000
SIG = b'XTIDEREQ'
FUNC = {0: 'READ', 1: 'WRITE', 2: 'VERIFY'}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for path in sys.argv[1:]:
        with open(path, 'rb') as f:
            f.seek(LBA * 512)
            blk = f.read(512)
        if len(blk) < 44 or blk[:8] != SIG:
            print('%-16s LBA %d: no marker' % (os.path.basename(path), LBA))
            continue
        (start, entry, func, lba, xfer, unit,
         cfg, want, isp) = struct.unpack('<9I', blk[8:44])
        print('%s  LBA %d' % (os.path.basename(path), LBA))
        print('    AEP_CONFIG_DCB calls   %d' % cfg)
        print('    ...claimed by us       %d' % want)
        print('    ISP_result of insert   %d%s' % (isp, '' if isp == 0 else '   <- IOS REFUSED'))
        print('    Port_request entries   %d' % entry)
        print('    XTIDE_StartRequest     %d' % start)
        if start:
            print('    last request           %s(%d) lba=%d sectors=%d unit=%d'
                  % (FUNC.get(func, '?'), func, lba, xfer, unit))
    return 0


sys.exit(main())
