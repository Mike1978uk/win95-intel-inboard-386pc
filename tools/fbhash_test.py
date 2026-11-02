# Collisions of candidate 16 bpp piece fingerprints (fb16cv.asm M8_FB16_ROWDIFF) over
# changes a desktop and DirectDraw make: even complemented dwords, swaps, one-pixel
# shifts, small colour deltas. 16 dwords a piece. Run: python -I tools/fbhash_test.py
import random
M = 0xFFFFFFFF
N = 16  # dwords per piece


def rol(x, k):
    return ((x << k) | (x >> (32 - k))) & M


def fnv(p):
    h = 0x811C9DC5
    for d in p:
        h = ((h ^ d) * 0x01000193) & M
    return h or 1


def rotadd(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = (rol(h, k) + d) & M
        return h or 1
    return f


def rotaddxor(k):
    # two lanes: rotate-add and plain sum, folded at the end
    def f(p):
        h = 0x811C9DC5
        s = 0
        for d in p:
            h = (rol(h, k) + d) & M
            s = (s + d) & M
        return (h ^ rol(s, 16)) or 1
    return f


def fletch(p):
    a = b = 0
    for d in p:
        a = (a + d) & M
        b = (b + a) & M
    return (((a ^ rol(b, 16)) * 0x01000193) & M) or 1


def rotxadd(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = ((rol(h, k) ^ d) + d) & M
        return h or 1
    return f


def addrotfin(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = (rol(h, k) + d) & M
        return ((h * 0x01000193) & M) or 1
    return f


def arx_a(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = rol((h + d) & M, k) ^ d
        return h or 1
    return f


def arx_c(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = (rol(h ^ d, k) + d) & M
        return h or 1
    return f


def arx_d(k):
    # add, then xor with the rotated sum: carries and xor interleave
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = (h + d) & M
            h ^= rol(h, k)
            h = (h + 0x9E3779B9) & M
        return h or 1
    return f


def oaat(p):
    h = 0x811C9DC5
    for d in p:
        h = (h + d) & M
        h = (h + (h << 10)) & M
        h ^= h >> 6
    return h or 1


def arx_e(k):
    def f(p):
        h = 0x811C9DC5
        for d in p:
            h = (h + d) & M
            h ^= h >> k
            h = (h + rol(h, 16)) & M
        return h or 1
    return f


H = {'fnv': fnv, 'arx_d13': arx_d(13), 'oaat': oaat, 'arx_e13': arx_e(13), 'arx_e7': arx_e(7)}

def px2dw(px):
    return [px[2 * i] | (px[2 * i + 1] << 16) for i in range(N)]


def rnd_px(r):
    return [r.getrandbits(16) for _ in range(32)]


def text_px(r):
    fg, bg = r.choice([(0, 0xFFFF), (0xFFFF, 0), (0, 0xC618), (0x001F, 0xFFFF)])
    return [fg if r.random() < 0.3 else bg for _ in range(32)]


def grad_px(r):
    a, b = r.getrandbits(16), r.randint(-200, 200)
    return [(a + b * i) & 0xFFFF for i in range(32)]


def solid_px(r):
    c = r.getrandbits(16)
    return [c] * 32


def mods(r, px):
    out = []
    q = px[:]
    # complement an even number of dwords (pixel pairs)
    k = r.choice([2, 4, 6, 8])
    idx = r.sample(range(N), k)
    for i in idx:
        q[2 * i] ^= 0xFFFF
        q[2 * i + 1] ^= 0xFFFF
    out.append(('evenflip', q))
    q = px[:]
    i = r.randrange(32)
    q[i] = r.getrandbits(16)
    out.append(('onepx', q))
    q = px[:]
    i, j = r.sample(range(32), 2)
    q[i], q[j] = q[j], q[i]
    out.append(('swap', q))
    q = px[:]
    c = r.randint(1, 50)
    q = [(v + ((i * i - 31 * i) * c)) & 0xFFFF for i, v in enumerate(q)]
    out.append(('quad', q))
    q = px[1:] + px[:1]
    out.append(('shift1', q))
    q = px[:]
    a = r.randint(0, 31)
    b = r.randint(a, 31)
    col = r.getrandbits(16)
    for i in range(a, b + 1):
        q[i] = col
    out.append(('span', q))
    for steps in ((1, -32), (1, -2048), (32, -1), (1, -1), (2048, -32)):
        q = px[:]
        i, j = r.sample(range(32), 2)
        q[i] = (q[i] + steps[0]) & 0xFFFF
        q[j] = (q[j] + steps[1]) & 0xFFFF
        out.append(('delta%d_%d' % steps, q))
    q = px[:]
    for i in r.sample(range(32), 3):
        q[i] = (q[i] + r.choice((1, -1, 32, -32, 2048, -2048))) & 0xFFFF
    out.append(('delta3', q))
    return out


r = random.Random(1)
coll = {k: {} for k in H}
total = {}
gens = [rnd_px, text_px, grad_px, solid_px]
for t in range(200000):
    px = r.choice(gens)(r)
    base = px2dw(px)
    hb = {k: f(base) for k, f in H.items()}
    for name, q in mods(r, px):
        d = px2dw(q)
        if d == base:
            continue
        total[name] = total.get(name, 0) + 1
        for k, f in H.items():
            if f(d) == hb[k]:
                coll[k][name] = coll[k].get(name, 0) + 1
print('trials', total)
for k in H:
    print(k, coll[k])
