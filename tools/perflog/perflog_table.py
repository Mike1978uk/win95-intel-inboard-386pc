"""Summarise a PERFLOG.CSV: one line every N samples with the memory counters
that matter on a 5 MB machine. Sizes in MB; page-ins, page-outs and discards
are running counts, so the change between lines is the activity.

    python tools/perflog/perflog_table.py PERFLOG.CSV [every] [label-prefix]

PERFLOG appends, and each run writes its own header: the counter set differs
between builds (the checksum build adds one), so each row is read against the
header line before it, never the file's first.
"""
import csv
import sys

BS = chr(92)


def read_rows(path, prefix=''):
    rows, names = [], None
    for f in csv.reader(open(path)):
        if not f:
            continue
        if f[0] == 'label':
            names = f
        elif names and f[0].startswith(prefix):
            rows.append(dict(zip(names, f)))
    return rows


def main():
    every = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    rows = read_rows(sys.argv[1], sys.argv[3] if len(sys.argv) > 3 else '')

    def g(row, k):
        return int(row.get(k) or 0)

    def vmm(row, k):
        return g(row, 'VMM' + BS + k)

    mb = 1 << 20
    print('   t  pg-in pg-out  disc  swap-use  free  dcache  commit  locked  cpu')
    for i, row in enumerate(rows):
        if i % every and i != len(rows) - 1:
            continue
        print('%4d %6d %6d %5d %9.2f %5.2f %7.2f %7.2f %7.2f %4d' % (
            g(row, 'ms') // 1000, vmm(row, 'cPageIns'), vmm(row, 'cPageOuts'),
            vmm(row, 'cDiscards'), vmm(row, 'cpgSwapfileInUse') / mb,
            vmm(row, 'cpgFree') / mb, vmm(row, 'cpgDiskcache') / mb,
            vmm(row, 'cpgCommit') / mb, vmm(row, 'cpgLocked') / mb,
            g(row, 'KERNEL' + BS + 'CPUUsage')))


if __name__ == '__main__':
    main()
