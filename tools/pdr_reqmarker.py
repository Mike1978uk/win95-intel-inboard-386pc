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
VOLSTEP = {0: 'XTIDE_VolCreate NEVER RAN', 1: 'no ILB', 2: 'already published',
           3: 'no physical DCB', 4: 'MBR read failed', 5: 'no 55AA signature',
           6: 'no partition 1', 7: 'ISP_CREATE_DCB failed', 8: 'logical calldown failed',
           9: 'ISP_ASSOCIATE_DCB failed', 10: 'VOLUME PUBLISHED'}
STAGE = {0: 'entry only', 2: 'passed the function check', 3: 'the DCB is ours', 5: 'enqueued', 6: 'about to dequeue', 7: 'about to drive the hardware', 9: 'refusing the request'}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for path in sys.argv[1:]:
        with open(path, 'rb') as f:
            f.seek(LBA * 512)
            blk = f.read(512)
        if len(blk) < 228 or blk[:8] != SIG:
            print('%-16s LBA %d: no marker' % (os.path.basename(path), LBA))
            continue
        (start, entry, func, lba, xfer, unit, cfg, want, isp,
         iopdcb, ourdcb, refused, stage,
         origdcb, cdddb, ourddb, iop) = struct.unpack('<17I', blk[8:76])
        raw = struct.unpack('<8I', blk[76:108])
        ring = struct.unpack('<16I', blk[108:172])
        cyls, heads, spt, total, devflags = struct.unpack('<5I', blk[172:192])
        (vcreate, vcd, vassoc, vdrive, pstart, plen,
         vstep) = struct.unpack('<7I', blk[192:220])
        schedrc = struct.unpack('<I', blk[224:228])[0]
        print('%s  LBA %d' % (os.path.basename(path), LBA))
        print('    DCB geometry written   %d cyl / %d head / %d spt, %d sectors'
              % (cyls, heads, spt, total))
        print('    DCB_device_flags       %08X' % devflags)
        print('    AEP_CONFIG_DCB calls   %d' % cfg)
        print('    -- volume publishing (we are our own TSD) --')
        print('      queue call returned %s' % ('never attempted' if schedrc == 0xFFFFFFFF else ('REFUSED' if schedrc == 0 else 'queued (%08X)' % schedrc)))
        print('      got as far as       %d  %s' % (vstep, VOLSTEP.get(vstep, '?')))
        print('      partition           start %d, %d sectors' % (pstart, plen))
        print('      ISP_CREATE_DCB      %d' % vcreate)
        print('      logical calldown    %d' % vcd)
        print('      ISP_ASSOCIATE_DCB   %d  drive %s' % (vassoc,
              chr(65 + vdrive) + ':' if vdrive else '-'))
        print('    ...claimed by us       %d' % want)
        print('    ISP_result of insert   %d%s' % (isp, '' if isp == 0 else '   <- IOS REFUSED'))
        print('    Port_request entries   %d' % entry)
        print('    XTIDE_StartRequest     %d' % start)
        print('    DCB unit_on_ctl        %d' % unit)
        print('    request DCB            %08X' % iopdcb)
        print('    DCB we inserted on     %08X%s' % (ourdcb,
              '   <- MATCH' if ourdcb and ourdcb == iopdcb else
              '   <- DIFFERENT' if iopdcb else ''))
        print('    requests refused       %d' % refused)
        print('    last stage reached     %d  %s' % (stage, STAGE.get(stage, '?')))
        print('    IOP_original_dcb       %08X' % origdcb)
        print('    calldown entry DDB     %08X' % cdddb)
        print('    our DDB                %08X%s' % (ourddb,
              '   <- OURS' if ourddb and ourddb == cdddb else
              '   <- NOT OURS' if cdddb else ''))
        print('    request pointer        %08X' % iop)
        for i, (nm, v) in enumerate(zip(['IOP_physical','IOP_physical_dcb','IOP_original_dcb','timer/timer_orig','IOP_calldown_ptr','IOP_callback_ptr','IOP_voltrk_private','IOP_Thread_Handle'], raw)):
            print('      +%02X %-20s %08X' % (i * 4, nm, v))
        if start:
            print('    first requests serviced:')
            for i in range(min(start, 8)):
                print('      %d  %-6s LBA %d' % (i + 1,
                      FUNC.get(ring[i*2], '?%d' % ring[i*2]), ring[i*2+1]))
            print('    last request           %s(%d) lba=%d sectors=%d unit=%d'
                  % (FUNC.get(func, '?'), func, lba, xfer, unit))
    return 0


sys.exit(main())
