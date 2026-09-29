# Next session - handoff from 2026-09-29 late (supersedes `next_session_2026_09_29b.md`)

## ▶ START HERE

The CF is at the desk (`D:`). Before it goes back in: `chkdsk D: /F` from an admin prompt, answer
**Y** (8 lost chains, 256 KB, from a forced power-off). Then at the 5160, in order:

1. Command prompt only, `DMAINV` - writes `C:\DMAINV.TXT` (read-only 8237 + PIT ch1 probe).
2. Boot FULL. Network panel: Dial-Up Adapter gone? Reboot FULL, then `RAMBASE B` first thing.
3. Boot LEAN, LS-120 powered off - is the ~80 s stall of #46 gone? Note the boot time.
4. Boot FULL, LS-120 on - it mounts as before.
5. Boot SAFE - reaches the Safe Mode desktop.
6. Optional: #45 rows 3-4.

Then bring the CF back: read `DMAINV.TXT` and each `BOOTLOG.TXT`.

## What this session found

| finding | evidence | where |
|---|---|---|
| Item 1 done: `INBRDPC` 3,760 bytes resident; video ROM `55 AA 40` | `MEM /C`, COMrade memory read | `docs/captures/mem_c_5160_2026-09-29.txt` |
| RAMBASE run 2 (800x600) ran, but Dial-Up still loaded on that boot | `BOOTLOG.TXT` 22:19 has `PPPMAC`/`SPAP` | `docs/ram_baseline_2026_09_28.md` Run 2 |
| `SLEEP.EXE` works: CPU 0-2% between steps, was 100% | PERFLOG | same |
| Client for Microsoft Networks is not installed (no `VREDIR`); drive mapping will need it | both boot logs | memory `planned-peripherals-2026-09-28` |
| **XT-CF is PIO only**: no DMA jumper, no DRQ/DACK logic. XT-CF DMA closed | owner checked the card | memory `xtcf-no-dma-channel3-parallel-2026-09-29` |
| DMA channel 3 is the LS-120 parallel card's (DMA 3 / IRQ 7) | owner | same; #29 closed |
| PIT channel 1 samples 4, 12, 8: consistent with the BIOS divisor 18 (#33 step 1), not proof | COMrade port reads | `DMAINV` repeats it with 32 samples |

## Built

- Boot menu (FULL / LEAN / SAFE) **installed on the CF**. Old pair kept as `CONFIG.B29` /
  `AUTOEXEC.B29`. Two menus at boot now (Windows Startup Menu, then this one); `BootMenu=0` in
  `MSDOS.SYS` would drop the first - left for the owner.
- `test_harness/dmainv/` - `DMAINV.BAT` / `.SCR`, on the CF.

## Traps hit

- COMrade `file_write` text mode writes **LF only**: a DEBUG script hung at EOF and needed a power-off.
  Long `run_command` lines also lose keystrokes on the 5160. Write CRLF from the host; type a short
  batch name. (Memory `reference-comrade-link-topology`.)
- `PERFLOG` appends to `C:\PERFLOG.CSV`; reusing a RAMBASE label overwrites `RB<label>1/2.TXT`.

## GitHub

- #29 closed: reply to @andrew-hoffman posted (channel 3 confirmed, XT-CF PIO only), status block
  replaced, ledger marked told.
- **Owed, owner's wording:** #35 - @andrew-hoffman asked 09-29 04:25 how the shadow region and the
  5 MB are laid out; unanswered. #10 - status block (09-27) predates Cimon's real-hardware result
  (08NOV82 ROM runs Windows 95 on his 5160); needs replacing.
- Red-ray (SIV): owner deleted the draft reply; nothing sent. Correspondence stays outside git.

## Commits this session

`96f2342` .. this handoff. Not pushed.

## Update, 2026-09-30 early

Done since: `chkdsk` (8 old DEBUG chains, deleted); memory-report boot (`docs/inboard_memory_layout_2026_09_30.md`,
#35 answered and its status block replaced); FULL + `RAMBASE B`, LEAN + `RAMBASE C`
(`docs/ram_baseline_2026_09_28.md` runs 3-4: LEAN -0.09 MB locked, -11% page-ins; Dial-Up gone from
both boots); `CONFIG.SYS` restored to `NODIAGS NOPAUSE`; #10 status block replaced; Cimon asked to
retest without the Future Domain drivers.

Still open at the 5160: `DMAINV` (Command prompt only), a SAFE boot, the System Properties RAM
figure (expect 4,992 KB), #46 with the LS-120 unpowered on FULL. Last boot was LEAN, so
`SD120PPD` and `NEROCD95` are `.OFF` until FULL runs.
