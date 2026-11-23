"""Frame time between M8CENSUS readings: wall (the "now" line) against the driver's own time.

    python frames.py M8CENSUS.TXT label-prefix [PERFLOG.CSV perflog-label]

ours = Direct3D draw calls, flip = Flip (engine wait included). With a PERFLOG, page-ins between
the readings, matched by the host-independent order of the two logs (PERFLOG ms from its start,
census from its own clock): give the census reading's offset from PERFLOG start with t0= if known.
"""
import csv
import sys

path, prefix = sys.argv[1], sys.argv[2]
reads = []
cur = None
for line in open(path, encoding='latin1'):
    w = line.split()
    if line.startswith('== M8CENSUS '):
        cur = {'label': w[2]} if w[2].startswith(prefix) else None
        if cur is not None:
            reads.append(cur)
    elif cur is not None and w and w[0] == 'now':
        cur['now'] = int(w[1]) * 2 ** 32 + int(w[2])
    elif cur is not None and w and w[0] == 'clock':
        cur.update(clock=int(w[1]), ours=int(w[3]), flip=int(w[5]), flips=int(w[9]))

print('from    to      wall s  flips  wall ms/f  ours ms/f  flip ms/f  rest ms/f')
for a, b in zip(reads, reads[1:]):
    c = a['clock']
    n = b['flips'] - a['flips']
    wall = (b['now'] - a['now']) / c
    ours = ((b['ours'] - a['ours']) % 2 ** 32) / c
    flip = ((b['flip'] - a['flip']) % 2 ** 32) / c
    if n:
        print(f"{a['label']:7} {b['label']:7} {wall:6.1f} {n:6} {1000 * wall / n:10.0f} {1000 * ours / n:10.0f}"
              f" {1000 * flip / n:10.0f} {1000 * (wall - ours - flip) / n:10.0f}")
    else:
        print(f"{a['label']:7} {b['label']:7} {wall:6.1f} {n:6}")
