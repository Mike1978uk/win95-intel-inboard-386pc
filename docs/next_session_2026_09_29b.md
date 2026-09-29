# Next session - handoff from 2026-09-28 evening (supersedes `next_session_2026_09_29.md`)

## ▶ START HERE

**At the 5160:** work down `docs/5160_checklist_2026_09_29.md`, items 1-9 plus 7a, one change per
boot. Its last section scopes every open item by where it runs.

**Without the 5160:** #46 first (find and patch the wait in `SD120PPD.MPD`; only the final check
needs the machine), then #41 (`T130.MPD` A18, `ELNK3.VXD` A21 - both can run in the bed).

## What this session found

| finding | evidence | where |
|---|---|---|
| System Monitor's "locked" includes the disk cache (r = 0.97). Outside the cache ~1.4 MB is locked | PERFLOG capture of 09-28 | technique 136, `docs/driver_audit_2026_09_28.md` |
| Driver files account for ~0.8 MB of that 1.4 MB; network is the biggest group (350 KB) | LE headers, `VMM32.VXD` unpacked to W3 | `docs/driver_audit_2026_09_28.md` |
| Dial-Up Adapter (PPPMAC + SPAP) locks 109 KB and no modem is planned - **first trim** | same | checklist item 3 |
| NetBIOS stays (drive mapping planned). Plotter and printer go by FTP/netcat, no resident driver | owner | memory `planned-peripherals-2026-09-28` |
| `EGACACHE` never shadowed `C0000`: it copies the video ROM into 32 KB of resident memory and repoints `INT 10h`. The 09-20 A/B timed the wrong thing. Stays off | static read of `INBRDPC.SYS` | `docs/egacache_static_read_2026_09_28.md`, #35 |
| `INBRDPC.SYS` keeps only 3,744 bytes resident - nothing to shrink; a VxD hand-off gains little | same read, end address `0EA0h` | `docs/driver_audit_2026_09_28.md` |
| F5 Safe Mode skips `INBRDPC.SYS`, so it cannot work here; the boot menu's SAFE entry (`WIN /D:M`) is the route | reasoning, untested | technique 137 |
| Sergey's floppy BIOS is fitted in the 3C509B's boot-ROM socket - keep the `D0000` window enabled | owner | skill ROM-scan note, #35 |
| ~~DMA channel 3 looks free and the XT-CF rev 3 can use it~~ - corrected 2026-09-29: the XT-CF has no DMA logic and channel 3 is the parallel card's | owner checked both cards | memory `xtcf-no-dma-channel3-parallel-2026-09-29` |

Ruled out, do not re-propose: Above Board EMS as a paging tier, a second COM port, the SCSI
WRITE BUFFER "sink", swapfile on another disk. All four full-length slots are taken and IRQs 2-7
are allocated. Memory `slot-layout-and-above-board-ruled-out-2026-09-28`.

## Built

- `tools/perflog/SLEEP.EXE` - replaces RAMBASE's busy `CHOICE` waits. On the CF with the new
  `RAMBASE.BAT` (old kept as `RAMBASE.OLD`). First real run is checklist item 4.
- `tools/le_locked.py` - locked, pageable and init-only bytes per VxD or W3 monolith member.
- `tools/bootmenu/` - `CONFIG.SYS` and `AUTOEXEC.BAT` with FULL / LEAN / SAFE, and `mkmenu.py`
  that rebuilds them from the CF's current pair. LEAN and SAFE rename `SD120PPD.MPD` and
  `NEROCD95.VXD` to `.OFF`; FULL renames them back. Untested; the owner applies it (item 7a).

## On the CF

- Owner deleted the retired `LS120MP.*` and the rollback copies (`XTIDEMP.B20/.ORG/.PB`,
  `SD120PPD.B4F/.EPF/.MP_`, `PORT.PD_`); all backed up. Every file the boot and the checklist need
  was checked present afterwards.
- New: `SLEEP.EXE`, `RAMBASE.BAT` (SLEEP version), `RAMBASE.OLD`.
- Not yet on it: the boot-menu files.

## GitHub

- Status blocks posted or replaced: #23, #28, #29, #31, #35. New #48 (optimisation timeline).
- Reply to @andrew-hoffman on #35 posted at the owner's request; ledger marked told.
- No outside comments since 09-24 (#42). 86Box #8076 and discussion #6447 unchanged.

## Skills

- `inboard-hw-debug`: techniques 136 (locked includes the cache), 137 (Safe Mode route),
  138 (boot-menu batch traps); `EGACACHE` section rewritten with the answer.
- `repo-hygiene`: keep an issue's status block current in the same session as the comment that
  changes it, plus a sweep command.

## Waiting on

- Cimon on #10: replied 09-29 (doc §6); asked again for his 5150 BIOS and `BOOTLOG.TXT`.
- red-ray on SIV Hope-11: analysed 09-29, owner to reply; notes kept outside git (memory).
- GLaBIOS bed run for #10 (Andrew's suggestion): parked.
- Owner: whether a Win32 `nc.exe` runs on Windows 95 (for the plotter and JetDirect).

## Commits this session

`dda6371` .. `e7ebdc9`, sixteen commits, all pushed.
