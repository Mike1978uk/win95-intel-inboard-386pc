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
