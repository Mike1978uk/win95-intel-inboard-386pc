# Next session - 2026-10-04

Supersedes `next_session_2026_10_03.md` (still the full outstanding list: sections 2, 3 and 6
there stand). This is what changed on 2026-10-03 and what to do first.

## Done 2026-10-03

- **`DMABufferSize=64` removed** from the card's `SYSTEM.INI` (default 16 KB; `DMABufferIn1MB=True`
  kept). Revert: `copy C:\WINDOWS\SYSTEM.BSR C:\WINDOWS\SYSTEM.INI`; pre-SHADRAM copy is `SYSTEM.B02`.
  Locked memory less cache ~40 KB lower - consistent with 48, not a clean measurement.
- **Read-ahead OFF** on the card, Hard Disk and CD-ROM (owner). Frees no locked memory
  (1,260 -> 1,264 KB). Registry starting values: `docs/settings_review_2026_10_03.md`.
- **SHADRAM: 0 of 16 pages on the 5160.** `PHYSPROBE` (unreal-mode DOS probe, `tools/physprobe/`)
  shows why: after boot the card keeps its reserved 128 KB out of the CPU's map - `0x5E0000` and
  `0x5F0000` read FF and writes do not stick, also with port `A0h` bit 7 set. The BIOS copy is
  served at `F0000`. `docs/shadram_5160_2026_10_03.md`.
- **#34 closed** at 800x600 (owner's decision, benchmark not run).
- Issues posted: #29, #35 (twice, latest has the probe result), #41, #45 status blocks; #48 comment.
- Checklist item 1 done (`C:\MEMMIKE.TXT`, 2026-09-29: INBRDPC 3,760 B, 578 KB free; video ROM
  `55 AA 40`).
- COMrade95 staged at `C:\CMR95\COMR95.EXE`, MCP `comrade95` registered; one bridge on COM2 at a
  time. Not run yet.

## ⚠ Alert, not fixed (owner's call)

`86box_full/src/device/inboard386.c` maps `0x5E0000` and `0x5F0000` permanently; the real card
does not after boot. That is why the bed gave SHADRAM 16 pages. Any bed result that depends on
that window is suspect.

## First things

1. **DMA buffer check, 5160 in Windows:** a DOS game with **sampled** sound in a DOS box
   (FastDoom, Wolf3D, Keen 4-6 - not Monkey Island, FM only), then a floppy copy. If DOS-box
   sound breaks, `DMABufferSize=32`.
