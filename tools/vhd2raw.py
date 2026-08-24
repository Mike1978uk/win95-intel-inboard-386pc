#!/usr/bin/env python3
r"""Convert a Microsoft VHD (fixed or dynamic) to a raw disk image.

Reporters attach VHDs; every tool in this repo that reads a disk (fatls.py,
fatput.py, sweep_image_dma.py) wants raw sectors. qemu-img is not installed on
this box, so this is the bridge.

  python tools/vhd2raw.py <in.vhd> <out.img>

Dynamic VHDs store a BAT of block offsets; unallocated blocks read as zeroes and
each allocated block carries a sector bitmap ahead of its data, which is skipped
here (a sector that is allocated but marked unused still reads back as whatever
the block holds, which is what a real reader sees).
"""
import struct, sys

def conv(src, dst):
    with open(src, 'rb') as f:
        data = f.read()
    if len(data) < 512:
        sys.exit("too small to be a VHD")
    foot = data[-512:]
    if foot[:8] != b'conectix':
        # Some writers put the footer only at the front for fixed disks.
        if data[:8] == b'conectix':
            foot = data[:512]
        else:
            sys.exit("no 'conectix' footer - not a VHD")
    disk_type = struct.unpack('>I', foot[60:64])[0]
    cur_size  = struct.unpack('>Q', foot[48:56])[0]
    print(f"VHD type={disk_type} ({'fixed' if disk_type==2 else 'dynamic' if disk_type==3 else 'other'}) "
          f"virtual size={cur_size} bytes ({cur_size//1024} KB)")

    if disk_type == 2:
        with open(dst, 'wb') as o:
            o.write(data[:cur_size])
        print(f"wrote {dst} ({cur_size} bytes)")
        return

    if disk_type != 3:
        sys.exit(f"unsupported VHD type {disk_type}")

    hdr_off = struct.unpack('>Q', foot[16:24])[0]
    hdr = data[hdr_off:hdr_off + 1024]
    if hdr[:8] != b'cxsparse':
        sys.exit("dynamic header missing 'cxsparse'")
    bat_off    = struct.unpack('>Q', hdr[16:24])[0]
    max_ent    = struct.unpack('>I', hdr[28:32])[0]
    block_size = struct.unpack('>I', hdr[32:36])[0]
    sectors_pb = block_size // 512
    bitmap_sz  = ((sectors_pb + 7) // 8 + 511) // 512 * 512
    print(f"BAT@{bat_off} entries={max_ent} block={block_size} bitmap={bitmap_sz}")

    bat = struct.unpack(f'>{max_ent}I', data[bat_off:bat_off + 4 * max_ent])
    zero = bytes(block_size)
    used = 0
    with open(dst, 'wb') as o:
        for e in bat:
            if e == 0xFFFFFFFF:
                o.write(zero)
            else:
                used += 1
                start = e * 512 + bitmap_sz
                o.write(data[start:start + block_size].ljust(block_size, b'\0'))
        o.truncate(cur_size)
    print(f"wrote {dst} ({cur_size} bytes, {used}/{max_ent} blocks allocated)")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    conv(sys.argv[1], sys.argv[2])
