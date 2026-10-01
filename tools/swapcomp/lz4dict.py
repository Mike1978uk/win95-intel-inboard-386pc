# LZ4 with a preset dictionary: pages from one half of the swap file form the
# dictionary, ratios are measured on 300 pages from the other half.
import random, sys, zlib
P = 4096
d = open(sys.argv[1] if len(sys.argv) > 1 else 'D:/WIN386.SWP', 'rb').read()
pages = [d[i:i+P] for i in range(0, len(d), P) if d[i:i+P].count(0) != P]
random.seed(2); random.shuffle(pages)
train, test = pages[:len(pages)//2], pages[len(pages)//2:][:300]
def ext(n): return 0 if n < 15 else 1 + (n - 15) // 255
def lz4(p, dic, hc):
    buf = dic + p; base = len(dic); n = len(buf); chains = {}
    for q in range(0, base - 3): chains.setdefault(buf[q:q+4], []).append(q)
    i = base; lit = 0; out = 0
    while i < n:
        best = 0
        if i + 4 <= n - 5:
            k = buf[i:i+4]; cands = chains.get(k, [])
            for j in (reversed(cands[-64:]) if hc else cands[-1:]):
                if i - j > 65535: continue
                m = 4
                while i + m < n - 5 and buf[j+m] == buf[i+m]: m += 1
                if m > best: best = m
            chains.setdefault(k, []).append(i)
        if best >= 4:
            out += 1 + ext(lit) + lit + 2 + ext(best - 4)
            for q in range(i + 1, min(i + best, n - 4)): chains.setdefault(buf[q:q+4], []).append(q)
            i += best; lit = 0
        else: lit += 1; i += 1
    out += 1 + ext(lit) + lit
    return min(out, P)
for kb in (0, 4, 16, 32, 60):
    dic = b''.join(train[:kb // 4])
    for hc in (False, True):
        s = [lz4(p, dic, hc) for p in test]
        print(f'dict {kb:2d} KB  {"HC  " if hc else "fast"}  {len(s)*P/sum(s):.2f}:1')
for kb in (0, 32):
    dic = b''.join(train[:kb // 4]); s = []
    for p in test:
        c = zlib.compressobj(9, zlib.DEFLATED, -15, 9, zlib.Z_DEFAULT_STRATEGY, dic) if dic else zlib.compressobj(9, zlib.DEFLATED, -15)
        s.append(min(P, len(c.compress(p) + c.flush())))
    print(f'deflate-9 dict {kb} KB  {len(s)*P/sum(s):.2f}:1')
