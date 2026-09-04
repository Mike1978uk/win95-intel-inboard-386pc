#!/usr/bin/env python3
r"""Decode the XT-IDE port driver's DBGPORT stream out of an 86Box run log.

  python tools/pdr_dbgport.py <run.log>

The driver reports through emulator ports 0E0h/0E1h (tag, then the dword
LSB-first), which the fork's inboard386.c prints as

    DBGPORT tag=NN val=XXXXXXXX (nnn)

Reading it with a case-sensitive grep is how a whole evening got spent on a
contaminated slice on 2026-09-04 - the log writes 0000000E and the grep looked
for 0000000e, matched nothing, and an empty shell variable produced a byte
offset that sliced the DOS boot phase into the shutdown window. Everything here
is case-insensitive, and every count that could be zero is printed explicitly
rather than left as an absent line (technique 91a).
"""
import re
import sys

# Tags the driver emits. Keep in step with XTIDE_DbgEmit's call sites.
TAGS = {
    0x01: "counter Offered  (IOPs WantIop was asked about)",
    0x02: "counter Accepted (of which we claimed)",
    0x03: "counter Started  (XTIDE_StartRequest entries)",
    0x04: "counter Done     (handed back through IOP_callback_ptr)",
    0x10: "AEP func",
    0x11: "AEP object at hdr+12",
    0x12: "AEP word at hdr+16",
    0x15: "QUIESCE matched DCB",
    0x20: "DCB observed",
    0x21: "DCB 76h dword: freeze/sg/io_pend/lock",
    0x22: "DCB_device_flags (20h)",
    0x23: "DCB_vrp_ptr (18h)",
    0x24: "our InsertedDcb[0]",
    0x25: "our InsertedDcb[1]",
}

AEP = {
    0: "INITIALIZE", 1: "SYSTEM_CRIT_SHUTDOWN", 2: "BOOT_COMPLETE",
    3: "CONFIG_DCB", 4: "UNCONFIG_DCB", 5: "IOP_TIMEOUT",
    6: "DEVICE_INQUIRY", 7: "HALF_SEC", 12: "CREATE_VRP",
    13: "REAL_MODE_HANDOFF", 14: "SYSTEM_SHUTDOWN", 15: "UNINITIALIZE",
    16: "DCB_LOCK", 17: "MOUNT_NOTIFY", 18: "ASSOCIATE_DCB",
    19: "DESTROY_VRP", 20: "REFRESH_DRIVE", 21: "PEND_UNCONFIG_DCB",
}

LINE = re.compile(rb"DBGPORT\s+tag=([0-9a-f]{1,2})\s+val=([0-9a-f]{8})", re.I)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    raw = open(sys.argv[1], "rb").read()
    rec = [(int(t, 16), int(v, 16)) for t, v in LINE.findall(raw)]
    if not rec:
        print("NO DBGPORT RECORDS AT ALL in %s" % sys.argv[1])
        print("  The driver never reached its first emit, or this build has")
        print("  no instrumentation, or the emulator's hook is not compiled in.")
        print("  This is a VOID run, not a negative result.")
        return 1

    print("%d DBGPORT records\n" % len(rec))

    # --- the AEP ladder, in arrival order -------------------------------
    print("=== AEP ladder ===")
    i = 0
    while i < len(rec):
        tag, val = rec[i]
        if tag == 0x10:
            obj = xtra = None
            if i + 1 < len(rec) and rec[i + 1][0] == 0x11:
                obj = rec[i + 1][1]
            if i + 2 < len(rec) and rec[i + 2][0] == 0x12:
                xtra = rec[i + 2][1]
            print("  %-3d %-22s obj=%s  x=%s" % (
                val, AEP.get(val, "?"),
                "%08X" % obj if obj is not None else "-",
                "%08X" % xtra if xtra is not None else "-"))
        elif tag == 0x15:
            print("      >> QUIESCE matched dcb=%08X" % val)
        i += 1

    # --- the completion ladder ------------------------------------------
    print("\n=== completion ladder at AEP_SYSTEM_SHUTDOWN ===")
    ladder = {t: v for t, v in rec if t in (1, 2, 3, 4)}
    if not ladder:
        print("  NOT EMITTED - AEP_SYSTEM_SHUTDOWN never reached the driver.")
    else:
        for t in (1, 2, 3, 4):
            print("  %-52s %d" % (TAGS[t], ladder.get(t, -1)))
        bal = len({ladder.get(t) for t in (2, 3, 4)}) == 1
        print("  -> %s" % ("BALANCED: nothing of ours is outstanding."
                           if bal else
                           "IMBALANCED: an IOP went in and did not come back."))

    # --- the per-DCB dump -----------------------------------------------
    print("\n=== DCB counters, read mid-teardown ===")
    dcbs, cur = [], None
    for tag, val in rec:
        if tag == 0x20:
            cur = {"dcb": val}
            dcbs.append(cur)
        elif cur is not None and tag in (0x21, 0x22, 0x23):
            cur[tag] = val
    if not dcbs:
        print("  NOT EMITTED - the shutdown dump never ran.")
    else:
        print("  %-10s %-7s %-4s %-8s %-6s %-10s %s" % (
            "DCB", "freeze", "sg", "io_pend", "lock", "dev_flags", "vrp_ptr"))
        for d in dcbs:
            w = d.get(0x21)
            if w is None:
                print("  %08X   <fields not emitted>" % d["dcb"])
                continue
            print("  %08X   %-7d %-4d %-8d %-6d %08X   %08X" % (
                d["dcb"],
                w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, (w >> 24) & 0xFF,
                d.get(0x22, 0), d.get(0x23, 0)))
        print("\n  io_pend is DCB_io_pend_count (78h), the VOLUME TRACKING layer's")
        print("  count of requests still outstanding on that DCB. ISP_DCB_DESTROY")
        print("  is documented to block until it drains, then free the VRPs.")
        print("  Non-zero on the DCB that stalled after 21/16 = that is the wait.")
        print("  Zero everywhere = the block is not pending I/O; look at lock.")

    ours = [v for t, v in rec if t in (0x24, 0x25)]
    if ours:
        print("\n  our claimed DCBs: %s" % ", ".join("%08X" % v for v in ours))
    return 0


if __name__ == "__main__":
    sys.exit(main())
