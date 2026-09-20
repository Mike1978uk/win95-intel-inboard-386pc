#!/usr/bin/env python3
r"""Build BPCKEE.COM - dump a BackPack pod's 93C46 identity EEPROM.

The vendor driver is not safe on an XT: it masks both PICs and probes 0x22/0x23,
which alias onto the 8259 on that bus. This program needs none of that. It
touches the parallel port and nothing else, so it runs on the 5160 with no
BackPack driver loaded.

The protocol is the one measured in drivers/microsolutions_backpack/README.md:

  register 0x06   0x08 enable, 0x04 CS, 0x02 DI, 0x01 CLK
  register 0x00   bit 7 is DO

Output is 128 raw bytes - 64 little-endian words - written to BPCKEE.BIN, so
nothing depends on getting hex formatting right in 8086.

  python tools/gen_bpckee.py [--port 0x378] [--unit 0] [--out BPCKEE.COM]
"""
import argparse
import struct
import sys

# Raw writes to base+2, which is what the vendor driver itself puts on the
# wire - the port inverts three of the four lines, and imitating the driver
# avoids having to model that.
STROBE = 0x01
AUTOFD = 0x02
INIT   = 0x04
SELECT = 0x08

ORG = 0x100


class Asm:
    def __init__(self, port):
        self.b = bytearray()
        self.data = port
        self.stat = port + 1
        self.ctrl = port + 2

    def emit(self, *vals):
        self.b.extend(vals)

    def here(self):
        return ORG + len(self.b)

    def mov_dx(self, imm):
        self.emit(0xBA, imm & 0xFF, (imm >> 8) & 0xFF)

    def mov_al(self, imm):
        self.emit(0xB0, imm & 0xFF)

    def out(self):
        self.emit(0xEE)

    def call(self, target):
        rel = target - (self.here() + 3)
        self.emit(0xE8, rel & 0xFF, (rel >> 8) & 0xFF)

    def ret(self):
        self.emit(0xC3)


def delay(a):
    """The settling pause the driver takes between connect steps."""
    a.emit(0xB9, 0x00, 0x08)                 # mov cx, 0800h
    a.emit(0xE2, 0xFE)                       # loop $


def sub_write_reg6(a):
    """Write CL to register 0x06.

    AUTOFD must fall while the data lines still carry the register number,
    because any AUTOFD edge re-latches the address from them. Only then may the
    value go out, committed by INIT falling with STROBE up.
    """
    at = a.here()
    a.mov_dx(a.data); a.mov_al(0x06); a.out()
    a.mov_dx(a.ctrl)
    a.mov_al(INIT); a.out()
    a.mov_al(INIT | AUTOFD); a.out()         # latch register 6
    a.mov_al(INIT); a.out()                  # AUTOFD down, address held
    a.mov_dx(a.data); a.emit(0x8A, 0xC1); a.out()   # mov al, cl
    a.mov_dx(a.ctrl)
    a.mov_al(INIT | STROBE); a.out()         # STROBE up, no INIT edge
    a.mov_al(STROBE); a.out()                # INIT falls -> commit
    a.mov_al(0x00); a.out()
    a.ret()
    return at


def sub_read_do(a):
    """Clock one DO bit out of register 0x00 and shift it into BX.

    A nibble read hands back the low half first and the high half second, and
    bit 7 of the byte is bit 3 of the high nibble - which is bit 7 of the
    status register on the second toggle. So only that read is needed, and the
    carry does the rest: no branches in the bit loop.
    """
    at = a.here()
    a.mov_dx(a.data); a.mov_al(0x00); a.out()
    a.mov_dx(a.ctrl)
    a.mov_al(INIT); a.out()
    a.mov_al(INIT | AUTOFD); a.out()         # latch register 0
    a.mov_al(INIT); a.out()                  # nibble phase reset
    a.mov_al(0x00); a.out()                  # first toggle: low nibble
    a.mov_al(INIT); a.out()                  # second toggle: high nibble
    a.mov_dx(a.stat); a.emit(0xEC)
    a.emit(0xD0, 0xE0)                       # shl al, 1  -> CF = bit 7
    a.emit(0xD1, 0xD3)                       # rcl bx, 1
    a.ret()
    return at


def clock_bit(a, wr6, state, di):
    """One EEPROM clock, exactly as the driver frames it: data, up, down."""
    state = state & 0xFC
    if di:
        state |= 0x02
    a.emit(0xB1, state); a.call(wr6)         # mov cl, state
    a.emit(0xB1, state | 0x01); a.call(wr6)
    a.emit(0xB1, state); a.call(wr6)
    return state


