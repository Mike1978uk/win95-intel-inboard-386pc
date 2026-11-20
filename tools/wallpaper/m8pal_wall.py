"""Dither a picture into the Mach8 driver's 16 bpp palette (fb16.c m8_fb16_tables), as an
8-bit BMP. Every colour in it is a palette entry, so the 16 bpp desktop shows its pixels as
they are, with no second dither: 480 KB at 800x600 against 1.44 MB for 24-bit.

    python m8pal_wall.py in.bmp out.bmp preview.png

The palette here must match m8_fb16_tables; change both together.
"""import struct, sys, numpy as np
from PIL import Image
rb = [0, 64, 128, 192, 255]; gl = [0, 32, 64, 96, 128, 160, 192, 224, 255]
pal = [None] * 256
for c in range(225):
    i = c + 15 if c < 112 else 127 if c == 112 else c + 16
    pal[i] = (rb[c // 45], gl[c // 5 % 9], rb[c % 5])
pal[128] = (128, 128, 128); pal[0] = (0, 0, 0); pal[255] = (255, 255, 255)
for k, v in enumerate([8, 16, 24, 32, 40, 48, 56, 72, 80, 88, 96, 104, 112, 120]):
    pal[k + 1] = (v, v, v); pal[254 - k] = (255 - v,) * 3
assert all(p is not None for p in pal)
P = np.array(pal, float)
src = np.asarray(Image.open(sys.argv[1]).convert('RGB'), float)
h, w, _ = src.shape
img = src.copy(); out = np.zeros((h, w), np.uint8)
for y in range(h):                               # Floyd-Steinberg, serpentine
    xs = range(w) if y % 2 == 0 else range(w - 1, -1, -1); d = 1 if y % 2 == 0 else -1
    for x in xs:
        old = np.clip(img[y, x], 0, 255)
        i = int(((P - old) ** 2).sum(1).argmin()); out[y, x] = i
        e = old - P[i]
        if 0 <= x + d < w: img[y, x + d] += e * 7 / 16
        if y + 1 < h:
            if 0 <= x - d < w: img[y + 1, x - d] += e * 3 / 16
            img[y + 1, x] += e * 5 / 16
            if 0 <= x + d < w: img[y + 1, x + d] += e * 1 / 16
q = Image.fromarray(out, 'P'); q.putpalette(sum(map(list, pal), []))
q.save(sys.argv[2]); q.convert('RGB').save(sys.argv[3])
import os; print(os.path.getsize(sys.argv[2]), 'bytes,', len(set(out.flat)), 'palette entries used')
