# Next session - handoff from 2026-09-27

## ▶ #10: early BIOS revisions (2026-09-27) - `docs/issue10_old_bios_2026_09_27.md`

- The 1982 XT ROM boots Win95 to the desktop on the Inboard in the emulator; the
  "1982 ROM incompatible" claim was a shadow-window bug. README corrected.
- Five BIOS-service differences applied to the 1986 ROM do not stop Win95.
- The 5150 ROM loops in POST in the emulator (emulator gap; a real 5150 boots). NEXT: log each
  reset's cause, and one boot with plain VGA instead of the Mach8.
- Diagnostic 86Box: branch `diag-issue10` in `86box_3c509b` (local, not pushed), build `build_log`.
  `INBOARD_OLDBIOS` bitmask; the Inboard machine also offers `ibm5160_1501512_5000027` and
  `ibm5150_1501476`. `vm_3c509b` is back on `ibm5160_050986`.
- Owed: a Cimon message (BOOTLOG.TXT, BIOS date, planar RAM/switches) and a #10 status block,
  both drafts for the owner.

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
