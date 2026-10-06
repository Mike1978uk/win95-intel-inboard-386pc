#!/usr/bin/env python3
"""Decode every port table ATI's TEST.COM plays through its table runner at 0641.

    python tools/testcom_tables.py TEST.COM [--full]

TEST.COM's tests are mostly data: a routine loads SI with a table and calls 0641, which
writes each (port, value) word pair, treats port FFFF as "wait for the engine to go idle"
and stops at a port whose low byte is 00. This finds every `mov si, imm16` followed within
a few bytes by `call 0641`, decodes the table, and prints it with 8514/A and Mach8 register
names. Without --full each table is summarised (writes, commands, mixes); with --full every
write is listed.

TEST.COM is ATI's code: this prints its tables for study, it does not reproduce the file.
"""

import struct
import sys

RUNNER = 0x641
NAMES = {
    0x42e8: "SUBSYS_CNTL", 0x4ae8: "ADVFUNC_CNTL", 0x82e8: "CUR_Y", 0x86e8: "CUR_X",
    0x8ae8: "DESTY_AXSTP", 0x8ee8: "DESTX_DIASTP", 0x92e8: "ERR_TERM", 0x96e8: "MAJ_AXIS_PCNT",
    0x9ae8: "CMD", 0x9ee8: "SHORT_STROKE", 0xa2e8: "BKGD_COLOR", 0xa6e8: "FRGD_COLOR",
    0xaae8: "WRT_MASK", 0xaee8: "RD_MASK", 0xb2e8: "COLOR_CMP", 0xb6e8: "BKGD_MIX",
    0xbae8: "FRGD_MIX", 0xbee8: "MULTIFUNC_CNTL", 0xe2e8: "PIX_TRANS", 0x22e8: "DISP_CNTL",
    0x02e8: "H_TOTAL", 0x06e8: "H_DISP", 0x0ae8: "H_SYNC_STRT", 0x0ee8: "H_SYNC_WID",
    0x12e8: "V_TOTAL", 0x16e8: "V_DISP", 0x1ae8: "V_SYNC_STRT", 0x1ee8: "V_SYNC_WID",
    0x82ee: "PATT_DATA_INDEX", 0x8eee: "PATT_DATA", 0x92ee: "MASK/PATT_DATA_HI",
    0x96ee: "BRES_COUNT", 0x9aee: "EXT_FIFO_STATUS/LINEDRAW_INDEX", 0xa2ee: "LINEDRAW_OPT",
    0xa6ee: "DEST_X_START", 0xaaee: "DEST_X_END", 0xaeee: "DEST_Y_END", 0xb2ee: "SRC_X/PROPS",
    0xb6ee: "SRC_X_START", 0xbaee: "SRC_X_END", 0xbeee: "SRC_Y_DIR", 0xc2ee: "EXT_SHORT_STROKE",
    0xc6ee: "SCAN_X", 0xcaee: "SCAN_TO_X", 0xceee: "DP_CONFIG", 0xd2ee: "PATT_LENGTH",
    0xd6ee: "PATT_INDEX", 0xdaee: "EXT_SCISSOR_L", 0xdeee: "EXT_SCISSOR_T",
    0xe2ee: "EXT_SCISSOR_R", 0xe6ee: "EXT_SCISSOR_B", 0xeeee: "DEST_CMP_FN", 0xf2ee: "DEST_COLOR_CMP_MASK",
    0xfeee: "LINEDRAW", 0x6eee: "GE_OFFSET_LO", 0x72ee: "GE_OFFSET_HI", 0x76ee: "GE_PITCH",
    0x7aee: "EXT_GE_CONFIG", 0x7eee: "MISC_OPTIONS?", 0x4aee: "CLOCK_SEL", 0x52ee: "ROM_ADDR_1",
    0x56ee: "ROM_ADDR_2", 0x5aee: "SHADOW_SET?", 0x32ee: "LOCAL_CNTL", 0x36ee: "FIFO_OPT",
}
MF = {0: "MIN_AXIS", 1: "SCISSOR_T", 2: "SCISSOR_L", 3: "SCISSOR_B", 4: "SCISSOR_R",
      5: "MEM_CNTL", 8: "FIXED_PATT", 0xa: "PIX_CNTL"}


def name(port, val):
    n = NAMES.get(port, "%04X" % port)
    if port == 0xbee8:
        n += "." + MF.get(val >> 12, "idx%X" % (val >> 12))
    return n


def decode(data, addr):
    p = addr - 0x100
    out = []
    while p + 2 <= len(data):
        port = struct.unpack_from("<H", data, p)[0]
        if port & 0xff == 0:
            return out, addr + (p - (addr - 0x100)) + 2
        if port == 0xffff:
            out.append(("WAIT_IDLE", None))
            p += 2
            continue
        val = struct.unpack_from("<H", data, p + 2)[0]
        out.append((port, val))
        p += 4
    return out, None


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], "rb").read()
    full = "--full" in sys.argv
    sites = []
    for i in range(len(data) - 6):
        if data[i] != 0xbe:                      # mov si, imm16
            continue
        si = struct.unpack_from("<H", data, i + 1)[0]
        for j in range(i + 3, min(i + 12, len(data) - 3)):
            if data[j] == 0xe8:                  # call rel16
                tgt = (0x100 + j + 3 + struct.unpack_from("<h", data, j + 1)[0]) & 0xffff
                if tgt == RUNNER:
                    sites.append((0x100 + i, si))
                break
    seen = {}
    for at, si in sites:
        seen.setdefault(si, []).append(at)
    print("%d call sites, %d distinct tables" % (len(sites), len(seen)))
    for si in sorted(seen):
        ops, end = decode(data, si)
        cmds = [v for p, v in ops if p == 0x9ae8]
        mixes = sorted({v for p, v in ops if p in (0xbae8, 0xb6e8)})
        print()
        print("table %04X-%s  called from %s  %d writes, %d CMD" % (
            si, "%04X" % end if end else "?", " ".join("%04X" % a for a in seen[si]), len(ops), len(cmds)))
        if mixes:
            print("  mixes " + " ".join("%02X" % m for m in mixes))
        if full or len(ops) <= 24:
            for p, v in ops:
                print("  WAIT_IDLE" if p == "WAIT_IDLE" else "  %-28s %04X" % (name(p, v), v))
        else:
            for p, v in ops[:12]:
                print("  WAIT_IDLE" if p == "WAIT_IDLE" else "  %-28s %04X" % (name(p, v), v))
            print("  ... %d more" % (len(ops) - 12))


if __name__ == "__main__":
    main()
