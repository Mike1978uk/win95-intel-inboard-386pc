#!/usr/bin/env python3
"""Emit ECPPROBE.COM - the ECP measurement the transport work has never had.

Why this exists rather than a DEBUG script: a sequenced bridge handshake cannot
be driven one port access at a time over the COMrade link.  Measured 2026-09-13,
seven of fifty io_out calls timed out mid-frame, and a timeout does not even say
whether the write landed.  The sequence has to execute on the box.  The DDK's
16-bit linker will not run on the Win11 host, so the bytes are emitted here
instead of assembled - which also means every byte in the artefact is accounted
for by the source below.

What it measures, none of which is currently known on the owner's hardware
(every probe to date - 2026-09-08, 2026-09-11 - was nibble/SPP):

  - does ECR bit 0 clear when the bridge has a byte ready, on this card
  - does an ECP address cycle to base+0 actually select a register
  - is one byte returned, and is it the right byte
  - how long the reverse wait takes, which is what sizes the driver's budgets

It replays the vendor's own ECP register read, SD120PPD.SYS rva 3CCEh, for ATA
status (CONT_TASKFILE 18h + 7 = 1Fh), which reads 50h on a healthy idle drive.
A nibble read of the same register runs first as the control: if nibble says 50h
and ECP says something else, the fault is ECP and not the drive.

The bridge is restored and disconnected before the program exits (technique 118
- a probe must not change state), and nothing here writes 22h/23h, which alias
onto the 8259 on this XT (technique 75).
"""

import struct
import sys

BASE = 0x378
STAT = BASE + 1
CTL = BASE + 2
FIFO = BASE + 0x400
ECR = BASE + 0x402

CONT_TASKFILE = 0x18
CONT_IDECTL = 0x10
NIB_WRITE_TAG = 0x60
ATA_REG_STATUS = 7
ATA_REG_BCLO = 4
ATA_REG_BCHI = 5
ATA_REG_DEVCTL = 6

ORG = 0x100

# Result slots.  Kept as named constants so the decoder below and the emitter
# cannot drift apart.
SLOTS = [
    "marker",        # 0  A5 - proves the program ran and the file is ours
    "ecr_entry",     # 1  ECR as found, before anything is touched
    "cpp_chk1",      # 2  status after the magic bytes   expect B0 in bits 7-4
    "cpp_chk2",      # 3  status after 87                expect 50
    "cpp_chk3",      # 4  status after 78                expect B0 in bits 7,5,4
    "nib_pre",       # 5  nibble ATA status before SRST
    "srst_flag",     # 6  0 = BSY cleared, 1 = poll expired
    "nib_post",      # 7  nibble ATA status after SRST   expect 50
    "nib_bclo",      # 8  expect 14 - the ATAPI signature
    "nib_bchi",      # 9  expect EB
    "ecp_ecr_mode",  # 10 ECR right after entering ECP mode (74h)
    "w1_flag",       # 11 forward wait, FIFO empty: 0 ok, 1 expired
    "w1_ecr",        # 12 last ECR seen in that wait
    "w2_flag",       # 13 same, after the address cycle
    "w2_ecr",        # 14
    "ecp_ecr_rev",   # 15 ECR after turning the port around
    "w3_flag",       # 16 REVERSE wait, data available: 0 ok, 1 expired
    "w3_ecr",        # 17 last ECR seen - the bit the whole fix rests on
    "ecp_byte",      # 18 the byte out of the FIFO      expect 50
    "ecr_after",     # 19 ECR after the read
    "ecr_restored",  # 20 ECR after teardown
    "br0d_pre",      # 21 bridge register 0Dh as found
    "br0d_post",     # 22 the same after configuring it - proves the write stuck
    "neg_flag",      # 23 IEEE-1284 ECP negotiation: 0 = peripheral answered
    "neg_stat",      # 24 last status byte seen while waiting for nAck
    "neg_flag2",     # 25 the vendor's retry after a failed negotiation
    "neg_stat2",     # 26
    "rec_stat",      # 27 status while waiting for nAck to go high again
    "pad",           # 28
]
SLOT = {n: i for i, n in enumerate(SLOTS)}
# Two 16-bit counters live after the byte slots: the residue of each wait's
# budget, which is how long the wait actually took.
CNT = len(SLOTS)
CNT_W1, CNT_W2, CNT_W3 = CNT, CNT + 2, CNT + 4
RESLEN = CNT + 6


