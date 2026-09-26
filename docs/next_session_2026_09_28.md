# Next session - handoff from 2026-09-27 (after midnight)

## ▶ NEXT SESSION START

The owner brings the CF back from the LS-120 timer A/B (`C:\LS120TMR.TXT`). Read first:
`LSA1.TXT`, `LSB.TXT`, `LSA2.TXT` (the 1 MB copy to the LS-120 at default / 1 ms / default) and
`TIMERRES.TXT`. **Run B counts only if both `Sleep(1)` lines in `TIMERRES.TXT` read ~2000 us**
(the start check and the new at-release check). The 09-26 file is kept as `TIMER926.TXT`.
The 1 ms tick costs 15% of the CPU while held, so B has to beat that to be worth keeping.

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

1. **SW1 on the CPU module** (#40): every I/O flushes L1, 90.7 us per access against 5.7 us of
   bus (`docs/captures/io_cache_flush_2026_09_21.md`). Likely most of the 15% tick cost -
   inferred. Try OFF ("DMA only"), maybe with SNP enabled; re-run `BUSFLUSH`, then a correctness
   gate (floppy copy + FC, sound, network, Zip copy + FC). Case open: the owner's hands.
2. #40 registers: `XTOUT` clear (feipoa), `LMROR`, `1000h:2`. One CTCHIP write, one boot each.
3. #45 row 1: cap VCACHE on 5 MB; time a paging-heavy workload.
4. #34 display mode (Mach8 is the biggest bus user). Owner's comfort call.
5. #45 row 2: auto insert notification off on the changer LUNs and CRW (idle polls, each a flush).
6. #33 DRAM refresh - a memory test that can fail comes first.
7. #41 poll pacing - re-rank after SW1. 8. #31 SCSI caches. 9. #29 items 2-4.

Also open: #46 (LS-120 boot stall), the IRQ 3 warm-up rerun (a metric only; not gating anything).
