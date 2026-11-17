"""Count the distinct face shapes flip2d's spinning cube draws, by replaying the
sample's own maths (references/dx61samples/ddraw/src/flip2d: flipcube.cpp, dumb3d.cpp).
A face's pixels depend only on its four whole-pixel corners, so two faces have the
same mask exactly when their corners match after a shift."""
import math, sys

def ident():
    return [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]

def rot(m, axis, deg):
    r = (deg / 360) * math.pi * 2
    s, c = math.sin(r), math.cos(r)
    a, b, sa, sb = {'x': (1, 2, s, -s), 'y': (0, 2, -s, s), 'z': (0, 1, s, -s)}[axis]
    for i in range(4):
        ta = m[i][a] * c + m[i][b] * sa
        tb = m[i][a] * sb + m[i][b] * c
        m[i][a], m[i][b] = ta, tb

def trans(m, col, d):
    for i in range(4):
        m[i][3] = m[i][col] * d + m[i][3]

def mul(m, v):
    return [sum(m[i][k] * v[k] for k in range(4)) for i in range(4)]

def mmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0], 0.0]

def norm(v):
    l = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    return [v[0]/l, v[1]/l, v[2]/l, v[3]]

verts = [[-10, 10, -10], [-10, 10, 10], [10, 10, 10], [10, 10, -10],
         [10, -10, -10], [10, -10, 10], [-10, -10, 10], [-10, -10, -10]]
verts = [[float(x), float(y), float(z), 1.0] for x, y, z in verts]
faces = [(0, 1, 2, 3), (2, 1, 6, 5), (3, 2, 5, 4), (0, 3, 4, 7), (1, 0, 7, 6), (4, 5, 6, 7)]

spin = ident(); rot(spin, 'y', 6.0); rot(spin, 'x', 3.5); rot(spin, 'z', 2.0)
tr = spin[0][0] + spin[1][1] + spin[2][2]
angle = math.degrees(math.acos(max(-1, min(1, (tr - 1) / 2))))

vp = [60.0, 60.0, 60.0]
vd = norm([-vp[0], -vp[1], -vp[2], 0.0])
view = ident(); trans(view, 0, -vp[0]); trans(view, 1, -vp[1]); trans(view, 2, -vp[2])
right = [-x for x in cross(vd, [0.0, 1.0, 0.0, 0.0])]
up = cross(right, vd)
look = ident()
for c in range(3):
    look[0][c], look[1][c], look[2][c] = right[c], up[c], vd[c]
view = mmul(look, view)
persp = ident(); persp[3][2] = 1 / 300.0; persp[3][3] = 0.0; persp[0][0] = 3.5; persp[1][1] = 3.5

frames = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
xo, yo = 320, 240
shapes, frames_seen = {}, set()
first_repeat = None
drawn = 0
for f in range(frames):
    verts = [mul(spin, v) for v in verts]
    scr = []
    for v in verts:
        t = mul(persp, mul(view, v))
        scr.append((int(t[0] / t[3]) + xo, int(t[1] / t[3]) + yo))
    vis = []
    for fc in faces:
        p0, p1, p2, p3 = (scr[i] for i in fc)
        v1, w1 = p2[0] - p1[0], p0[0] - p1[0]
        v2, w2 = p2[1] - p1[1], p0[1] - p1[1]
        if v1 * w2 - v2 * w1 <= 0:
            continue
        mx = min(p[0] for p in (p0, p1, p2, p3)); my = min(p[1] for p in (p0, p1, p2, p3))
        key = tuple((p[0] - mx, p[1] - my) for p in (p0, p1, p2, p3))
        shapes[key] = shapes.get(key, 0) + 1
        vis.append(key)
        drawn += 1
    fk = tuple(scr)
    if fk in frames_seen and first_repeat is None:
        first_repeat = f
    frames_seen.add(fk)

print(f"spin per frame: {angle:.4f} degrees about one fixed axis")
print(f"frames {frames}, faces drawn {drawn} ({drawn / frames:.2f} a frame)")
print(f"distinct face shapes {len(shapes)}; reused draws {drawn - len(shapes)} ({100 * (drawn - len(shapes)) / drawn:.1f} %)")
print(f"distinct whole frames {len(frames_seen)}; first exact frame repeat at {first_repeat}")
for n in (100, 1000, 5000):
    if n <= frames:
        pass
top = sorted(shapes.values(), reverse=True)[:5]
print(f"most-used shapes drawn {top} times")

# Reuse as the run builds up, and what the masks would cost as 1-bit images.
def rerun(n):
    global verts
    vs = [[float(x), float(y), float(z), 1.0] for x, y, z in
          [[-10, 10, -10], [-10, 10, 10], [10, 10, 10], [10, 10, -10],
           [10, -10, -10], [10, -10, 5 * 2], [-10, -10, 10], [-10, -10, -10]]]
    seen, d, sizes = set(), 0, {}
    curve = {}
    for f in range(n):
        vs = [mul(spin, v) for v in vs]
        scr = []
        for v in vs:
            t = mul(persp, mul(view, v))
            scr.append((int(t[0] / t[3]) + xo, int(t[1] / t[3]) + yo))
        for fc in faces:
            p = [scr[i] for i in fc]
            v1, w1 = p[2][0] - p[1][0], p[0][0] - p[1][0]
            v2, w2 = p[2][1] - p[1][1], p[0][1] - p[1][1]
            if v1 * w2 - v2 * w1 <= 0:
                continue
            mx = min(q[0] for q in p); my = min(q[1] for q in p)
            k = tuple((q[0] - mx, q[1] - my) for q in p)
            w = max(q[0] for q in p) - mx + 1; h = max(q[1] for q in p) - my + 1
            sizes[k] = ((w + 7) // 8) * h
            d += 1
            seen.add(k)
        if f + 1 in (50, 100, 300, 1000, 3000):
            curve[f + 1] = (d, len(seen), sum(sizes.values()))
    return curve
for n, (d, s, b) in rerun(3000).items():
    print(f"after {n:5d} frames: draws {d:6d}, new shapes {s:5d}, reuse {100 * (d - s) / d:5.1f} %, masks {b / 1024:7.1f} KB")
