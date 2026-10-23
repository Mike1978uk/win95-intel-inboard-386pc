# T130.MPD: what it sends a target (static read)

`drivers/trantor_t130b/T130.MPD`, 10,176 bytes, md5 `9cc532791b9e911bfba89afbc920c4c7`, PE
image base 0x10000. Disassembled with `objdump -d -M intel`; addresses below are virtual.
Static reading only; nothing here was traced on the 5160.

## Findings

1. **No IDENTIFY message, so no disconnect privilege.** Selection (`0x1118e`) writes
   `(1 << target) | 0x80` (initiator ID 7 plus the target) to the 53C80 data register (base+8),
   then sets Initiator Command (base+9) bits `0x05`, data bus and SEL (`0x111c6`, helper `0x1168a`).
   ATN (`0x02`) is never set anywhere: every write to base+9 is `0x01` (chip detect), `0x00`, or
   the ACK handshake `|0x10` / `&~0x10` (`0x11522`-`0x11563`). After selection it goes straight to
   COMMAND phase (`0x10c42`: phase helper `0x11465` with 2). A target therefore sees every T130.MPD
   command as SCSI-1: no MESSAGE OUT, LUN only in CDB byte 1 bits 5-7.
2. **No reselection.** The 53C80 Select Enable register (base+0xC, write) is never written; base+0xC
   is only read as Current Bus Status (`0x110c9`, `0x1144a`). The chip is never armed to answer a
   reselection.
3. **One request on the adapter at a time.** HwStartIo (`0x108ce`) stores the SRB in the LU
   extension (`ScsiPortGetLogicalUnit`, `0x10931`) and calls `NextRequest` only after completion
   (`0x10956`, `0x109f2`), never while a request is outstanding.
4. **What IRQ 3 does.** HW_INITIALIZATION_DATA (built at `0x104ad`): HwStartIo `0x108ce`,
   HwInterrupt `0x109a4`, HwFindAdapter `0x10572`, HwResetBus `0x10a9c`, HwInitialize `0x1082f`,
   interface ISA, 4 access ranges. HwInterrupt completes the request in progress; while one is
   pending, a 2 ms `RequestTimerCall` (`0x10970`, callback `0x10857`) does the same when no
   interrupt comes. The IRQ ends the current command; it does not bring in a reselection.
5. SRB flags are read only for data direction (`0x10a4d`: 0x80 out, 0x40 in);
   `SRB_FLAGS_DISABLE_DISCONNECT` is never tested, consistent with 1.

## Consequences for #50

- **LUNs depend on the CDB.** A hosted LUN is reached only if the requester puts the LUN in CDB
  byte 1. `DAYNAPRB.EXE` does. Whether Win95's own disk and CD layers do is unknown; if they do not,
  every command for LUN 1 reaches LUN 0 instead. REPORT.md test 1 now checks for this before any write.
- **Unit attention never reaches Win95.** BlueSCSI drops unit attention for hosts that select
  without ATN (`scsi.c:983-988`), which T130.MPD always does. Media change on a removable LUN
  shows only as NOT READY / MEDIUM NOT PRESENT while ejected (REPORT.md test 3).
- **Interrupt-driven receive (DESIGN.md §3 B) is not possible with T130.MPD as it is**: it never
  grants disconnect and never accepts reselection. It would need a different T130B miniport. We
  have written one Win95 miniport (`drivers/xtide_mpd/`), and the T130B's chip is a documented
  53C80; that is a separate decision for the owner.
- Measured-interval receive (§3 A) is unaffected and is the design.
