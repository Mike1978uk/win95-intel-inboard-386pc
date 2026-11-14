# Next session, after 2026-11-14 (night)

Earlier: `docs/next_session_2026_11_14.md`. Newest wins.

## Owner's aims (11-14, standing)

Tighten the code: fewer crashes, less RAM, more speed. **1% wins are worth taking**; they add up.
Fable is expensive: one job at a time, only where it adds what Opus + bed cannot
(`docs/fable_driver_review_brief.md`, "Revised checklist"; memory `feedback_agent_roles_standing`).

## State left

- Card: HM driver (`b6936f4`), 800x600x16, Windows. SW1 back as fitted. CPU registers as CPUSET
  sets them (read back 92 / 8C). Card files from this session: `C:\FLUSHA.BAT`, `C:\A1-A3.BAT`,
  `C:\R*.TXT`, `C:\F*_*.TXT`, `C:\M8VGAW.EXE` (move to `C:\OLD\` in the C:\ tidy).
- Bed `vm_5160_page` (agent on host port **9870**): runs Fable's XT-IDE (`a3f3eead`, no
  `-SkipDrvHd`) and `SWAPZFC` (`349776fd`). Original swapcomp kept as `C:\SWAPCOMP.BAK`; original MPD
  in `%TEMP%\bed_fable_xt\orig.mpd` (`a67fefc4`). `vm_5160_now` has no agent port forward.
- Uncommitted: `docs/captures/2026-11-14_flush_asnp/R84.TXT` (commit with this file).

## Settled this session (commits b6936f4 .. d9c974c)

- **Feeder**: at 16 bpp in twist every feed stop is at Flip, 0 cut, ~23 writes left per frame.
  Not a cost. Counter kept as M8MON's "feed stopped by".
- **CPU flush on I/O is fixed in the I-O Data PK-A486BL module.** Inboard drives no cache pin
  (386 socket: HOLD grounded, FLUSH#/KEN# unconnected), INBRDPC.SYS never touches the cache, SW1
  and ASNP measured with no effect, CRLD's byte locks after CPUSET. It is probably what keeps DMA
  coherent (the module cannot see Inboard DMA). Do not reopen; avoid it in software instead.
  `docs/fable_pack_f1_flush.md`, captures `2026-11-13_flush_sw1`, `2026-11-14_flush_asnp`.
- **Fable Part 2 (swapcomp) and Part 3 (XT-IDE)**: reports on branches
  `worktree-agent-a5284dbe897a9358e` and `worktree-agent-a5f2e4562c5d10217`
  (`docs/fable_review_2026_11_13_{swapcomp,xtide}.md`). Both pass in the bed.
- #10: Simon's data-bus pointer written up (section 14); owner replied.

## Order for next session

1. **Card trip: Fable's two builds** (both bed-proven, one reboot):
   - XT-IDE: build at the usual path, flags `-PollBackoff -FastXfer -WaitFirst -YieldCommit
     -TightTaskfile` (no `-SkipDrvHd`), from the XT-IDE branch head.
   - swapcomp: `SWAPZFC` first (checksum), then `SWAPZF` (`07154eb7`) to keep.
   - Measure: `VMM\cpgLocked` old vs new (expect ~4 KB + ~3 KB less), one PAGETIME loop
     (ChecksumErrors 0), XTSECT, system-file read-back (hashes in `docs/next_session_2026_11_08.md`).
   - Pass: Haiku merges both branches to master. `dist/` after the PAGETIME soak.
2. **Code tightening, Opus + bed** (from the Fable reports; each: build, bed, card, keep or drop):
   - swapcomp: 7-sector pages stored compressed (raw = 27% of swap reads; count first with
     `tools/swapcomp/sectors.py` on a copy of the card's WIN386.SWP); early give-up on
     incompressible pages (12% of writes, ~0.5 ms each); compress inner loop (~1%); SlotTab at
     4 bits (another 4 KB locked), checksum bed loop first.
   - XT-IDE: CDB length vs DataTransferLength (diagnostic count in the bed first); time the
     wait loops against the PIT on the card (timeouts calibrated before the cache fix);
     `PORT=` range check; stale comments listed in the report.
3. **Fable F2** (the one Fable job with clear value): the Direct3D frame build, `m8d3d_replay`
   phases 36-41, ~22.6 ms CPU of 32.7 ms ours per twist frame; asm candidates in that path; burst
   shaping given the flush (78 ns/B refill per burst, port word ~7.6 us, ISA memory write
   1.83 us/B with no flush). Sonnet writes `docs/fable_pack_f2.md` first (<= ~1,500 source
   lines), Opus checks it, owner starts Fable. Output: <= 5 findings with diffs; Opus benches.
4. **VGA half per program** (worklist D5b/D6): for programs that draw every pixel themselves
   (MTM's whole-buffer Lock, video, FastDoom), the VGA half's memory path (1.83 us/B, no flush,
   FLUSHP2) beats PIX_TRANS (3.82 us/B plus flushes). Add to the worklist with these numbers.
   `M8VGAW.EXE` (Win16) does not run on the card ("A device attached to the system is not
   functioning"); never run before; FLUSHP2's DOS figures stand in.
5. X14 test titles (`docs/test_titles_2026_11_13.md`).

## Where Fable adds value, and where it does not

- **Yes**: F2 above; later, optionally, F3 Mach8 correctness (stale caches, register ownership,
  FIFO accounting, exit paths, m8feed).
- **No**: the flush (settled by measurement); applying or bench-testing fixes; Parts 4-8 (SHADRAM,
  T130B, patched MS drivers, real mode, scripts) unless the owner asks - Opus can use the brief's
  shared checklist.

## Rules learned

- Fable: two parallel jobs spent a 5-hour limit in minutes, twice. One job, packed, report-only.
- COMrade: load it once per boot. DOS batch files over `file_write` need CRLF: send base64, or
  DOS runs the whole file as one line. Redirect CTCHIP34's display to a file to read registers.
  Streamed `file_read` can report a timeout and still land; check the size.
- `CTCHIP34 IBM486 > file` (no write argument) is the register read-back; `1000h:1` ignores
  writes after CPUSET.
- Bed restarts by keystroke can miss; `card_restart.ps1`'s retrying restart works in a bed too.

## Progress, 2026-11-14 day session

| Item | Who | State |
|---|---|---|
| Card staged: `C:\V9XREMOTE\STAGE\FSWAPZFC.VXD` `349776fd`, `FSWAPZF.VXD` `07154eb7`, `FXTIDE.MPD` `a3f3eead`; active files unchanged; 26 flush-work files to `C:\OLD\` | Haiku | done |
| `C:\PICCHK.COM` `a16b4996` (#10 H1 probe, `tools/picchk/`): 5160 control run and `/T` before Simon | Opus | on card, not run |
| Boot 1 baseline locked memory; boot 2 checksum build + PAGETIME/XTSECT/read-back; boot 3 locked again (expect ~8 KB less: one page each) | Opus | waiting on 5160 |
| Census `docs/le_census_2026_11_14.md`: no new 4 KB win beyond the two builds; T130.MPD 838 B over | Sonnet | done |
| Issues `docs/issue_audit_2026_11_14.md`: #50, #47 owe replies; README issue list stale; #10 thread lacks the e-mail exchange | Sonnet | done, owner to act |
| Benchmarks `docs/benchmark_choice_2026_11_14.md`: Doom timedemo, 3DBench, SpeedSys | Sonnet | done, none on card |
| F2 pack `docs/fable_pack_f2.md` checked | Sonnet, Opus | ready for owner to start Fable |
| Showcase search `docs/showcase_3d_candidates_2026_11_14.md`; worklist X16 | Sonnet | done |
| Code tightening (swapcomp, XT-IDE, asm) | Opus + bed | after the card boots |
