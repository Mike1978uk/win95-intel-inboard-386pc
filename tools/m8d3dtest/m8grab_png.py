# Pictures from M8D3DF's grab file: each test's two pages as the card held them, side by side.
#
#   python -I m8grab_png.py M8D3DF.GRB <out dir>
#
# Each record: 16-byte name, 768 bytes of 6-bit DAC palette, then two pages of 320 x 240 indices.
import os
import struct
import sys
import zlib

W, H = 320, 240


def png(path, w, h, rgb):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 6)))
        f.write(chunk(b'IEND', b''))


def main():
    data = open(sys.argv[1], 'rb').read()
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    rec = 16 + 768 + 2 * W * H
    for i in range(len(data) // rec):
        r = data[i * rec:(i + 1) * rec]
        name = r[:16].split(b'\0')[0].decode('ascii', 'replace')
        pal = bytes(min(255, b * 4 + (b >> 4)) for b in r[16:784])
        a = r[784:784 + W * H]
        b = r[784 + W * H:]
        rows = bytearray()
        for y in range(H):
            for page in (a, b):
                for v in page[y * W:(y + 1) * W]:
                    rows += pal[v * 3:v * 3 + 3]
        path = os.path.join(out, '%02d_%s.png' % (i + 1, name))
        png(path, 2 * W, H, bytes(rows))
        print(path)


if __name__ == '__main__':
    main()
