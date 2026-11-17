# Next session, after 2026-11-15

Earlier: `docs/next_session_2026_11_16.md`. Newest wins.

## State

- **Card (5160):** boot 404+, display driver `m8nof.drv` (`32bfaac`), `SWAPZF7`, XT-CF `9C64B4D1`. Safe at the
  desktop. **Run no DirectDraw program on it** until task 1 is done: a blue screen follows (below).
- **Mach8 bed `vm_5160_mcp`:** off. Driver on its disk is the "8 bpp refused" test build, not a commit:
  reinstall `3aaea3a` before using it (`build.ps1`, then WININIT rename of `M8D3GG.DRV` and `M8DD32.DLL`).
- **Debug bed `vm_5160_dbg`:** a copy of `vm_5160_mcp` (cfg, card.img, nvr, roms), for task 1. Untracked.
- **Git:** master `2fd140a`, pushed, clean.

## Done 2026-11-15

| item | result |
|---|---|
| swapcomp A/B, 4 fresh boots | SWAPZF7 126.5 s vs SWAPZFN 126.8 s; 72 % fewer raw reads. Card keeps ZF7. `dist/` updated (`3d7d3a5`) |
| XT-CF in `dist/` | `9C64B4D1` = `2e17fc5` rebuilt **into `build\`** (the code hash includes the output path) (`801047f`) |
| PERFLOG reader | each row read against its own header (`1c70f36`) |
| 8 bpp shadow pinned 620 KB after a round trip | freed (`33b1104`); bed 0.63 -> 0.90 |
| Overlays at 16 bpp | mosquito, ddovrly run on the bed (`074bfa2`); merged in RAM |
| flip2d | fill keeps the Lock shadow (`760f004`), Unlock sends the changed span (`3aaea3a`); bed ~2.5x |
| Bed pictures at 16 bpp | `tools\bed_launch.ps1 -Automation <port>` + `tools\bed_shot.ps1` |
| X14 rerun | `docs/captures/2026-11-15_x14_ddraw_bed/` |
| Blue screen | `docs/directdraw_raise_bsod_2026_11_15.md` |

## Tasks, in order (Opus; commits by Haiku)

1. **Catch the RaiseException.** The card's and the bed's blue screen `0E at 0137:BFF9A07C` is KERNEL32's
   RaiseException (entry `BFF9A013`, not `BFF9A00E` as first written; dispatcher `BFF76588`); not the depth change, not the
   shadow free. Steps:
   - 86Box with the GDB stub: `86box_auto\build_gdb\src\86Box.exe` (built 11-15 from the same working tree
     as the bed's exe, `-DGDBSTUB=ON`; check `86box_auto\build_gdb.log` ends `exit 0`). The MCP server
     is pinned to the non-debug exe (`BOX86_BINARY` in `~/.claude.json`): drive this one directly.
   - `tools\bed_launch.ps1 -Exe 86box_auto\build_gdb\src\86Box.exe -VmPath vm_5160_dbg -Automation 9891`;
     the stub listens on 12345 (`gdbstub_port` in the cfg to change).
   - `C:\msys64\mingw64\bin\gdb.exe`: `target remote :12345`, `hbreak *0xbff9a013`, on each hit print
     `x/6wx $esp` (return address, code, flags, count, args). Legitimate handled raises also hit; log all.
   - In the same run, turn the HAL trace on (`C:\M8D3D.INI` `[diag] trace=1`, read once per boot): the last
     lines of `C:\M8TRACE.TXT` give the DirectDraw step in progress at the raise.
   - Sequence that reproduced it on the bed: wormhole (refused 8 bpp, needs the "refused" build) or any
     8 bpp round trip, then flip2d, then mosquito. The last raise before the blue screen is the one.
   - The return address names the caller. If it is in `M8DD32.DLL` (shared arena, every DirectDraw
     process) or its Watcom runtime, the fix is ours. Write the result into the doc above.
2. **Fix, bed, then card.** On the card: `tools\m8cardscripts\card_dd_roundtrip.ps1 -Build <folder>` (bench,
   wormhole, 5 minutes of pings via `card_wh_probe.ps1`, bench). Owner restarts a hung card.
3. **Then on the card:** overlays (mosquito, ddovrly), flip2d (fill + span), measured; optimise the overlay
   merge (only changed rows; asm if the timing shows it). Owner wants every demo running *well*, not just running.
4. **Then:** fswindow (dialog body not drawn), donuts (silent at 30 s), Doom 95, X16 DOS 3D APIs.

## Owner jobs

- #10: `C:\PICCHK.COM` from a DOS boot (SAFE), `PICCHK /T` must show `mask_err=65535`; then Opus drafts the
  reply to Simon with the program.

## Learned 2026-11-15

- **Win95 DirectDraw takes the primary's depth from GDI.** Keeping GDI at 16 bpp under an 8 bpp program
  gives the program a 16 bpp primary (`tools/m8probe/M8DDMODE.C`, each call's result). Not a route.
- **The bed prices port I/O at ~1 us against the card's 7.6 us:** bus-byte savings show only on the card.
- **One BSOD address can be read offline:** pull the DLL from the bed image (`tools/fatls.py --get`), map
  the offset to exports, disassemble with MSYS2 `objdump`. Took minutes; do it first next time.
- Bed `vm_5160_mcp` reproduced a card-only-looking fault once the same programs ran: try the bed first.
