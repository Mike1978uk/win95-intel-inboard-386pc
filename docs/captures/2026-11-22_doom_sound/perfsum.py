"""Summarise one PERFLOG run: PERFLOG.CSV path, label prefix."""
import csv
import sys

path, prefix = sys.argv[1], sys.argv[2]
rows = list(csv.reader(open(path, newline='', encoding='latin1')))
hi = max(i for i, r in enumerate(rows) if r and r[0] == 'label')
h = rows[hi]
data = [r for r in rows[hi + 1:] if r and r[0].startswith(prefix)]
print('rows', len(data), 'SWAPCOMP columns:', sum('SWAPCOMP' in c for c in h))
want = ['ms', 'cpgFree', 'cPageIns', 'cPageOuts', 'cpgSwapfileInUse',
        'cpgDiskcache', 'cpgLocked', 'CPUUsage', 'BReadsSec']
ix = [next(i for i, c in enumerate(h) if c.split(chr(92))[-1] == w) for w in want]
print('\t'.join(want))
step = int(sys.argv[3]) if len(sys.argv) > 3 else 10
for r in data[::step] + data[-3:]:
    print('\t'.join(r[i] for i in ix))
