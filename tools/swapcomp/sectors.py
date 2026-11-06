# Which swap compressor moves the fewest sectors per page-in on the 5160, and
# what each costs once its CPU time is added. Run against the card's swap file:
#
#   python sectors.py D:/WIN386.SWP [pages]
#
# The bus moves whole sectors (1.96 ms each, XTIDETR.ASM's 3.82 us a byte), so a
# page that compresses to 1,030 bytes costs three, the same as one of 1,536. This
# counts sectors as SWAPCOMP.VXD stores them: slot N is at file offset N * 4096
# and holds a u16 length, an LZ4 block, then stale bytes; a page needing more than
# SC_MAX_SECTORS is stored raw (8 sectors). Slots are decoded before analysis.
# Never commit the swap file: it holds whatever was in memory.
import lzma, random, sys, zlib

P, SECTOR, MAX_SECTORS = 4096, 512, 6
BUS_MS = 1.96            # one sector across the bus (measured, 3.82 us/B)
CMD_MS = 0.29            # first-DRQ latency 238 us + ~50 us of taskfile (measured)
WRITE_SHARE = 0.35       # page-outs per page-in (SWAPCNT bed run, 2026-10-02)

# CPU per 4 KB page on the 5160, ms. LZ4 figures measured (LZ4BENCH 2026-10-02,
# GCC's shift-and-add hash); the rest are estimates to override when measured.
CPU = {
    'store':        (0.0, 0.0),
    'lz4':          (4.71, 1.17),
    'lz4-hc':       (9.29, 1.17),     # LZ4BENCH 4-candidate search; same decoder
    'lz4-sector':   (None, 1.17),     # fast, then the search only where it saves a sector
    'deflate-1':    (8.0, 6.0),       # estimate: Huffman decode on a 486
    'deflate-9':    (40.0, 6.0),      # estimate
    'lzma':         (200.0, 30.0),    # estimate; a ceiling on ratio, not a candidate
}


def ext(n):
    return 0 if n < 15 else 1 + (n - 15) // 255


def lz4(p, hc):
    """LZ4 block size: token, literals, 2-byte offset, extended lengths; min match 4."""
    chains = {}; i = 0; lit = 0; out = 0; n = len(p)
    while i < n:
        best = 0
        if i + 4 <= n - 5:
            k = p[i:i+4]
            cands = chains.get(k, [])
            for j in (reversed(cands) if hc else cands[-1:]):
                m = 4
                while i + m < n - 5 and p[j+m] == p[i+m]:
                    m += 1
                if m > best:
                    best = m
            chains.setdefault(k, []).append(i)
        if best >= 4:
            out += 1 + ext(lit) + lit + 2 + ext(best - 4)
            for q in range(i + 1, min(i + best, n - 4)):
                chains.setdefault(p[q:q+4], []).append(q)
            i += best; lit = 0
        else:
            lit += 1; i += 1
    return out + 1 + ext(lit) + lit


