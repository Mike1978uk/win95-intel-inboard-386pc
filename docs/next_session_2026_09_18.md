# LS-120 — handoff, 2026-09-18 (early hours)

⭐ **Read `drivers/imation_ls120/MEASURED_FACTS.md` first.** Everything measured
on the real 5160 is there, each row naming its capture. Do not re-measure it.

## What was staged, and it is correct

`code fe657dc8` / md5 `55327f8e` — SPP forced, `LS_StatusRead` fixed,
`LS_MAX_XFER = 3584`, seconds-long reset settle. Deployed **over COMrade** to
both paths (`IOSUBSYS` is unlocked at a DOS prompt):

| path | CRC |
|---|---|
| `C:\WINDOWS\SYSTEM\IOSUBSYS\LS120MP.MPD` | `2791467437` verified |
| `C:\LS120MP\LS120MP.MPD` | `2791467437` verified |

Confirmed on the card afterwards: `IOSUBSYS` holds md5 `55327f8e`. **The right
binary loaded.**

## The driver is healthy. The boot is healthy.

```
[0000B486] Initing ls120mp.mpd
[0000B496] Init Success ls120mp.mpd      <- 16 units, the HEALTHY figure
```

- `IOS.LOG` **does not exist** — IOS had no complaint.
- The `INITCOMPLETESUCCESS` driver set is **identical** to the previous boot —
  diffed, zero difference. Nothing failed to load.
- Windows reached `Init = Final USER`, `Installable Drivers`, `TSRQuery`. It
  came fully up.

2 ticks is a vacuous bail; ~16 is real bring-up work. This is 16.

## ⛔ MY ERROR: the registry edit did NOT take, despite REGEDIT reporting success

`REGEDIT /L:… /R:… C:\LSFIX2.REG` printed **`Importing file (100% complete)`**
and the hive is **unchanged**:

```
UserDriveLetterAssignment = II   still on ROOT&SCSIADAPTER&000000
ROOT&SCSIADAPTER&000200          still present (the stale duplicate)
```

**Almost certainly the `.REG` syntax.** Windows 95's REGEDIT4 format supports
`[-Key]` to delete a KEY, but **not `"value"=-` to delete a VALUE** — that is a
REGEDIT5/NT-era feature. So the import "succeeded" having done nothing, which
is this project's most familiar failure shape and I walked straight into it.

**Do not retry the same file.** Routes that actually work:

1. **Device Manager** → Disk drives → `MATSHITA LS-120 COSM 04` → Settings →
   reserved drive letters → clear, or set `K`. Free letters are **K** and **M**
   (`LASTDRIVE=M`); `I:` is held by a Nakamichi slot and always has been.
2. A `.REG` that **rewrites** the value to something satisfiable rather than
   deleting it — e.g. `"UserDriveLetterAssignment"="KK"` — since setting a value
   *is* supported by REGEDIT4.

Route 2 is testable without the GUI and is the one to try first.

## The Explorer hang — third occurrence, and it correlates

Recorded occurrences, all with the LS-120 present:

- 2026-09-15: owner changed the adapter's port to `378h` in Device Manager,
  Explorer froze. Never diagnosed.
- 2026-09-17 earlier: reinstalled, first boot, entering Explorer froze.
- 2026-09-18 tonight: Explorer hung again.

**This boot vs the previous one — the only difference is how far the teardown
got:**

| | last lines |
|---|---|
| previous (clean) | `Terminate = KERNEL / RIT / Win32 … EndTerminate = KERNEL` |
| tonight | `Terminate = User / Query Drivers / EndTerminate = Query Drivers` |

Same driver set, same init, same everything up to that point. Tonight's log
stops partway through the teardown at **Query Drivers**. That may simply be
where the power cut landed — **do not read it as a wedge without checking what a
force-powered-off boot normally truncates at.** `EndTerminate = KERNEL` is the
last line on a HUNG shutdown too (technique 88), so log position alone proves
nothing either way.

## The open question, stated precisely

**The driver loads, initialises healthily, and Windows comes up — but no drive
letter, and Explorer hangs.** The drive-letter reservation (`II`, unsatisfiable
because a Nakamichi holds `I:`) is still in the hive and is the leading
candidate for the missing letter. Whether it also explains the Explorer hang is
**not established**.

## Next session, in order

1. **Clear the reservation properly** — Device Manager, or a `.REG` that SETS
   `"UserDriveLetterAssignment"="KK"` rather than deleting it. Verify from the
   card afterwards that the hive actually changed; do not trust REGEDIT's
   success message again.
2. **Boot and look for the letter.** If it appears, the question is closed.
3. **If Explorer still hangs with a letter assigned**, that is a separate fault
   and needs its own bisect — start by renaming `LS120MP.MPD` out of `IOSUBSYS`
   (the cheapest A/B this project has) to confirm it is ours at all.
4. Only then the ECP build (`code cb34891b`, `LS_Term1284`). It is built and
   **proven in the PROBE only** — the driver version has never run.

## Not done, deliberately

- **Emulator realism** — parked by the owner. We now have the data: 3584 burst
  ceiling, 1284 terminate, ECP-block-is-not-a-thing, the refused `READ BUFFER`
  mode 3. The bed currently passes ECP code the real bridge refuses.
- **Small/large file transfer** — needs a working drive letter first.
- **Drive buffer capacity** — the drive refuses to report it. Bound it
  empirically (largest accepted `WRITE BUFFER`, or the throughput knee).
