# LS120MP.MPD - parallel-port LS-120 miniport (issue #22)

Windows 95 SCSI miniport for an Imation LS-120 SuperDisk behind a Shuttle EPAT
parallel-port bridge, on an IBM 5160 with an Intel Inboard 386/PC.

| File | |
|---|---|
| `LS120MP.MPD` | the miniport, 8,192 bytes, md5 `d8154f1d`, code hash `5084a71c` |
| `LS120MP.INF` | installs it as a SCSI adapter, base `0x378` via `AdapterSettings` |

Built from `drivers/imation_ls120_mpd` at commit `28e85f4`, clean tree, with
`build.ps1 -Phase 2 -Mode spp`. The ledger row is in `build_ledger.tsv`.

## What works, measured on the real 5160

**The drive enumerates, mounts and reads**, at `J:`, 2026-09-18. Confirmed on the
machine, not inferred: the device appears in Explorer and files read back correctly.

## What does not work

**Writes fail, and it is this driver's bug, not the drive's.** The miniport never
sets `ScsiStatus`, never fills `SenseInfoBuffer`, and never raises
`SRB_STATUS_AUTOSENSE_VALID`, so the class driver cannot tell `6/28h`
medium-changed from `7/27h` write-protected and gives up. Fixing that is the next
job on this driver.

Note also that an **empty** drive reports write-protected - medium type `00h` with
WP set - so WP alone must never be read as "the disk is protected". Full device
characterisation: `docs/ls120_media_state_responses.md`.

## Transport

This is the **SPP / nibble** build. An ECP build exists and is faster in principle,
but it does not enumerate on hardware; the open defect and the sources for it are in
`docs/resources_and_sources.md` under IEEE 1284. Ship SPP.

## Install

Add New Hardware, decline autodetect, Have Disk, point at this directory. Then set
`PORT=0x378` on the adapter's Settings tab if the node is assigned elsewhere - the
driver takes its base from `AdapterSettings`, not from the node's resources.
