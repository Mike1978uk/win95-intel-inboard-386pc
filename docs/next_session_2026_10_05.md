# Next session - 2026-10-05

Supersedes `next_session_2026_10_04.md` (still the record of that day: SHADRAM v4, #46, NMI work).

## Done 2026-10-04/05

- **SHADRAM v4** on the 5160: 28 pages, 112 KB of the reserved block to Windows; Andrew told on #35.
- **#46 closed:** master-only `SD120PPD.MPD` (~49 s -> ~6 s), shipped. Unplugged drive: boot LEAN.
- **#10:** NMI ruled out (Cimon's `NMICHK /m`); Super PC source clean. Cimon asked for `WIN /D:X` and
  `SystemROMBreakPoint=FALSE` tests. `vm_5150` bed exists but does not boot (XT-CF ROM -> BASIC).
- **Network:** `ELNK3.VXD` multicast patch, shipped in `dist/post-install-fixes/`; web works on the 5160.
- **Video:** `TXTBENCH` - text ~81 us/char, glyphs cross the bus each time.
- **86Box (diagnostic tree, local):** window gating on port 670h bit 0; NMI injection; 3C509B filter log;
  Mach8 write counter.
- Issue status blocks current on #10, #35, #46.

## On the card now

`SHADRAM.VXD` v4, `SD120PPD.MPD` master-only, `ELNK3.VXD` no-multicast, `TXTBENCH.EXE` on `C:\`.
**OFFLINE boot entry** added (`CONFIG.SYS` and `AUTOEXEC.BAT`; backups `CONFIG.BSR`, `AUTOEXEC.BSR`).

## First things

1. **OFFLINE test boot:** pick OFFLINE, note any network-card message, `START PERFLOG X 30`. Expect
   ~200 KB less locked than FULL. Then one FULL boot to confirm the network comes back.
2. **Mach8 text driver (#49)** - `docs/mach8_text_and_driver_plan.md`, wider ideas in `docs/mach8_graphics_vision.md`: off-screen memory map, mono source
   from card memory, choose patch vs own driver (DDK XGA sample), find a 16-bit linker.
   Re-run `TEST.COM` (`M8UTL.ZIP`, Andrew on #49) on the 5160 from a DOS boot, photograph it: the
   reference for 86Box's Mach8. Not on the CF now; local copy in `COMrade_Latest/XT_5160_rework_claude/ATI/ATIMACH8/M8UTL/`.
3. Cimon's `WIN /D:X` / `SystemROMBreakPoint` results when they come; `vm_5150` boot (DOS boot floppy).

## Open, lower

86Box timing on bit 0 (why INBRDPC's diagnostic needs the slow phase); garbled mouse name in the
registry; slow right-click menu; #46 unplugged-drive driver refusal (LEAN covers it).

## Done later on 2026-10-05: three Inboard fixes for upstream 86Box (#7638)

Opened 2026-10-05 as 86Box/86Box#8216 (memory map) and #8217 (BIOS); **both merged by OBattler the same day**
(`e748927c9`, `6b09b25c6`). The owner posted the #7638 reply. #10 status block updated.
Write-up: `docs/inboard_memory_layout_2026_09_30.md` (last four sections). Skill technique 143.

- **`inboard-memmap`** (`842b7b82d`, 3 commits, `inboard386.c` +79/-5): the card's own 256 KB of
  extended memory; 670h bit 0 window gating; the BIOS window's exec pointer (the dynarec stall).
- **`inboard-1982-bios`** (`0d234a599`, `m_xt.c` +13/-17): the 08NOV82 ROM offered again. Only that
  ROM: the owner does not want to support ROMs not known to work.

Gates: G1 clean (no debug in either diff). G5 comments re-read. G6 claims hold for a stranger.
G7 both build; upstream moved to `501e717d0` with no change to the touched files. G8 minimal.
G9 last runs on the pushed HEADs (memmap exe 15:48:46 after its 15:48:32 commit; BIOS-only exe
15:54:53 after 15:23:43). G2 not run: the changes sit inside the Inboard device and its BIOS list,
so other machines cannot reach them - by construction, not by measurement. G3/G10: owner checks by
hand in a Qt build before opening (BIOS list shows 08NOV82, RAM box 1/3/5 MB, one boot). G4 n/a.

RonnyRoy's board is public (https://github.com/ronnyroy111/inboard386) and is linked in #8216. Follow-ups
from Andrew on #7638: the 128 KB read-only low view (E0000h vs our F0000/C0000) and the ROM-select bit
changing RAM wait states for all memory - check both against the netlist. SHADRAM v4 published (`dist/shadram/`).
