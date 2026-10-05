#!/usr/bin/env python3
"""Emit M8GLYPH.COM, the one-glyph test for the Mach8 glyph cache (#49), and
decode the file it writes.

    python gen_m8glyph.py build  [out.com]     default M8GLYPH.COM beside this file
    python gen_m8glyph.py decode M8GLYPH.BIN

What it answers, on the real card:

  1. Which RD_MASK bit selects which plane for a colour-expand blit.  Richter
     and Smith's register table and XFree86's mach8cachemaskswapped[] both say
     plane n is bit (n+1) mod 8.  Eight blits measure all eight bits.
  2. Whether a 1-bit glyph stored in one plane of off-screen memory expands to
     the screen in a given colour, with the background left untouched - the
     whole glyph-cache mechanism, once.

Everything happens in the 8514 side's own memory, below line 900.  That memory
is separate from the VGA memory DOS is displaying, and the CRT, clock and
ADVFUNC_CNTL registers are never written, so the screen does not change.  The
engine is reset and its pitch set as XFree86's mach8 server does; the Windows
driver re-initialises all of it on its next start.  ROM_ADDR_1 (52EE), which
XFree86 also probes, is not touched: on this card it moves the video BIOS.

Every result is read back through PIX_TRANS and written to M8GLYPH.BIN.  The
destinations are filled with a known value first, so a blit that did nothing
cannot pass, and both sources are read back as controls for the fills and for
the read path itself.  Every wait is bounded; a timeout is counted, not hung on.

Register sequence and constants: XFree86 3.3.6 accel/mach8 (regmach8.h,
mach8.c, mach8init.c, mach8im.c, mach8fc.c), Kevin E. Martin, MIT-style
licence - see docs/mach8_text_and_driver_plan.md.  No XFree86 code is copied;
this emits 8086 machine code from the register sequence.
"""

import struct
import sys
from pathlib import Path

ORG = 0x100

# 8514/A and Mach8 registers (regmach8.h)
SUBSYS_CNTL = 0x42E8      # write
SUBSYS_STAT = 0x42E8      # read
CUR_Y = 0x82E8
CUR_X = 0x86E8
DESTY_AXSTP = 0x8AE8
DESTX_DIASTP = 0x8EE8
ERR_TERM = 0x92E8
MAJ_AXIS_PCNT = 0x96E8
GP_STAT = 0x9AE8          # read
CMD = 0x9AE8              # write
BKGD_COLOR = 0xA2E8
FRGD_COLOR = 0xA6E8
WRT_MASK = 0xAAE8
RD_MASK = 0xAEE8
BKGD_MIX = 0xB6E8
FRGD_MIX = 0xBAE8
MULTIFUNC_CNTL = 0xBEE8
PIX_TRANS = 0xE2E8
GE_OFFSET_LO = 0x6EEE
GE_OFFSET_HI = 0x72EE
GE_PITCH = 0x76EE

MIN_AXIS_PCNT = 0x0000    # MULTIFUNC_CNTL indexes
SCISSORS_T = 0x1000
SCISSORS_L = 0x2000
SCISSORS_B = 0x3000
SCISSORS_R = 0x4000
MEM_CNTL = 0x5000
PIX_CNTL = 0xA000
MIXSEL_EXPBLT = 0x00C0    # the source plane chosen by RD_MASK picks fg or bg mix
VRTCFG_4 = 0x0004
HORCFG_8 = 0x0002

GPCTRL_RESET = 0x8000
GPCTRL_ENAB = 0x4000
CHPTEST_NORMAL = 0x1000
EIGHT_PLANE = 0x0080      # SUBSYS_STAT: 1 MB fitted
GPBUSY = 0x0200
DATARDY = 0x0100

FSS_FRGDCOL = 0x0020
FSS_PCDATA = 0x0040
BSS_BKGDCOL = 0x0000
MIX_DST = 0x0003
MIX_SRC = 0x0007

CMD_FILL = 0x40B3         # CMD_RECT | INC_Y | INC_X | DRAW | PLANAR | WRTDATA
CMD_BLIT = 0xC0B3         # CMD_BITBLT | INC_Y | INC_X | DRAW | PLANAR | WRTDATA
CMD_READ = 0x53B0         # CMD_RECT | BYTSEQ | _16BIT | PCDATA | INC_Y | INC_X | DRAW

