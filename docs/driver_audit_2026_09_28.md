# Driver and VxD audit - what holds the locked memory (2026-09-28)

Desk work from the real CF (`BOOTLOG.TXT`, `SYSTEM.INI`, `IOSUBSYS`, `VMM32.VXD`), no boot.
Issues: #28 (per-component audit), RAM track from `docs/ram_baseline_2026_09_28.md`.
Checklist for the machine: `docs/5160_checklist_2026_09_29.md`.

## Method

`tools/le_locked.py` splits each VxD's LE object table into locked (preloaded,
non-discardable), pageable, and init-only bytes. `VMM32.VXD` on the CF is W4; a copy was
converted with `patcher9x -force-w3 --vxd-convert` and split into its members. These are
header flags, not a measurement: a VxD can lock more at run time. The three `.MPD` miniports
are PE files and are counted at full file size, the upper bound.

## Finding 1: System Monitor's "locked" includes the disk cache

In `docs/captures/perflog_5160_2026-09-28_A.csv`, `cpgLocked` tracks `cpgDiskcache` at
r = 0.97 over 151 samples; the first step is +106,496 bytes in both. So the ~2 MB locked
baseline is:

| | at boot | four programs open |
|---|---|---|
| locked (System Monitor) | 1,980 KB | 2,048 KB |
| of which disk cache | 560 KB | 536 KB |
| **locked, excluding cache** | **1,420 KB** | **1,512 KB** |

The cache is already capped (#45 row 1). Trims come out of the other ~1.4 MB.

## Finding 2: drivers account for ~0.8 MB of it

| group | locked KB |
|---|---|
| Static core in `VMM32.VXD` (VMM 105, VFAT 42, IFSMGR 30, VPICD 11, VDMAD 10, IOS 9, ...) | 279 |
| Network (PPPMAC 102, VNBT 77, VIP 49, VTCP 38, NDIS 29, ELNK3 22, VDHCP 13, VNETBIOS 8, SPAP 7, VNETSUP 5, VTDI 1) | 350 |
| Miniports, file size (SD120PPD 78, T130 10, XTIDEMP 10) | 98 |
| Storage and other dynamic VxDs (NEROCD95 18, UMAXIS11 9, SERIAL 4, CDVSD 4, RMM 4, CDFS 3, ...) | 60 |
| **total** | **~790** |

The other ~0.6-0.7 MB is not in any driver file. Candidates, none measured yet: the
first megabyte (DOS, `INBRDPC.SYS`, real-mode drivers - item 1 of the checklist reads it),
the 64 KB DMA buffer (`DMABufferSize=64`), 16-bit Windows' fixed segments, and VMM's own
heaps and page tables.

## Classification

| driver | locked KB | status | action |
|---|---|---|---|
| PPPMAC + SPAP (Dial-Up Adapter) | 109 | no modem planned | **remove - first trim** |
| VNBT + VNETBIOS (NetBIOS over TCP/IP) | 85 | needed for drive mapping (planned) | keep |
| TCP/IP, NDIS, ELNK3 | 156 | network in use | keep |
| SD120PPD.MPD | <=78 | LS-120, optional | boot-menu toggle (rename to `.OFF`) |
| NEROCD95 | 18 | Nero, occasional | boot-menu toggle |
| UMAXIS11.386 | 9 | scanner, occasional | leave; not worth a menu entry |
| SCSI, CD, disk, floppy stack | ~40 | T130B chain, SyQuest planned | keep |
| `[mci]` devices, codecs, ACM | 0 at rest | load on demand | nothing to gain |
| NECATAPI, ATAPCHNG, TORISAN3, DRVSPACX | 0 (inferred) | loaded, absent from `INITCOMPLETE` - presumably unloaded | nothing to gain |

Real-mode lines that may be removable (conventional memory, sizes to come from `MEM /C`):
`SETVER.EXE`, `DISPLAY.SYS` and the two `mode con codepage` lines. Keep `KEYB` - DOS boxes
need it for the UK layout.

## Housekeeping found

- 23 retired `LS120MP.*` builds in `IOSUBSYS`, not loaded; backed up host-side for the owner
  to delete (checklist).
- `SYSTEM.INI [386Enh]` repeats devices the registry already loads: `*vshare` and `ebios` fail
  as duplicates in `BOOTLOG.TXT`. Harmless; not worth a boot.

## `INBRDPC.SYS`: shrink it, or hand off to a VxD? (gated, not started)

It has to stay: it sets up the Inboard's RAM before `HIMEM` and Windows load, so a VxD could only
take over afterwards, XTIDE-style. After boot it hooks `INT 13h` (wait states around real-mode disk
calls, not in the data path - `docs/ios_safelist_howto.md`) and `INT 15h 87h/88h`. With 32-bit disk
drivers neither is a hot path, so a VxD probably buys little speed.

The likelier gain is memory: whatever it leaves resident in the first megabyte stays locked all
session. Gates, cheapest first:

1. Resident size: **answered statically, 2026-09-28** - `0EA0h` = 3,744 bytes without `EGACACHE`
   (`docs/egacache_static_read_2026_09_28.md`). Nothing to shrink; `MEM /C` confirms.
2. A bed trace counting entries into its resident code after the desktop appears.

Build a VxD only if the trace shows Windows really calling it. With 3.7 KB resident, memory is no
longer a reason.

## Safe Mode with `INBRDPC.SYS`

F5 Safe Mode skips `CONFIG.SYS`, so `INBRDPC.SYS` never loads and Windows has no extended memory:
Safe Mode is unusable on this machine. The boot menu's **SAFE** entry processes `CONFIG.SYS`
normally and starts `WIN /D:M` itself, straight after `IVT68FIX`. That also gives a minimal-Windows
RAM figure to compare against LEAN.
