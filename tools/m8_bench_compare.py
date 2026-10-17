"""Set two M8BENCH runs side by side: operations per second, the ratio, and whether
the pixels matched (equal hashes of the same fixed sequence).

Usage: m8_bench_compare.py <M8BENCH.TXT> <label A> <label B>
The last run with each label is used. The ratio is B / A: above 1, B is faster.
"""
import sys


def runs(path):
    out = {}
    cur = None
    for line in open(path, errors='replace'):
        p = line.split()
        if not p:
            continue
        if p[0] == '==':
            cur = {'head': line.strip(), 'tests': {}}
            out[p[1]] = cur
        elif cur is not None and len(p) >= 7 and p[2] == 'ops/s':
            cur['tests'][p[0]] = (float(p[1]), float(p[3]), p[6])
    return out


def main(path, a, b):
    r = runs(path)
    for lab in (a, b):
        if lab not in r:
            sys.exit(f'no run labelled {lab}; labels: {", ".join(r)}')
    ra, rb = r[a], r[b]
    print(ra['head'])
    print(rb['head'])
    print(f"{'test':16} {a + ' ops/s':>14} {b + ' ops/s':>14} {'B/A':>7}  pixels")
    worse = better = differ = 0
    for t, (oa, ma, ha) in ra['tests'].items():
        if t not in rb['tests']:
            continue
        ob, mb, hb = rb['tests'][t]
        ratio = ob / oa if oa else 0
        same = 'same' if ha == hb else 'DIFFER'
        worse += ratio < 0.95
        better += ratio > 1.05
        differ += ha != hb
        flag = '  <--' if ratio < 0.95 else ''
        print(f'{t:16} {oa:14.1f} {ob:14.1f} {ratio:7.2f}  {same}{flag}')
    print(f'{b} faster in {better}, slower in {worse}; pixels differ in {differ} tests')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