# Off-screen layout, all 8x8 at y = 900.
Y = 900
S1 = 0                    # plane-ID source: row r holds colour 1 << r
S2 = 16                   # glyph source: an F in plane GLYPH_PLANE, other planes solid
D_ID = [32 + 16 * k for k in range(8)]   # one destination per RD_MASK bit
D_ROT = 176               # glyph expanded with the rotated mask
D_UNROT = 192             # glyph expanded with the plain mask
GLYPH_PLANE = 5
F_RECTS = [(0, 0, 7, 1), (0, 1, 2, 6), (2, 3, 3, 1)]   # x, y, w, h

# Result file layout
R_MAGIC, R_ERR1, R_ERR2, R_STAT, R_TO_IDLE, R_TO_DATA, R_PRESENT = 0, 4, 6, 8, 10, 11, 12
R_ID = 16                 # 8 x 64 bytes
R_ROT = R_ID + 8 * 64
R_UNROT = R_ROT + 64
R_SRC2 = R_UNROT + 64
R_SRC1 = R_SRC2 + 64
RESLEN = R_SRC1 + 64


class Emitter:
    """Enough 8086 to express the test.  Labels resolve in a second pass and a
    rel8 that does not reach is an error, not a silent truncation."""

    def __init__(self):
        self.buf = bytearray()
        self.labels = {}
        self.fix8 = []
        self.fix16rel = []
        self.fixabs = []
        self.n = 0

    def uniq(self, stem):
        self.n += 1
        return "%s_%d" % (stem, self.n)

    def db(self, *b):
        self.buf.extend(b)

    def dw(self, w):
        self.buf.extend(struct.pack("<H", w & 0xFFFF))

    def label(self, name):
        assert name not in self.labels, name
        self.labels[name] = len(self.buf)

    def abs16(self, name, add=0):
        self.fixabs.append((len(self.buf), name, add))
        self.dw(0)

    def rel8(self, opcode, name):
        self.db(opcode)
        self.fix8.append((len(self.buf), name))
        self.db(0)

    def rel16(self, opcode, name):
        self.db(opcode)
        self.fix16rel.append((len(self.buf), name))
        self.dw(0)

    # instructions
    def mov_dx(self, v): self.db(0xBA); self.dw(v)
    def mov_ax(self, v): self.db(0xB8); self.dw(v)
    def mov_cx(self, v): self.db(0xB9); self.dw(v)
    def mov_bx_ax(self): self.db(0x89, 0xC3)
    def mov_di_lbl(self, name, add=0): self.db(0xBF); self.abs16(name, add)
    def mov_dx_lbl(self, name): self.db(0xBA); self.abs16(name)
    def out_ax(self): self.db(0xEF)
    def in_ax(self): self.db(0xED)
    def stosw(self): self.db(0xAB)
    def cld(self): self.db(0xFC)
    def ret(self): self.db(0xC3)
    def int_(self, n): self.db(0xCD, n)
    def test_ah(self, v): self.db(0xF6, 0xC4, v)
    def test_al(self, v): self.db(0xA8, v)
    def cmp_ax(self, v): self.db(0x3D); self.dw(v)
    def store_ax(self, name, add): self.db(0xA3); self.abs16(name, add)
    def store_al(self, name, add): self.db(0xA2); self.abs16(name, add)
    def inc_byte(self, name, add): self.db(0xFE, 0x06); self.abs16(name, add)
    def jz(self, name): self.rel8(0x74, name)
    def jnz(self, name): self.rel8(0x75, name)
    def loop(self, name): self.rel8(0xE2, name)
    def jmp(self, name): self.rel16(0xE9, name)
    def call(self, name): self.rel16(0xE8, name)

    def out16(self, port, value):
        self.mov_dx(port)
        self.mov_ax(value)
        self.out_ax()

    def resolve(self):
        for pos, name in self.fix8:
            d = self.labels[name] - (pos + 1)
            if not -128 <= d <= 127:
                raise SystemExit("rel8 to %s is %d - out of reach" % (name, d))
            self.buf[pos] = d & 0xFF
        for pos, name in self.fix16rel:
            d = self.labels[name] - (pos + 2)
            struct.pack_into("<h", self.buf, pos, d)
        for pos, name, add in self.fixabs:
            struct.pack_into("<H", self.buf, pos, ORG + self.labels[name] + add)
        return bytes(self.buf)


