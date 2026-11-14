# Issue audit, 2026-11-14

Read-only pass: `gh` for threads, `docs/` and `git log --since=2026-10-15` (613 commits) for the repo
side. Nothing was posted. All 24 issues below are by Mike1978uk; outside voices are
@andrew-hoffman (#10 excepted) and Simon/Cimon on #10 via other channels.

## Verdicts

| # | Title (short) | State | Last activity | Verdict |
|---|---|---|---|---|
| 10 | 1982-era ROMs / BIOS shim | open | 2026-10-20 (body edit; last comment 10-04, Mike) | **we owe a reply** |
| 18 | Floppy garbage, DMA above 1 MB | open | 2026-10-20 (last comment 09-21, Mike) | stale claim in thread |
| 41 | Pace polling in spinning drivers | open | 2026-10-21, Mike | up to date |
| 43 | IOCHRDY cost | open | 2026-10-20, Mike (body only) | up to date |
| 47 | FastDoom SB + frame rate | open | 2026-10-20 (last comment 10-05, Mike) | **we owe a reply** |
| 48 | Optimisation timeline | open | 2026-10-11, Mike | stale claim in thread |
| 49 | Mach8 driver | open | 2026-10-22, Mike | stale claim in thread |
| 50 | BlueSCSI v2 bridge | open | 2026-10-23, Mike | **we owe a reply** |
| 51 | Filesystem vs bus bytes | open | 2026-10-23, Mike | up to date |
| 52 | WINDISK ideas for xtide_mpd | open | 2026-11-04, Mike | up to date |
| 53 | Leads from linked repos | open | 2026-10-23, Mike | up to date |
| 54 | Change detection by hash | open | 2026-11-04, Mike | up to date |
| 55 | 16 bpp restart hang | open | 2026-11-02, Mike (no comments) | stale claim in thread |
| 56 | dxdiag hang + screenshot | open | 2026-11-02, Mike (no comments) | up to date |
| 57 | Mach8 lessons for other drivers | open | 2026-11-06, Mike | up to date |
| 58 | DirectX on the 5160 | open | 2026-11-06, Mike | up to date |
| 59 | TODO for local Claude | open | 2026-11-06, Mike | up to date |
| 45 | Win95 settings / timer tick | closed 10-21 | Mike | up to date (closed) |
| 35 | Shadow RAM inventory | closed 10-21 | Mike | up to date (closed) |
| 33 | DRAM refresh | closed 10-20 | Mike | up to date (closed) |
| 31 | SCSI chain disconnect | closed 10-21 | Mike | up to date (closed) |
| 29 | Measure DMA | closed 10-21 | Mike | up to date (closed) |
| 28 | Per-component audit | closed 10-21 | Mike | up to date (closed) |
| 23 | XT-IDE Hi-Speed map | closed 10-21 | Mike | up to date (closed) |

### #10 (Simon and Cimon are one contributor)

- Thread: two comments, both Mike's (10-04). Simon/Cimon has never posted on GitHub; his input
  arrives by forum and email and is recorded in `docs/issue10_old_bios_2026_09_27.md` sections 11-14
  and ledger rows 65-66.
- Not on the thread: his 11-02 Phoenix and Ruud ROM results, his 11-07/08 Supersoft test (VGA 5
  hi-lo + 8 short beeps, two 5150A error photos, "Interrupt level 0" the only failing test on both
  boards) and his 11-13 data-bus pointer. Ledger "Told?" column for rows 65-66 still reads "reply
  drafted, owner to send".
- The owner's 11-13/14 reply asking which slot the Inboard is in (hypothesis H4, section 14) is **not
  on GitHub**: no comment after 10-04. `next_session_2026_11_15.md` says only "owner replied", with
  no channel. I cannot confirm where it went, and no answer from Simon is recorded anywhere in the
  repo. The 8259 test program (H1) is not written; no commit mentions it.
- Verdict: we owe a reply, or at least a thread note, because the thread is silent on three rounds of
  his work and README line 531 says only "a real 5150 still stops". Contributor not told: Simon
  (verified in part: the Phoenix result removed "Anonymous BIOS" as the cause).

### Other verdicts

- **#18** stale claim: README "Open issues" and the thread both say the repro bed "is still to be
  built". `next_session_2026_10_21.md` line 116 plans it as the first cheap test; no later doc or
  commit says it was run. Not closable until it is. Andrew's last 09-09 point (corruption in emulation
  with Monster Floppy at the lowest option-ROM address) is the pending test, and he is not told that
  nothing has happened since.
- **#47** owe a reply: Andrew's 10-05 FastDoom lead and the Blue Lightning/40 MHz crystal reply were
  the last exchange. Since then `3ed37d3` frees the bus for SB and XT-CF in vblank, and the 11-12/13
  handoffs plan FastDoom on the VGA-half memory path (1.83 us/B, no flush). Draft below.
- **#48** stale: the timeline stops at 10-11 (XT-CF waits on measured times). Since then: swapcomp,
  XT-IDE `XTBOTH.MPD`, tight taskfile, I/O flush finding. Nobody is owed anything; it is a log.
