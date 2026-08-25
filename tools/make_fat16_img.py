"""Build a bootable-partition-less FAT16 hard-disk image from a host directory tree.

Written because installing Windows 3.0 [Inboard 386] from its fifteen 360K disks means
fifteen media swaps driven by screenshot polling. Putting the whole disk set on a second
hard disk and running SETUP from there is what a period user would do with COPY, and it
turns the install into one unattended run.

Host-side equivalence note (see memory/feedback_real_hardware_reproducibility): on real
hardware this is "copy the fifteen disks to a spare partition", nothing more.

Usage: make_fat16_img.py <out.img> <srcdir> [cyls heads spt]
"""
import os, struct, sys

def lfn_safe(name):
    base, _, ext = name.upper().partition('.')
    return base[:8].ljust(8), ext[:3].ljust(3)

def build(out, src, cyls=1024, heads=8, spt=17):
    SEC = 512
    total = cyls * heads * spt
    part_start = spt                      # CHS 0/1/1 - track 0 reserved for the MBR
    part_secs = total - part_start
    spc = 8                               # 4 KB clusters
    root_ents = 512
    root_secs = (root_ents * 32) // SEC
    reserved = 1
    # Solve for the FAT size: every cluster needs two bytes in each of two FATs.
    fat_secs = 1
    while True:
        data = part_secs - reserved - 2 * fat_secs - root_secs
        clusters = data // spc
        need = -(-((clusters + 2) * 2) // SEC)
        if need <= fat_secs:
            break
        fat_secs = need
    assert 4085 < clusters < 65525, f"cluster count {clusters} is not FAT16"

    fat = bytearray(fat_secs * SEC)
    struct.pack_into('<HH', fat, 0, 0xFFF8, 0xFFFF)
    root = bytearray(root_secs * 32 * 0)  # filled below
    root = bytearray(root_ents * 32)
    dataarea = bytearray()
    next_free = [2]

    def alloc_chain(nbytes):
        n = max(1, -(-nbytes // (spc * SEC)))
        first = next_free[0]
        for i in range(n):
            c = first + i
            struct.pack_into('<H', fat, c * 2, 0xFFFF if i == n - 1 else c + 1)
        next_free[0] += n
        return first, n

    def write_dir(path, entries, is_root, self_clus=0, parent_clus=0):
        """Returns (first_cluster, dirbytes) - the root gets first_cluster 0."""
        buf = bytearray()
        if not is_root:
            for nm, cl in ((b'.       ', self_clus), (b'..      ', parent_clus)):
                buf += nm + b'   ' + bytes([0x10]) + bytes(14) + struct.pack('<HIH', cl, 0, 0)[0:2] \
                       + struct.pack('<H', cl) + struct.pack('<I', 0)
                buf = buf[:len(buf)]  # keep it explicit; layout fixed up below
        return buf

    # A flat two-pass build: recurse, laying each directory's cluster chain out as we go.
    def dir_entry(name8, ext3, attr, clus, size):
        return (name8.encode() + ext3.encode() + bytes([attr]) + bytes(10)
                + struct.pack('<HH', 0x6000, 0x1D01)      # time 12:00, date 1994-08-01
                + struct.pack('<H', clus) + struct.pack('<I', size))

    pending = []  # (dir_path, entries_bytearray, cluster_or_None_for_root)

    def emit_file(fp):
        size = os.path.getsize(fp)
        first, n = alloc_chain(size)
        with open(fp, 'rb') as f:
            blob = f.read()
        blob += bytes(n * spc * SEC - len(blob))
        need = (first - 2 + n) * spc * SEC
        if len(dataarea) < need:
            dataarea.extend(bytes(need - len(dataarea)))
        dataarea[(first - 2) * spc * SEC:need] = blob
        return first, size

    def emit_dir(path, self_clus, parent_clus, self_nclus=1):
        ents = bytearray()
        if self_clus:
            ents += dir_entry('.       ', '   ', 0x10, self_clus, 0)
            ents += dir_entry('..      ', '   ', 0x10, parent_clus, 0)
        children = sorted(os.listdir(path))
        subdirs = []
        for nm in children:
            fp = os.path.join(path, nm)
            b, e = lfn_safe(nm)
            if os.path.isdir(fp):
                # Size the subdirectory before allocating: 279 files in one directory needs
                # three 4 KB clusters, and a one-cluster assumption silently truncates it.
                n_ents = len(os.listdir(fp)) + 2
                first, n = alloc_chain(n_ents * 32)
                subdirs.append((fp, first, self_clus, n))
                ents += dir_entry(b, '   ', 0x10, first, 0)
            else:
                first, size = emit_file(fp)
                ents += dir_entry(b, e, 0x20, first, size)
        if self_clus:
            need_clus = max(1, -(-len(ents) // (spc * SEC)))
            assert need_clus <= self_nclus, f"{path}: needs {need_clus} clusters, got {self_nclus}"
            blob = bytes(ents) + bytes(self_nclus * spc * SEC - len(ents))
            end = (self_clus - 2 + self_nclus) * spc * SEC
            if len(dataarea) < end:
                dataarea.extend(bytes(end - len(dataarea)))
            dataarea[(self_clus - 2) * spc * SEC:end] = blob
        else:
            root[:len(ents)] = ents
        for fp, first, parent, nclus in subdirs:
            emit_dir(fp, first, parent, nclus)

    emit_dir(src, 0, 0)

    boot = bytearray(SEC)
    boot[0:3] = b'\xEB\x3C\x90'
    boot[3:11] = b'MSDOS5.0'
    struct.pack_into('<HBHBHHBHHHII', boot, 11,
                     SEC, spc, reserved, 2, root_ents, 0, 0xF8, fat_secs,
                     spt, heads, part_start, part_secs)
    boot[36] = 0x80; boot[38] = 0x29
    struct.pack_into('<I', boot, 39, 0x12345678)
    boot[43:54] = b'WIN30SRC   '
    boot[54:62] = b'FAT16   '
    boot[510:512] = b'\x55\xAA'

    mbr = bytearray(SEC)
    endc, endh, ends = cyls - 1, heads - 1, spt
    mbr[446:462] = bytes([0x00, 0x01, 0x01, 0x00, 0x06,
                          endh, ((endc >> 2) & 0xC0) | ends, endc & 0xFF]) \
                   + struct.pack('<II', part_start, part_secs)
    mbr[510:512] = b'\x55\xAA'

    img = bytearray()
    img += mbr + bytes((part_start - 1) * SEC)
    img += boot
    img += fat + fat
    img += root
    img += dataarea
    img += bytes(total * SEC - len(img))
    with open(out, 'wb') as f:
        f.write(img)
    print(f"{out}: {total} sectors ({total*SEC//1048576} MB), {clusters} clusters of {spc*SEC//1024}K, "
          f"{next_free[0]-2} used ({(next_free[0]-2)*spc*SEC//1048576} MB), geometry {cyls}/{heads}/{spt}")

if __name__ == '__main__':
    a = sys.argv[1:]
    build(a[0], a[1], *(int(x) for x in a[2:]))
