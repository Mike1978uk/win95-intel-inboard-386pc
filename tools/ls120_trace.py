#!/usr/bin/env python3
"""Parse an LS-120 miniport trace ring into ordered records.

Input is a raw dump of LS_TRACE_BASE (0B9000h by default), however it was
collected - COMrade mem_read, or LSTRACE.SCR writing C:\\LSTRACE.LOG on the
guest. See docs/ls120_trace_channel.md.

The ring is 200 x 16-byte ASCII records after a 16-byte header. The header's
record count keeps climbing past the ring size, so this can say how many
records were lost to wrap rather than quietly showing a partial picture.

    python tools/ls120_trace.py LSTRACE.LOG
"""

import sys

MAGIC = b'LS12'
RECS = 200
RECLEN = 16
HDR = 16


def parse(blob):
    if len(blob) < HDR + RECS * RECLEN:
        raise SystemExit(
            'dump is %d bytes, need at least %d - collect the whole ring'
            % (len(blob), HDR + RECS * RECLEN))

    if blob[0:4] != MAGIC:
        # Not a failure to parse - a finding. Say which of the two it is.
        raise SystemExit(
            'NO MAGIC at +0 (found %r).\n'
            'Either the driver never reached its first TRACE, or the channel\n'
            'does not retain - and those are different problems. Run the\n'
            'poison test in docs/ls120_trace_channel.md before reading this\n'
            'as "the driver did not run".' % blob[0:4])

    count = int.from_bytes(blob[4:8], 'little')
    lost = max(0, count - RECS)
    shown = min(count, RECS)
    first = count % RECS if count > RECS else 0

    out = []
    for i in range(shown):
        slot = (first + i) % RECS
        off = HDR + slot * RECLEN
        rec = blob[off:off + RECLEN]
        tag = rec[0:4].decode('latin1')
        val = rec[5:13].decode('latin1')
        ok = rec[4:5] == b'=' and all(c in b'0123456789ABCDEF' for c in rec[5:13])
        out.append((count - shown + i, tag, val, ok))
    return count, lost, out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    blob = open(sys.argv[1], 'rb').read()
    count, lost, recs = parse(blob)

    print('records written : %d' % count)
    print('lost to wrap    : %d' % lost)
    print('shown           : %d' % len(recs))
    print()
    for n, tag, val, ok in recs:
        print('%5d  %-4s = %s%s' % (n, tag, val, '' if ok else '   <- MALFORMED'))

    tags = [t for _, t, _, _ in recs]
    print()
    if 'CDB0' not in tags:
        print('No CDB0 record: SCSIPORT never sent this adapter a command.')
    elif not any(t == 'CDB0' and v.endswith('12') for _, t, v, _ in recs):
        print('CDB0 records present but no 12h: INQUIRY was never issued.')
    else:
        print('INQUIRY was issued - read the DONE that follows it.')


if __name__ == '__main__':
    main()
