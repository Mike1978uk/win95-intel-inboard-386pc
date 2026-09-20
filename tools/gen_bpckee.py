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


XLAT_OFF = 0xE9E   # the status->nibble table, inline in the vendor driver


def delay(a):
    """The settling pause the driver takes between every port step."""
    a.emit(0x51)                             # push cx
    a.emit(0xB9, 0x00, 0x04)                 # mov cx, 0400h
    a.emit(0xE2, 0xFE)                       # loop $
    a.emit(0x59)                             # pop cx


def sub_addr(a, ctl):
    """Address a register: number to DATA, then CTRL = (ctl | 1) ^ 2.

    Transliterated from the vendor driver at 0xC05, mode-0 arm (0x0C1F). The
    control byte is carried forward in memory exactly as the driver carries it
    in [si+9] - each call toggles AUTOFD, so it cannot be recomputed.
    """
    at = a.here()
    a.mov_dx(a.data); a.out()
    delay(a)
    a.mov_dx(a.ctrl)
    a.emit(0xA0); a.emit(ctl & 0xFF, ctl >> 8)      # mov al, [ctl]
    a.emit(0x0C, 0x01)                              # or al, 1
    a.emit(0x34, 0x02)                              # xor al, 2
    a.out()
    a.emit(0xA2); a.emit(ctl & 0xFF, ctl >> 8)      # mov [ctl], al
    delay(a)
    a.ret()
    return at


def sub_write(a, ctl):
    """Write AL to the addressed register - driver 0xC90, mode-0 arm 0x0CAA."""
    at = a.here()
    a.mov_dx(a.data); a.out()
    delay(a)
    a.mov_dx(a.ctrl)
    a.emit(0xA0); a.emit(ctl & 0xFF, ctl >> 8)
    a.emit(0x0C, 0x01); a.out()                     # STROBE up
    a.emit(0x34, 0x04); a.out()                     # INIT toggles -> commit
    delay(a)
    a.emit(0x24, 0xFE)                              # and al, 0FEh
    for _ in range(5):
        a.out()
    a.emit(0xA2); a.emit(ctl & 0xFF, ctl >> 8)
    a.ret()
    return at


def sub_read(a, ctl, sav, table):
    """Read the addressed register - driver 0xDFC, the mode-0 read.

    Five settling writes with STROBE down, then INIT toggled to clock the low
    nibble out and back again for the high one. The status byte is decoded
    through the driver's own 256-entry table rather than a reconstruction of
    it, so the mapping cannot be got wrong.
    """
    at = a.here()
    a.mov_dx(a.ctrl)
    a.emit(0xA0); a.emit(ctl & 0xFF, ctl >> 8)
    a.emit(0x24, 0xFE)                              # and al, 0FEh
    for _ in range(5):
        a.out()
    a.emit(0xA2); a.emit(sav & 0xFF, sav >> 8)      # save it
    a.emit(0x34, 0x04); a.out()                     # xor al,4 -> clock
    delay(a)
    a.emit(0xBB); a.emit(table & 0xFF, table >> 8)  # mov bx, table
    a.emit(0xFE, 0xCA)                              # dec dl -> STATUS
    a.emit(0xEC); a.emit(0xD7)                      # in al,dx ; xlatb
    a.emit(0x8A, 0xE0)                              # mov ah, al
    a.emit(0xFE, 0xC2)                              # inc dl -> CTRL
    a.emit(0xA0); a.emit(sav & 0xFF, sav >> 8)
    a.out()
    a.emit(0xA2); a.emit(ctl & 0xFF, ctl >> 8)
    delay(a)
    a.emit(0xFE, 0xCA)
    a.emit(0xEC); a.emit(0xD7)
    a.emit(0x25, 0xF0, 0x0F)                        # and ax, 0FF0h
    a.emit(0x0A, 0xC4)                              # or al, ah
    a.ret()
    return at


def ee_write(a, addr_s, write_s, val):
    """One write to register 6: address it, then hand it the value."""
    a.mov_al(0x06); a.call(addr_s)
    a.mov_al(val);  a.call(write_s)


def clock_bit(a, addr_s, write_s, state, di):
    state = state & 0xFC
    if di:
        state |= 0x02
    ee_write(a, addr_s, write_s, state)
    ee_write(a, addr_s, write_s, state | 0x01)
    ee_write(a, addr_s, write_s, state)
    return state


