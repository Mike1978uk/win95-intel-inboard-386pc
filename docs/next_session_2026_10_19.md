# Next session - from 2026-10-18

Supersedes `docs/next_session_2026_10_18.md`. Worklist first item is this plan.

## Done 10-18

- 86Box PR #8394 merged (Mach8 data-available fix, Win 3.0 Solitaire hang); report
  `docs/mach8_data_available_2026_10_18.md`. Contributors told (ledger).
- 86box-mcp installed, working natively on Windows; `wait text` stall fixed upstream by dafugg (#8396),
  retested. Automation build `86box_auto/` = master `5468d5952`. `docs/86box_mcp_on_windows.md`.
- Item 1+2 of the ATI plan: uploads, mono and read-back by `rep outsw`/`rep insw`; read-back polls once
  (`ec1d733`). M8BENCH `strio` run (`docs/captures/2026-10-18_strio/m8bench_strio.txt`): pixels unchanged
  in all 30 tests; scr_to_mem 8.2x (now 0.76 of ATI); stretch/mono/dash 1.2-1.8x; **bmp_card unchanged
  (10/s vs ATI 447)** - its cost is our hash/cache, not the port.
- M8BENCH numbers its tests: taskbar `M8BENCH n/35 name`, a `started n/35 name` line before each, `#n` on
  each result (`476db12`). New exe is in the bed image (`C:\M8BENCH.EXE`); the exe is gitignored, build
  line in the source header (Open Watcom at `C:\Users\lycet\tools\openwatcom`, quote each arg in PowerShell).

## Numbered compare (10-18 evening, `docs/captures/2026-10-18_strio/m8bench_numbered.txt`, runs ours2/ati2)

Owner watched both: tests 1-30 run on both drivers. 10-12 (blit_small, blit_large, scroll), 22 (scr_to_mem)
and 30 (getpixel) show nothing on either - they leave a blank area by design (hash 0d2b3285 on both). 24
(drag_frame): ATI visibly draws the frame line, ours does not show one (5/s vs 112/s; final pixels equal).
31 polygon faults under ours; ATI runs 31-35. Pixels differ only in 27 lines_dash. Ours slower in all 28
timed-and-compared tests; engine paths 0.86-0.96 of ATI, pixel paths 0.02-0.24, scr_to_mem 0.76.

## Next, in order

1. **The DIBENG fault** (handoff 10-18 item 5): M8BENCH faults at test 31 `polygon` under ours, in
   DIBENG.DLL `0006:1397`, DS=`15E7`, writing `[edi]` (ES=03B7, EDI=34725). Same selector as 10-17. It
   stops tests 31-35 (polygon, rectangle, floodfill, cursor_move, ellipse) from ever running on ours.
   Cheap first step: which code owns selector 15E7 (our strip device? a freed DIB header?). Screenshots
   `vm_5160_now/screenshots/Monitor_1_20261018-1657*.png`.
2. **Cheap 32-bit hashing** for the string and bitmap caches (worklist first item (4)): bmp_card and
   text_sys are where we lose (0.02 and 0.09 of ATI). Then held bitmaps (proxies) back on.
3. **Brushes and dashed lines on PATT_DATA** (fill_hatch 0.02, lines_dash 0.04 of ATI; lines_dash also
   differs in pixels from ATI's).
4. Re-run M8BENCH under ours after each; ATI's run needs re-running once (the bench now numbers tests).

## Bed

`vm_5160_now` on the `86box_bisect` build (the build all M8BENCH runs used; keep it for comparability):
`tools/bed_launch.ps1 -Exe 86box_bisect\build\src\86Box.exe -VmPath vm_5160_now -AllowMouse -Log ...`.
Image holds our PLAIN build with string I/O. Driver switch: `C:\ATIM8.REG` / `C:\M8DISP.REG` (F8, command
prompt, `REGEDIT`). Results: `python -I tools/fatls.py vm_5160_now/card.img --get M8BENCH.TXT <out>`, then
`tools/m8_bench_compare.py <file> <labelA> <labelB>`.

MCP (86box-mcp) is ready but not used on this bed yet: it rewrites `86box.cfg` (use a copy) and needs the
`86box_auto` build, so a bench on it is not comparable with the stored runs.
