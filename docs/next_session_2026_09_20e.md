# 2026-09-20e — EPAT submitted; optimisation track opened as issues

## Shipped

- **86Box PR [#8010](https://github.com/86Box/86Box/pull/8010)** — the parallel-port LS-120.
  Verified OPEN, 9 files, +1,578 / −21. Framed as one transaction: the bridge, the
  `RDISK_BUS_LPT` attach path and the SuperDisk type are each inert alone.
  Tested on master: drive enumerates at `J:`, directory reads, a 92,870-byte write lands;
  a non-LPT machine is unaffected; the SuperDisk also mounts on **SCSI with no bridge**.
  Disclosed in "NOT tested": `RDISK_TYPE_ZIP_750` is enabled as a side effect of the
  contiguous enum block, and the Qt new-image dialog still cannot create SuperDisk media.
- **[`dist/ls120_vendor_spp/`](../dist/ls120_vendor_spp/)** — the vendor Have Disk package.
  One INF line applies the probe suppressors at install, so the keyboard survives it.
- **Andrew's loop closed** on [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23),
  where he actually asked (2026-09-12, naming BackPack = `bpck`, one of the `paride` bridges the
  PR cites). Ledger updated.
- Console box gone: `tools/pe_subsystem_gui.py`, re-run after each rebuild.

## Issues opened for the optimisation track

**#28** driver + VxD audit (umbrella, six questions, device register) · **#29** DMA: reach,
µs/byte, channel inventory, CPU overlap · **#30** XT-IDE request merging · **#31** SCSI
disconnect + cache pages · **#32** 3C509B · **#33** DRAM refresh · **#34** display mode ·
**#35** shadow RAM + memory-mapped storage · **#36** the FAT12 tooling bug.

⭐ **#29 is the real gap.** Every other path has a measured µs/byte; DMA has none, so it
cannot be ranked. Andrew's OS/2 Museum method has never been run.

## Answered today, do not re-ask

- **`T130.MPD` already uses string I/O.** `ScsiPortReadPortBufferUshort` (2 sites) and
  `WritePortBufferUshort` (1) carry bulk; `...PortUchar` (28/32) is register traffic.
  ⚠ `pedis.py io` shows one `in al, dx` — a SCSIPORT miniport does I/O through imported
  helpers, so **resolve the IAT thunks and count call sites**, or the instrument lies.
- **The card carries `5140442c`**, a 09-19 trace build of `LS120MP.MPD`, not the enumerating
  `d8154f1d`. Restore from `dist/ls120_mpd/` before any hardware test.
- **The `[0117:0000B929] Illegal instruction` is not ours** — it appears with no EPAT device.

## #18 — PARKED, and the owner's machine is fine

The card runs the **patched** `HSFLOP.PDR` (`8e695d00`) and it loads (`INITCOMPLETESUCCESS`).
Not a fault the owner has.

⛔ **Two bed runs withdrawn.** The medium was a synthetic image built by a brand-new tool and
checked only by that same tool. The method for circling back is recorded in the issue's status
block: make the media with a trusted writer, prove a known-good boot reads it, **then** do
Andrew's actual test — boot and *swap disks* — on `monster_fdc`.

## Bed hygiene

`ls120win_clean.img` now carries `d8154f1d` so it always boots; the old 7168-byte miniport
looped forever on the 2500 ms settle and ate two runs. A repeating `SRST released` in the log
means that, not the thing under test. Technique 131 extended with both of today's repeat
offences.
