#!/usr/bin/env python3
"""Emit a .COM that PIT-times a cache-resident CPU loop, for the DMA/CPU overlap test.

This is the instrument for A4 and A6 in `docs/bus_optimisation_plan.md`, built
before the thing it measures (technique 123).

The loop touches **no memory at all** - it is register-only, so on a machine
whose cache is enabled it runs entirely out of L1 and occupies no bus cycle.
That is the whole point: if the same loop takes longer while a DMA transfer is
in flight, the Inboard is stalling for the 8237 rather than running through it,
and the difference is bus cycles stolen per DMA byte.

Stage 1 (this file, `--passes N`) measures the loop alone, several times, so the
harness itself can be trusted before any device is involved: the passes should
agree to within the PIT's own noise. A spread here means the instrument is
wrong, not the machine.

⛔ Deliberately NOT a DEBUG script. `DEBUG < file` takes stdin from the file, so
at EOF no keystroke reaches it and it wedges at the `-` prompt with no remote
recovery. That has cost this project two sessions. A .COM takes no stdin and
exits on its own.

Results land in the BIOS Intra-Applications Communications Area at 0040:00F0,
16 bytes reserved for exactly this, so they survive the program exiting and can
be read back with a plain memory read.

    python tools/gen_busload_com.py --passes 4 --out BUSLOAD.COM
"""
import argparse
import struct
import sys

IACA = 0x00F0          # 0040:00F0, BIOS scratch, 16 bytes
MAX_PASSES = 8         # 8 x 16-bit results is the whole reserved area


def pit_read():
    """Latch PIT channel 0 and leave the 16-bit count in BX.

    Latching (out 43h, 0) is the standard non-destructive read-back: it copies
    the counter into the output latch without touching the count itself, so
    this cannot disturb the system timer it is borrowing.
    """
    return bytes([0xB0, 0x00,        # mov al,0        latch ch0
                  0xE6, 0x43,        # out 43h,al
                  0xE4, 0x40,        # in  al,40h      low
                  0x88, 0xC3,        # mov bl,al
                  0xE4, 0x40,        # in  al,40h      high
                  0x88, 0xC7])       # mov bh,al  -> BX


def spin(iters):
    """A register-only delay loop: no memory operand, so no bus cycle.

    `loop` is used rather than dec/jnz because it is a single instruction and
    cannot be accidentally widened by a prefix.
    """
    return (bytes([0xB9]) + struct.pack("<H", iters) +   # mov cx,iters
            bytes([0x90,                                  # nop
                   0xE2, 0xFD]))                          # loop $-1


def one_pass(iters):
    b = pit_read()
    b += bytes([0x89, 0xDD])          # mov bp,bx        t0
    b += spin(iters)
    b += pit_read()                   # BX = t1
    b += bytes([0x29, 0xDD])          # sub bp,bx        PIT ch0 counts DOWN
    b += bytes([0x55])                # push bp
    return b


BUF = 0x4000           # 1 KB working set inside our own segment


def cached_pass(outer, dwords, io_port=None):
    """Re-read a small buffer `outer` times, optionally with one port read first.

    The buffer is 1 KB and the BL3's L1 is 16 KB, so after the first pass every
    read should hit cache and cost no bus cycle. Inserting ONE I/O access per
    outer iteration is the whole experiment: if SW1 is ON ("DMA + I/O
    read/write"), that access flushes the cache and the next 1 KB has to come
    back from Inboard RAM at ~135 ns/byte instead of from L1.

    The port is read, not written, and 0x21 is the 8259's interrupt mask
    register - a standard, non-destructive read with no side effects. Interrupts
    are already masked by the caller's cli.
    """
    b = pit_read()
    b += bytes([0x89, 0xDD])                        # mov bp,bx      t0
    b += bytes([0xBF]) + struct.pack("<H", outer)   # mov di,outer
    body = b""
    if io_port is not None:
        body += bytes([0xE4, io_port])              # in al,port
    body += bytes([0xBE]) + struct.pack("<H", BUF)  # mov si,BUF
    body += bytes([0xB9]) + struct.pack("<H", dwords)
    body += bytes([0x66, 0xF3, 0xAD])               # rep lodsd
    body += bytes([0x4F])                           # dec di
    # jnz back to the top of body
    body += bytes([0x75, (256 - (len(body) + 2)) & 0xFF])
    b += body
    b += pit_read()
    b += bytes([0x29, 0xDD])                        # sub bp,bx
    b += bytes([0x55])                              # push bp
    return b


def build_flush(outer, dwords, io_port):
    """Two passes: without the I/O access, then with it. Then repeat, to show
    the pair is stable rather than drifting."""
    c = bytes([0xFA, 0xFC])                         # cli ; cld
    c += cached_pass(outer, dwords, None)
    c += cached_pass(outer, dwords, io_port)
    c += cached_pass(outer, dwords, None)
    c += cached_pass(outer, dwords, io_port)
    c += bytes([0xFB])                              # sti
    c += bytes([0xB8, 0x40, 0x00, 0x8E, 0xD8])      # mov ax,40h ; mov ds,ax
    for i in range(3, -1, -1):
        c += bytes([0x5D])
        c += bytes([0x89, 0x2E]) + struct.pack("<H", IACA + i * 2)
    c += bytes([0xB4, 0x4C, 0xCD, 0x21])
    return c


