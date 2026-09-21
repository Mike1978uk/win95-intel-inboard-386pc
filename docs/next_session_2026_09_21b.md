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
| [#8010](https://github.com/86Box/86Box/pull/8010) | head **`25e28f178`**, **MERGEABLE**, CI green |
| [#8012](https://github.com/86Box/86Box/pull/8012) | head **`90d81f013`**, **MERGEABLE**, CI green |

### ✅ Second round: the LPT dropdown, and both are now answered

`dhrdlicka` left a **review comment** on `src/char/char.c` — *"I don't think this belongs here"* —
which is easy to miss because review comments do not appear in the main thread. It anchored to the
whole added block, both device entries.

**We complied, and it was the right call.** Both bridges are now instantiated from the drive's own
bus assignment, exactly as `lpt_ditto` has been since `e684a9add`:

```c
if (rdisk_drives[c].bus_type == RDISK_BUS_LPT)      /* rdisk_hard_reset */
    device_add_inst(&lpt_epat_device, rdisk_drives[c].res + 1);

case CDROM_BUS_LPT:                                 /* cdrom_hard_reset */
    device_add_inst(&lpt_bpck_device, dev->res + 1);
```

⭐ **`src/char/char.c` has dropped out of both diffs entirely.**

**Both retested with no `lpt1_device` line in either config**, so the bridge can only have come
from the drive assignment: LS-120 reaches a drive letter with the write landing
(124,841,984 → 124,747,776), BackPack lists the Win98 ISO. Byte-identical to the pre-change runs,
and the owner watched the BackPack run himself.

⛔ **One capability deliberately dropped**: the EPAT-carries-a-CD commit. Never exercised, and
unreachable once the bridge is implied by bus type. It returns with a per-drive bridge setting and
a test if a real EPAT CD-ROM appears.

⚠ **`lpt.h` declares `lpt_t` only inside `#ifdef _TIMER_H_`.** Including it without `timer.h`
first compiles at the include and fails confusingly further down. `rdisk.c` already had `timer.h`;
`cdrom.c` did not. Cost one build.

#8012 is stacked on #8010, so one rebase cleared both. Both replied to, by the owner's
approval, and verified posted.

**Retested, not just rebuilt** — the drift shows up at compile and run time, not at apply time:

- builds clean **standalone** (#8010 alone) and **stacked**, GCC/MinGW/Ninja, `QT=OFF`
- **LS-120 bed**: enumerates, takes a drive letter, directory reads, 92,870-byte file copied,
  free space 124,841,984 → 124,747,776
- **BackPack bed**: `BPDRIVES` reports the drive, `DIR D:\` lists the Win98SE ISO, through the
  vendor DOS driver in its own installer's configuration

⏳ **CI was still running when the session ended** — 39 of 45 checks, nothing failed. Our build
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

## ✅ SETTLED: the CONFIG.SYS claim, read off the card

The headline **"No real-mode storage drivers are left"** was first written from the *bed's*
copy of `C:`. It has since been checked against the machine's own CF card, mounted locally at
`D:`, and the README now **quotes the file** instead of paraphrasing it. Four `DEVICE` lines,
none of them storage; `SD120PPD.SYS` and `ASPIHDRM.SYS` both `REM`-ed.

⭐ **The CF reads on the host at `D:` when it is out of the machine.** That is the cheapest way
to settle any claim about the real configuration - no COMrade, no boot. Use it before quoting
a bed image for anything the real machine decides.

### `EGACACHE` is off the line, and now explained

It was on the line for the 2026-09-20 A/B and the owner removed it on 2026-09-21. The switch
*"reserves up to 32K bytes ... for caching the EGA ROM BIOS"* and this machine's card is an ATI
Mach8 - a **VGA**. So the null A/B was not "the switch is inert", it was "the switch is aimed at
a ROM this machine does not present".

⚠ **Which state is the baseline matters.** The line *without* `EGACACHE` is the long-standing
one - recorded since 2026-07-26 and booted hundreds of times. Removing it restored the proven
state; the question was ever only about having it **on**. This handoff originally said the
opposite.

✅ **That also un-breaks a comment in upstream 86Box.** `src/device/inboard386.c` on master says
the reference machine's CONFIG.SYS is `DEVICE=c:\INBRDPC.SYS NODIAGS NOPAUSE`, no EGACACHE -
which had gone stale and is now true again, character for character. **No upstream fix needed.**
Only `vm_bpck/dos.img` still carries the old line, harmlessly.

---

## Corrections made to our own record

**Nine**, all in one session, most of them to things written earlier the same day, and
most caught by the owner rather than by me. Listed because the pattern matters more than
the count — see the note at the end:

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
5. **Quoted the bed's `CONFIG.SYS` as if it were the machine's.** The card was readable on the
   host at `D:` the whole time. A capture is not the configuration.
6. **Called removing `EGACACHE` an unproven change.** It is the revert; the line without it has
   the boot history. Ask which state has the history before calling either one untested.
7. **Followed the ACTIONS table over the register**, and redid A10/A14 which the register already
   recorded as done on 09-20. The plan contradicted itself; the register is now marked as the
   authority.
8. **Closed E7 on a derived source** (the FaxBACK catalog) without reading the product manual,
   which says the opposite. Retracted the same session.
9. **Proposed a switch test that does not exist** — `SW1-3/4` has no setting below bank 0, and the
   machine was already there.

⭐ **The pattern worth carrying**: every one of these was caught by checking a cheap fact against
the machine, the owner, or a primary source — never by reasoning harder. Three proposals died on
contact with a fact the owner already had. **Ask before writing the plan.**

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

## The optimisation track moved a long way — read `work_order_2026_09_21.md`

**Track A ran, offline, and two of three levers were already taken.** That is the argument for
doing the free checks first; both would otherwise have cost a hardware run.

| | answer |
|---|---|
| **A10/A14** `T130.MPD` string I/O? | ✅ already `...BufferUshort`. Pseudo-DMA at `base+4`, 128 B/call |
| **A13** 3C509B drained in bulk? | ✅ already, **at dword** — `ELNK3.VXD` `shr ecx,2` / `rep insd`, disassembled |
| **A15a** SCSI disconnect? | ⛔ **never granted** — no `0xc0`, no `or ...,0x40`, and **no setting in either INF** |
| **A19** do our four VxDs poll? | ✅ **NEGATIVE, recorded**: VKD 1, VDMAD 1, VPICD 1, KEYBOARD.DRV 2 |
| **A21** (new) | ⛔ **`ELNK3.VXD` spins flat out** — 27 loops in 30 KB, confirmed at `0xd03`, no delay |

⭐ **The pattern**: of four data paths audited for width, **three were already optimal**. Polling
is the inverse — **only XT-IDE is paced**. That is where the free wins are, and it is why the
owner's *"bus polling is the big lever"* is the right read. One poll = **5.55 us** of bus moving
nothing; a cache-resident delay = **0.22 us** and no bus at all.

**New actions: A17** pace `HSFLOP.PDR` · **A18** pace `T130.MPD` (✅ owner approved patching it) ·
**A19** done · **A20** dword on the T130B pseudo-DMA port (candidate) · **A21** pace `ELNK3.VXD`
(unquantified) · **A21a** model the 3C509B so A21 has a bed at all — that reframes
[#20](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/20) from *fidelity only* to a
**prerequisite**.

⛔ **A3 stood down** — the owner answered it: *"it works and has been for a few weeks, sound works
fine."* Sound DMAs into that range every play.

---

## ⛔ E1 WAS WRONG FOR TEN DAYS — 64 KB planar, not 256 KB

The owner read bank 0's chips: **HYB4164 P3EF 8437**, Siemens 64K x 1. So this is the
**64-256 KB** 5160 planar and `SW1-3/4 = ON/ON` (bank 0 only) means **64 KB**, not 256 KB.

| | recorded | actual |
|---|---|---|
| planar on the bus | 256 KB | **64 KB** |
| served by the card | 384 KB | **576 KB** |
| E1's reduction | 60% | **90%** |

Nobody checked the chips for ten days because the note asserted a number.

## E7 — remove the planar RAM: investigated, and it is a hardware question

The owner's idea. Chased through four offline sources in one sitting, no case opened.

1. ⛔ I **closed it on the FaxBACK catalog**, which says *"disabled down to 256K bytes"* — read as a
   floor.
2. ⭐ **Retracted the same session.** Intel's own manual (`inboard_files/DOX1.TXT`) says the card
   works when the board supplies *"256k **(or less)**"* and to disable *"to 256k **(or lower)**"*.
   It is a **ceiling**. The FaxBACK catalog is derived; the manual is primary. **Read the primary
   source first.**
3. ⭐ **@RonnyRoy dumped the card's PALs unsecured** and published CUPL equations — cloned to
   `references/inboard386_ronnyroy/` (gitignored). **`logic/U71.pld` is the memory decoder**, the
   only one touching `A16`-`A23`. Its inputs are address lines and control signals and **nothing
   else: no jumper, no register, no switch.** ➡ **There is no software disable.**
4. ⚠ Which range each `RAS_EN_*` covers needs the schematic and the unnamed `i9`/`i13`. **Decoding
   U71 properly is a session of its own** — and the material is now local, so it is paper work, not
   hardware work.

✅ **And most of the prize was already collected**: only **64 KB** still crosses the bus, not
256 KB. E7 is a small remainder, not a big lever.

⭐ **Still worth running: `gen_ramstride.py`.** It was parked because *"no outcome changes the
decision"*. One does now — whether card RAM is genuinely faster under a cache-defeating pattern.

---

## Still outstanding

- ✅ **Both PRs are green, MERGEABLE, and every maintainer comment is answered.** Waiting on
  maintainers only; nothing is owed.
- ✅ **`AGENTS.md` written** — the promise to @JoshRodd on #8010 is kept. Nine sections, each
  credited to whoever caught the breach; linked from the README's Contributing section.
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
