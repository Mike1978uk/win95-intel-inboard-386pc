# WKdm compressed size per 4 KB page (Wilson & Kaplan 1999): each 32-bit word
# gets a 2-bit tag - zero, exact dictionary hit (4-bit index), partial hit on
# the high 22 bits (4-bit index + 10 low bits), or miss (32 bits). Dictionary:
# 16 entries, direct-mapped on the high bits.
import struct, sys
P = 4096
d = open(sys.argv[1] if len(sys.argv) > 1 else 'D:/WIN386.SWP', 'rb').read()
rest = [d[i:i+P] for i in range(0, len(d), P) if d[i:i+P].count(0) != P]
def wk(p):
    dic = [0] * 16; bits = 0; tags = {0: 0, 1: 0, 2: 0, 3: 0}
    for w in struct.unpack('<1024I', p):
        if w == 0: t = 0
        else:
            hi = w >> 10; s = (hi ^ (hi >> 4) ^ (hi >> 8)) & 15
            if dic[s] == w: t = 1; bits += 4
            elif dic[s] >> 10 == hi: t = 2; bits += 14
            else: t = 3; bits += 32
            dic[s] = w
        tags[t] += 1
    return 256 + (bits + 7) // 8, tags
sizes = []; tot = {0: 0, 1: 0, 2: 0, 3: 0}
for p in rest:
    n, t = wk(p); sizes.append(min(n, P))
    for k in t: tot[k] += t[k]
words = sum(tot.values())
print(f'non-zero pages {len(rest)}: WKdm ratio {len(rest)*P/sum(sizes):.2f}:1')
print('  words: zero {:.0f}%  exact {:.0f}%  partial {:.0f}%  miss {:.0f}%'.format(*[tot[k]*100/words for k in range(4)]))
h = [0] * 4
for n in sizes: h[min(3, int(n / P * 4))] += 1
print('  pages <25% <50% <75% >=75%:', ' '.join(f'{x*100/len(sizes):.0f}%' for x in h))