2. **#45 row 3 speed:** `T130AB` + `PERFLOG` with read-ahead None (current), then Full.
3. ~~SHADRAM~~ **removed** from `SYSTEM.INI` by the owner, 2026-10-03 (it adds nothing). `SHADRAM.VXD`
   stays on `C:\` unloaded.
4. **Desk:** how `INBRDPC.SYS` opens and closes the reserved window while copying the BIOS
   (not port `A0h` bit 7 on its own). Only then is the EGA half reachable,
   and Windows would need the window left open. Not a quick lever - rank it against #46 and #41.
5. Then section 2 of the 10-03 handoff: #46 (LS-120 boot stall - the LEAN entry removes it on
   every boot, confirmed by the owner over several boots; LEAN also drops Nero, but `BOOTLOG.TXT`
   puts the wait in `sd120ppd.mpd`. The patch is still the fix), #41 re-scope incl. ELNK3 buffers,
   SWAPCOMP `HASH_LOG 10` (5160-only A/B).

## COMrade notes

- Keystroke injection timed out repeatedly on 2026-10-03; file read/write worked. Write a batch
  out-of-band, start it with one short command (or the owner types it).
- `MEM` is not on the path in a command-prompt-only boot (`CHK.BAT` produced nothing).
- Posting: one plain `gh issue edit|comment|close` per call matches the allow rules; a python
  wrapper or an `&&` chain gets blocked.

## State of the CF

`C:\PHYSPROB.COM`, `C:\CHK.BAT`, `C:\PP*.TXT`, `C:\CHKDONE.TXT` (probe and checks, all captured in
`docs/captures/2026-10-03_physprobe/`), `C:\CMR95\`. Last image `win95_shdw_swp.img` predates the
`SYSTEM.INI` edit and read-ahead change. `C:\SNP92.TMP` (400 KB) still there, deletable.

## Done 2026-10-03, evening (owner at the 5160)

- **ROMPROBE: parity error, no output.** Read never-written RAM at `5E0000` with port 670h bit 0
  clear - see `docs/romcache_decode_2026_10_03.md` 3a. `ROMPROB2` staged (`C:\RP2.BAT`), command
  prompt boot.
- **Read-ahead, hard disk:** `T130AB` on `C:` (the CF) - off 7.20 + 7.20 + 6.38 = 20.78 s, on
  7.09 + 6.43 + 6.86 = 20.38 s. No difference beyond the scatter. The owner prefers off; it stays
  off. The PERFLOG files ran after the copies (counters flat), so they hold memory state only.
  Raw: `docs/captures/2026-10-03_readahead/`. `t130ya` (`G:`, a changer LUN) failed in 0.4 s - not
  a result.
- **CD-ROM read-ahead on:** Explorer hung after a CD was inserted (once). Left at "No read-ahead".
- **DMA buffer (default 16 KB since 2026-10-03):** Wolf3D in a DOS box plays sampled sound. FastDoom
  `-xt` with sound hung the machine (#47 settings, not a verdict on the buffer). Floppy in Windows
  unreliable: `B:` copied once, then not; `A:` difficult. In DOS both work - `B:` formatted, copied
  and `FC` identical. Open: was Windows floppy reliable with `DMABufferSize=64`?
- **SAFE boot works** (checklist 7a complete). Device Manager shows two duplicates; identified in
  the card's `SYSTEM.DAT`:
  - `TRANSCEN D` - `ESDI\TRANSCEND___1`, parent `ROOT\HDC\0000` (the real-mode/ESDI era),
    `DiskDrive\0003`. Stale; the live CF is `XT-IDE TRANSCEND` under the XT-CF miniport.
  - `MATSHITA LS-120 COSM 04`, twice. Live: parent `ROOT\SCSIADAPTER\0002` (the Imation
    `sd120ppd.mpd` adapter), `DiskDrive\0007`. Stale: parent `ROOT\SCSIADAPTER\0000`, which is now
    the XT-CF adapter, `DiskDrive\0002`, the one with reserved drive letters `I`-`I`.
  Registry backed up host-side before any removal: `..\image_archive
