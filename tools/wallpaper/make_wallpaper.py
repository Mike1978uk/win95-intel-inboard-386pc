"""DirectXT wallpaper: 800x600, a Penrose rhomb tiling under block letters.

Writes DXTWALL.BMP (24-bit, what Windows 95 loads as wallpaper) and DXTWALL.png (preview).
The background is the one desktop area that is repainted and never copied, so it is where
the aperiodic dither belongs (worklist X34/X35a); the gradients are there to show it.

    python make_wallpaper.py [outdir]
"""
import cmath
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 800, 600
PHI = (1 + 5 ** 0.5) / 2
FONT = r"C:\Windows\Fonts\ariblk.ttf"


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def penrose(subdivisions, radius, centre):
    """Robinson triangles subdivided from a wheel of ten; pairs make P3 rhombs."""
    tris = []
    for i in range(10):
        b = cmath.rect(radius, (2 * i - 1) * math.pi / 10) + centre
        c = cmath.rect(radius, (2 * i + 1) * math.pi / 10) + centre
        if i % 2 == 0:
            b, c = c, b  # mirror every second triangle so neighbours match
        tris.append((0, centre, b, c))
    for _ in range(subdivisions):
        out = []
        for colour, a, b, c in tris:
            if colour == 0:  # thin-rhomb half
                p = a + (b - a) / PHI
                out += [(0, c, p, b), (1, p, c, a)]
            else:  # thick-rhomb half
                q = b + (a - b) / PHI
                r = b + (c - b) / PHI
                out += [(1, r, c, a), (1, q, r, b), (0, r, q, a)]
        tris = out
    return tris


def main(outdir):
    outdir.mkdir(parents=True, exist_ok=True)

    # Background: a vertical gradient, night blue to deep teal.
    img = Image.new("RGB", (W, H))
    px = img.load()
    top, bottom = (12, 18, 48), (6, 70, 86)
    for y in range(H):
        row = lerp(top, bottom, y / (H - 1))
        for x in range(W):
            px[x, y] = row

    # Tiling: each rhomb shaded by its height so the gradient runs through it too.
    tiles = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    td = ImageDraw.Draw(tiles)
    centre = complex(W * 0.62, H * 0.45)
    tris = penrose(6,1.25 * math.hypot(W, H) / 2, centre)
    for colour, a, b, c in tris:
        pts = [(z.real, z.imag) for z in (a, b, c)]
        t = min(max(a.imag / H, 0), 1)
        if colour == 0:
            fill = lerp((40, 52, 120), (20, 120, 130), t) + (110,)
        else:
            fill = lerp((24, 30, 80), (10, 90, 104), t) + (90,)
        td.polygon(pts, fill=fill)
    # Edges: draw only the rhomb sides (a-b and a-c), not the shared base b-c.
    for colour, a, b, c in tris:
        for p, q in ((a, b), (a, c)):
            td.line([(p.real, p.imag), (q.real, q.imag)], fill=(150, 190, 230, 70), width=1)
    img = Image.alpha_composite(img.convert("RGBA"), tiles)

    # Letters: block capitals, gradient fill, outline and soft shadow.
    text = "DirectXT"
    font = ImageFont.truetype(FONT, 120)
    probe = ImageDraw.Draw(img)
    l, t, r, b = probe.textbbox((0, 0), text, font=font)
    tw, th = r - l, b - t
    ox, oy = (W - tw) // 2 - l, int(H * 0.36) - th // 2 - t

    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).text((ox, oy), text, font=font, fill=255)

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shadow.putalpha(mask.filter(ImageFilter.GaussianBlur(6)).point(lambda v: v * 0.8))
    img.alpha_composite(shadow, (8, 10))

    outline = mask.filter(ImageFilter.MaxFilter(7))
    img.paste((8, 10, 30, 255), (0, 0), outline)

    grad = Image.new("RGBA", (W, H))
    gp = grad.load()
    for y in range(H):
        ty = min(max((y - (oy + t)) / max(th, 1), 0), 1)
        for x in range(W):
            tx = min(max((x - ox) / max(tw, 1), 0), 1)
            base = lerp((255, 214, 64), (255, 92, 40), ty)  # gold at the top, orange below
            gp[x, y] = lerp(base, (236, 64, 122), tx * 0.45) + (255,)
    img.paste(grad, (0, 0), mask)

    # Subtitle.
    small = ImageFont.truetype(FONT, 22)
    sub = "Windows 95 on an IBM 5160  -  Inboard 386/PC  -  ATI Mach8"
    letters_bottom = oy + t + th
    l, t, r, b = probe.textbbox((0, 0), sub, font=small)
    sx, sy = (W - (r - l)) // 2 - l, letters_bottom + 34 - t
    d = ImageDraw.Draw(img)
    d.text((sx + 2, sy + 2), sub, font=small, fill=(0, 0, 0, 200))
    d.text((sx, sy), sub, font=small, fill=(200, 225, 245, 255))

    img = img.convert("RGB")
    img.save(outdir / "DXTWALL.BMP")
    img.save(outdir / "DXTWALL.png")
    print(outdir / "DXTWALL.BMP", (outdir / "DXTWALL.BMP").stat().st_size, "bytes")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "out")