def read_word(a, addr_s, write_s, read_s, addr):
    """93C46 READ: START, opcode 10, six address bits, sixteen bits back."""
    state = 0x08
    ee_write(a, addr_s, write_s, state)
    state |= 0x04
    ee_write(a, addr_s, write_s, state)

    for bit in (1, 1, 0):
        state = clock_bit(a, addr_s, write_s, state, bit)
    for i in range(5, -1, -1):
        state = clock_bit(a, addr_s, write_s, state, (addr >> i) & 1)

    a.emit(0xBE, 0x00, 0x00)                        # mov si, 0
    for _ in range(16):
        state = clock_bit(a, addr_s, write_s, state, 0)
        a.mov_al(0x00); a.call(addr_s)
        a.call(read_s)
        a.emit(0xD0, 0xE0)                          # shl al,1 -> CF = bit 7
        a.emit(0xD1, 0xD6)                          # rcl si,1

    a.emit(0x89, 0x35)                              # mov [di], si
    a.emit(0x83, 0xC7, 0x02)                        # add di, 2

    state &= ~0x04
    ee_write(a, addr_s, write_s, state)


def build(port, unit):
    a = Asm(port)
    xlat = open("drivers/microsolutions_backpack/vendor/BPCDDRV.SYS",
                "rb").read()[XLAT_OFF:XLAT_OFF + 256]

    a.emit(0xE9, 0x00, 0x00)
    jmp_fix = 1
    # Placeholders for the variables, patched once their addresses are known.
    ctl, sav, table = 0x9000, 0x9002, 0x9004
    addr_s  = sub_addr(a, ctl)
    write_s = sub_write(a, ctl)
    read_s  = sub_read(a, ctl, sav, table)
    struct.pack_into("<H", a.b, jmp_fix, a.here() - (ORG + 3))

    # Connect, from the vendor driver at 0x09EE: the address inverted, then
    # straight, three SELECT edges, then AUTOFD for the address probe.
    a.mov_dx(a.ctrl)
    for _ in range(5):
        a.mov_al(0x00); a.out()
    a.mov_dx(a.data); a.mov_al((~unit) & 0xFF); a.out()
    a.mov_dx(a.ctrl); a.mov_al(INIT); a.out()
    delay(a)
    a.mov_dx(a.data); a.mov_al(unit); a.out()
    delay(a)
    a.mov_dx(a.ctrl)
    ctlv = INIT
    for _ in range(3):
        ctlv ^= SELECT
        a.mov_al(ctlv); a.out()
        delay(a)
    ctlv |= AUTOFD
    a.mov_al(ctlv); a.out()
    delay(a)

    # The tail of the connect, driver 0x0AB1-0x0B64. The ident probe leaves
    # AUTOFD set; the link is only taken once AUTOFD is dropped and SELECT is
    # toggled once more. Stopping at the probe is why the pod answered the
    # presence test and then went idle again.
    ctlv &= ~AUTOFD
    a.mov_al(ctlv); a.out()
    delay(a)
    ctlv ^= SELECT
    a.mov_al(ctlv); a.out()
    a.mov_al(ctlv)
    a.emit(0xA2); a.emit(ctl & 0xFF, ctl >> 8)      # the carried CTRL, as [si+9]
    delay(a)

    a.emit(0xBF); buf_fix = len(a.b); a.emit(0x00, 0x00)

    for word in range(64):
        read_word(a, addr_s, write_s, read_s, word)

    a.emit(0xBA); name_fix = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB9, 0x00, 0x00)
    a.emit(0xB4, 0x3C); a.emit(0xCD, 0x21)
    a.emit(0x93)
    a.emit(0xBA); buf_fix2 = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB9, 0x80, 0x00)
    a.emit(0xB4, 0x40); a.emit(0xCD, 0x21)
    a.emit(0xB4, 0x3E); a.emit(0xCD, 0x21)
    a.emit(0xBA); msg_fix = len(a.b); a.emit(0x00, 0x00)
    a.emit(0xB4, 0x09); a.emit(0xCD, 0x21)
    a.emit(0xB8, 0x00, 0x4C); a.emit(0xCD, 0x21)

    name_off = a.here(); a.b.extend(b"BPCKEE.BIN" + bytes([0]))
    msg_off = a.here(); a.b.extend(b"BPCKEE: 128 bytes written to BPCKEE.BIN" + bytes([13,10]) + b"$")
    ctl_off = a.here(); a.b.append(0)
    sav_off = a.here(); a.b.append(0)
    tbl_off = a.here(); a.b.extend(xlat)
    buf_off = a.here(); a.b.extend(bytes(128))

    blob = bytes(a.b)
    blob = blob.replace(struct.pack("<H", 0x9000), struct.pack("<H", ctl_off))
    blob = blob.replace(struct.pack("<H", 0x9002), struct.pack("<H", sav_off))
    blob = blob.replace(struct.pack("<H", 0x9004), struct.pack("<H", tbl_off))
    a.b = bytearray(blob)

    struct.pack_into("<H", a.b, buf_fix, buf_off)
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
    if len(com) > 64000:
        print("TOO BIG for a .COM: %d" % len(com), file=sys.stderr)
        return 1

    with open(args.out, "wb") as f:
        f.write(com)

    print("%s: %d bytes, port %s, unit %d"
          % (args.out, len(com), args.port, args.unit))
    return 0


if __name__ == "__main__":
    sys.exit(main())