def build():
    e = Emitter()

    def idle():
        e.call("wait_idle")

    def fill(x, y, w, h, colour, wmask=0xFF):
        idle()
        e.out16(WRT_MASK, wmask)
        e.out16(MULTIFUNC_CNTL, PIX_CNTL)
        e.out16(FRGD_MIX, FSS_FRGDCOL | MIX_SRC)
        e.out16(FRGD_COLOR, colour)
        idle()
        e.out16(CUR_X, x)
        e.out16(CUR_Y, y)
        e.out16(MAJ_AXIS_PCNT, w - 1)
        e.out16(MULTIFUNC_CNTL, MIN_AXIS_PCNT | (h - 1))
        e.out16(CMD, CMD_FILL)

    def expand(src, dst, rdmask, fg, bgmix, bg=0):
        idle()
        e.out16(WRT_MASK, 0xFF)
        e.out16(RD_MASK, rdmask)
        e.out16(FRGD_COLOR, fg)
        e.out16(BKGD_COLOR, bg)
        e.out16(FRGD_MIX, FSS_FRGDCOL | MIX_SRC)
        e.out16(BKGD_MIX, BSS_BKGDCOL | bgmix)
        e.out16(MULTIFUNC_CNTL, PIX_CNTL | MIXSEL_EXPBLT)
        idle()
        e.out16(CUR_X, src)
        e.out16(CUR_Y, Y)
        e.out16(DESTX_DIASTP, dst)
        e.out16(DESTY_AXSTP, Y)
        e.out16(MAJ_AXIS_PCNT, 7)
        e.out16(MULTIFUNC_CNTL, MIN_AXIS_PCNT | 7)
        e.out16(CMD, CMD_BLIT)
        idle()
        e.out16(MULTIFUNC_CNTL, PIX_CNTL)

    def readback(x, slot):
        idle()
        # After any timeout, stop reading: a PIX_TRANS read with no data
        # behind it may hold the bus.  Go straight to writing the file.
        e.db(0xA1); e.abs16("res", R_TO_IDLE)     # mov ax,[timeouts]: both counters
        e.db(0x85, 0xC0)                          # test ax,ax
        go = e.uniq("go")
        e.jz(go)
        e.jmp("finish")
        e.label(go)
        e.out16(RD_MASK, 0xFF)              # a read takes only the planes RD_MASK enables
        e.out16(MULTIFUNC_CNTL, PIX_CNTL)
        e.out16(FRGD_MIX, FSS_PCDATA | MIX_SRC)
        e.out16(CUR_X, x)
        e.out16(CUR_Y, Y)
        e.out16(MAJ_AXIS_PCNT, 7)
        e.out16(MULTIFUNC_CNTL, MIN_AXIS_PCNT | 7)
        idle()
        e.out16(CMD, CMD_READ)
        e.mov_di_lbl("res", slot)
        e.mov_cx(32)                        # 64 pixels, two per word
        top = e.uniq("rd")
        e.label(top)
        e.db(0x51)                          # push cx
        e.call("wait_data")
        e.db(0x59)                          # pop cx
        e.mov_dx(PIX_TRANS)
        e.in_ax()
        e.stosw()
        e.loop(top)
        idle()
        e.out16(FRGD_MIX, FSS_FRGDCOL | MIX_SRC)

    # --- entry at 0100h: real code, no data first ---
    e.cld()

    # Is there an 8514 engine at all?  ERR_TERM is read/write on every 8514
    # family part; an absent card floats.
    e.out16(SUBSYS_CNTL, GPCTRL_RESET | CHPTEST_NORMAL)
    e.out16(SUBSYS_CNTL, GPCTRL_ENAB | CHPTEST_NORMAL)
    e.out16(ERR_TERM, 0x5A5A)
    idle()
    e.mov_dx(ERR_TERM)
    e.in_ax()
    e.store_ax("res", R_ERR1)
    e.cmp_ax(0x5A5A)
    e.jnz("absent")
    e.out16(ERR_TERM, 0x2525)
    idle()
    e.mov_dx(ERR_TERM)
    e.in_ax()
    e.store_ax("res", R_ERR2)
    e.cmp_ax(0x2525)
    e.jnz("absent")
    e.jmp("present")
    e.label("absent")
    e.jmp("finish")
    e.label("present")
    e.mov_ax(1)
    e.store_al("res", R_PRESENT)

    e.mov_dx(SUBSYS_STAT)
    e.in_ax()
    e.store_ax("res", R_STAT)
    e.test_al(EIGHT_PLANE)
    e.jz("no_memcntl")
    idle()
    e.out16(MULTIFUNC_CNTL, MEM_CNTL | VRTCFG_4 | HORCFG_8)
    e.label("no_memcntl")

    idle()
    e.out16(GE_PITCH, 1024 >> 3)
    e.out16(GE_OFFSET_HI, 0)
    e.out16(GE_OFFSET_LO, 0)
    e.out16(MULTIFUNC_CNTL, SCISSORS_T | 0)
    e.out16(MULTIFUNC_CNTL, SCISSORS_L | 0)
    idle()
    e.out16(MULTIFUNC_CNTL, SCISSORS_B | 1023)
    e.out16(MULTIFUNC_CNTL, SCISSORS_R | 1023)
    e.out16(RD_MASK, 0xFF)
    e.out16(BKGD_MIX, BSS_BKGDCOL | MIX_SRC)

    # Sources
    for r in range(8):
        fill(S1, Y + r, 8, 1, 1 << r)
    fill(S2, Y, 8, 8, 0xFF & ~(1 << GLYPH_PLANE))
    for x, y, w, h in F_RECTS:
        fill(S2 + x, Y + y, w, h, 0xFF, wmask=1 << GLYPH_PLANE)

    # Destinations hold a value no blit writes, so "did nothing" is visible
    fill(D_ID[0], Y, D_ID[-1] + 8 - D_ID[0], 8, 0x33)
    fill(D_ROT, Y, 16 + 8, 8, 0x11)

    for k in range(8):
        expand(S1, D_ID[k], 1 << k, 0xFF, MIX_SRC, bg=0x00)
    rot = 1 << ((GLYPH_PLANE + 1) % 8)
    expand(S2, D_ROT, rot, 0x5A, MIX_DST)
    expand(S2, D_UNROT, 1 << GLYPH_PLANE, 0x5A, MIX_DST)

    for k in range(8):
        readback(D_ID[k], R_ID + 64 * k)
    readback(D_ROT, R_ROT)
    readback(D_UNROT, R_UNROT)
    readback(S2, R_SRC2)
    readback(S1, R_SRC1)

    # Leave the engine in a plain state
    idle()
    e.out16(MULTIFUNC_CNTL, PIX_CNTL)
    e.out16(WRT_MASK, 0xFF)
    e.out16(RD_MASK, 0xFF)
    e.out16(FRGD_MIX, FSS_FRGDCOL | MIX_SRC)
    e.out16(BKGD_MIX, BSS_BKGDCOL | MIX_SRC)

    # --- write M8GLYPH.BIN, say so, exit ---
    e.label("finish")
    e.db(0xB4, 0x3C)                        # mov ah,3Ch  create
    e.mov_cx(0)
    e.mov_dx_lbl("fname")
    e.int_(0x21)
    e.rel8(0x72, "fail")                    # jc
    e.mov_bx_ax()
    e.db(0xB4, 0x40)                        # mov ah,40h  write
    e.mov_cx(RESLEN)
    e.mov_dx_lbl("res")
    e.int_(0x21)
    e.db(0xB4, 0x3E)                        # mov ah,3Eh  close
    e.int_(0x21)
    e.mov_dx_lbl("msg_ok")
    e.jmp("say")
    e.label("fail")
    e.mov_dx_lbl("msg_fail")
    e.label("say")
    e.db(0xB4, 0x09)
    e.int_(0x21)
    e.mov_ax(0x4C00)
    e.int_(0x21)

    # wait_idle: GP_STAT busy bit clear, ~0.5 s at most.  Uses AX CX DX.
    e.label("wait_idle")
    e.mov_dx(GP_STAT)
    e.mov_cx(0)
    e.label("wi_top")
    e.in_ax()
    e.test_ah(GPBUSY >> 8)
    e.jz("wi_ok")
    e.loop("wi_top")
    e.inc_byte("res", R_TO_IDLE)
    e.label("wi_ok")
    e.ret()

    # wait_data: GP_STAT data-ready set.  Uses AX CX DX.
    e.label("wait_data")
    e.mov_dx(GP_STAT)
    e.mov_cx(0)
    e.label("wd_top")
    e.in_ax()
    e.test_ah(DATARDY >> 8)
    e.jnz("wd_ok")
    e.loop("wd_top")
    e.inc_byte("res", R_TO_DATA)
    e.label("wd_ok")
    e.ret()

    # --- data ---
    e.label("fname")
    e.db(*b"M8GLYPH.BIN\0")
    e.label("msg_ok")
    e.db(*b"M8GLYPH: done, results in M8GLYPH.BIN\r\n$")
    e.label("msg_fail")
    e.db(*b"M8GLYPH: could not create M8GLYPH.BIN\r\n$")
    e.label("res")
    e.db(*b"M8G1")
    e.db(*([0] * (RESLEN - 4)))
    return e.resolve()