class Emitter:
    """Just enough 8086 to express the probe, with labels resolved in a second
    pass so no jump displacement is computed by hand."""

    def __init__(self):
        self.buf = bytearray()
        self.labels = {}
        self.fix8 = []   # (position of rel8 byte, label)
        self.fix16 = []  # (position of imm16, label) - absolute address

    # --- primitives ---
    def db(self, *b):
        self.buf.extend(b)

    def dw(self, w):
        self.buf.extend(struct.pack("<H", w & 0xFFFF))

    def label(self, name):
        self.labels[name] = len(self.buf)

    def addr_of(self, name):
        """Emit a placeholder for the absolute (ORG-relative) address."""
        self.fix16.append((len(self.buf), name))
        self.dw(0)

    # --- instructions ---
    def mov_dx(self, imm):
        self.db(0xBA)
        self.dw(imm)

    def mov_al(self, imm):
        self.db(0xB0, imm & 0xFF)

    def mov_ah(self, imm):
        self.db(0xB4, imm & 0xFF)

    def mov_cx(self, imm):
        self.db(0xB9)
        self.dw(imm)

    def mov_bx_ax(self):
        self.db(0x89, 0xC3)

    def out_dx(self):
        self.db(0xEE)

    def in_dx(self):
        self.db(0xEC)

    def store_al(self, slot):
        self.db(0xA2)
        self.dw(ORG_RES + slot)

    def load_al(self, slot):
        self.db(0xA0)
        self.dw(ORG_RES + slot)

    def store_cx(self, slot):
        self.db(0x89, 0x0E)
        self.dw(ORG_RES + slot)

    def and_al(self, imm):
        self.db(0x24, imm & 0xFF)

    def or_al(self, imm):
        self.db(0x0C, imm & 0xFF)

    def test_al(self, imm):
        self.db(0xA8, imm & 0xFF)

    def shr_al_4(self):
        # 286+.  This machine is an Inboard 386, so shifting by an immediate is
        # available and it keeps CX free for the wait-loop counters.
        self.db(0xC0, 0xE8, 0x04)

    def mov_bl_al(self):
        self.db(0x88, 0xC3)

    def or_al_bl(self):
        self.db(0x08, 0xD8)

    def mov_al_bl(self):
        self.db(0x88, 0xD8)

    def dec_cx(self):
        self.db(0x49)

    def _jrel(self, opcode, label):
        self.db(opcode)
        self.fix8.append((len(self.buf), label))
        self.db(0)

    def jnz(self, label):
        self._jrel(0x75, label)

    def jz(self, label):
        self._jrel(0x74, label)

    def jmp(self, label):
        self._jrel(0xEB, label)

    def int_(self, n):
        self.db(0xCD, n & 0xFF)

    def cli(self):
        self.db(0xFA)

    def sti(self):
        self.db(0xFB)

    # --- port helpers ---
    def outp(self, port, val):
        self.mov_dx(port)
        self.mov_al(val)
        self.out_dx()

    def out_again(self, val):
        """Another write to whatever port DX already holds."""
        self.mov_al(val)
        self.out_dx()

    def inp(self, port):
        self.mov_dx(port)
        self.in_dx()

    def inp_store(self, port, slot):
        self.inp(port)
        self.store_al(slot)

    # --- protocol helpers ---
    def cpp_frame(self, cmd, checkpoints=False):
        """The EPAT connect packet.  Eight bytes to the SPP data port, each
        written twice - the second write is the I/O delay - then commit by
        pulsing nINIT 04/05/04.  Verified on the real 5160 2026-09-08.

        Runs under cli: a frame cannot tolerate an interrupt landing in the
        middle of it (TRANSPORT_SPEC section 1).  The polling waits elsewhere
        in this probe are NOT order-sensitive and deliberately stay
        interruptible, so the COMrade agent keeps running."""
        self.cli()
        self.outp(CTL, 0x04)
        self.mov_dx(BASE)
        for b in (0x22, 0xAA, 0x55, 0x00, 0xFF):
            self.out_again(b)
            self.out_again(b)
        if checkpoints:
            self.inp_store(STAT, SLOT["cpp_chk1"])
            self.mov_dx(BASE)
        for b in (0x87,):
            self.out_again(b)
            self.out_again(b)
        if checkpoints:
            self.inp_store(STAT, SLOT["cpp_chk2"])
            self.mov_dx(BASE)
        for b in (0x78,):
            self.out_again(b)
            self.out_again(b)
        if checkpoints:
            self.inp_store(STAT, SLOT["cpp_chk3"])
            self.mov_dx(BASE)
        self.out_again(cmd)
        self.out_again(cmd)
        self.outp(CTL, 0x04)
        self.out_again(0x05)
        self.out_again(0x04)
        self.sti()

    def nibble_read(self, reg):
        """Read one register as two nibbles, each arriving in the TOP four bits
        of the status port.  Result in AL.  Clobbers BL."""
        self.cli()
        self.outp(BASE, reg)
        self.outp(CTL, 0x01)
        self.out_again(0x03)
        self.out_again(0x03)
        self.inp(STAT)
        self.and_al(0xF0)
        self.shr_al_4()
        self.mov_bl_al()
        self.outp(CTL, 0x04)
        self.out_again(0x04)
        self.inp(STAT)
        self.and_al(0xF0)
        self.or_al_bl()
        self.sti()

    def reg_write(self, reg, val):
        """A write TAGS the register number with 60h; a read sends it bare."""
        self.cli()
        self.outp(BASE, reg | NIB_WRITE_TAG)
        self.outp(CTL, 0x01)
        self.out_again(0x01)
        self.outp(BASE, val)
        self.outp(CTL, 0x04)
        self.sti()

    def reg_write_bl(self, reg):
        """Same write, but the value comes from BL - so a register can be
        read, modified and written back without an assembler."""
        self.cli()
        self.outp(BASE, reg | NIB_WRITE_TAG)
        self.outp(CTL, 0x01)
        self.out_again(0x01)
        self.mov_dx(BASE)
        self.mov_al_bl()
        self.out_dx()
        self.outp(CTL, 0x04)
        self.sti()

    def negotiate_ecp(self, tag="neg", flag="neg_flag", stat="neg_stat"):
        """IEEE-1284 negotiation into ECP, SD120PPD.SYS 0x2993.

        Setting the host ECR to ECP mode configures only this side of the
        cable.  The peripheral stays in compatibility mode until it is asked,
        and an unnegotiated bridge never acknowledges the forward handshake -
        so the FIFO fills once and never drains.  10h is the ECP extensibility
        byte, where epat.c puts 40h to request EPP.
        """
        self.cli()
        self.outp(CTL, 0x0C)
        self.out_again(0x04)
        # 0x24D1: w0(0); w2(1); w2(4) - idle into SPP.  epat_connect does the
        # same before requesting a mode; a peripheral mid-transfer will not
        # answer a negotiation.
        self.outp(BASE, 0x00)
        self.out_again(0x00)
        self.outp(CTL, 0x01)
        self.out_again(0x01)
        self.out_again(0x04)
        self.out_again(0x0C)
        self.outp(BASE, 0x10)
        self.outp(CTL, 0x06)
        self.out_again(0x06)
        self.out_again(0x06)
        # The peripheral answers by pulling nAck low.  Budget 100h, as the
        # vendor uses - this is a handshake, not a transfer.
        self.mov_cx(0x100)
        self.label(tag + "_lp")
        self.inp(STAT)
        self.store_al(SLOT[stat])
        self.test_al(0x40)
        self.jz(tag + "_ok")
        self.dec_cx()
        self.jnz(tag + "_lp")
        self.mov_al(1)
        self.jmp(tag + "_done")
        self.label(tag + "_ok")
        self.mov_al(0)
        self.label(tag + "_done")
        self.store_al(SLOT[flag])
        self.outp(CTL, 0x07)
        self.out_again(0x07)
        self.out_again(0x04)
        self.out_again(0x04)
        self.sti()

    def wait_ecr(self, set_wanted, budget, flag_slot, ecr_slot, cnt_slot, tag):
        """Spin on ECR bit 0 with the vendor's own budgets.  The ECR is stored
        every iteration, so the slot ends up holding the last value seen -
        which is the evidence when the wait expires."""
        lp, ok, done = f"{tag}_lp", f"{tag}_ok", f"{tag}_done"
        self.mov_cx(budget)
        self.label(lp)
        self.inp(ECR)
        self.store_al(ecr_slot)
        self.test_al(0x01)
        if set_wanted:
            self.jnz(ok)
        else:
            self.jz(ok)
        self.dec_cx()
        self.jnz(lp)
        self.mov_al(1)
        self.jmp(done)
        self.label(ok)
        self.mov_al(0)
        self.label(done)
        self.store_al(flag_slot)
        self.store_cx(cnt_slot)

    # --- assembly ---
    def resolve(self):
        for pos, name in self.fix8:
            if name not in self.labels:
                raise KeyError(f"undefined label {name}")
            rel = self.labels[name] - (pos + 1)
            if not -128 <= rel <= 127:
                raise ValueError(f"rel8 out of range for {name}: {rel}")
            self.buf[pos] = rel & 0xFF
        for pos, name in self.fix16:
            struct.pack_into("<H", self.buf, pos, ORG + self.labels[name])
        return bytes(self.buf)


