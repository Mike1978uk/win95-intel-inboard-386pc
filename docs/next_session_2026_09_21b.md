# Handoff — 2026-09-21b

Read this one first. Supersedes `next_session_2026_09_21.md`, which planned a work order
that this session did not get to — the session went on the two upstream PRs and on making
the public face honest.

---

## The two upstream PRs are unblocked

`jriwanek` called a merge conflict on both. It was **one line**, the same on each:
`src/char/char.c`. Upstream removed the Ditto from the LPT dropdown
(`e684a9add`, dhrdlicka) while our branches were open, and our line sat directly beneath it.

| | |
|---|---|
| [#8010](https://github.com/86Box/86Box/pull/8010) | rebased onto `e08ee5a52`, head `f3fa3e1bd`, **MERGEABLE** |
| [#8012](https://github.com/86Box/86Box/pull/8012) | rebased onto the above, head `43d668a1e`, **MERGEABLE** |

#8012 is stacked on #8010, so one rebase cleared both. Both replied to, by the owner's
approval, and verified posted.

**Retested, not just rebuilt** — the drift shows up at compile and run time, not at apply time:

- builds clean **standalone** (#8010 alone) and **stacked**, GCC/MinGW/Ninja, `QT=OFF`
- **LS-120 bed**: enumerates, takes a drive letter, directory reads, 92,870-byte file copied,
  free space 124,841,984 → 124,747,776
- **BackPack bed**: `BPDRIVES` reports the drive, `DIR D:\` lists the Win98SE ISO, through the
  vendor DOS driver in its own installer's configuration

⏳ **CI was still running when the session ended** — 29 of 45 checks, nothing failed. Our build
is `QT=OFF` and CI builds Qt, so **check it before assuming the rebase is clean.**

### The open design question on #8010

We kept `lpt_epat_device` and `lpt_bpck_device` in the LPT dropdown the Ditto has just left.
Upstream now auto-instantiates a parallel bridge from the drive's bus setting
(`device_add_inst` in `tape_hard_reset`). Our argument, put to them in the reply: the Ditto is
the **only** parallel tape bridge, so "tape on LPT" is unambiguous; we have **two** bridges with
different wire protocols, and there is no per-drive setting to choose between them. If they want
auto-instantiation, they have to say what shape that setting takes. **Do not change it
unilaterally.**

---

## The LS-120 miniport is retired

Ours **reads** on the real 5160 and **never wrote** — it never sets `ScsiStatus`, never fills
`SenseInfoBuffer`, never raises `SRB_STATUS_AUTOSENSE_VALID`, so the class driver cannot tell
`6/28h` medium-changed from `7/27h` write-protected. The vendor driver with `/fe` does both at
75-99 KiB/s with 36.7 MB verified byte-identical, so there was nothing left for ours to be
better at, and the card no longer binds it.

Marked do-not-install rather than deleted, in four places: `README.md` (repo structure),
`dist/ls120_mpd/README.md` (the download — it previously opened with "What works"),
`docs/what_worked_and_what_didnt.md` (driver dead ends), and `CLAUDE.md`.
`drivers/imation_ls120_mpd/README.md` already carried a banner from 2026-09-19.

⭐ **The transport work was not wasted** — `TRANSPORT_SPEC.md` and the disassemblies are what
made the 86Box EPAT device possible, and that is #8010.

**`120PPD95.INF` is now in `FIXES.md`** — the one patched vendor file in the project, and the
only fix here that is not a binary patch.

---

## ⚠ One claim the owner should check

`README.md` now asserts, under the new headline **"No real-mode storage drivers are left"**:

> `CONFIG.SYS` carries no storage device line at all — `SD120PPD.SYS` and the real-mode SCSI
> chain are both out of it.

That was read off the **bed's copy** of `C:` (`vm_bpck/dos.img`), where `SD120PPD.SYS` is
`REM`-ed out. If the live `CONFIG.SYS` has drifted since that capture, **that sentence is the
load-bearing one under the headline** and needs correcting.

---

## Corrections made to our own record

Four, all in the same session, three of them to things written earlier the same day:

1. **`J:` removed from #8010's body.** Caught by the owner. It is a property of this machine's
   SCSI chain — Zip at `D:`, five Nakamichi LUNs at `E:`-`I:` — not of the bridge. Nobody
   reproducing it gets `J:`. Now `repo-hygiene` §8.
2. **Technique 128d called the T130B memory aperture "unchecked".** It was closed the same day
   it was written, three independent ways — no `55 AA` Trantor ROM, no `MemConfig` in
   `T130.INF`, `T130.MPD` imports only I/O-space calls. The skill would have sent the next
   reader back down it. Corrected in place as a retraction.
3. **Kevin Moonlight written off as "a citation, not a contribution".** Wrong — the contributor
   ledger already had him right, as the validation path for the whole project.
4. **Then over-corrected**, crediting him on BlueSCSI by reading `yyzkevin/BlueSCSI-v2` as his
   work. It is his **fork**. His contributions are COMrade, PicoPCMCIA, and the **CD-ROM
   emulation** and **WiFi code** in PicoMEM and PicoGUS.

The pattern worth carrying: **reading outside work is most valuable for what it makes you
re-read at home.** Nothing in PicoMEM found the 128d error; going to check a claim against it
did.

---

## Modern ISA cards — read, recorded, not a work item

`docs/resources_and_sources.md` §9, with what each gave **and a ❌ for what each did not**.

- **ISA-PicoMEM** — moves disk data through a **memory aperture**, not ports, 16 KB granularity.
  Independently reproduces #30's request-merging result. Bounds our own claim: "no card has a
  data aperture" is true of **this machine**, not of ISA. Relevant to #35 as a worked design.
- **PicoGUS** — the ISA deadline from the card's side, and the caution that holding IOCHRDY
  *"effectively halt[s] ISA bus"*. Raises a fidelity question: **does 86Box model an IOCHRDY
  stall at all?** Our own data says the real machine charges I/O 4.25× more than memory on the
  fixed sync term and 86Box charges them alike — so emulated timing may flatter every driver
  we test.
- **BlueSCSI v2** — for #31: sync is 10 MHz / 5 MHz / **0 = async**, and on old hosts async is
  often faster *and* more reliable. **The fastest negotiated mode is not the fastest real mode.**
  ❌ Nothing on target-side caching or disconnect policy — that still has to be measured here.

**Two questions a person would answer in a sentence** (§9): an 8-bit XT aperture-vs-port figure,
and what holding IOCHRDY actually costs a period machine. ⛔ **Nothing sent, and contact is the
owner's to make.**

---

## Still outstanding

- **Check CI on both PRs**, then they are waiting on maintainers only.
- **`AGENTS.md` does not exist** — promised to @JoshRodd on #8010 (*"Noted I'm happy to add
  that"*). `CLAUDE.md` already carries the four rules it would point at.
- **The work order** that `next_session_2026_09_21.md` was for. Live leads unchanged: **#38**
  (EPP — the blocker for running the vendor miniport in the bed), **#30** (request merging,
  1.88×), **#31** (SCSI chain), **#18** (the media-change bed, now the README's headline ask).
- **#18's reproduction bed** is now *"the most useful thing anyone could pick up"* on the front
  page, replacing the ask on **#8**, which TC1995 closed on 2026-09-18.

---

## Bed note

`vm_bpck/AUTOEXEC.BAT` now redirects `CDDRIVES` and `DIR D:\` into `C:\BPRES.TXT`, so the run
reports its own result to a file the host can read instead of something to be read off a screen.
Keep it — evidence generated by the guest beats evidence inferred by us.