def cells(block, swap):
    out = []
    for i in range(0, 64, 2):
        a, b = block[i], block[i + 1]
        out += [b, a] if swap else [a, b]
    return [out[r * 8:(r + 1) * 8] for r in range(8)]


def glyph_expected(on, off):
    g = [[off] * 8 for _ in range(8)]
    for x, y, w, h in F_RECTS:
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                g[yy][xx] = on
    return g


def show(rows, legend):
    for r in rows:
        print("    " + "".join(legend.get(v, "?") for v in r) + "   " + " ".join("%02X" % v for v in r))


def decode(path):
    d = Path(path).read_bytes()
    if len(d) != RESLEN or d[:4] != b"M8G1":
        raise SystemExit("not an M8GLYPH result (%d bytes)" % len(d))
    err1, err2, stat = struct.unpack_from("<HHH", d, R_ERR1)
    print("ERR_TERM read back %04X / %04X (expect 5A5A / 2525)" % (err1, err2))
    if not d[R_PRESENT]:
        print("No 8514 engine answered. Nothing else ran.")
        return
    print("SUBSYS_STAT %04X: %s" % (stat, "1 MB, 8 planes" if stat & EIGHT_PLANE else "512 KB, 4 planes"))
    print("timeouts: engine idle %d, data ready %d" % (d[R_TO_IDLE], d[R_TO_DATA]))

    # Byte order inside a PIX_TRANS word, taken from the glyph source control
    exp_src2 = glyph_expected(0xFF, 0xFF & ~(1 << GLYPH_PLANE))
    swap = None
    for s in (False, True):
        if cells(d[R_SRC2:R_SRC2 + 64], s) == exp_src2:
            swap = s
    print()
    print("Controls (the fills, read straight back):")
    src1 = cells(d[R_SRC1:R_SRC1 + 64], bool(swap))
    ok1 = all(src1[r] == [1 << r] * 8 for r in range(8))
    print("  plane-ID source, row r = 1<<r: %s" % ("OK" if ok1 else "WRONG"))
    if not ok1:
        show(src1, {})
    print("  glyph source, F in plane %d:    %s" % (GLYPH_PLANE, "OK" if swap is not None else "WRONG"))
    if swap is None:
        show(cells(d[R_SRC2:R_SRC2 + 64], False), {})
        print("Controls failed: the results below cannot be trusted.")
    elif swap:
        print("  (pixels come high byte first in each PIX_TRANS word)")
    sw = bool(swap)

    print()
    print("RD_MASK bit -> plane read (expand blit, one bit at a time):")
    mapping = {}
    for k in range(8):
        blk = cells(d[R_ID + 64 * k:R_ID + 64 * k + 64], sw)
        lit = [r for r in range(8) if blk[r] == [0xFF] * 8]
        dark = [r for r in range(8) if blk[r] == [0x00] * 8]
        if len(lit) == 1 and len(dark) == 7:
            mapping[k] = lit[0]
            print("  bit %d (%02X) -> plane %d" % (k, 1 << k, lit[0]))
        elif all(v == 0x33 for row in blk for v in row):
            print("  bit %d (%02X) -> blit did not happen (destination untouched)" % (k, 1 << k))
        else:
            print("  bit %d (%02X) -> unexpected:" % (k, 1 << k))
            show(blk, {0xFF: "#", 0x00: ".", 0x33: "-"})
    if len(mapping) == 8:
        rotated = all(mapping[k] == (k - 1) % 8 for k in range(8))
        plain = all(mapping[k] == k for k in range(8))
        print("  => %s" % ("ROTATED: plane n is bit (n+1) mod 8" if rotated
                          else "plain: plane n is bit n" if plain else "neither pattern"))

    print()
    want = glyph_expected(0x5A, 0x11)
    for name, slot, mask in (("rotated", R_ROT, 1 << ((GLYPH_PLANE + 1) % 8)),
                             ("plain", R_UNROT, 1 << GLYPH_PLANE)):
        blk = cells(d[slot:slot + 64], sw)
        verdict = "THE GLYPH" if blk == want else "no glyph"
        print("Glyph, %s mask %02X, colour 5A over 11: %s" % (name, mask, verdict))
        show(blk, {0x5A: "#", 0x11: ".", 0x33: "-"})


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "decode" and len(sys.argv) == 3:
        decode(sys.argv[2])
        return
    if len(sys.argv) >= 2 and sys.argv[1] == "build":
        out = Path(sys.argv[2]) if len(sys.argv) == 3 else Path(__file__).with_name("M8GLYPH.COM")
        code = build()
        if len(code) > 0xFE00:
            raise SystemExit("too big for a .COM")
        out.write_bytes(code)
        print("%s: %d bytes" % (out, len(code)))
        return
    raise SystemExit(__doc__.split("\n\n")[1])


if __name__ == "__main__":
    main()
