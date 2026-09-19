#!/usr/bin/env python3
r"""Decompress PKWare DCL "implode" data - the format InstallShield Z archives use.

Written 2026-09-19 to pull 120PPD95.INF and SD120PPD.MPD out of the Imation
LS-120 installer's DATA.Z, because the vendor never shipped a loose INF and
the one inside the installer carries the hardware IDs Windows binds on.

  python tools/blast.py <file> <offset> <complen> <uncomplen> <out>

A DCL stream starts with two bytes: literal mode (0 = uncoded, 1 = coded) and
the dictionary size exponent (4, 5 or 6 -> 1K, 2K, 4K). Everything after is a
bit stream, least-significant bit first. Ported from Mark Adler's blast.c; the
three code tables below are his compact run-length form, one byte per run
holding ((count - 1) << 4) | bit_length.
"""
import sys

LITLEN = bytes([
    11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
    9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
    7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
    8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
    44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
    44, 173])
LENLEN = bytes([2, 35, 36, 53, 38, 23])
DISTLEN = bytes([2, 20, 53, 230, 247, 151, 248])

BASE = [3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264]
EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]


class Huffman:
    """Canonical Huffman table in blast.c's count/symbol form."""

    def __init__(self, rep):
        # Expand the run-length form into a per-symbol bit-length list.
        lengths = []
        for byte in rep:
            count = (byte >> 4) + 1
            lengths.extend([byte & 15] * count)

        self.count = [0] * 16
        for ln in lengths:
            self.count[ln] += 1

        # Offsets into the symbol table for each length, then place symbols.
        offs = [0] * 16
        for ln in range(1, 15):
            offs[ln + 1] = offs[ln] + self.count[ln]
        self.symbol = [0] * len(lengths)
        for sym, ln in enumerate(lengths):
            if ln:
                self.symbol[offs[ln]] = sym
                offs[ln] += 1


class BitReader:
    def __init__(self, data):
        self.data = data
        self.pos = 0
        self.bitbuf = 0
        self.bitcnt = 0

    def bits(self, need):
        val = self.bitbuf
        while self.bitcnt < need:
            val |= self.data[self.pos] << self.bitcnt
            self.pos += 1
            self.bitcnt += 8
        self.bitbuf = val >> need
        self.bitcnt -= need
        return val & ((1 << need) - 1)

    def decode(self, h):
        """blast.c's inverted-code Huffman decode: bits arrive complemented."""
        code = first = index = 0
        for ln in range(1, 16):
            code |= self.bits(1) ^ 1          # invert, as DCL stores them
            count = h.count[ln]
            if code - count < first:
                return h.symbol[index + (code - first)]
            index += count
            first = (first + count) << 1
            code <<= 1
        raise ValueError("bad DCL code")


def blast(data):
    lit = data[0]
    dictbits = data[1]
    if lit not in (0, 1):
        raise ValueError(f"bad literal flag {lit}")
    if not 4 <= dictbits <= 6:
        raise ValueError(f"bad dictionary size {dictbits}")

    litcode = Huffman(LITLEN) if lit else None
    lencode = Huffman(LENLEN)
    distcode = Huffman(DISTLEN)

    br = BitReader(data)
    br.pos = 2
    out = bytearray()

    while True:
        if br.bits(1):                         # 1 = length/distance pair
            sym = br.decode(lencode)
            length = BASE[sym] + br.bits(EXTRA[sym])
            if length == 519:                  # the end-of-stream length
                break
            sym = 2 if length == 2 else dictbits
            dist = br.decode(distcode) << sym
            dist += br.bits(sym)
            dist += 1
            if dist > len(out):
                raise ValueError("distance before start of output")
            for _ in range(length):
                out.append(out[len(out) - dist])
        else:                                  # 0 = literal
            out.append(br.decode(litcode) if litcode else br.bits(8))

    return bytes(out)


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    path, off, csz, usz, dest = sys.argv[1], *map(lambda s: int(s, 0), sys.argv[2:5]), sys.argv[5]
    blob = open(path, "rb").read()[off:off + csz]
    out = blast(blob)
    if len(out) != usz:
        print(f"WARNING: got {len(out)} bytes, index said {usz}", file=sys.stderr)
    open(dest, "wb").write(out)
    print(f"wrote {dest} ({len(out)} bytes)")


if __name__ == "__main__":
    main()
