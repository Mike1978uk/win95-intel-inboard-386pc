# Next session - handoff from 2026-09-30 (supersedes `next_session_2026_09_29c.md`)

## ▶ START HERE

**At the 5160 (only two items left from the checklist):**
1. SAFE from the boot menu - reaches the Safe Mode desktop? (technique 137, untested).
2. Optional, hands-off: #45 rows 3-4 (read-ahead slider, CD-ROM cache A/B).

The card was last booted to DOS after a LEAN Windows boot, so `SD120PPD` and `NEROCD95` are `.OFF`
until FULL runs. `CONFIG.SYS` is the menu version with `NODIAGS NOPAUSE` (`CONFIG.MNU` = same;
`CONFIG.B29`/`AUTOEXEC.B29` = pre-menu pair).

**At the desk:** #46 (patch the wait in `SD120PPD.MPD` - FULL always stalls with the drive
unpowered; LEAN avoids it), then #41 (`T130.MPD` A18, `ELNK3.VXD` A21, both bed-testable).

## Results since 09-29

| finding | where |
|---|---|
| Boot menu works on the 5160: FULL and LEAN confirmed; SAFE untried | `docs/ram_baseline_2026_09_28.md` |
| LEAN: -0.09 MB locked, -11% page-ins vs FULL. Dial-Up gone from both boots; its own saving not separable from the 800x600 change | same, runs 3-4 |
| Inboard 5 MB accounted: 640k conventional + 4352k extended, 0k bad; 128 KB reserved (F000 shadow in use, EGA half idle) + 64 KB under the planar. Windows shows 5.0 MB | `docs/inboard_memory_layout_2026_09_30.md`, #35 |
| Planar is **64 KB** (bank 0 of a 64-256 KB board); Inboard serves 576 KB | memory `inboard-backfill-switches` |
| XT-CF is PIO only; DMA 3 is the parallel card's (DREQ3 pending in the status read) | #29 closed |
| Refresh divisor 18 (BIOS default), refresh DMA running | `test_harness/dmainv/RESULT_2026-09-30.md`, #33 |
| Cimon's 5150 dies at the switch to protected mode, but also runs Future Domain SCSI drivers (own `INT13.386`); asked to retest without them | `docs/issue10_old_bios_2026_09_27.md` §7, #10 |

## GitHub

- Posted: #29 reply + closed; #35 reply; status blocks on #10, #33, #35. Ledger: Andrew told on
  #29 and #35.
- Waiting: Cimon's 5150 retest. red-ray (SIV): nothing sent; correspondence stays outside git.
- `README` open-issues table matches GitHub.

## Traps

- COMrade `file_write` text mode writes LF only - DEBUG hangs at EOF. Write CRLF from the host.
- `PERFLOG` appends to `PERFLOG.CSV`; split runs at the header rows before `perflog_table.py`.
- A LEAN boot still logs `Init Failure sd120ppd.mpd` (registry names it; 1 ms, harmless).
- `inboard386.c:53` and `:88` disagree on how the shadow alias address is derived - alerted, not
  fixed.

## Commits

`96f2342` .. this handoff, all pushed.
