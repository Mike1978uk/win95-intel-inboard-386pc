"""Strip the project's diagnostics out of the EPAT patch, for upstream.

Every site removed here is marked in the source as temporary - "DIAGNOSTIC
2026-09-13 - remove once X is proven" or "DIAGNOSTIC, not for upstream". They
are what made the bridge debuggable here; none of them belongs in someone
else's emulator.

Exact-string replacements with an assertion on each, so a source that has moved
fails loudly instead of being silently half-stripped. Run against the master
worktree, not against 86box_upstream.
"""
import os
import sys

ROOT = r"C:\Users\lycet\AppData\Local\Temp\claude\86box_master"

EDITS = {
    "src/device/lpt.c": [
        # the counter declarations
        ("""/* DIAGNOSTIC 2026-09-13 - ECP write path census. Remove once ECP is proven. */
uint32_t ecpdiag_wr_port = 0, ecpdiag_wr_gated = 0, ecpdiag_wr_fifo = 0,
         ecpdiag_wr_full = 0, ecpdiag_wr_dev = 0, ecpdiag_wr_spp = 0,
         ecpdiag_wr_addr = 0, ecpdiag_wr_relief = 0;

""", ""),
        # the relief counter, keeping the deliver call
        ("""        /* Counted, because a driver that paces itself against the ECR never
           gets here. A non-zero count means that driver is relying on the
           stall, and would lose bytes on a port that does not provide one. */
        ecpdiag_wr_relief++;
        lpt_fifo_deliver(dev);""",
         """        lpt_fifo_deliver(dev);"""),
        # the fifo/full census
        ("""    if (!fifo_get_full(dev->fifo))
        ecpdiag_wr_fifo++;
    else
        ecpdiag_wr_full++;

""", ""),
        ("        ecpdiag_wr_addr++;\n", ""),
        ("            ecpdiag_wr_dev++;\n", ""),
        ("            ecpdiag_wr_spp++;\n", ""),
        # the write-progress census in the data case
        ("""                case 3:
                    /* DIAGNOSTIC 2026-09-13 - remove once ECP is proven. */
                    ecpdiag_wr_port++;
                    /* A long transfer that never finishes reports nothing at
                       all when the census only prints on an ECR write, so
                       mark progress as it goes. */
                    if ((ecpdiag_wr_port % 4096) == 0)
                        pclog("[ECPDIAG] write progress %u bytes (fifo=%u full=%u ->dev=%u)\\n",
                              ecpdiag_wr_port, ecpdiag_wr_fifo, ecpdiag_wr_full,
                              ecpdiag_wr_dev);
                    if (!(lpt_get_ctrl_raw(dev) & 0x20))
                        lpt_write_fifo(dev, val, 0x01);
                    else
                        ecpdiag_wr_gated++;
                    break;""",
         """                case 3:
                    if (!(lpt_get_ctrl_raw(dev) & 0x20))
                        lpt_write_fifo(dev, val, 0x01);
                    break;"""),
        # the ECR-write census dump and its counter reset
        ("""            /* DIAGNOSTIC 2026-09-13 - remove once ECP is proven. */
            pclog("[ECPDIAG] ECR write %02X (mode %i), was %02X | port=%u gated=%u "
                  "fifo=%u full=%u relief=%u ->dev=%u ->spp=%u ->addr=%u ctrl=%02X state=%i tmr=%i\\n",
                    val, (val >> 5) & 7, dev->ecr,
                    ecpdiag_wr_port, ecpdiag_wr_gated, ecpdiag_wr_fifo,
                    ecpdiag_wr_full, ecpdiag_wr_relief, ecpdiag_wr_dev,
                    ecpdiag_wr_spp, ecpdiag_wr_addr,
                    lpt_get_ctrl_raw(dev), dev->state,
                    !!timer_is_enabled(&dev->fifo_out_timer));
            ecpdiag_wr_port = ecpdiag_wr_gated = ecpdiag_wr_fifo = 0;
            ecpdiag_wr_full = ecpdiag_wr_dev = ecpdiag_wr_spp = 0;
            ecpdiag_wr_addr = ecpdiag_wr_relief = 0;
""", ""),
    ],
}


def main():
    total = 0
    for rel, edits in EDITS.items():
        p = os.path.join(ROOT, rel)
        s = open(p, encoding="utf-8", errors="strict").read()
        for old, new in edits:
            n = s.count(old)
            if n != 1:
                sys.exit(f"FAILED in {rel}: pattern found {n} times, expected 1\n"
                         f"---\n{old[:200]}\n---")
            s = s.replace(old, new)
            total += 1
        tmp = p + ".tmp"
        open(tmp, "w", encoding="utf-8", newline="").write(s)
        os.replace(tmp, p)
        print(f"  {rel}: {len(edits)} sites removed")

    print(f"Stripped: {total}")
    if total == 0:
        sys.exit("Stripped: 0 - refusing a no-op.")


if __name__ == "__main__":
    main()
