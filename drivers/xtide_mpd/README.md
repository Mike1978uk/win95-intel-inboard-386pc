# XTIDEMP.MPD — a Windows 95 SCSI miniport for the Lo-tech XT-CF / XT-IDE

Issue [#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).
Replaces `drivers/xtide_pdr/` — the IOS port driver that reached the same disk and then
wedged Windows at shutdown.

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
4. Pick *Lo-tech XT-CF / XT-IDE 8-bit disk controller (polled)*. Reboot.

Verify it **ran**, not that it installed — the two are different and the difference has cost
this project runs (technique 94):

```
grep -i xtidemp BOOTLOG.TXT     -> Initing xtidemp.mpd / Init Success xtidemp.mpd
```

Delete `BOOTLOG.TXT` before the run. A stale one names a driver from two images ago and reads
exactly like a result.

## Open questions

**Boot-disk takeover.** The XT-CF is the boot disk, served by real-mode `RMM.PDR` until Windows
takes over. `T130.MPD` was only ever proven on a *secondary* disk, so this is untested either
way. `PORT_CONFIGURATION_INFORMATION.RealModeInitialized` is the documented mechanism and is
left **FALSE**, because FALSE is what `PC2X` and every sample use and is therefore the only
value with evidence behind it. If the driver loads and the volume is not taken over, rebuild
with `-RealModeInit` — that is the first thing to try and it is one build.

**Two miniports at once.** `T130.MPD` is installed on the bed image. SCSIPORT is built for
multiple adapters, each bound to its own device node, but it has not been checked here.

**Synchronous completion.** `XtStartIo` runs the transfer inline and completes before
returning. zikolas/cfu1-win9x records what that costs — the UI freezes during a transfer — and
what it buys: it tears down cleanly, which is the entire point of this port. Making it
asynchronous from SCSIPORT's own `RequestTimerCall` is the obvious next step and deliberately
is not in the first build, because that would be two variables at once.

## Status

Builds; structurally verified against `T130.MPD` (PE32, subsystem NATIVE, same image base,
stdcall callbacks confirmed by `ret 18h` on the six-argument entry point). **Not yet run.**
