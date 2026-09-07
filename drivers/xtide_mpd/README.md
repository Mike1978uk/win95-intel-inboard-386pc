# XTIDEMP.MPD — a Windows 95 SCSI miniport for the Lo-tech XT-CF / XT-IDE

Issue [#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).
Replaces `docs/archive/xtide_pdr_retired/source/` — the IOS port driver that reached the same disk and then
wedged Windows at shutdown.

## ✅ Confirmed on the real 5160 — 2026-09-06

`XTIDEMP.MPD` md5 `561fb45b598ef5985e5a803016321f76`, published byte-identical as
`dist/xtide_mpd/XTIDEMP.MPD`. Evidence read off the CF card afterwards, not reported from
the screen:

```
[001612B7] Initing xtidemp.mpd
[001612CE] Init Success xtidemp.mpd
[0016136F] INITCOMPLETESUCCESS = DiskTSD
[00161372] INITCOMPLETESUCCESS = SCSIPORT
           rmm.pdr  Dynamic load success ... never reaches INITCOMPLETE
shutdown   7 stages started, 7 closed, none unpaired
WINDOWS\IOS.LOG   absent
```

Desktop reached, `C:` navigable, no errors in Device Manager, clean shutdown to the
safe-to-turn-off screen. `BOOTLOG.TXT` is itself a write that reached the medium through this
driver and was read back host-side.

**The forced-node question in the previous handoff is moot, not answered.** The installed node
held a *forced* `ForcedConfig 0300-031F` and the shutdown was clean — but this build takes its
base from `AdapterSettings` (`PORT=0x300`) and never reads the node's resources, so what
CONFIGMG assigned stopped mattering. Do not read this as "forcing is safe" in general.

**Install order.** Remove the old `PORT.PDR` node, **reboot**, then Add New Hardware. Without
that reboot CONFIGMG skipped `0300` — which is free, and measured free — assigned `0340`, and
then reported a conflict when `0300` was set by hand. Recommended sequence, mechanism unproven.

**Build provenance caveat.** The ledger records this binary's build tree as `DIRTY` at commit
`6869455`, so it is not provably rebuildable from a commit (technique 89). It was not
re-derived, by the owner's call, because the artefact itself is tracked and published.

### Not measured

- sustained write load — this was a boot, a look at `C:`, and a shutdown
- responsiveness under a heavy teardown flush; `XtStartIo` still completes inline (technique 98)
- **stride 1** — the *Compatibility* map: XT-IDE Rev 1, or a Rev 2/3/4 switched into it. Supported
  in code, and **passed in emulation 2026-09-07** (#24): `Init Success`, `RMM` stands down,
  host-verified write, 7/7 teardown stages paired. Never run on a physical card. Previously this
  said the path had never executed at all. (The
  retired `.PDR` did run against 86Box's stock stride-1 `xtide` device, but never properly claimed
  the disk there. Code running is not the map being driven.) Stock 86Box models this map, so it is
  testable in the VM without a card.
- the **Hi-Speed** map (the default on XT-IDE Rev 2/3/4) is **not supported at all** — it is an
  A3/A0 address-line swap, a permutation rather than a stride, and `base + index * stride` cannot
  express it. Expected to be declined by the read-only probe rather than misdriven, but unobserved.
  See [`docs/xtide_register_maps.md`](../../docs/xtide_register_maps.md).

## Why a miniport

Not an argument, a control. Adaptec's `T130.MPD` — polling, no IRQ — holds a written FAT16
volume through a clean Windows teardown on the *same image* our `.PDR` wedges on. So the SCSI
stack is sound on this machine and the failing layer is the one this driver deletes. Evidence
and costing: [`docs/scsi_miniport_costing.md`](../../docs/scsi_miniport_costing.md), technique 94.

What SCSIPORT now owns, that we used to get wrong:

| | `.PDR` | `.MPD` |
|---|---|---|
| polling contract | ours, and we violated it | SCSIPORT's (`Polling=1`) |
| DCB lifecycle, volume publishing | ours, ~1,500 lines | SCSIPORT + DiskTSD |
| scatter/gather | ours — misread the list and corrupted a volume (technique 85) | SCSIPORT, via `MapBuffers` |
| file format | LE VxD, `.DEF`, page size, `W4` combine | plain PE — none of it applies |

## What is actually new

Only the SRB→ATA dispatch in `src/XTIDEMP.ASM`. `src/XTIDETR.ASM` is the transport from the
port driver, carried across by `tools/`-style mechanical transform, with **three** edits, each
marked `MPD:` in the source:

1. the I/O base arrives from `HwFindAdapter`, not from an IOS DDB;
2. no boot-log delay channel — a miniport can return a value and call `ScsiDebugPrint`;
3. `XTIDE_Probe` may report not-found, because `SP_RETURN_NOT_FOUND` carries no penalty where
   failing `AEP_INITIALIZE` made IOS drop the driver for the rest of the boot.

Everything else — stride autodetect, 8-bit PIO vs high-byte latch, IDENTIFY, geometry, the
taskfile programmer, the read and write paths — is unchanged and already exercised on hardware.

## Build

```powershell
pwsh -File drivers/xtide_mpd/build.ps1
```

MASM 6.11c + the DDK's own LINK 2.60, against `BLOCK/LIB/SCSIPORT.LIB`. No C compiler is
involved and the DDK does not ship one. The link line is `PC2X.LNK` argument for argument.

Switches: `-Stride 1|2` pins the register map, `-WriteTest` re-enables the slave write
self-test (**off by default — on a single-disk machine the unit that answers is the boot
volume**), `-RealModeInit` sets `RealModeInitialized` (see Open questions).

Every build prints its commit and appends to `build_ledger.tsv`. A binary that cannot be traced
to a commit is not evidence — technique 89, which this project paid a day to learn.

## Install

Prerequisite, not optional: `inbrdpc.sys` must be in `[SafeList]` in `WINDOWS\IOS.INI` or IOS
declines every miniport on this machine (issue #17).

1. Remove any old XT-IDE **port driver** node in Device Manager and delete `PORT.PDR` from
   `WINDOWS\SYSTEM\IOSUBSYS`. It installs under class `hdc` and claims the same I/O range;
   two nodes on `300-31F` conflict.
2. Control Panel → Add New Hardware → **No**, do not autodetect → SCSI controllers → Have Disk.
3. Type the path by hand. **`A:\` will hang the dialog** — this machine has no floppy
   controller installed at all (issue #3). On the emulator bed the files are at `C:\XTIDEMP`.
4. Pick *Lo-tech XT-CF / XT-IDE 8-bit disk controller (polled)*.
5. **Do not force the configuration** if a conflict is offered - let Windows assign it.
   See Open questions; a forced node correlates with a hung shutdown.
6. Check Device Manager -> the controller -> **Settings** reads `PORT=0x300`, or whatever
   base your card is set to in the XTIDE Universal BIOS. Then reboot.

Verify it **ran**, not that it installed — the two are different and the difference has cost
this project runs (technique 94):

```
grep -i xtidemp BOOTLOG.TXT     -> Initing xtidemp.mpd / Init Success xtidemp.mpd
```

Delete `BOOTLOG.TXT` before the run. A stale one names a driver from two images ago and reads
exactly like a result.

## Open questions

**Boot-disk takeover — answered.** It works, with `RealModeInitialized` left FALSE. `RMM.PDR`
loads and never reaches `INITCOMPLETE`, which is the real-mode mapper finding nothing to claim
because the miniport already owns the disk. The `-RealModeInit` switch stays available and has
never been needed.

**The forced-configuration hang — still open; the hardware run made it moot, not answered.** A manually
forced device node correlates with a hung Windows shutdown, 3 runs of 3, on two builds, at the
same VMM addresses the old `.PDR` wedged at. Auto-assignment is clean. But forced-versus-auto
and `0300`-versus-`0320` moved together in every run, so they are not separated: the untested
cell is a node **auto-assigned** to `0300`. Technique 97 has the full table. This matters on the
5160 because `0320` and `0340` are the 3C509B and the T130B there, so the node will land
somewhere else — quite possibly `0300`.

**Two miniports at once.** `T130.MPD` is installed on the bed image. SCSIPORT is built for
multiple adapters, each bound to its own device node, but it has not been checked here.

**Synchronous completion — now a defect, not a trade.** `XtStartIo` runs the transfer inline
and completes before returning, so a heavy teardown blocks the machine for its whole duration.
Measured on 2026-09-05 at long enough that the owner would have switched the machine off, which
is a failed shutdown regardless of what the CPU is doing (technique 98). Moving to
`ScsiPortNotification(RequestTimerCall, ...)` is the next real piece of work.

## Status

**Confirmed on the real 5160, 2026-09-06** — see the top of this file for the evidence. Before
that it was confirmed on an emulator bed that faithfully models the card, including 5.9 MB written
through it and verified byte-for-byte from the host.

The I/O base is supplied by the owner on the Settings tab (`PORT=0x300`) rather than probed, and
the driver never touches the device node's resources.

Still open, and both are listed above with their evidence: the forced-configuration shutdown hang
(moot on the hardware run, not answered), synchronous completion under a heavy teardown, and
**stride 1**, which needs a card presenting the *Compatibility* register map — a Rev 1, or a
Rev 2/3/4 switched into it with XUB device type `XTIDE rev1`. Neither exists here. Submission drafts asking for that testing: [`docs/xtide_submission_drafts.md`](../../docs/xtide_submission_drafts.md).
