# SBDS.VXD: a DirectSound driver for the SB Pro 2 (T5, #58)

Why: emulated DirectSound costs a Mode X frame ~21 %; ~10 % of it is the emulation running at all, ~15 %
when open and idle (`captures/2026-11-23_t5_split/`). Sources: `sbpro_dsound_sources_2026_11_23.md`, the
decoded DDK guide (`references/dxddk/ds_ddk_decoded/`), `MSSBLST` disassembly (`tools/vxddis.py`).

## What it is

A static VxD, separate from `MSSBLST.VXD` (no source; we keep the wave driver untouched). It registers one
primary-only `IDsDriver` with `DSOUND.VXD`. DirectSound still mixes secondaries on the CPU (DDK guide: ISA
hardware mixing is "only useful if the card has on-board memory"; the SB Pro has none). What goes: the
emulation's waveOut path, its silence stream, and the 16-bit callbacks in `Msgsrv32`.

## Decisions

| | Choice | Why |
|---|---|---|
| Buffer | 16 KB, `_PageAllocate` `PG_SYS`, `PageContig+PageFixed+PageUseAlign`, align = size, `maxPhys` 9Fh; halves on failure | XT DMA reach (`win9x-dma-driver-audit`: strict 640 KB); aligned to its size so it cannot cross 64 KB. Same flags as the DDK sample, whose buffer DSOUND writes from ring 3 |
| Device | `CONFIGMG_Locate_DevNode("ROOT\MEDIA\0000")` | DSOUND pairs driver and wave device by `dnDevNode`; card registry `Enum\Root\MEDIA\0000` = `*PNPB002`. Not found: no registration, DSOUND keeps its emulation |
| Registration | Init_Complete: `VXDLDR_LoadDevice("dsound.vxd")`, `_DSOUND_RegisterDeviceDriver(&drv, 0)` | DDK sample `dsInitializeDriver` |
| Flags | `DOMMSYSTEMOPEN + DONTNEEDPRIMARYLOCK` | DSOUND holds `waveOut` open while we play, so no other program reprograms the DSP; the primary is a DMA buffer, no Lock. Not `DOMMSYSTEMSETFORMAT`: we set rate and stereo ourselves |
| Interrupts | none of our own; DSP acknowledged by reading 22Eh in `GetPosition` | `MSSBLST.VXD` owns IRQ 5. With `waveOut` open the System VM owns the DSP (`[+28h]`), the IRQ is reflected to `MSSBLST.DRV`, whose wave ISR (`0EFEh`) returns at once when it is not playing. Position comes from the 8237 count, as in the sample |
| DMA | ring-0 writes to the 8237: mode 59h (single, auto-init, read, ch 1), page 83h | Ring-0 I/O is not trapped. `VDMAD`'s channel 1 belongs to `MSSBLST` |
| DSP | time constant (40h), block 48h = half buffer, 1Ch normal or 90h high speed above 23 kHz aggregate; stereo = mixer 0Eh bit 1; Stop = DSP reset | DSP 3.xx: high speed is left only by reset |
| Formats | 8-bit PCM, mono 4-44.1 kHz, stereo to 22.05 kHz | `DSCAPS_PRIMARYMONO+STEREO+8BIT` |
| Controls | primary with CTRLVOLUME/PAN/FREQUENCY refused (`DSERR_CONTROLUNAVAIL`) | DDK guide: primaries must refuse them (Win95 mixer conflict) |
| Silence | Stop masks DMA and resets the DSP: nothing moves on the bus | the idle ~15 % |
| Counters | PERF statistics (NMIWATCH's way): state bits, physical address, register result, opens, plays, stops, GetPosition calls, last format, DSP timeouts | readable without a debugger |

## DirectX 7 mixes on its own only with a 486-class FPU (found 2026-11-24)

DSOUND.DLL chooses between two mixers. The timer-driven one lives in DSOUND.VXD (ioctl 44 creates it,
42/43 run it, `Set_Global_Time_Out` 22-200 ms). The fallback runs only when an application calls in. DSOUND.DLL
`beb36ee8` (via `beb36da4`) takes the timer mixer only if PAGEFILE's version is non-zero with BL >= 2 and
**VMCPD is 4.02 or later with coprocessor type >= 3**. Measured with the diagnostic build:

| | VMCPD type | mixer | dxdiag / DSTEST |
|---|---|---|---|
| 5160 card (387 on the interposer) | 3 | timer | one-shot plays and stops at 1,144 ms |
| bed, no FPU (every bed until 11-24) | 0 | app-driven | silent unless the program keeps calling |
| bed, `fpu_type = 387` | 3 | timer | dxdiag: all four tests successful |

So the 11-23 dxdiag failure was the bed, not the driver. Write cursor = play cursor is correct (DDK sample
`IDsDriverBuffer_GetPosition`); a lead of 4 bytes changed nothing. DSOUND.VXD's ring-0 mixer carries x87
code and saves FPU state through VMCPD, which is why the check exists. Evidence and the diagnostic source:
`captures/2026-11-24_sbds_dxdiag/`. Bed rule: `tools/bed_launch.ps1` refuses a BL3 Inboard bed without
`fpu_type = 387`.

## Known risks, to test in this order

1. DSOUND (DX 7.0a) accepts a DX5-style driver with no certification (`wReserved` = 0).
2. `MSSBLST.DRV` does something to the DSP at `waveOutOpen` (`DOMMSYSTEMOPEN`) that clashes with ours.
3. SB Pro stereo channel order: Creative's sequence sends one silent byte first; not done in v1.
4. 16 KB of contiguous memory below 640 KB at Device_Init; the counters show what was granted.

## Measure

`M8DDB modexd` / `modexdf` / `modexdi` against the split (`captures/2026-11-23_t5_split/`); `modexd` should
approach PlaySound's 0.94, `modexdi` should approach silent. M8DDB prints DSOUND's caps: `DSCAPS_EMULDRIVER`
(20h) clear means our driver is in use.
