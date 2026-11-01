# Next session - from 2026-11-01 (day)

Supersedes `docs/next_session_2026_11_01.md`; its open items 4-6 and the boot stall still stand.
Card log: `docs/captures/2026-11-01_card/m8_card_bench.txt`.

## State: at or above ATI in all 35

master `58c8237`. Card: `M8POLL.DRV` selected (= master).

ATIM8 was benched again today (`card_atit1/2`, installed as ATIM8T.DRV): within a few percent of the
10-20 runs, so the baseline holds. Against it, master's runs 2-4 after a reboot (`card_poll2-4`):
every test's mean at or above ATI, pixels = ATI in all 35. fill_large is level (0.998, 221.6-223.3
against 222.8): both drivers run at the engine's fill rate. Geometric mean ~1.8x.

Judge on runs 2+: the first run after a reboot reads low in tests no change touched (text_varied 0.97
in run 1, 1.08-1.10 after). Two runs are not enough to call a 2-3% difference: the same driver moved
`lines` 1606 -> 1197 and fill_small 657 -> 543 between consecutive runs. Use 4 and drop the first.

## Done today (all card-benched, pixels = ATI in all 35)

| commit | change | effect |
|---|---|---|
| `0e66ded` | Pixel and BitBlt short paths in aligned assembly (`pixel.asm`, `blt.asm`); MAJ/MIN written only when the size changes (`M8_MM_FORGET`) | setpixel 1.05-1.09; 2 of 6 writes off each setpixel / repeated fill |
| `d26ed72` | RealizeObject skips M8PXREALIZE except bitmap deletion and BS_PATTERN | fill_small 0.95 -> 1.01-1.02; lines_dash ~1.02 |
| `58c8237` | Short paths poll one EXT_FIFO_STATUS bit for room instead of draining empty (ATIM8's way) | dstinvert 0.97 -> 1.03; fill_large 0.985 -> 1.00 |
| `e4b136e` | `M8MAJMN` probe: fills, blits, LINEDRAW keep MAJ/MIN; SCAN_TO_X changes both | basis for the shadow |
| `f95c834` | Card cycle: card's C:\ root was full (512 entries, one .REG per driver); .REG files now in C:\V9XREMOTE\REG | uploads failed with Win32 5 / 82 |

## Disproved / closed (one line each)

- Pixel in assembly alone: no gain (2139/2039); GDI is ~385 of ~485 us a setpixel. The port writes were the lever.
- FIFO room via C (`m8_fifo_room`, 16-bit scan, C fallback): fill_small worse (551/609); the inline one-bit poll is what paid.
- dstinvert / fill_large are engine-bound: CPU-side trims cannot pass the engine's rate; only overlap could, and now does.

## Next

1. **Owner's call: the aspirational track** - DirectDraw (both halves), modes above 256 colours, 3D, unused silicon
   (`docs/mach8_levers.md`, `docs/mach8_unused_silicon_audit.md`).
2. Retrospective across all 35 (owner, 11-01): today's three levers - assembly short path, register shadow,
   FIFO-room poll - checked against every other path. Known candidates: the C fast paths still drain empty
   (`m8_hw_fill_fast`/`copy_fast` callers other than the asm, line_fast, spans); `rectangle` and `fill_hatch` do
   their own writes of MAJ/MIN; text's mono expansion idle wait (11-01 handoff item).
3. M8BENCH warm-up pass, so run 1 counts (ATI must be re-benched with the same build).
4. The C:\M8IDLE.EXE lock seen after boot 147 (the upload rename failed) was most likely the full root; confirm next cycle.
