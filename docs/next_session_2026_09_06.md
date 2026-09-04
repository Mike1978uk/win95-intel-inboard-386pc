# Next session — 2026-09-06

**Read `docs/scsi_miniport_costing.md` first.** This file is the plan; that one is the evidence.

## What changed on 2026-09-05

The XT-IDE `.PDR` shutdown wedge is **our code**, not the machine. Measured, not argued:
Adaptec's `T130.MPD` — a polling, no-IRQ SCSI miniport — holds a written FAT16 volume through a
clean Windows 95 teardown on the same image the wedge reproduces on. SCSIPORT is itself an IOS
port driver, so IOS's teardown path, the DCB lifecycle, the volume lock and the cache flush are
all sound here.

**Decision: stop debugging the `.PDR` and write a SCSI miniport instead.** It discards ~1,500
lines of IOS glue — where every bug in this investigation has lived — and keeps ~1,120 lines of
transport unchanged.

Retired: the polling-contract diagnosis (technique 88's "THE GAP") as the *cause*. It is a real
contract violation worth fixing for responsiveness; it is not what wedges the shutdown.

## Do these first — neither costs machine time

1. **Read `T130.MPD`.** 7,598 bytes of `.text`, a working polling no-IRQ ISA miniport, with
   `tools/vxd_disasm.py`. It is the closest exemplar to what we are about to write.
2. **Settle boot-disk takeover.** The XT-CF is the boot disk, served by real-mode `RMM.PDR`, so
   a miniport must take it over. `PORT_CONFIGURATION_INFORMATION.RealModeInitialized` looks like
   the mechanism — *"indicates the real-mode driver has initialized the card; always initialized
   by ScsiPort"* — with `ESDI_506.PDR` as the worked example. **Read how it is consumed before
   spending a Windows install on testing it.** This is the one genuinely open question.

## Then build

Skeleton from `BLOCK/SAMPLES/MINIPORT/PC2X/PC2X.C` (the DDK's own, an 8-bit ISA SCSI adapter).
Transport carried over from `drivers/xtide_pdr/src/XTIDETR.ASM` — `DetectStride` through
`WriteReadBack`, plus `XferRun`, `Delay`, `Probe`.

**New code owed:** an SRB dispatch translating `READ(10)`, `WRITE(10)`, `INQUIRY`,
`READ CAPACITY`, `TEST UNIT READY`, `MODE SENSE` into ATA taskfile operations. 400–600 lines.
`ESDI_506.PDR` already contains both halves — it handles ATAPI, which is SCSI CDBs over ATA — so
it is the reference. `SD120PPD.MPD` is the same translation in a miniport, which is why #21 and
#22 are one problem twice.

Build with `ML.EXE /coff` + the DDK `LINK.EXE /SUBSYSTEM:NATIVE` against `BLOCK/LIB/SCSIPORT.LIB`.
A `.MPD` is a **PE** file, not an LE VxD — none of the `.DEF`/LE-page/dynamic-load problems apply.
No C compiler is needed or available (the DDK ships `LINK` but no `CL`).

## The bed

```
vm_t130b/86box.cfg.master       T130B 0x340 no IRQ; SCSI disk ID 0; SCSI CD-ROM ID 3
vm_t130b/t130b_master.img       Win95, T130.MPD installed, PORT.PDR absent, NIC removed
vm_t130b/scsi_test_master.img   partitioned FAT16 (tools/mkfatimg.py)
```

Masters are never booted; copy over the working image each run. Gitignored — ~4 GB.

**Run both arms in this bed**, not across beds. The NIC was removed here, so a comparison against
`vm_xtcf_faithful` carries two variables. To put our driver back head-to-head, `PORT.INF` must be
reinstalled via Have Disk — the device node was deleted, and IOS binds port drivers to device
nodes, not by scanning `IOSUBSYS` (technique 77).

## Traps paid for on 2026-09-05

- **A bed whose config names a missing image boots to ROM BASIC.** 86Box silently *creates* a
  blank image at that name. Every harness check passed. Assert `hdd_01_fn` resolves to a file
  with an MBR signature before booting.
- **Install ≠ loaded.** The first clean shutdown meant nothing: the driver was installed but the
  machine had not been rebooted. `BOOTLOG.TXT` was stale and still named `port.pdr` from two
  images ago. Delete the log before a run; require `Initing` / `Init Success` in the new one.
- **`EndTerminate = KERNEL` is the last line on a hung shutdown too.** Count `Terminate` against
  `EndTerminate` and name unpaired stages; the safe-to-turn-off screen is the real evidence.

## Parked, worth an hour when convenient

`Windows95_ddk/DEBUG/` ships **debug builds with symbols** of `IOS.VXD`, `SCSIPORT.PDR`,
`DISKTSD`, `DISKVSD` and `VMM.VXD`, plus `WDEB386.EXE` and `DEBUGCMD` (`.pthcb`, `.psem`, `.pmtx`,
labelled ring-0 stack). Technique 91 spent a session doing `.pthcb`'s job by hand.

**Check version compatibility on the bed first, never on the 5160:** the binaries are dated
1996-06-06 (OSR2 era), the guest is OSR1 build 950, and the `950/951/952/953` per-build
subdirectories are empty.

## Owed

- **#19 on real hardware.** `T130.MPD` with the real card and the real SCSI chain. Package is
  staged at `C:\T130` on the CF. Pick the INF entry named *(XT / Inboard, polled)*, accept
  `0340-034F`. If it misbehaves, `IOS.LOG` names the blocker — the MO/Zip/CD chain caused the
  IOS punt before (technique 86).
- Andrew has been told (#21 comment, 2026-09-05) and asked for his real-hardware T130B notes.