def read_word(a, wr6, rddo, addr):
    """A 93C46 READ: START, opcode 10, six address bits, then 16 bits back."""
    state = 0x08                             # enable, CS low
    a.emit(0xB1, state); a.call(wr6)
    state |= 0x04                            # CS up
    a.emit(0xB1, state); a.call(wr6)

    for bit in (1, 1, 0):
        state = clock_bit(a, wr6, state, bit)
    for i in range(5, -1, -1):
        state = clock_bit(a, wr6, state, (addr >> i) & 1)

    a.emit(0xBB, 0x00, 0x00)                 # mov bx, 0
    for _ in range(16):
        state = clock_bit(a, wr6, state, 0)
        a.call(rddo)

    a.emit(0x89, 0x1D)                       # mov [di], bx
    a.emit(0x83, 0xC7, 0x02)                 # add di, 2

    state &= ~0x04                           # CS down ends the frame
    a.emit(0xB1, state); a.call(wr6)


def build(port, unit):
    a = Asm(port)

    a.emit(0xE9, 0x00, 0x00)                 # jmp over the subroutines
    jmp_fix = 1
    wr6 = sub_write_reg6(a)
    rddo = sub_read_do(a)
    struct.pack_into("<H", a.b, jmp_fix, a.here() - (ORG + 3))

    # The connect sequence, transliterated from the vendor driver at 0x09EE.
    # The pod is looking for the unit address presented INVERTED and then
    # straight, with a settling pause between each step - a complement pair an
    # empty port cannot produce. Three SELECT toggles follow, then AUTOFD for
    # the address probe. Leaving out the inverted write is why an earlier
    # version of this program was ignored by a pod that works.
    a.mov_dx(a.ctrl)
    a.mov_al(0x00); a.out()
    a.mov_al(0x00); a.out()
    a.mov_al(0x00); a.out()
    a.mov_al(0x00); a.out()
    a.mov_al(0x00); a.out()                  # five writes, as the driver does

    a.mov_dx(a.data); a.mov_al((~unit) & 0xFF); a.out()
    a.mov_dx(a.ctrl); a.mov_al(INIT); a.out()
    delay(a)
    a.mov_dx(a.data); a.mov_al(unit); a.out()
    delay(a)

    a.mov_dx(a.ctrl)
    ctl = INIT
    for _ in range(3):
        ctl ^= SELECT
        a.mov_al(ctl); a.out()
        delay(a)
    ctl |= AUTOFD
    a.mov_al(ctl); a.out()
    delay(a)

    a.emit(0xBF); buf_fix1 = len(a.b); a.emit(0x00, 0x00)   # mov di, buffer

    for word in range(64):
        read_word(a, wr6, rddo, word)

    # Release the link so a driver loaded afterwards finds the pod idle.
    a.mov_dx(a.ctrl)
    a.mov_al(AUTOFD); a.out()
    a.mov_al(INIT | SELECT); a.out()

    a.emit(0xBA); name_fix = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB9, 0x00, 0x00)                 # mov cx, 0
    a.emit(0xB4, 0x3C); a.emit(0xCD, 0x21)   # create
    a.emit(0x93)                             # xchg ax, bx  -> handle in bx
    a.emit(0xBA); buf_fix2 = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB9, 0x80, 0x00)                 # mov cx, 128
    a.emit(0xB4, 0x40); a.emit(0xCD, 0x21)   # write
    a.emit(0xB4, 0x3E); a.emit(0xCD, 0x21)   # close

    a.emit(0xBA); msg_fix = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB4, 0x09); a.emit(0xCD, 0x21)   # print
    a.emit(0xB8, 0x00, 0x4C); a.emit(0xCD, 0x21)

    name_off = a.here()
    a.b.extend(b"BPCKEE.BIN\x00")
    msg_off = a.here()
    a.b.extend(b"BPCKEE: 128 bytes written to BPCKEE.BIN\r\n$")
    buf_off = a.here()
    a.b.extend(b"\x00" * 128)

    struct.pack_into("<H", a.b, buf_fix1, buf_off)
    struct.pack_into("<H", a.b, buf_fix2, buf_off)
    struct.pack_into("<H", a.b, name_fix, name_off)
    struct.pack_into("<H", a.b, msg_fix, msg_off)

    return bytes(a.b)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="0x378")
    p.add_argument("--unit", type=int, default=0)
    p.add_argument("--out", default="BPCKEE.COM")
    args = p.parse_args()

    com = build(int(args.port, 0), args.unit)
    if len(com) > 60000:
        print("TOO BIG for a .COM: %d" % len(com), file=sys.stderr)
        return 1

    with open(args.out, "wb") as f:
        f.write(com)

    print("%s: %d bytes, port %s, unit %d"
          % (args.out, len(com), args.port, args.unit))
    return 0


if __name__ == "__main__":
    sys.exit(main())