def sectors(nbytes):
    s = -(-(nbytes + 2) // SECTOR)
    return 8 if s > MAX_SECTORS else s


def raw_deflate(p, level):
    c = zlib.compressobj(level, zlib.DEFLATED, -15)
    return len(c.compress(p) + c.flush())


def lz4_decode(blk):
    """Page bytes if blk is a block LZ4_Decompress accepts as exactly P bytes, else None."""
    i = 0; n = len(blk); out = bytearray()
    while i < n:
        tok = blk[i]; i += 1
        lit = tok >> 4
        if lit == 15:
            while True:
                if i >= n:
                    return None
                b = blk[i]; i += 1; lit += b
                if b != 255:
                    break
        if lit > n - i or len(out) + lit > P:
            return None
        out += blk[i:i+lit]; i += lit
        if i >= n:
            break
        if n - i < 2:
            return None
        off = blk[i] | blk[i+1] << 8; i += 2
        if off == 0 or off > len(out):
            return None
        ml = tok & 15
        if ml == 15:
            while True:
                if i >= n:
                    return None
                b = blk[i]; i += 1; ml += b
                if b != 255:
                    break
        ml += 4
        if len(out) + ml > P:
            return None
        for _ in range(ml):
            out.append(out[-off])
    return bytes(out) if len(out) == P else None


def main():
    pages = []; zero = 0; comp = 0; hist = {}
    with open(sys.argv[1], 'rb') as f:
        while True:
            slot = f.read(P)
            if len(slot) < P:
                break
            if slot.count(0) == P:
                zero += 1
                continue
            n = slot[0] | slot[1] << 8
            page = lz4_decode(slot[2:2+n]) if n + 2 <= P else None
            if page is None:
                pages.append(slot)
            else:
                pages.append(page); comp += 1
                s = -(-(n + 2) // SECTOR)
                hist[s] = hist.get(s, 0) + 1
    want = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    used = pages
    print(f'{zero + len(used)} slots: {zero} never written, {comp} compressed, {len(used) - comp} raw')
    if used:
        print(f'{100.0 * comp / len(used):.1f}% of used slots decoded; sectors on disk, compressed slots: '
              + ', '.join(f'{k}: {hist[k]}' for k in sorted(hist)))
    if used and comp * 2 < len(used):
        print('WARNING: under 50% of used slots decode as SWAPCOMP blocks; the slot layout is wrong '
              'or this swap file was not written by a SWAPCOMP boot. Figures below are not meaningful.')
    same = sum(1 for p in used if p == p[:4] * (P // 4))
    random.seed(1)
    samp = random.sample(used, min(want, len(used)))
    print(f'{same} of used slots decode to one repeated dword ({100.0 * same / max(1, len(used)):.1f}%, '
          f'no I/O needed); sample {len(samp)}')

    sizes = {
        'store': [P] * len(samp),
        'lz4': [lz4(p, False) for p in samp],
        'lz4-hc': [lz4(p, True) for p in samp],
        'deflate-1': [raw_deflate(p, 1) for p in samp],
        'deflate-9': [raw_deflate(p, 9) for p in samp],
        'lzma': [len(lzma.compress(p, format=lzma.FORMAT_RAW,
                                   filters=[{'id': lzma.FILTER_LZMA2, 'preset': 9}])) for p in samp],
    }
    # Sector-aware LZ4: run the search only when the fast result has a chance to
    # drop a sector, i.e. it ends in the first quarter of its last sector.
    tried = saved = 0
    sa = []
    for f, h in zip(sizes['lz4'], sizes['lz4-hc']):
        if (f + 2) % SECTOR and (f + 2) % SECTOR <= SECTOR // 4:
            tried += 1
            if sectors(h) < sectors(f):
                saved += 1
                sa.append(h)
                continue
        sa.append(f)
    sizes['lz4-sector'] = sa
    frac = tried / len(samp)
    CPU['lz4-sector'] = (CPU['lz4'][0] + frac * CPU['lz4-hc'][0], CPU['lz4'][1])

    print(f'lz4-sector: search tried on {100 * frac:.0f}% of pages, saved a sector on {100.0 * saved / len(samp):.0f}%')
    print(f'{"codec":<11} {"ratio":>6} {"sectors":>8} {"read ms":>8} {"write ms":>9} {"per page-in":>12}  CPU source')
    for name in ('store', 'lz4', 'lz4-hc', 'lz4-sector', 'deflate-1', 'deflate-9', 'lzma'):
        sz = sizes[name]
        sec = sum(sectors(x) for x in sz) / len(sz)
        comp, dec = CPU[name]
        rd = CMD_MS + sec * BUS_MS + dec
        wr = CMD_MS + sec * BUS_MS + comp
        src = 'measured' if name in ('store', 'lz4', 'lz4-hc') else 'derived' if name == 'lz4-sector' else 'ESTIMATE'
        print(f'{name:<11} {len(sz) * P / sum(sz):6.2f} {sec:8.2f} {rd:8.2f} {wr:9.2f} {rd + WRITE_SHARE * wr:12.2f}  {src}')


if __name__ == '__main__':
    main()
