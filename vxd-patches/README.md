# Patched files — which one to use, and which will break your machine

Several files here have similar names and **only one of each is the one you want**. Check the md5
before you copy anything onto a card.

## Sound Blaster Pro — `MSSBLST.VXD`

| Use | md5 | File |
|---|---|---|
| ✅ **YES** | `dcf32b4a7d8dbcc47e659847742417b6` | `sound/MSSBLST_INBOARD.VXD` |
| reference | `cc7e63aacb1f599fcd5b3fa1eb98169c` | `sound/MSSBLST_stock.VXD` (untouched original) |

Two bytes, `maxPhys 0xFFF → 0xFF` at both `_PageAllocate` sites. Fixes the distorted audio: the
XT's 4-bit DMA page latch gives 20-bit reach, and the stock driver allocates its DMA buffer above
1 MB, so the page register truncates and the card plays adapter ROM.

**Confirmed on real hardware 2026-08-24.** Deploy with `tools/deploy_sound_fix.sh <drive>`, which
md5-checks what it is overwriting and refuses anything it was not derived from.

**This goes on AFTER you install the Sound Blaster Pro driver**, not into a pre-monolith image — a
pre-monolith image has no `MSSBLST.VXD` at all; it arrives stock from the CABs at driver install.

## VDMAD — ⚠️ three of the four copies here are the BSOD build

| Use | md5 | File |
|---|---|---|
| ✅ **YES** | `09af55b032aa7139d92d34b4246e7a05` | `VDMAD_INBOARD_FIXED.VXD` |
| ❌ **NO** | `a4fd183baa2de369166d60199dbf6225` | `VDMAD_INBOARD.VXD` |
| ❌ **NO** | `a4fd183baa2de369166d60199dbf6225` | `VDMAD_stage1_patched.VXD` |
| ❌ **NO** | `a4fd183baa2de369166d60199dbf6225` | `osr1/VDMAD_INBOARD.VXD` |
| reference | `aff4ed5ec9ac989574ccf816ce52b004` | `osr1/VDMAD.VXD`, `VDMAD_stage1_stock.VXD` (untouched) |

The three marked ❌ are kept only because the investigation history refers to them. They contain the
defect that caused **issue #5's Sound Blaster Pro BSOD**: this repo's own `patch_vdmad.py` wrote two
bytes into the middle of an `AND AH,0C0h` at `OBJ1:0x1660`, turning a harmless register-only
instruction into a wild write. The BSOD offset was a byte-exact match to the corruption.

`VDMAD_INBOARD_FIXED.VXD` has those two bytes reverted and the three genuinely-correct patches left
in place. **Confirmed on real hardware 2026-08-23.**

## Everything else

| md5 | File | What |
|---|---|---|
| `d3c458017c296fe01a13bceb19f34106` | `osr1/INBRDPC_selftest_skip.SYS` | **Load-bearing, not a convenience.** Skips the driver's self-test. Without it, stock `INBRDPC.SYS` fails a check real hardware passes — see [86Box#7638](https://github.com/86Box/86Box/issues/7638) and Technique 63. Do not drop it as "no longer needed". |
| `c25b951a0a6093dcfa5138d89159cbf6` | `osr1/INBRDPC_stock.SYS` | untouched original |
| — | `osr1/VPICD_INBOARD.VXD` | phantom slave 8259 at `0xA0`/`0xA1` neutered |
| — | `osr1/KEYBOARD_INBOARD.DRV` | port-64h fix |
| — | `dma/LPT_INBOARD.VXD`, `dma/QIC117_INBOARD.VXD` | same `maxPhys` fix as the sound driver. **Built and audited, NOT tested** — neither device is in use on the development machine. Correctness, not a known fix. |

## Deploying a bundled VxD

`VDMAD`, `VKD` and `VPICD` are combined into `VMM32.VXD` by Setup. **A replacement dropped in after
that combine is silently ignored** — `BOOTLOG.TXT` keeps reporting the bundled copy, and `VMM32.VXD`
is `W4` compressed so it cannot be patched in place. Use a pre-monolith image and
`vxd-patches/deploy_premonolith.sh <drive>`.

`MSSBLST.VXD`, `LPT.VXD` and `QIC117.VXD` are dynamically loaded from `WINDOWS\SYSTEM`, so a plain
file copy does take. `BOOTLOG.TXT` tells you which path a driver used:

```
Loading Vxd = VDMAD                     <- bundled; a file copy will NOT take
Dynamic load device  mssblst.vxd        <- from disk; a file copy WILL take
```
