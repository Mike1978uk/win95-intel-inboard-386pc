#!/usr/bin/env python3
"""Compare two M8SEQ.BIN files (real card vs 86Box) and name the first command that differs.

    python m8seq_diff.py REAL.BIN BED.BIN [TEST.COM]

Checkpoint k holds the 8x8 corner block after Test Sequence 1's table was replayed up to,
not including, its k-th CMD write, then folded. If checkpoint k is the first to differ,
command k-1 (0-based) - the last one that ran - is the first the model gets wrong.
Given TEST.COM, the writes leading up to that command are printed with register names.
"""

import struct
import sys
from pathlib import Path

REC = 68


def load(p):
    d = Path(p).read_bytes()
    if d[:4] != b"M8S1":
        raise SystemExit("%s: not an M8SEQ result" % p)
    n = struct.unpack_from("<H", d, 4)[0]
    recs = [d[6 + k * REC:6 + (k + 1) * REC] for k in range(n + 1)]
    return n, recs


def cmd_writes(testcom):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    import testcom_tables as tt
    data = Path(testcom).read_bytes()
    ops, _ = tt.decode(data, 0x29A4)
    return ops, tt


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    na, a = load(sys.argv[1])
    nb, b = load(sys.argv[2])
    print("commands: %d / %d" % (na, nb))
    for k in range(min(len(a), len(b))):
        ta, tb = a[k][:2], b[k][:2]
        if a[k][4:] != b[k][4:] or ta != tb:
            print("first difference at checkpoint %d (after %d commands)" % (k, k))
            print("  timeouts idle/data: %d/%d vs %d/%d" % (ta[0], ta[1], tb[0], tb[1]))
            for r in range(8):
                ra, rb = a[k][4 + r * 8:12 + r * 8], b[k][4 + r * 8:12 + r * 8]
                print("  %d %s | %s %s" % (r, ra.hex(" "), rb.hex(" "), "" if ra == rb else "<--"))
            if len(sys.argv) > 3 and k > 0:
                ops, tt = cmd_writes(sys.argv[3])
                seen, start, end = 0, 0, len(ops)
                for i, (p, v) in enumerate(ops):
                    if p == 0x9ae8:
                        seen += 1
                        if seen == k - 1:
                            start = i + 1
                        if seen == k:
                            end = i + 1
                            break
                print("command %d and the writes since the one before it:" % (k - 1))
                for p, v in ops[start:end + 8]:
                    print("  WAIT_IDLE" if p == "WAIT_IDLE" else "  %-28s %04X" % (tt.name(p, v), v))
            return
    print("no difference in %d checkpoints" % min(len(a), len(b)))


if __name__ == "__main__":
    main()