egistry_2026-10-03_pre_phantom\`.
- **`DMABufferSize=64` restored** on the card (reader, 2026-10-03 evening), the setting under which
  Windows floppy worked. One change, to test whether the 16 KB default broke Windows floppy.
  Backups: `SYSTEM.BSR` = the file just before (no line), `SYSTEM.B03` = the old `.BSR` (SHADRAM on,
  64). Next: in Windows, copy to `A:` and `B:` and `FC /B`, with a disk that just worked in DOS.
- **Floppy with `DMABufferSize=64` back (Windows):** `B:` copy, read and write good, `FC` identical
  (`docs/captures/2026-10-03_readahead/t130comp.txt`). `A:` still flaky. One run each way, so
  64 stays on for now; `A:` looks like the drive or media rather than the buffer - it was flaky at
  16 KB too, and works in DOS.
- **SHADRAM v2** (`drivers/shadowram/`, md5 `01de5ce6`): clears port 670h bit 0 as INBRDPC's
  `0B60h` routine does, writes `5E0000` before reading it, donates 16 pages, maps V86 `F0000` onto
  the BIOS copy at `5F0000`. **Bed** (`vm_magnaram_shadram2`, clone of `vm_magnaram_off`, SHADRAM
  the only `[386Enh]` driver, `PAGERUN V`): Status 0, 16/16 added, `BiosPage` 5F0h, 3 VMs mapped,
  workload completed. Page-ins 16,568 - at the A level of 2026-10-02, not the S level; one run,
  and 86Box ties 670h bit 0 to the timing of every memory access (`inboard386_apply_mem_timing`:
  bit clear = all reads and writes charged at XT-bus rate), so the bed cannot judge speed with this
  driver. On the card bit 0 reaches only U71 and U101 (netlist). Model gap, alerted, not fixed. Raw: `docs/captures/2026-10-03_shadram2_bed/`.
- **On the card now:** `C:\SHADRAM.VXD` = v2 (v1 kept as `SHADRAM.V1`), `DEVICE=C:\SHADRAM.VXD`
  enabled in `SYSTEM.INI`. Backups: `SYSTEM.BSR` = just before (line commented), `SYSTEM.B04` = the
  previous `.BSR`. Revert: `copy C:\WINDOWS\SYSTEM.BSR C:\WINDOWS\SYSTEM.INI`. First boot: read
  `SHADRAM\*` with `START PERFLOG X 30` (want Status 0, PagesAdded 16, BiosPage 1520 = 5F0h).

## SHADRAM v2 on the 5160, 2026-10-03 late

Raw: `docs/captures/2026-10-03_shadram2_card/`. Loads clean (`BOOTLOG.TXT`). PERFLOG: Status **7**
(I/O channel check), PagesTested 16, PagesFailed 0, **PagesAdded 0**, BiosPage 1520 (5F0h),
VMsMapped 2. The window opened and every page passed its data tests; port 62h bit 6 was set when
read once at the end, so all 16 were refused. Which access raised it is not known - the driver
reads the latch only once.
`PAGERUN ml` and `T130AB` (6.81 s x 3) sit at the 10-02 B level, as expected with no pages added.

**Alert, not fixed:** on Status 7 the driver still leaves port 670h bit 0 clear and maps V86
`F0000` onto `5F0000` - a changed machine for no pages.

`SYSTEM.INI` (owner's edit, 23:11): `DMABufferSize=` has no value. The last good Windows floppy
run was with 64.

**On the card now (late 2026-10-03):** `C:\SHADRAM.VXD` = the diagnostic build `SHADRAMD`
(md5 `73d6eea9`, `a01b508`), v2 kept as `C:\SHADRAM.V2`. Same accept/refuse logic as v2; it adds
`SHADRAM\Diag*` counters naming the stage that sets port 62h bit 6. `DMABufferSize=64` restored
(the blank line is in `SYSTEM.BSR`, previous `.BSR` is `SYSTEM.B05`). Next boot: `START PERFLOG X 30`.
Revert: `copy C:\SHADRAM.V2 C:\SHADRAM.VXD`.

## SHADRAMD on the 5160, 2026-10-04 (one boot)

Raw: `docs/captures/2026-10-04_shadramd_card/` (last block of `PERFLOG.CSV`). Status 7, 16/16 tested,
0 failed, 0 added, as v2. `DiagSteps` = 16: the flag was clear on entry (`DiagPortsEntry` 4834h:
61h = 48h, 62h = 34h), clear after the 670h write and the 5F0000 compare, clear after the pulse
(so the 61h bit 5 pulse does clear it), and **set after the first zero-fill write of 5E0000**.
Every later stage - both patterns on all 16 pages, the ROM compares, the address fill and check,
the final zero - left it clear (`DiagParityPages` 0, `DiagParityRom` 0). `DiagLast62` = 74h.
So the first write to never-written RAM raises the I/O channel check; the RAM then tests clean.
v2's single check at the end sees that write. Candidate v3: clear the latch after the first fill
and judge on the tests only. Not built - owner's call.

**v3 built and on the card (2026-10-04):** the latch is cleared after the first fill; a check
raised by any later test still refuses every page. `C:\SHADRAM.VXD` = v3 with the Diag counters
(`SHADRAMD`, md5 `4badd94b`, one byte from the last diagnostic: the refusal mask F8h -> E8h).
`C:\SHADRAM.V3` = v3 without them (md5 `bb633ec5`). Next boot: `START PERFLOG X 30`, want
Status 0, PagesAdded 16. Revert: `copy C:\SHADRAM.V2 C:\SHADRAM.VXD`.

**SHADRAM v3 WORKS on the 5160 (2026-10-04, one boot):** Status 0, PagesTested 16, PagesFailed 0,
**PagesAdded 16**, BiosPage 1520 (5F0h), VMsMapped 2, DiagSteps 16 (the first fill only),
DiagParityPages/Rom 0. Raw: `docs/captures/2026-10-04_shadram3_card/`. Windows has the 64 KB.
Not yet measured: whether it helps - `PAGERUN` A/B against v2-off, on the card only.

**SHADRAM v3 A/B on the 5160 (2026-10-04, one pair, no WinZip):** raw `docs/captures/2026-10-04_shadram3_ab/`.

| | S16 (on) | N (off) |
|---|---|---|
| open / switch1 / 2 / 3 (ms) | 36,819 / 5,578 / 2,716 / 2,852 | 36,283 / 6,096 / 2,736 / 2,873 |
| total (ms) | 47,965 | 47,988 |
| page-ins / page-outs | 2,832 / 1,140 | 2,879 / 1,132 |

No measurable difference: totals within 0.05%, page-ins -1.6%. 64 KB is ~1.3% of RAM; the bed's -13.8%
did not carry over. The tagged labels (`R-S16-003522`, `R-N-004410`) worked first time.

**Kept (owner, 2026-10-04):** `C:\SHADRAM.VXD` = v3 without counters (md5 `bb633ec5`), ~60 KB net
(16 pages added, one 4 KB locked page). Diagnostic v3 kept as `C:\SHADRAM.DG3`.

**`DMABufferSize=64` commented out (2026-10-04, reader):** default 16 KB, `DMABufferIn1MB=True` kept.
Backups: `SYSTEM.BSR` = just before (64 active), `SYSTEM.B06` = the previous `.BSR`. The 10-03 floppy
corruption happened at 64, so 64 is not shown to help. Test: Windows copy + `FC /B` to A: and B:,
then check the same disks from a DOS boot. Same steps at 64 only if 16 misbehaves.

## Next session (owner, 2026-10-04 01:00)

1. Floppy test at the 16 KB default (above). If it passes, the buffer saves 48 KB.
2. **The driver track** - RAM trim, `docs/driver_audit_2026_09_28.md`, technique 136 (subtract the
   disk cache from locked before attributing).
Tonight's net: SHADRAM ~60 KB to Windows; the DMA buffer default would add 48 KB - ~108 KB if it holds.

## Desk notes, 2026-10-04 afternoon

- **Network panel (owner):** no File and Printer Sharing; TCP/IP is the only protocol. Nothing
  left to unbind; the stack that remains is what drive mapping needs.
- **`PAGERUN` screen leftovers** (photo `XT_project/photos/IMG_1442.jpeg`): intact pieces of closed
  windows (Notepad on `WIN.INI`), partly repainted. A missed desktop repaint under paging, not bus or
  VRAM corruption. Check next time: F5 on the desktop or drag a window over it. Optional: a desktop
  `RedrawWindow` as `PAGERUN`'s last step (cosmetic, not done).
- **Mouse clicks lag, pointer does not:** a serial mouse sends buttons and movement in the same
  3-byte packet, so the delay is after the driver - the click waits for its program to run. Test:
  Mouse control panel test area, idle and during `PAGERUN`.
- **Driver size:** `SD120PPD.MPD` `.text` 64,018 B against our retired `LS120MP.MPD` 3,932 B and
  `XTIDEMP.MPD` 3,360 B. Compressing drivers gains no RAM (locked code runs decompressed). A
  coverage trace of the vendor driver in the LS-120 bed is the spec for a lean rewrite; pair it
  with #46.
- **Mach8 VRAM as RAM:** no - reachable only through `PIX_TRANS` at ~1.9 us/byte, no faster than
  paging to the CF. A12b (glyph cache timing, `docs/bus_optimisation_plan.md`) is the open item.
- **#10:** section 9 of `docs/issue10_old_bios_2026_09_27.md`.

**Alert, not fixed:** `ivt68fix/IVT68FIX.ASM` and the matching comment in
`86box_full/src/cpu/386_dynarec.c` still say 1982 ROMs fail `INBRDPC.SYS`'s signature check.
Disproved (#10 section 1, and Cimon's 5160 on the 08NOV82 ROM).

**Owed replies (owner, 2026-10-04):** only @andrew-hoffman. disruptor was answered on the forum;
red-ray the owner answers in his own time, not tracked here; nothing is owed to Michal Necasek.

## 86Box window gating, 2026-10-04 evening (diagnostic tree `86box_3c509b`, local)

`696cd8cd1` gates the `5E0000`/`5F0000` windows on port `670h` bit 0, as the card does;
`32bbc97c0` puts the memory timing back on bit 0. Keying the timing on the wait-state field
instead let `INBRDPC.SYS`'s memory diagnostic fall back into POST (`F000:E05B` from `E059`)
in the bed; with bit 0 it passes. Bed boots: gating with and without `NODIAGS` reach the
desktop. Why the diagnostic needs that slow phase is open. Raw: `docs/captures/2026-10-04_romcache_bed/`.

## SHADRAM v4, 2026-10-04 late (Andrew's suggestion on #35)

`e1e7bfd`, `SHADRAM.VXD` md5 `bd529d50`. Keeps BIOS-copy pages only for code (`F0`, `F1`, `FE`, `FF` on
the 1986 ROM) and any page INBRDPC patched; donates the filler and cassette-BASIC pages of `5F0000`
too; V86 maps donated pages to the ROM. **Bed** (`vm_romcache`, 1986 ROM, `NODIAGS`, gated 86Box):
Status 0, Tested 28, Added 28, KeepMask 49155 (`C003h`), desktop. Raw: `docs/captures/2026-10-04_shadram4_bed/`.

**On the card now:** `C:\SHADRAM.VXD` = v4, v3 kept as `C:\SHADRAM.V3` (md5 `bb633ec5`). Next boot:
`START PERFLOG X 30`, want Status 0, PagesAdded 28, KeepMask 49155; System Properties should show more
memory. Revert: `copy C:\SHADRAM.V3 C:\SHADRAM.VXD`. NMIWATCH/NMICHK removed from `SYSTEM.INI` and
`AUTOEXEC.BAT` (files left on `C:\`). `IOSUBSYS\SD120PPD.MPD` = the #46 master-only driver.

## 5150 bed (#10), parked

`vm_5150` (gitignored): `vm_nmitest` image (no tools, `NODIAGS`), `bios = cimon5150_superpc25` - Cimon's
Super PC v2.5 dump with INBRDPC's `E05B` jump undone (8 KB checksums to 0), at
`vm_5150\roms\machines\ibmpc82\CIMON_SUPERPC25_FE000.BIN`, his file, kept out of git. The diagnostic
build offers it (`0f993dfb5`). **It does not boot:** the XT-CF ROM finds the disk, shows `W»HDD [C]`, tries
C then A, and falls to ROM BASIC; one floppy is configured. Cimon's machine boots through a Future Domain
SCSI ROM instead. Next: a DOS boot floppy image (none found in the tree), or look at why the XT-CF ROM
cannot boot under this BIOS. Screenshots: `tools`-free PrintWindow capture in the session scratchpad.

**SHADRAM v4 on the 5160 (2026-10-04 22:22, one boot):** Status 0, Tested 28, Added 28, Failed 0,
KeepMask 49155, VMsMapped 2; owner reports it stable. 112 KB of the reserved block now with Windows
(v3: 64 KB). Raw: `docs/captures/2026-10-04_shadram4_card/`.

Owner, same evening: a FULL boot with the LS-120 **unplugged** does not reach Windows (#46 fix tested
powered only - reopen); the next unattended boot came up at the Safe Mode prompt with a "press
Ctrl+Alt+Del" message, wording not captured; Mouse Properties > General shows a garbled device name
and Change does nothing (photo `XT_project/photos/IMG_1464.jpeg`).