def build(passes, iters):
    c = bytes([0xFA, 0xFC])                       # cli ; cld
    for _ in range(passes):
        c += one_pass(iters)
    c += bytes([0xFB])                            # sti
    c += bytes([0xB8, 0x40, 0x00, 0x8E, 0xD8])    # mov ax,40h ; mov ds,ax
    # Pops come back in reverse order, so walk the slots backwards.
    for i in range(passes - 1, -1, -1):
        c += bytes([0x5D])                        # pop bp
        c += bytes([0x89, 0x2E]) + struct.pack("<H", IACA + i * 2)
    c += bytes([0xB4, 0x4C, 0xCD, 0x21])          # mov ah,4Ch ; int 21h
    return c


def verify_flush(code, io_port):
    """The flush test is only meaningful if the two pass types differ by exactly
    one instruction, so check that rather than trusting the builder."""
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_16
    except ImportError:
        print("WARNING: capstone missing, code NOT verified")
        return
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    text = [i.mnemonic + " " + i.op_str for i in md.disasm(code, 0x100)]
    n_lods = sum(1 for t in text if t.startswith("rep lodsd"))
    n_io   = sum(1 for t in text if t == "in al, 0x%x" % io_port)
    n_latch = sum(1 for t in text if t.startswith("out 0x43"))
    if n_lods != 4:
        sys.exit("FAILED: %d rep lodsd, expected 4" % n_lods)
    if n_io != 2:
        sys.exit("FAILED: %d reads of port %02Xh, expected exactly 2 "
                 "(the with-IO passes only)" % (n_io, io_port))
    if n_latch != 8:
        sys.exit("FAILED: %d PIT latches, expected 8" % n_latch)
    stray = [t for t in text if t.startswith(("in ", "out "))
             and "0x4" not in t and ("0x%x" % io_port) not in t]
    if stray:
        sys.exit("FAILED: unexpected port access: %r" % stray)
    if any("int3" in t for t in text):
        sys.exit("FAILED: an INT 3 survived")
    print("verified: 4 rep lodsd, exactly 2 port reads, 8 PIT latches, nothing stray")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--passes", type=int, default=4)
    ap.add_argument("--iters", type=lambda s: int(s, 0), default=0x4000,
                    help="loop iterations per pass (default 0x4000)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--flush", action="store_true",
                    help="SW1 cache-flush test: 4 passes, alternating without/with "
                         "one I/O read per outer iteration")
    ap.add_argument("--outer", type=lambda s: int(s, 0), default=64)
    ap.add_argument("--dwords", type=lambda s: int(s, 0), default=256,
                    help="dwords re-read per outer iteration (256 = 1 KB)")
    ap.add_argument("--io-port", type=lambda s: int(s, 0), default=0x21,
                    help="port to read; 0x21 is the 8259 IMR, non-destructive")
    a = ap.parse_args()

    if a.flush:
        code = build_flush(a.outer, a.dwords, a.io_port)
        verify_flush(code, a.io_port)
        open(a.out, "wb").write(code)
        print("wrote %s: %d bytes, 4 passes (no-IO, IO, no-IO, IO), "
              "%d x %d dwords, port %02Xh, results at 0040:%04X"
              % (a.out, len(code), a.outer, a.dwords, a.io_port, IACA))
        return

    if not 1 <= a.passes <= MAX_PASSES:
        sys.exit("passes must be 1..%d - the IACA is only 16 bytes" % MAX_PASSES)
    if not 1 <= a.iters <= 0xFFFF:
        sys.exit("iters must fit in CX")

    code = build(a.passes, a.iters)

    # Verify the emitted bytes really are the instructions intended. A loop
    # that is one prefix wrong still runs and still produces a plausible
    # number, which is the failure mode this check exists for.
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_16
        md = Cs(CS_ARCH_X86, CS_MODE_16)
        text = [i.mnemonic + " " + i.op_str for i in md.disasm(code, 0x100)]
        n_loop = sum(1 for t in text if t.startswith("loop "))
        n_latch = sum(1 for t in text if t.startswith("out 0x43"))
        if n_loop != a.passes:
            sys.exit("FAILED: %d loop instructions, expected %d" % (n_loop, a.passes))
        if n_latch != a.passes * 2:
            sys.exit("FAILED: %d PIT latches, expected %d" % (n_latch, a.passes * 2))
        if any(t.startswith(("in ", "out ")) and "0x4" not in t for t in text):
            sys.exit("FAILED: a port access outside the PIT - this pass must touch nothing else")
        if any("int3" in t for t in text):
            sys.exit("FAILED: an INT 3 survived - this must not need DEBUG")
        print("verified: %d passes, %d PIT latches, no stray port access"
              % (n_loop, n_latch))
    except ImportError:
        print("WARNING: capstone missing, code NOT verified")

    open(a.out, "wb").write(code)
    print("wrote %s: %d bytes, %d passes x %d iters, results at 0040:%04X"
          % (a.out, len(code), a.passes, a.iters, IACA))


if __name__ == "__main__":
    main()
