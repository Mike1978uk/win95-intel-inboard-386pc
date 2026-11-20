"""M8SHOW.RAW (640x480 palette indices) -> PNG, palette rebuilt as m8show.c build_palette."""
import sys, struct, zlib

def palette():
    rgb = [(0, 0, 0)] * 256
    hue = [(1.00, 0.55, 0.15), (0.25, 0.65, 1.00)]
    fog = (0.55, 0.62, 0.80)
    for z in range(15):
        f = 0.75 * z / 14
        for c in range(16):
            lum = 0.25 + 0.75 * (c & 7) / 7.0
            rgb[z * 16 + c] = tuple(int((hue[c >> 3][k] * lum * (1 - f) + fog[k] * f) * 63 + 0.5) for k in range(3))
    for c in range(8):
        t = c / 7
        rgb[240 + c] = (int((0.08 + t * (fog[0] - 0.08)) * 63 + .5), int((0.12 + t * (fog[1] - 0.12)) * 63 + .5),
                        int((0.35 + t * (fog[2] - 0.35)) * 63 + .5))
    rgb[248] = (58, 58, 54); rgb[249] = (14, 18, 30); rgb[250] = (10, 20, 12)
    rgb[251] = (18, 24, 34); rgb[252] = (63, 63, 20); rgb[253] = (0, 0, 0)
    return [tuple(v * 255 // 63 for v in p) for p in rgb]

def main(src, dst):
    data = open(src, 'rb').read()
    w, h = 640, 480
    pal = palette()
    rows = b''.join(b'\0' + bytes(c for i in data[y * w:(y + 1) * w] for c in pal[i]) for y in range(h))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b'')
    open(dst, 'wb').write(png)

main(sys.argv[1], sys.argv[2])
