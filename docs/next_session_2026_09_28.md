# Next session - handoff from 2026-09-27 (evening)

## ▶ RESULTS in 2026-09-27 late (CF read on the host)

**XTOUT (#40), FastDoom `-timedemo demo1`, realtics:** set 46514 / clear 46002 / clear 46003 /
set 46516. Clearing XTOUT (`8C`) is 1.1% faster (512 tics = 14.6 s), run-to-run spread 1-2 tics.
Small but real. Next: the Windows gate on `8C`, then `CPUSET.BAT`. FastDoom does exit after
`-timedemo`: each run is ~22 min (46514 / 35 Hz), so no `-benchmark` rewrite is needed.

**VCACHE cap (#45 row 1), one boot each, seconds:**

| | A (no cap) | B (512-1024 KB) |
|---|---|---|
| PSP cold start | 26.04 | 23.51 |
| WinZip | 58.22 | 58.27 |
| 3 copies on C: | 7.03 / 7.03 / 7.46 | 6.92 / 6.59 / 6.26 |

Only the copies are hands-off: PSP needed Alt+F4 and WinZip a splash-screen click, so both are
discounted. The copy ranges do not overlap (mean 7.17 vs 6.59 s). **Owner kept the cap on.**
Future workloads for the RAM track must run without input. The cap is live in `SYSTEM.INI`; `SYSTEM.VC` is gone from the CF,
`SYSTEM.BVC` (the uncapped backup) is present.

**Floppy:** A: is 1.44 MB 3.5", B: 1.2 MB 5.25", both through the Sergey ROM, which loads on
the 5160 (only the emulator leaves it out). The old `FDTYPE.TXT` probe reporting 720 KB for both
is not trusted. Owner reports reads and writes feel slow in Windows and DOS mode; the timed B:
copy in `SNPTEST` is the first number.

## ▶ ON THE CF NOW: XTOUT clear and the SNP test (2026-09-27 late)

- `C:\CTCHIP\CPUSET.BAT` now writes `1000h:1 = 8C` (XTOUT clear); old file `CPUSET.B9C`.
  `dist/` still holds the `9C` version until the Windows gate passes on `8C`.
- `C:\SNPTEST.BAT <file on B:>`: BUSFLUSH x2 on `92`, then `1000h:0 = 8A` (SNP on, flush
  snooping off), BUSFLUSH x2, two floppy copies `FC /B` against a copy made on `92`, back to
  `92`. Timed floppy copy included. Results `C:\SNP.TXT` (ticks as the `0040:00F0` dump).
  No read-back of `8A` in the batch - CTCHIP is interactive; run `CPUSHOW` by hand if wanted.

## ▶ SNP RESULT (2026-09-27 late): `8A` hangs the machine - ruled out

`SNPTEST` log, `C:\SNP.TXT`:

| step | result |
|---|---|
| BUSFLUSH on `92` (XTOUT clear), ticks | 3090 / 10074 / 3092 / 9954 and 3090 / 10072 / 3090 / 9952 |
| B: copy on `92`, 400,880 B | 17.31 s = 23.2 KB/s (1.2 MB drive, ceiling ~45 KB/s) |
| `1000h:0 = 8A`, then `ECHO` to the log | completed |
| first BUSFLUSH on `8A` | never finished; keyboard dead, motor left on; power cycle |

`82` ran the same BUSFLUSH on 09-27, so the SNP bit is the difference. XTOUT clear leaves the
I/O flush unchanged, as expected for a port read. #40 registers are done bar MOVS Split (not
worth a boot); #40 closes after the `8C` Windows gate. `SNP92.TMP` (floppy DMA read on `8C`)
is byte-identical to `C:\DOS\MWBACKUP.HLP` (md5 `c585bad3`).

Floppy, owner: B: needs the disk ejected and reinserted before each use. A COMrade `dir_list B:\`
left DOS at Abort, Retry, Fail, which is why the later COMrade calls timed out.

## ▶ PLAN addition (owner, 2026-09-27): RAM footprint after bus and CPU

Once bus and cycle work is done: make Windows 95 as lean as it can be in 5 MB, so it pages less
to the XT-CF. Measure first (System Monitor page-ins and swapfile in use during `WLOAD`), then
trim. The VCACHE cap is the first item on this track.

## ▶ START HERE - three things waiting on the owner or others

1. **The CF.** Stop `XTAB` if it is still running (Ctrl+Break, or F10/Y in FastDoom), **reboot**
   (XTOUT may be left clear), then run `C:\VCTEST.TXT`'s steps. Bring back `BENCH.CSV`,
   `WLA`/`WLB`, `VCA`/`VCB`. Then rewrite `XTAB.BAT` with `-benchmark single demo1` and rerun.
2. **Cimon's reply** (#10): DEBUG DMA probe on his 5150 and 5160, `BOOTLOG.TXT` from the failing
   5150 boot, BIOS date, planar RAM, SW1/SW2. If DMA and CPU disagree, model 5150 planar RAM in
   the emulator and test workarounds there.
3. #10 is active, not parked: the old "parked idea" status block was removed 2026-09-27.

Also open: the owner can run the same DEBUG probe on the 5160 (answers ledger item E5c).

## ▶ #10: early BIOS revisions (2026-09-27) - `docs/issue10_old_bios_2026_09_27.md`

- The 1982 XT ROM boots Win95 to the desktop on the Inboard in the emulator; the
  "1982 ROM incompatible" claim was a shadow-window bug. README corrected.
- Five BIOS-service differences applied to the 1986 ROM do not stop Win95.
- The 5150 ROM also reaches the desktop in the emulator once SW2 reports <=640 KB, so the BIOS
  is ruled out as far as the emulator models it. Leading suspect: 5150 planar RAM (not
  modelled). Cimon asked for the DEBUG DMA probe + BOOTLOG; the owner can run the probe on the 5160.
- Diagnostic 86Box: branch `diag-issue10` in `86box_3c509b` (local, not pushed), build `build_log`.
  `INBOARD_OLDBIOS` bitmask; the Inboard machine also offers `ibm5160_1501512_5000027` and
  `ibm5150_1501476`. `vm_3c509b` is back on `ibm5160_050986`.
- Cimon's message sent by the owner. #10 status block posted 2026-09-27 (Cimon not named).
- XTAB did not finish: FastDoom 1.2 appears not to exit after -timedemo. Rewrite with
  `-benchmark single demo1` once the CF shows how many runs completed. Reboot after
  interrupting it - XTOUT may be left clear.

## ▶ NOW: #40 XTOUT, measured with FastDoom

`1000h:1` bit 4 (`XTOUT`, "Wait for Ready after Output"): the CPU stalls after every OUT for
the whole bus cycle. feipoa: set costs DOOM realtics. FastDoom's Mode Y renderer issues OUTs
every frame, so it is the benchmark. Sound off - see the parked item below.

1. At the DOS prompt: `CD \GAMES\FDOOM`, then twice:
   `FDOOM -xt -nosound -iwad DOOM1.WAD -timedemo demo1 -csv` (XTOUT set, `9C`).
2. `CD \CTCHIP`, `CTCHIP34 IBM486 /1000h:1=&10001100` (`8C`, XTOUT clear). Same two runs.
3. A reboot restores `9C` via `CPUSET.BAT`. Only if 8C is faster: the Windows gate, then make
   it permanent in `CPUSET.BAT`.

## On the CF now (owner has it, 2026-09-27)

`C:\XTAB.BAT` (XTOUT A/B, set / clear / clear / set, results in `C:\GAMES\FDOOM\BENCH.CSV`),
then `C:\VCTEST.TXT` (#45 row 1: `SYSTEM.VC` capped, `SYSTEM.BVC` backup; results `VCA.TXT`,
`VCB.TXT`). The heavy workload is still the owner's pick; if it runs from a DOS prompt, time it
with `TIME` rather than a stopwatch. CF root tidied: 369 test files archived to the owner's
`XT_project\cf_root_archive_2026-09-27` (hash-verified, list in `_ARCHIVED_FILES.txt`), 53 kept.

## SW1 and flush snooping: ruled out as the I/O flush (2026-09-27)

`BUSFLUSH` ticks (no I/O / with I/O / no I/O / with I/O):

| | |
|---|---|
| 09-21, SW1 as fitted, `1000h:0 = 92` | 3092 / 10072 / 3090 / 9956 |
| switch 1 flipped, full power cycle, `92` | 3090 / 10076 / 3092 / 9956 |
| switch 1 flipped, `82` (bit 4 flush snooping clear) | 3094 / 10092 / 3088 / 9956 |

Neither SW1 nor bit 4 changes it. What still flushes on I/O is unknown; inferred, not measured:
if the Inboard holds the CPU off the bus for each XT cycle, the module sees it as DMA and flushes
in either SW1 position. No register lever is left for it. (The `82` write is shown only by
CTCHIP's own print, which is not a read-back.) FastDoom ran in both positions with sound off.
Switch 1 is back as fitted.

**Parked:** FastDoom with SB sound freezes (Apogee Sound System); `FDSETUP` has other SB
options. Not yet checked with the switch back as fitted. Owner: later, not a priority.

## LS-120 timer A/B (#45 row 6) - ruled out, closed by the owner

1 MB copy to the LS-120, seconds: default tick 29.44 / 43.67 / 29.33 and 31.42 / 30.21 /
37.35 (mean 33.57); 1 ms tick 30.97 / 34.33 / 35.32 (mean 33.54). `TIMERRES` held 1 ms at the
start (2983 us) but not at release (9391 us), so B is void by the pre-agreed rule - and even
taken as read it shows no gain against a 29-44 s spread in the default runs. No rerun.

## Done this session

- 5160 results recorded (`docs/t130_mpd_review_2026_09_26.md`): IRQ 3 matches IRQ 5 on the
  third copy (14.28 vs 14.17 s); the P2 "1 ms" arm is void; a 1 ms tick costs 15% CPU; the real
  ~10 ms tick explains the IRQ's gain (and a 1.44 -> 4.32 s arithmetic slip was corrected).
- #7: the 5160 shows WAIT86 does no harm; HSFLOP.PDR keeps the fix unexercised there
  (`docs/issue7_setup_stall_2026_09_26.md`). **Closed** on the 86Box proof, owner's call.
- #42: status block, comment to Andrew with thanks, **closed**. #45: TIMERRES results posted.
- `FIXES.md` lists the VPICD IRQ 2 patch, `WAIT86.COM` and `T130-XT-IRQ3.INF`.
- `TIMERRES HOLD` measures again at release (`bfc4dd1`); rebuilt, md5 `93c62854`, on the CF.
- #46 opened: the unpowered LS-120 stalls boot ~2 x 40 s in `SD120PPD.MPD` init (892 ticks in
  `BOOTLOG.TXT`, 00:02 boot). Plan: find the wait by disassembly, give up on `80h`.
- #36 fixed (`4c35149`): `fatls`/`fatcp` read FAT12 and unpartitioned floppies.

## Posted after midnight, owner-approved

- #14 closed as not reproducible. #29 item 1 answered in practice (clean SB audio; buffer
  address not read back - the 8237 is write-only); items 2-4 open. #45 row 5 corrected: the
  machine is on Network server and stays there unless an A/B says otherwise.
- #10 stays open (owner: it widens the project to early 5160s).

## Optimisation order - agreed with the owner, not started

1. **SW1 on the CPU module** (#40) - in progress, see the top. Every I/O flushes L1, 90.7 us
   per access against 5.7 us of bus (`docs/captures/io_cache_flush_2026_09_21.md`). Not "with
   SNP": the book says flush snooping (`1000h` bit 4) is used only with SNP (bit 3) clear.
2. #40 registers: **`XTOUT` clear first** - it is "Wait for Ready after Output" (van Gilluwe,
   §8c): the CPU stalls after every OUT for the whole bus cycle. Same gate as SW1. `1000h:2 = 00`
   is correct (test and power bits): dropped. LMROR only affects writes to ROM: low value.
3. #45 row 1: cap VCACHE on 5 MB; time a paging-heavy workload.
4. #34 display mode (Mach8 is the biggest bus user). Owner's comfort call.
5. #45 row 2: auto insert notification off on the changer LUNs and CRW (idle polls, each a flush).
6. #33 DRAM refresh - a memory test that can fail comes first.
7. #41 poll pacing - re-rank after SW1. 8. #31 SCSI caches. 9. #29 items 2-4.

Also open: #46 (LS-120 boot stall), the IRQ 3 warm-up rerun (a metric only; not gating anything).