# The result buffer and the strings sit after the code.  Their addresses are
# only known once the code is emitted, so the emitter is run twice: the first
# pass fixes the code length, the second places the data at the right address.
ORG_RES = 0


def build(res_addr):
    global ORG_RES
    ORG_RES = res_addr
    e = Emitter()

    # Marker first, so a truncated or stale file is obvious.
    e.mov_al(0xA5)
    e.store_al(SLOT["marker"])

    e.inp_store(ECR, SLOT["ecr_entry"])

    # --- cold bring-up, as LS_BringUp does it ---
    # 1. Mask the ECR rather than storing a value into it.
    e.inp(ECR)
    e.and_al(0x34)
    e.mov_dx(ECR)
    e.out_dx()
    # 2. The control-port kick.
    e.outp(CTL, 0x04)
    e.out_again(0x0C)
    e.out_again(0x0E)
    e.out_again(0x0E)
    e.out_again(0x0E)
    e.out_again(0x04)
    e.out_again(0x04)
    # 3. The CPP preamble, then connect with the checkpoints armed.
    for cmd in (0x30, 0x40, 0x50, 0x00):
        e.cpp_frame(cmd)
    e.cpp_frame(0xE0, checkpoints=True)

    # --- configure the bridge, which nothing here has ever done ---
    # SD120PPD.SYS 0x25EB, reached with AX=1 from every call site: bit 1 of the
    # mask is clear so register 12h is left alone, and (mask & 1Bh) is true so
    # register 0Dh gets (old & FCh) | 1.  epat.c configures the bridge at the
    # same point for the same reason.  Our bring-up writes no bridge registers
    # at all, which is fine for nibble and is the open question for ECP.
    e.nibble_read(0x0D)
    e.store_al(SLOT["br0d_pre"])
    e.and_al(0xFC)
    e.or_al(0x01)
    e.mov_bl_al()
    e.reg_write_bl(0x0D)
    e.nibble_read(0x0D)
    e.store_al(SLOT["br0d_post"])

    # --- the control: nibble, which is the proven path ---
    e.nibble_read(CONT_TASKFILE + ATA_REG_STATUS)
    e.store_al(SLOT["nib_pre"])

    # The drive powers up held in ATA reset; until SRST is pulsed every task
    # file register reads 00h and the bridge looks dead (TRANSPORT_SPEC 5).
    e.reg_write(CONT_IDECTL + ATA_REG_DEVCTL, 0x04)
    e.reg_write(CONT_IDECTL + ATA_REG_DEVCTL, 0x00)

    e.mov_cx(0x4000)
    e.label("bsy_lp")
    e.nibble_read(CONT_TASKFILE + ATA_REG_STATUS)
    e.store_al(SLOT["nib_post"])
    e.test_al(0x80)
    e.jz("bsy_ok")
    e.dec_cx()
    e.jnz("bsy_lp")
    e.mov_al(1)
    e.jmp("bsy_done")
    e.label("bsy_ok")
    e.mov_al(0)
    e.label("bsy_done")
    e.store_al(SLOT["srst_flag"])

    e.nibble_read(CONT_TASKFILE + ATA_REG_BCLO)
    e.store_al(SLOT["nib_bclo"])
    e.nibble_read(CONT_TASKFILE + ATA_REG_BCHI)
    e.store_al(SLOT["nib_bchi"])

    # --- negotiate the peripheral into ECP before using it ---
    e.negotiate_ecp()
    # 0x2A21: a failed negotiation is not fatal to the vendor.  It drives
    # 0Ch then 0Eh, waits for nAck to return high, and negotiates once more.
    e.cli()
    e.outp(CTL, 0x0C)
    e.out_again(0x0E)
    e.mov_cx(0x8000)
    e.label("rec_lp")
    e.inp(STAT)
    e.store_al(SLOT["rec_stat"])
    e.test_al(0x40)
    e.jnz("rec_done")
    e.dec_cx()
    e.jnz("rec_lp")
    e.label("rec_done")
    e.outp(CTL, 0x0C)
    e.sti()
    e.negotiate_ecp("neg2", "neg_flag2", "neg_stat2")

    # --- the measurement: the vendor's ECP register read, 3CCEh ---
    e.cli()
    e.outp(CTL, 0x04)                      # forward
    e.outp(ECR, 0x74)                      # ECP mode
    e.sti()
    e.inp_store(ECR, SLOT["ecp_ecr_mode"])
    e.wait_ecr(True, 0xFFFF, SLOT["w1_flag"], SLOT["w1_ecr"], CNT_W1, "w1")

    e.outp(BASE, CONT_TASKFILE + ATA_REG_BCLO)     # the ECP address cycle
    e.wait_ecr(True, 0xFFFF, SLOT["w2_flag"], SLOT["w2_ecr"], CNT_W2, "w2")

    e.cli()
    e.outp(ECR, 0x34)                      # leave ECP
    e.outp(CTL, 0x20)                      # REVERSE
    e.outp(ECR, 0x74)                      # back into ECP
    e.sti()
    e.inp_store(ECR, SLOT["ecp_ecr_rev"])
    e.wait_ecr(False, 0x8000, SLOT["w3_flag"], SLOT["w3_ecr"], CNT_W3, "w3")

    e.inp_store(FIFO, SLOT["ecp_byte"])
    e.inp_store(ECR, SLOT["ecr_after"])

    # --- teardown: leave the port as it was found ---
    e.outp(CTL, 0x04)
    e.outp(ECR, 0x34)
    e.outp(CTL, 0x0C)                      # terminate 1284, back to compatibility
    e.out_again(0x04)
    e.cpp_frame(0x30)                      # disconnect
    e.load_al(SLOT["ecr_entry"])
    e.mov_dx(ECR)
    e.out_dx()
    e.inp_store(ECR, SLOT["ecr_restored"])
    e.outp(CTL, 0x0C)                      # as snapshotted before any of this

    # --- hand the results back as a file ---
    e.mov_ah(0x3C)
    e.mov_cx(0)
    e.db(0xBA)
    e.addr_of("fname")
    e.int_(0x21)
    e.mov_bx_ax()
    e.mov_ah(0x40)
    e.mov_cx(RESLEN)
    e.db(0xBA)
    e.addr_of("res")
    e.int_(0x21)
    e.mov_ah(0x3E)
    e.int_(0x21)

    e.mov_ah(0x09)
    e.db(0xBA)
    e.addr_of("msg")
    e.int_(0x21)
    e.int_(0x20)

    e.label("fname")
    e.db(*b"C:\\ECPPROBE.BIN\x00")
    e.label("msg")
    e.db(*b"ECPPROBE DONE\r\n$")
    e.label("res")
    for _ in range(RESLEN):
        e.db(0)

    return e


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "ECPPROBE.COM"
    # Pass 1 to size the code, pass 2 to place the data.
    first = build(0)
    first.resolve()
    res_addr = ORG + first.labels["res"]
    second = build(res_addr)
    blob = second.resolve()
    if ORG + second.labels["res"] != res_addr:
        raise RuntimeError("code length changed between passes")
    with open(out, "wb") as f:
        f.write(blob)
    print(f"{out}  {len(blob)} bytes  res at {res_addr:04X}")
    print("slots:", ", ".join(f"{i}:{n}" for i, n in enumerate(SLOTS)))


if __name__ == "__main__":
    main()
