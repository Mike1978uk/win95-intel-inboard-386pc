# LZ4 block-format size estimate per 4 KB page: token byte, literal run,
# 2-byte offset, extended lengths; min match 4. 'fast' = first hash hit,
# 'hc' = best of all earlier positions with the same 4-byte prefix.
import random, sys
P = 4096
d = open(sys.argv[1] if len(sys.argv) > 1 else 'D:/WIN386.SWP', 'rb').read()
pages = [d[i:i+P] for i in range(0, len(d), P) if d[i:i+P].count(0) != P]
def ext(n):            # bytes for an extended length field beyond 15
    return 0 if n < 15 else 1 + (n - 15) // 255
def lz4(p, hc):
    chains = {}; i = 0; lit = 0; out = 0; n = len(p)
    while i < n:
        best, bpos = 0, 0
        if i + 4 <= n - 5:          # LZ4: last 5 bytes are literals
            k = p[i:i+4]
            cands = chains.get(k, [])
            for j in (reversed(cands) if hc else cands[-1:]):
                m = 4
                while i + m < n - 5 and p[j+m] == p[i+m]: m += 1
                if m > best: best, bpos = m, j
                if not hc: break
            chains.setdefault(k, []).append(i)
        if best >= 4:
            out += 1 + ext(lit) + lit + 2 + ext(best - 4)
            for q in range(i + 1, min(i + best, n - 4)):
                chains.setdefault(p[q:q+4], []).append(q)
            i += best; lit = 0
        else:
            lit += 1; i += 1
    out += 1 + ext(lit) + lit
    return min(out, P)
random.seed(1); samp = random.sample(pages, min(400, len(pages)))
for hc in (False, True):
    s = [lz4(p, hc) for p in samp]
    print(('LZ4-HC  ' if hc else 'LZ4 fast'), f'{len(s)*P/sum(s):.2f}:1')
