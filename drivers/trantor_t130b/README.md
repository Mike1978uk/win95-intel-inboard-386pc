# Trantor T130 SCSI miniport for Windows 95

The 32-bit protected-mode miniport Microsoft never shipped. Tracked as
[issue #19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19).

**Not written or modified here.** These are the original Trantor/Adaptec files, unaltered, kept so
the test is reproducible. Copyright remains with Adaptec; redistributed as a freely downloadable
driver, not under this project's licence.

## Provenance

Downloaded 2026-08-28 from <https://archive.org/details/T130_EXE>
(`https://archive.org/download/T130_EXE/T130.EXE`), a PKZIP self-extractor.

| file | size | dated | md5 |
|---|---|---|---|
| `T130.EXE` | 23,176 | — | `257a1721174ead6e5efe706c4b8acac4` |
| `T130.MPD` | 10,176 | 1995-03-02 | `9cc532791b9e911bfba89afbc920c4c7` |
| `T130.INF` | 1,132 | 1993-01-12 | `c238b46d5b4fca6c0d87006485dbd6ec` |

Siblings exist for the [T128](https://archive.org/details/T128_EXE) and
[T338](https://archive.org/details/T338_EXE); neither is held here.

## What inspecting the files established

**It supports the T130B.** `T130.MPD` carries the literal strings `T13B SCSI Host Adapter`,
`T130B` and `2.00` at offset `0x20a0`. Issue #19 recorded the B variant as unverified; it is not
unverified any more, short of running it.

**It is programmed I/O only — no DMA.** The INF sets `DMAConfig=0`, and the driver's entire import
list from `SCSIPORT.SYS` is PIO:

```
ScsiPortReadPortUchar    ScsiPortWritePortUchar
ScsiPortReadPortUshort   ScsiPortWritePortUshort
ScsiPortReadPortBufferUshort  ScsiPortWritePortBufferUshort
ScsiPortStallExecution   ScsiPortNotification  ...
```

No DMA or uncached-extension call appears. **The 20-bit DMA reach problem cannot affect this
driver** — which removes the single largest risk this machine poses to a storage driver.

**Installation is manual, not Plug and Play.** There is no PnP hardware ID anywhere in the INF —
`Class=SCSIAdapter` with a `LogConfig` and `NoSetupUI`. It installs via Have Disk and binds by
selection, so the card variant cannot cause a *detection* failure; only the driver's own probe can.

**Two things the INF will need editing for on this machine:**

1. **No polling.** `T130.INF` has no `[Poll]` section — `T128.INF` does. This T130B is jumpered for
   no IRQ, and all three IRQs the INF offers (`IRQConfig=3,5,7`) are taken: 3 by the 3C509B, 5 by
   the Sound Blaster Pro, 7 by the Intek21 parallel card. `HKR,,Polling,,1` has to be added.
2. **Double buffering** is switched on globally: `UpdateInis=DoubleBuffer` writes
   `DoubleBuffer=1` into `MSDOS.SYS`. Worth knowing before attributing a later change to something
   else.

`IOConfig` offers `350-35F, 250-25F, 340-34F, 240-24F`; this card is at **0x340**, which is on the
list.

## Status

**Works in emulation.** Installed on the faithful bed on 2026-09-05 as the *control* that
justified rewriting the XT-IDE driver as a miniport — it held a written FAT16 volume through a
clean Windows teardown on the same image our own `.PDR` wedged on, and the guest's write was
read back host-side, so it reached the medium and the shutdown flushed the cache:

```
[000001BB] Initing t130.mpd
[00000219] Init Success t130.mpd
INITCOMPLETESUCCESS = DiskTSD / SCSIPORT / VFAT / IFSMGR
shutdown stages started 7, closed 7   UNCLOSED: none
```

Both arms passed: CD-ROM only, and CD-ROM plus a written FAT16 disk. See technique 94 and
[`docs/scsi_miniport_costing.md`](../../docs/scsi_miniport_costing.md).

## ✅ Confirmed on the real 5160, 2026-09-06 — #19 closed

Installed from `C:\T130XT\`. The real SCSI chain runs in protected mode:

```
[000C262E] Initing t130.mpd
[000C267F] Init Success t130.mpd
INITCOMPLETESUCCESS = IOS / SCSIPORT / DiskTSD / CDTSD / VFAT / CDFS / IFSMGR
rmm.pdr   Dynamic load success ... and never reaches INITCOMPLETE
shutdown  7 stages started, 7 closed, none unpaired
IOS.LOG   does not exist
```

All six targets enumerate — Fujitsu MO (ID 0), Nakamichi changer (ID 2), Zip 100 (ID 3), Yamaha
CD-RW, HP C1537A DAT (ID 4) and UMAX Astra 610S (ID 6). The MO, Zip and both CD devices bind to a
class driver; the tape and scanner stay `Unknown`, which is correct — Windows 95 ships no class
driver for either and both are reached over ASPI by their own applications.

Coexists with `XTIDEMP.MPD`: the XT-CF serves `C:`, the T130B serves the chain, both under
`SCSIPORT`. Boot log kept at
[`docs/bootlogs/BOOTLOG_2026-09-06_t130b_hardware_pass.TXT`](../../docs/bootlogs/BOOTLOG_2026-09-06_t130b_hardware_pass.TXT).

**Not tested:** reads and writes to actual MO, Zip, CD or CD-RW *media* under Windows. The devices
enumerate and bind; that is not the same as verified transfers.

Prerequisites that were verified on the card beforehand, and still matter for anyone repeating
this: `inbrdpc.sys` in `[SafeList]` in `WINDOWS\IOS.INI`, and the real-mode SCSI/ASPI chain
(`MA13B`, `TSLCDR`, `MODISK2`, `NASPIBUF`) REM'd out of `CONFIG.SYS`.

If you are repeating it on another machine, do the **CD-ROM** arm first — it proves the driver
loads and claims the bus without exercising DiskTSD/VFAT/IFSMGR, which is where a port driver's
teardown bugs live.

`T130-XT.INF` in this directory is the deployed INF; it is staged on the card under the 8.3 name
`T130XT.INF` so that only one INF is visible to Have Disk.
