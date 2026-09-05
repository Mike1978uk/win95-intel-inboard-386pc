# Next session — after 2026-09-05

**Read `drivers/xtide_mpd/README.md` first.** This file is the plan; that one is the driver.

## Where it got to

The SCSI miniport is written, builds, installs and **works on the faithful emulator bed**:

```
Init Success xtidemp.mpd
INITCOMPLETESUCCESS = SCSIPORT / DiskTSD / VFAT / IFSMGR
RMM.PDR   loads, and never reaches INITCOMPLETE   <- boot-disk takeover
shutdown  7 stages started, 7 closed
5.9 MB copied through it, verified byte-for-byte from the host
```

`RealModeInitialized` turned out not to be needed — the one field flagged as the open question in
the costing was a non-issue.

The port discarded 1,504 lines of IOS glue and kept the transport unchanged. Every bug the `.PDR`
investigation chased lived in the discarded half.

## The one thing to settle before the 5160

**A manually forced device node correlates with a hung Windows shutdown.** Three runs of three, on
two different builds — including the exact binary that shuts down cleanly when the node is
auto-assigned. Removing the driver is clean. So on that bed it is our code, and the trigger is
something about the node rather than anything in the driver's own logic.

But two variables moved together in every single run and have never been separated:

| node | assigned how | teardown |
|---|---|---|
| `0320` — wrong address | auto | clean |
| `0300` — correct address | **forced** | hung, 3 of 3 |
| **`0300`** | **auto** | **never tested — run this** |
| `0320` | forced | never tested |

**Run the third row.** Install from an INF offering only `300-31F` so CONFIGMG has nowhere else to
put it, and *decline to force anything* if a conflict is offered. That decides whether the wedge
comes from forcing or from the address, and it is one install plus one shutdown.

It matters on hardware because `0320` and `0340` are the **3C509B** and the **T130B** on the real
5160, so the node cannot land where it lands on the bed. `0300` is the likely assignment there —
i.e. the untested cell, on the machine you care about.

### And it may be the `.PDR` bug as well

`PORT.INF` carried the same single-range `IOConfig=300-31F`, which produces the same conflict
callout and very likely the same manual force. The wedge addresses match — `C0003187`, `C000318D`,
`C000323C`, `C000324D`, `C0003257`, `C0008FA4`, `C0009073/7` — across both investigations. Four
sessions of bisecting IOS glue held the node constant throughout, so nothing in them could have
seen it. If the third row hangs, the `.PDR` deserves re-reading before it is called dead.

## Then, in order

1. **Async completion.** `XtStartIo` completes inline, so a heavy teardown blocks the machine for
   minutes. Long enough that the owner would switch the machine off — a failed shutdown, not a slow
   one (technique 98). `ScsiPortNotification(RequestTimerCall, …)` is the mechanism and it is half
   the reason a miniport was worth writing.
2. **The 5160**, with the CF imaged first: remove `PORT.PDR` and its `hdc` node, confirm
   `inbrdpc.sys` is in `[SafeList]` in `IOS.INI`, keep the real-mode SCSI/ASPI chain out of
   `CONFIG.SYS`, and check the Settings tab before rebooting.
3. **Stride 1.** Never executed. It needs its own bed — a stride-1 card *and* a stock XTIDE option
   ROM, because `xtcf_lotech` drives stride 2 and the option ROM could not boot the disk otherwise.
   Does not affect this machine; does affect anyone else's card.
4. **Dead data.** `XTIDE_SgCur` / `XTIDE_SgLeft` and the marker fields survived the transform and
   are now unreferenced — SCSIPORT does scatter/gather. Deliberately left alone so as not to change
   a binary mid-investigation.

## Beds

```
vm_xtide_mpd2/xtidemp2_master.img         pre-install, genuinely clean (no node, no driver)
vm_xtide_mpd2/xtidemp2_clean_install.img  the working install: auto-assigned 0320, nothing forced
vm_xtide_mpd/xtidemp_WORKING_master.img   the earlier bed; node 0320, driver pinned — historic
```

`xtidemp2_clean_install.img` is the one to clone. It reaches a desktop and shuts down cleanly.

## Traps paid for on 2026-09-05

- **Do not narrow an INF's `LogConfig` to one range.** It removes the user's ability to reconfigure
  entirely — Device Manager reports *"resources cannot be modified"* — and here it forced a
  conflict callout that led to the hangs. Technique 97.
- **Do not byte-edit `SYSTEM.DAT`.** CREG validates more than length; a same-length edit produced
  *"Not enough memory to load the registry"* and an unbootable machine. Technique 99.
- **Extract `BOOTLOG.TXT` before restoring an image.** Two hung runs were restored before the log
  was pulled, destroying the ownership evidence that would have settled the question hours earlier.
- **`EndTerminate = KERNEL` appears on a hung shutdown too.** A wedged machine logged 7 stages
  started and 7 closed. The log corroborates; the safe-to-turn-off screen is the evidence.
- **The XT-IDE access trace costs ~440 MB and drops the machine to 2–14% of speed.** Armed once for
  a question that was already answered. Leave `XTIDE_TRACE` unset.