- **#49** stale: last note (10-22) is the first real-5160 measurement. README row 543 still says "the
  first timed run against ATIM8 on the 5160 is still to come", contradicted by that note. Andrew was
  thanked 10-12 and 10-20; no new input from him since. Owner's "ATIM8 comparison" claim in the repo
  line is stale, not Andrew's input.
- **#50** owe a reply: ledger and `next_session_2026_10_24.md` line 48 record "OWED to Andrew": time
  a 512-word `rep insw` from the XT-CF with the L1 on and off. Related result now exists (I/O-only L1
  invalidation, once per burst, `project_io_flush_per_burst`; flush captures `2026-11-13_flush_sw1`,
  `2026-11-14_flush_asnp`). Andrew's cache question (10-23) was answered with a promise, not a number.
  Draft below.
- **#55** stale: "restart hangs" still open, and `next_session_2026_11_08.md` records an exit hang
  after Alt+F4 seen again on boot 306; the 11-07 handoff says "#55 and #59 updates not posted". No
  thread comments, so no outsider can see the progress.
- **#41** up to date: 10-21 status says XT-CF done, T130.MPD and ELNK3.VXD left; README row 538
  matches. Check the 10-21 note about HSFLOP.PDR omitted from the status (handoff line 118).
- **#43** up to date; probe still needs a safe port (handoff 10-21 line 117).
- **#59** checklist: 11-06 status says no box ticked; the 11-07 and 11-08 handoffs carry the "tests are
  in #59" line. Fine.
- **#52, #54, #57** up to date; branches `claude/driver-optimisation-review` and
  `claude/xtcf-tight-taskfile` hold the work.
- **#58** up to date; nothing built yet (D3D 60 Hz accepted, not built).
- **Closed in the last 30 days** (#23, #28, #29, #31, #33, #35, #45): closing comments are in place.
  README "Open issues" still lists #23, #28, #29, #31, #35, #45 as open (see below).

### Contributors not told

- **Simon/Cimon (#10)**: three rounds of data, none acknowledged on a public thread, two ledger rows
  say "owner to send".
- **@andrew-hoffman (#50)**: owed the L1-on/off `rep insw` timing; (#18) owed a status that his
  Monster Floppy repro is still unbuilt; (#47) FastDoom path. 

## Drafts (owner to edit and post)

```
Thanks for your continued input. On the 10-23 cache question: the CPU
flushes its L1 on every I/O burst (I-O Data PK-A486BL module; the Inboard
drives no cache pin), so disabling the cache buys little for a disk driver
doing 'rep insw'. Measured 2026-11-13/14, SW1 and ASNP made no
difference. Details in docs/fable_pack_f1_flush.md. The 512-word XT-CF
timing with L1 on and off is still to be run.
```

```
Thanks for your continued input. FastDoom: the vblank wait now reads the
latch every ~90 us, so the bus is free for the SB and XT-CF. Next is
drawing through the VGA half's memory path (1.83 us/B, no cache flush)
instead of the 8514 engine. Not built or tried on the card yet.
```

```
Thanks for your continued input, Simon. The Phoenix and Supersoft tests
narrow it: only "Interrupt level 0" fails, on both 5150As. Which slot is
the Inboard in on each machine? Look only. A short 8259 test program may
follow, to read and write the mask and compare against my 5160.
```

(Third draft is for the forum or email thread, not GitHub, since that is where Simon replies.)

## Elsewhere in the repo

- **README stale**:
  - "Open issues" table lists #23, #28, #29, #31, #35, #45 (all closed 10-20/21), and omits #41's
    later state, #50-#59.
  - Line 95: Mach8 "1024x768x256, confirmed on real hardware 2026-08-24". The card runs the project's
    own driver at 800x600x16 (`next_session_2026_11_15.md`); the 1024x768 line is the stock ATIM8.
  - #49 row says the first 5160 timing is still to come (done 10-22).
  - README names `XTIDEMP.MPD`; the card runs `XTBOTH.MPD` `DAEF6830` and `SWAPZF.VXD`
    (`docs/5160_tuned_config.md`).
- **dist/ lags the card** (known, pending a soak): `dist/xtide_mpd` (10-11, `a67fefc4`) vs card
  `XTBOTH.MPD` `DAEF6830`; `dist/swapcomp` (10-02) vs `SWAPZF.VXD` `C2585E38`. Fable's builds
  (`a3f3eead`, `SWAPZFC` `349776fd`) are bed-proven only; PAGETIME soak not run.
- **Open PRs on our repo**: none.
- **Branches on origin**: `claude/xtcf-tight-taskfile` is fully merged into master (0 ahead): delete.
  `queue-wip` (10-21, "WIP, untested", 2 ahead), `claude/dinput-probe` (1), `claude/video-playback` (1),
  `claude/driver-optimisation-review` (5), `claude/bluescsi-5160-firmware-o12wij` (12): unmerged, no
  open PR. Fable review branches (`worktree-agent-a5284dbe897a9358e`, `worktree-agent-a5f2e4562c5d10217`)
  are local only. Local master matches origin/master.
- **Untracked**: `vm_5160_mtm/`, `vm_5160_page/` (VM beds, by design). 11-15 handoff notes
  `docs/captures/2026-11-14_flush_asnp/R84.TXT` uncommitted (not in the status snapshot at the start of
  this pass; may be committed since).
