# Next session - handoff from 2026-09-27

## ▶ NOW: SW1 on the CPU module, case open (#40)

The switches read transposed: they look `1 OFF, 2 OFF, 3 ON, 4 ON` and act as `1 ON, 2 ON,
3 OFF, 4 OFF` (FPU works; I/O flush measured). Sources: `resources_and_sources.md` §8c. The
test does not depend on that reading: **flip switch 1 to its other position** and let
`BUSFLUSH` say which position flushes on I/O.

feipoa: *"cannot even run DOOM"* on IBM systems without SW1 ON; his 2025 post calls OFF
optimal. So DOOM is the gate. Both positions flush on DMA, so the risk is that the module's DMA
detection misses the XT's DMA and the I/O flush was covering for it - hence floppy and sound.

1. **Baseline, SW1 as now.** Boot to DOS. Vanilla DOOM loses the keyboard on this machine, so
   use FastDoom with its XT keyboard switch: in `C:\GAMES\FDOOM`,
   `FDOOM -xt -iwad DOOM1.WAD -timedemo demo1 -csv` - appends gametics / realtics / fps to
   `BENCH.CSV`. Note anything visibly wrong. (The 2026-04-13 row predates the CMLR fix: not a
   baseline.) Sound is AdLib, so DOOM does not exercise DMA; step 3 does. Then `BUSFLUSH` with
   `COMRADE` up; results read over COMrade as on 09-21.
2. **Power off, flip switch 1, power on.** Same FastDoom timedemo, same `BUSFLUSH`.
3. If DOOM runs clean and `BUSFLUSH` shows the with-I/O passes near the no-I/O passes, boot
   Windows 95 and run the gate: floppy copy + FC, a WAV through the SB, a network copy, Zip
   copy + FC. Any failure: flip back.
4. Record both DOOM results and both `BUSFLUSH` tables.

Then #40 registers (item 2 below), starting with XTOUT.

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
