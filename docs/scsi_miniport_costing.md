# Moving the XT-CF driver to a SCSI miniport — costing and the control that justifies it

2026-09-05. Issue [#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).

The `.PDR` IOS port driver works — it claims the disk, mounts a volume, reads and writes — and
then wedges Windows at shutdown. Four sessions narrowed that to the IOS layer without naming a
cause. This document costs the alternative Andrew proposed, and records the measurement that
turned it from an argument into a decision.

## The control: someone else's miniport, on this machine

**Run it before believing any of the costing below.** The question was whether the destination
is sound: does the SCSI stack tear down cleanly on the Inboard 5160 at all?

Bed `vm_t130b/`, built from `vm_xtcf_faithful`'s image so it is the same Windows install the
wedge reproduces on. `PORT.PDR` removed from `IOSUBSYS` and its device node removed in Device
Manager, so the only 32-bit storage driver in play is Adaptec's.

| arm | driver | device held | shutdown |
|---|---|---|---|
| 1 | `T130.MPD` | SCSI CD-ROM (D:) | **clean** |
| 2 | `T130.MPD` | SCSI CD-ROM **+ FAT16 disk, read and written** | **clean** |

Verified rather than observed:

```
[000001BB] Initing t130.mpd
[00000219] Init Success t130.mpd
INITCOMPLETESUCCESS = DiskTSD / SCSIPORT / VFAT / IFSMGR
shutdown stages started 7, closed 7    UNCLOSED: none
scsi_test.img read host-side:  HELLO.TXT 161   TESTTX~1.TXT 23
```

The guest's write is physically on the medium, so the path ran guest → IFSMGR → VFAT → DiskTSD →
SCSIPORT → miniport → disk, and the shutdown flushed the cache before tearing down. That is the
same path our driver corrupts (technique 85) and then wedges in.

**Conclusion: the shutdown wedge is not a property of this machine's Win95 storage teardown.**
It is our `.PDR`'s IOS-layer implementation. The port removes the failing layer wholesale.

Caveat, stated plainly: `EndTerminate = KERNEL` is the last line of `BOOTLOG.TXT` on a hung
shutdown too (technique 88). The evidence that these were clean is the owner watching the
safe-to-turn-off screen. The log corroborates; it does not prove on its own.

## What a miniport is, structurally

Checked against the binaries, not assumed.

- **A `.MPD` is a PE file, not an LE VxD.** `T130.MPD` and `SD120PPD.MPD` are both i386 PE,
  subsystem NATIVE. That deletes the whole `PORT.DEF` / LE page size / dynamic-load-flag /
  `W4`-combine problem family that cost sessions on the `.PDR`.
- **The toolchain is already here.** `ML.EXE /coff` + the DDK's `LINK.EXE` 2.60 with
  `/SUBSYSTEM:NATIVE`, against `BLOCK/LIB/SCSIPORT.LIB`. `T130.MPD` was linked with 2.50, the
  same family. The DDK ships `SRB.INC`, `SCSIPORT.INC`, `MINIPORT.INC` so the driver can be
  written in MASM — there is no `CL.EXE` in the DDK, and none is needed.
- **The DDK ships a miniport sample in C**: `BLOCK/SAMPLES/MINIPORT/PC2X/PC2X.C`, an Iomega
  8-bit ISA SCSI adapter. All entry points, including a timer/polling path.

## What SCSIPORT gives us that we had to write ourselves

This is the argument for the port, and every line is from the headers or the binaries.

| what | where |
|---|---|
| `ScsiPortNotification(RequestTimerCall, …)` — arm a callback, return without completing, finish later | `SRB.INC` = 6; used at four sites in `PC2X.C` |
| `PollingSupportNeeded` — polling is a registry-selectable SCSIPORT mode | literal string in `SCSIPORT.PDR`, beside `AdapterSettings` |
| `SCSIPORT_GF_EMUL_SG` — scatter/gather emulation | `SCSIPORT.INC` |
| `AdapterSettings` — a GUI Settings tab passed to `HwFindAdapter` as `ArgumentString` | confirmed present in Device Manager on the T130B this session |
| `Set_Global_Time_Out` / `Cancel_Time_Out` / `Set_Async_Time_Out` / `Schedule_Global_Event` | SCSIPORT calls all four (technique 88's table) |

The polling contract in technique 88 — the one we do not meet — is SCSIPORT's job, not ours.

**Correction to an earlier claim.** `BufferAccessScsiPortControlled` was described in a previous
session as "makes SCSIPORT emulate scatter/gather for a PIO device". `STORAGE.DOC` says it
"indicates that the miniport will **not** touch the data buffers directly" (ref KB Q116450). For
a PIO transport that moves bytes with `in`/`out`, it probably wants to be FALSE. Settle it before
relying on it; `SCSIPORT_GF_EMUL_SG` is likely the flag that was meant.

## What ports across

Measured from `drivers/xtide_pdr/src/XTIDETR.ASM`'s own procedure map.

| | lines | fate |
|---|---|---|
| Transport — `DetectStride` … `WriteReadBack`, `XferRun`, `Delay`, `Probe` | ~1,120 | ports across nearly unchanged |
| IOS glue — `WantDcb`, `ConfigDcb`, `StartRequest`, `VolCreate`, `SchedVol`, all `Mark*` | ~1,500 | discarded |

The DDK's `PORT.ASM` / `PORTAER.ASM` / `PORTREQ.ASM` go too. The discarded half is where every
bug in this investigation has lived.

**New code owed:** an SRB dispatch translating `READ(10)`, `WRITE(10)`, `INQUIRY`,
`READ CAPACITY`, `TEST UNIT READY`, `MODE SENSE` into ATA taskfile operations. 400–600 lines,
and the only genuinely new thing.

## ATAPI is the same problem, already standardised

ATAPI is SCSI CDBs over an ATA transport — which is what the SRB dispatch above *is*, minus the
packet wrapper. Two consequences:

- `ESDI_506.PDR` already contains both ATA taskfile code and SCSI command handling, so it is the
  reference for the one new piece.
- `SD120PPD.MPD` is an ATAPI-to-SCSI translator in a miniport — an ATAPI LS-120 presented as a
  SCSI disk over a parallel port. Issues #21 and #22 are the same problem twice, so work on
  either feeds the other.

## Open, and the cheapest way to close it

**Boot-disk takeover.** The XT-CF is the boot disk, served by real-mode `RMM.PDR` until Windows
takes over. A miniport must claim it. `PORT_CONFIGURATION_INFORMATION` has
`RealModeInitialized` — *"indicates the real-mode driver has initialized the card; always
initialized by ScsiPort"* — which is the mechanism, and `ESDI_506.PDR` is the worked example.
**Read how it is consumed before spending a Windows install on testing it.**

**Not tested:** whether a second miniport interferes with the T130B. SCSIPORT is built for
multiple adapters, each bound to its own device node, and this machine already carries several.
Cheap to check on the bed.

## The DDK debug tree — unopened since 2026-08-23

`Windows95_ddk/DEBUG/` ships debug builds with symbols of the components this project has spent
weeks reverse-engineering from stripped retail binaries:

- `IOS.VXD` + `IOS.SYM` — named entry points at real offsets (`IOS_System_Exit`,
  `IOS_BD_Command_Complete`, `IOS_Send_Next_Command`, `IOS_Set_Async_Time_Out`, `PrintLog`)
- `SCSIPORT.PDR` (83,037 bytes debug vs 23,133 retail) + `SCSIPORT.SYM` (`_ScsiDebug`)
- `DISKTSD.VXD`, `DISKVSD.VXD`, `CDTSD.VXD`, `CDVSD.VXD`, `VMM.VXD` + `VMM.SYM`
- `WDEB386.EXE` and `DEBUGCMD.TMP`, providing `.pthcb` (dump a thread control block), `.psem`,
  `.pmtx`, `.ps` (ring-0 stack **with labels**)

Technique 91 spent a session identifying TCBs by dumping bytes for a `"THCB"` signature. `.pthcb`
prints them.

**Caveat: dated 1996-06-06, i.e. OSR2 era; this guest is OSR1 build 950, and the `950/951/952/953`
subdirectories that would hold per-build versions are empty.** Version compatibility is unchecked.
Whether `WDEB386` runs under 86Box is also untested, though the bed can spare a serial port where
the 5160 cannot (technique 76).

## Recommended order

1. Read `T130.MPD` — 7,598 bytes of `.text`, a working polling no-IRQ ISA miniport, with
   `tools/vxd_disasm.py`. Zero machine time.
2. Settle `RealModeInitialized` from `ESDI_506.PDR`. Zero machine time.
3. Stand up the debug stack on the bed if the OSR1/OSR2 question resolves favourably.
4. Then write the miniport.

## Reproducing the control

```
vm_t130b/86box.cfg.master     T130B at 0x340 no IRQ; SCSI disk ID 0; SCSI CD-ROM ID 3
vm_t130b/t130b_master.img     Win95 with T130.MPD installed, PORT.PDR absent
vm_t130b/scsi_test_master.img partitioned FAT16, built by tools/mkfatimg.py
drivers/trantor_t130b/T130-XT.INF   polled, no IRQConfig, IOConfig 340-34F first
```

The install is manual (Have Disk, `C:\T130`) because the INF carries no PnP hardware ID. Pick the
entry named *(XT / Inboard, polled)* — the stock Adaptec INF is in the same directory and sets
`IRQConfig=3,5,7` with no polling, which is wrong for this card.
