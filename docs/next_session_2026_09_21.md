# Handoff — 2026-09-21

Read this one first. Supersedes `next_session_2026_09_20g.md`.

---

## What next session is for

**Planning: the outstanding issues and the work order.** The owner asked for that
explicitly. Nothing below is urgent; the tree is clean and nothing is half-finished.

The material for that plan is already written down:

- `README.md` → **Open issues**, rebuilt 2026-09-21 from `gh issue list`, 19 rows, accurate.
- `docs/bus_optimisation_plan.md` → every optimisation question as an action with a method
  and a cost.
- `docs/contributor_input_ledger.md` → what is owed to whom. Currently **nothing** is owed.

---

## Where things stand

**Two upstream PRs open.**

| | |
|---|---|
| [#8010](https://github.com/86Box/86Box/pull/8010) | parallel-port LS-120 — the EPAT bridge, the ECP callbacks, the SuperDisk type |
| [#8012](https://github.com/86Box/86Box/pull/8012) | parallel-port CD-ROM — `CDROM_BUS_LPT` implemented, the BackPack modelled, a `scsi_cdrom_current_mode()` fix. Based on #8010 |

#8012 was built, cleaned and verified in one session: the vendor DOS driver in its own
installer's configuration enumerates the drive and MSCDEX reads an ISO.

**All contributor threads are answered.** @JoshRodd on #8010 (by the owner), @andrew-hoffman
on #22 with cross-posts to #18 and #23. The ledger records each.

---

## The one thing outstanding anywhere

**The #18 reproduction bed.** 86Box + the Monster Floppy controller + the DMA-patched
driver + **a media change**, which is the trigger as @andrew-hoffman described it. Our
2026-09-08 harness wrote twice to one disk and never changed media, so it never tested the
recorded trigger. Needs no real hardware, and it is now stated publicly on #18 as ours.

---

## Live leads, for the work order

- **#38 — wire `lpt_epat.c` to upstream's EPP callbacks.** Now scoped as the blocker for
  running the **vendor** 32-bit miniport (`sd120ppd.mpd`, bound with `/fe`) in the bed. That
  is the driver a stranger would use, and the driver the real machine runs.
- **#30 — request merging**, modelled at **1.88x** on sequential; a 1-sector command is 53%
  overhead. Not started, host-side, and the most likely source of more LS-120 throughput.
  ⛔ The LS-120's measured **75-99 KiB/s is a floor, not a ceiling** — and *"the Windows
  stack is the bigger lever"* stays **retracted**, it compared an unverified transfer
  against a verified one.
- **#31 — the SCSI chain**: every target has a cache and disconnect is probably off. The
  plan calls this the most on-point mechanism in the machine. One DOS run reads mode page 8.
- **#20 — 3C509B.** Upstream [discussion #6447](https://github.com/86Box/86Box/discussions/6447)
  has the documents; our measured values are already posted there. Base any port on QEMU's
  3C509B (**MIT**, Antony T Curtis), never on `net_3c503.c` — EtherLink II is a DP8390
  design and shares nothing architecturally.

---

## Rules adopted this session

Four, all now in `CLAUDE.md`. Three are @JoshRodd's, asked for on #8010:

1. **Comments are a canary.** Re-read every comment in a generated or ported file. Proved
   itself within the hour: `lpt_bpck.c` went out describing the LS-120 and calling `rdisk_*`
   functions it never uses — corrected in #8012.
2. **Alert, do not fix.** When something is wrong in committed or submitted work, say so and
   stop. A silent fix destroys the evidence.
3. **PR text and replies to people are the owner's.** Draft on request; never post.
4. **Always thank contributors for their continued input** (owner). A reply that is all
   findings and no thanks reads as extraction.

---

## Method notes worth keeping

- **Measurement beat reasoning every time.** The BackPack chain address was found by scanning
  all eight, after reading it out of the live driver's structure gave the wrong answer. The
  connect tail, the byte-mode question and the final `current_mode` bug were all settled by
  hardware or by a trace, never by inference.
- **A model cannot validate its own handshake.** Our driver talked to our bridge because both
  halves came from the same guess. Three machines were needed to falsify it.
- **Check the issue list before opening an issue.** #39 was opened as a duplicate of #20 and
  closed the same night, with two factual errors in it.
- **Check a branch before pushing it.** The working branch carried 34 unrelated commits and an
  `io.c` WIP; the submission was rebuilt from #8010's head and re-tested before going out.
