# #46: where `SD120PPD.MPD` spends ~892 ticks at start-up - desk read, 2026-10-04

Source: the full listing `drivers/imation_ls120/SD120PPD_MPD.asm` (stock miniport, md5 `08104ffb`).
Nothing below has run on hardware yet.

## The constant

`BOOTLOG.TXT` `Initing` -> `Init Success` for `sd120ppd.mpd`: 892 ticks (working config, 2026-09-19),
891 (`ECP=1 IRQ=7 DMA=3`), 897 (drive powered, 2026-09-29), 892 (drive unpowered). The same cost
in every configuration points at a timeout that always expires, not at the drive.

## Waits on the start-up path (init routine rva `0x1fc1`)

| rva | what | budget |
|---|---|---|
| `0x203e` | fixed delay, `0x520d(2000)` | 2 s |
| `0x206b` | fixed delay per unit, `0x520d(50)` | 50 ms |
| `0x1ac7` | BSY wait: read status (reg 7) until bit 7 clears, `stall(10)` per pass | 500,000 passes |
| `0x14b2` | command wait: `[0x203f0] / 10` passes of `stall(10)` + a status read | `[0x203f0]` us |

`0x5201` is `ScsiPortStallExecution` (the only call site); `0x520d(n)` is n x `stall(1000)`.

The init routine loops over two units (`cmp byte [ebp-74h], 2` at `0x209a`): master `A0h` and slave
`B0h`. Per unit:

| step | rva | ATA command | `[0x203f0]` |
|---|---|---|---|
| probe | `0x242a` -> `0x1a34` (select, BSY wait) -> `0x144e` | `00h` | 1,000,000 |
| identify | `0x20c4` -> `0x2339` -> `0x2587` | `A1h` IDENTIFY PACKET DEVICE | **45,000,000** |
| (fallback) | `0x23ab` | | |
| then | `0x22a6` | | |

A unit is used only if its present flag (`[unit*48h + 0x20ff0]`, set at `0x2086`) was set after all
three steps passed; the check is at `0x0856`.

## Hypothesis and test

The LS-120 is the bridge's only device, as master. Probing the empty slave position can leave BSY set,
so `0x1ac7` runs all 500,000 passes. Each pass reads the status through the bridge, several port
accesses at ~5.7 us each here, so the vendor's nominal ~5 s budget becomes tens of seconds.

**Not explained yet:** an unpowered drive should also time out the master's probe and double the cost,
but the unpowered boot measured the same 892 ticks. So this is a test, not yet a fix.

**Test:** `patch_sd120ppd_masteronly.py` changes the loop bound at `0x209a` from 2 to 1 (file offset
`0x209d`, md5 `4fafb2e9`). One boot with the drive powered, `BOOTLOG.TXT` on. If init drops from
~892 ticks to a few dozen, the slave probe is the cost; then the unpowered case.

## Result on the 5160, 2026-10-04 (one FULL boot, drive powered)

Master-only driver loaded (md5 `4fafb2e9` in `IOSUBSYS`, stock kept as `SD120PPD.STK`):
`[00159E2E] Initing sd120ppd.mpd` -> `[00159E9F] Init Success`, **113 ticks** (~6 s) against
891-897 with the stock driver. The drive mounted, took a write, and `FC` matched a copied file
(owner). So the empty slave position cost ~779 ticks (~43 s) every boot. Raw:
`docs/captures/2026-10-04_issue46_card/BOOTLOG.TXT`.

Still to run: the same driver with the drive unpowered, then publish it in `dist/ls120_vendor/`.

## Drive unplugged, 2026-10-04 late

Master-only driver: a FULL boot did not reach Windows. Master-only plus a 65,536-pass BSY wait
(`patch_sd120ppd_bsywait.py`, md5 `8723f3d7`): start-up took 630 ticks (~35 s), reported Init Success
with no drive, and Explorer then crashed. Other timeouts on the path (the 45 s IDENTIFY budget) still
dominate, and the driver claims the adapter without a drive. Not a small patch. **Decision (owner):
boot LEAN when the LS-120 is not connected.** The card is back on the master-only driver.
Raw: `docs/captures/2026-10-04_issue46_unplugged/BOOTLOG.TXT`.
