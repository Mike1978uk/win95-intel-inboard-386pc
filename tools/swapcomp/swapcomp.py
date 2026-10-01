# Compressibility of WIN386.SWP, 4 KB pages. zlib level 1 (deflate) and a
# byte-oriented LZ with no entropy coding, the kind a 486 decompresses cheaply.
import zlib, random, sys
P = 4096
d = open(sys.argv[1] if len(sys.argv) > 1 else 'D:/WIN386.SWP', 'rb').read()
pages = [d[i:i+P] for i in range(0, len(d), P)]
zero = [p for p in pages if p.count(0) == P]
rest = [p for p in pages if p.count(0) != P]
def lz(p):
    # LZ77, 4 KB window, 3-byte hash; literal run or (offset, length) token.
    # Cost model: 1 flag bit per token, literal 8 bits, match 16 bits.
    h = {}; i = 0; bits = 0
    while i < len(p):
        k = p[i:i+3]; j = h.get(k); best = 0
        if j is not None:
            while i+best < len(p) and best < 18 and p[j+best] == p[i+best]: best += 1
        h[k] = i
        if best >= 3: bits += 17; i += best
        else: bits += 9; i += 1
    return (bits + 7) // 8
z = [len(zlib.compress(p, 1)) for p in rest]
random.seed(1); samp = random.sample(rest, min(400, len(rest)))
l = [lz(p) for p in samp]
def hist(xs):
    b = [0]*5
    for x in xs:
        r = x / P
        b[0 if r < .25 else 1 if r < .5 else 2 if r < .75 else 3 if r < 1 else 4] += 1
    return ' '.join(f'{n*100/len(xs):4.0f}%' for n in b)
print(f'pages {len(pages)}, all-zero {len(zero)} ({len(zero)*100/len(pages):.0f}%), non-zero {len(rest)}')
print(f'non-zero, zlib-1:  ratio {len(rest)*P/sum(z):.2f}:1   <25% <50% <75% <100% >=100%: {hist(z)}')
print(f'non-zero, LZ  :    ratio {len(samp)*P/sum(l):.2f}:1   <25% <50% <75% <100% >=100%: {hist(l)}  (sample {len(samp)})')
