"""Replay engine writes from an M8W log; flag copies whose off-screen source was
partly overwritten since it was stored (stale cache/proxy reads).
Usage: m8_offscreen_audit.py <log> <screen width> <screen height>. "line" counts M8W lines."""
import os, sys, importlib.util, numpy as np
_s = importlib.util.spec_from_file_location('tally', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'm8_fallback_tally.py'))
tally = importlib.util.module_from_spec(_s); _s.loader.exec_module(tally)
SX, SY = int(sys.argv[2]), int(sys.argv[3])     # screen size
own = np.zeros((1024, 1024), np.int32)          # op id of the last write per pixel
reg = {}; who = 'start'; ops = {}; opn = 0; bad = 0; lineno = 0; mark = []
def wr(x, y, w, h, tag):
    global opn
    opn += 1; ops[opn] = (tag, x, y, w, h, lineno)
    x0, y0 = max(x, 0), max(y, 0); x1, y1 = min(x + w, 1024), min(y + h, 1024)
    if x1 > x0 and y1 > y0: own[y0:y1, x0:x1] = opn
for raw in tally.lines(sys.argv[1]):
    lineno += 1
    p = raw.split(); port = p[1]; val = int(p[2], 16)
    if port == '56EE':
        if (val & 0xFF00) == 0xB000 or (val & 0xFF00) == 0xA000: mark.append((lineno, val))
        if (val & 0xFF00) == 0xB000: who = hex(val)
        continue
    if port == 'BEE8':
        if val < 0x1000: reg['min'] = val
        continue
    reg[port] = val
    if port == '9AE8':
        x, y = reg.get('86E8', 0), reg.get('82E8', 0)
        w, h = reg.get('96E8', 0) + 1, reg.get('min', 0) + 1
        if val == 0x40F3 or val == 0x53B1:
            wr(x, y, w, h, f'{who} {"fill" if val == 0x40F3 else "host"}')
        elif (val & 0xE000) == 0xC000:
            dx, dy = reg.get('8EE8', 0), reg.get('8AE8', 0)
            sx, sy = x, y
            if not (val & 0x20): sx -= w - 1; dx -= w - 1
            if not (val & 0x80): sy -= h - 1; dy -= h - 1
            if sx >= SX or sy >= SY:              # source off-screen
                blk = own[max(sy,0):sy + h, max(sx,0):sx + w]
                ids = np.unique(blk)
                if len(ids) > 1 or (ids[0] and ops[ids[0]][1:5] != (sx, sy, w, h) and
                                     not (ops[ids[0]][1] <= sx and ops[ids[0]][2] <= sy)):
                    bad += 1
                    if bad <= 12:
                        print(f'line {lineno} {who}: copy from ({sx},{sy}) {w}x{h} to ({dx},{dy}); writers:')
                        for i in ids[:6]:
                            print('   ', ops.get(i, ('never written',)), int((blk == i).sum()), 'px')
            wr(dx, dy, w, h, f'{who} copy from ({sx},{sy})')
print('stale off-screen copies:', bad)
