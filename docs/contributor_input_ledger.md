# Contributor input ledger

Who gave us what, whether it **actually paid off**, and **whether they've been told**.

**Why this exists:** people who give good technical input tend to give more of it when they learn it
landed. Reporting an outcome back — especially a *specific* one, with the evidence — costs minutes
and is the single cheapest way to keep expert help flowing. The failure mode is silent success: the
input works, gets absorbed, and the person never hears about it.

**Rule:** when a lead is verified or shipped, the "told?" column becomes a task, not a nicety. Credit
in the README/docs as well, but a direct reply is what actually closes the loop.

**On provenance tagging:** the `[PRIMARY]` / `[AI-SOURCED]` marks used across these docs are about
*where a claim came from*, not how much it's worth. A named person with relevant expertise gets
checked immediately and carefully — that's why Michal Necasek's suggestion was byte-verified within
the hour. Unsourced AI claims go in the "test cheaply, in cost order" pile. Human-sourced data has
been consistently the more reliable of the two on this project.

---

## Outstanding — owed a reply

| Contributor | What they gave | Status | Told? |
|---|---|---|---|
| **Michal Necasek** (OS/2 Museum) | Use the BIOS's own IRET at `F000:FF53` instead of injecting a stub at `0x3C0` | ✅ **Verified correct** — `CF` at offset `0x7F53` in both 1986 ROMs. Strictly better: no injected code, no assumption that INT F0h is unused | ✅ **replied by email 2026-08-23** — byte evidence from both 1986 ROMs, why the `0x3C0` stub was worse, and verification on both emulator and real hardware |
| **Michal Necasek** | Asked whether Win9x could never support the Inboard because the card couldn't hold enough RAM | ✅ **Answered 2026-08-23, and conceded plainly**: the 5 MB only exists because of a *modern reproduction* daughterboard (Stynx / Harrison Frazier, ParrotyError). The software obstacles were surmountable but were never the binding constraint — RAM was, and in 1995 it was not removable. Framing used: Win9x *could* have worked on an Inboard, not that anyone could have run it | ✅ same email |
| **@andrew-hoffman** | XT DMA page register is only 4 bits → the SB Pro DMA buffer must live under 1 MB (#5, 2026-08-20) | ✅ **CONFIRMED BY MEASUREMENT 2026-08-24.** A page-register trace caught it live: `MSSBLST.VXD` allocates at `0x4E0000` (4.875 MB), the 4-bit latch truncates to `0x0E0000` (896 KB, adapter ROM) — the card plays ROM contents. He called it from the hardware alone. Note his lead was **not** what caused the BSOD (that was our own corrupt VDMAD patch) and `DMABufferIn1MB` is **not** the fix — the lever is `_PageAllocate`'s `maxPhys` | ✅ **replied and closed 2026-08-24** on [#5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5#issuecomment-5393237068). Fix deployed and confirmed on the real 5160 — `maxPhys` `0xFFF` → `0xFF`, page `0x4E` → `0x09`, clean audio. Told him his parenthetical ("or really first 640k") was the stricter and more accurate half, and that `DMABufferIn1MB` was my wrong call, not his |
| **@andrew-hoffman** | Follow-up 2026-08-23: primary sources (os2museum ×2, martypc book) **and** the pointer at the 386MAX source | ✅ **Paid off twice over.** 386MAX's `MARK_XT` sets `DMA_MAX = 0A0000h` — the XT ceiling is **640 KB, not 1 MB**, stricter than we assumed; and it independently confirms port `0xA0` as the XT NMI register (plan step 5). Checking it also surfaced an emulator fidelity bug: `dma_at = is286` makes 86Box give the Inboard a full 8-bit DMA page register where real hardware latches 4 — a concrete mechanism for why the old `vmad` patch passed in emulation and failed on hardware | ✅ **replied 2026-08-23** on [#5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5#issuecomment-5385997588) — gave him the 640 KB `MARK_XT` evidence, the `dma_at` fidelity bug his pointer surfaced, and an explicit "nothing is fixed yet"; asked him to keep the leads coming |
| **@andrew-hoffman** | Follow-up 2026-08-24: could DMA buffers live between 640 KB and 1 MB, if the Inboard can map RAM into UMBs? Cites the 1987 installation guide's 64 KB EMS page frame and asks whether it can backfill the rest | ✅ **Right about the capability, and it argues the opposite way.** `$386.SYS`'s `vremap` maps arbitrary 4 KB pages anywhere in meg 0 — further than the manual suggests — but it does it with the **386 page tables** (`pd_addr`/`pt_addr`, `cr0=8000_0001h`, `int 6Fh "will have to modify the page table"`), which a bus master never consults. The 1988 code already knew: `maint_dmabank` tracks the lowest remapped bank on every `vremap`, and the INT 13h hook **stages** (bounces) any transfer reaching it. So RAM up there would be CPU-visible and DMA-invisible — this exact bug class | ✅ **replied 2026-08-24** on [#5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5#issuecomment-5396115327). Gave him the `vremap`/`dmabank` primary source and the measurement (post-fix buffer at page `0x09`; no UMBs configured on this machine at all). His question moves `maxPhys` `0x9F` from pedantry to the plan: `0xFF` is only safe because VMM has nothing allocatable in `0xA0`–`0xFF` |
| **@andrew-hoffman** | 2026-08-25 on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3): "I don't think the 20 bit DMA range limit patch has made it to upstream 86box yet." | ✅ **Correct, and a real gap.** `dma_force_xt` reached 86Box in #7626, but only as an AT/XT *detection* override. `dma_page_is_xt()` and the `dma[addr].page = val & 0x0f` truncation did **not** — confirmed by diffing `86box_upstream/src/dma.c` against `86box_full/src/dma.c`. Upstream still hands guests 24-bit DMA reach on 20-bit hardware, so his own original bug class is invisible in emulation upstream. Not Inboard-specific; correct for any PC/XT-class machine | ✅ **replied 2026-08-25** on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5414247347). Gave him the exact missing lines. **Superseded twice below**: the description was overstated (upstream *does* truncate, it just gates on `dma_at`), and the PR is now raised as [#7771](https://github.com/86Box/86Box/pull/7771) |
| **@andrew-hoffman** | 2026-08-25 on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3): adopt a variant of [Fabien Sanglard's `agent.md`](https://fabiensanglard.net/agent.md/index.html) in `CLAUDE.md`, especially short commit messages and PRs — "Let it ramble in the plan files but it's hard to read through the history as it is" | ✅ **Adopted in full.** `CLAUDE.md` rewritten: subjects 50/72, bodies ≤ 5 lines, "ramble in the plan files, not in the history". Also became the `repo-hygiene` skill. Knock-on tidy: `git status` 93 entries → 0, seven session-scratch files archived, README credits cut to two lines per person, issue bodies given status headers, and a flat [what worked / what didn't](what_worked_and_what_didnt.md) page added | ✅ **replied 2026-08-25** on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5414247347), crediting him and Sanglard in `CLAUDE.md` and the skill itself |
| **@andrew-hoffman** | 2026-08-25 on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5414257699): add a `.gitattributes` pinning line endings, then `git add --renormalize .` and one normalising commit. Reference: [his WDMHDA file](https://github.com/andrew-hoffman/WDMHDA/blob/main/.gitattributes) | ✅ **Adopted, and it caught a live latent bug.** `tools/*.sh` and `vxd-patches/deploy_premonolith.sh` were LF by accident — with `core.autocrlf=true` and no rules, **any Windows clone got CRLF copies bash refuses to run**, and those are the scripts that write patched files to a real CF card. Also pinned every patch binary (`*.VXD/.DRV/.SYS/.PDR/.386/.COM`, `roms/**`) explicitly rather than trusting git's NUL-byte heuristic. **Two deviations:** `86box_full/` left under upstream 86Box's own `.gitattributes` so our tree stays diffable against master; and the normalising commit was **a no-op** here — autocrlf had already kept the database LF-clean | ✅ **replied 2026-08-25** on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5414315383). Gave him both deviations, the `git diff --cached --numstat \| awk '$1=="-" && $2=="-"'` safety check for binaries, and the directory-macro trap that marked six `roms/` READMEs binary in my first draft. Now in the `repo-hygiene` skill, credited |
| **Andrew's floppy question** (#3, 2026-08-25) | How was HD floppy support added on the real hardware? And: Windows hangs in file dialogs if the BIOS reports drives with no controller behind them — try Drive Letter Manipulator | ✅ **Answered by the project owner** (Sergey Kiselev ISA floppy/serial card + his BIOS ROM, driving a Teac FD-505: 1.44 MB 3.5" as A:, 1.2 MB 5.25" as B:). His hang diagnosis was close to the mark but inverted: the BIOS was reporting drives Windows had **no controller installed** for at all | ✅ **replied 2026-08-25** in the same comment. Told him the inversion and that `HSFLOP.PDR`'s `maxPhys` — his original finding, third occurrence — is what remains behind it |
| **@TC1995** | Mach8/Graphics Ultra EEPROM "power up configuration" angle (#7) — independently matched #4's own finding | 🟡 Still untested. Unblocking action remains `C-INFO.EXE` at a DOS prompt | 🟠 **partially** — referenced in the 2026-08-23 comment on #4, but not addressed to them directly and no result to report yet |
| **@TC1995** | Asked us to implement the Mach8 **RAM banks** so the self-test reports "Ok." rather than "RAM addressing" (#4) | 🟠 Not started. Note this is a *different* defect from the PIT delay-loop hack we ship — would be a second upstream PR | ❌ not yet — worth saying so explicitly rather than leaving them thinking it's covered |
| **@TC1995** | 2026-08-25 on [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8#issuecomment-5412550427): "By RAM banks, I mean the Mach8 ones, not the VGA side ones, especially during the self-test procedure, it does some bitblt tests, then a rectangle fill test and a pixtrans read to return 0xaaaa, 0x5555, etc. (in 8514/A mode)." | ✅ **Correcting two wrong readings of mine.** Checked against source: `mach->bank_r`/`bank_w` (regs `0xB2`/`0xAE`) are used in exactly **one** place in `vid_ati_mach8.c` — feeding `svga->read_bank`/`write_bank`, the **VGA aperture**. The accelerator path (bitblt / rect fill / PIXTRANS) addresses `dev->vram` with **no bank applied**. Self-test drives the accelerator, so banks alias → "RAM Addressing". **Hypothesis, not traced** | ⏳ **replied 2026-08-25** on [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8#issuecomment-5415251550) with the code evidence and **four specific questions** (which register selects the accelerator bank; bank-relative vs windowed addressing; the self-test sequence; meaning of `0007`). Offered an I/O trace of `0x82E8`–`0xE2E8`. **Awaiting his answer — blocked on information, not effort** |
| **@andrew-hoffman** | — (correction owed to him, not input from him) | ⚠️ **I overstated the upstream gap and corrected it.** Upstream *does* truncate: `dma[addr].page = dma_at ? val : val & 0xf`. What is missing is that it gates on `dma_at` (= `is286`), so an XT board with a 386 on it is handed 8 bits. Fix is **one line** — gate on `dma_force_xt \|\| !dma_at` | ✅ **corrected 2026-08-25** on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5415254386). Told him plainly the PR is still not written rather than implying otherwise |
| **@andrew-hoffman** | Outcome of his 20-bit DMA lead, emulator side | ✅ **SHIPPED UPSTREAM.** [86Box/86Box#7771](https://github.com/86Box/86Box/pull/7771) raised 2026-08-25, +14 −2 in `src/dma.c`: gate the page-register truncation on `dma_force_xt \|\| !dma_at` rather than `dma_at` alone. Built clean at upstream `2f6fff3`. PR states explicitly what is proven (equivalence for non-forcing machines, by inspection; the predicate already validated on real hardware) and what is **not** (no boot regression matrix across other XT/AT machines). His sources cited: os2museum ×2, MartyPC book, 386MAX `MARK_XT` | ✅ **told 2026-08-25** on [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3#issuecomment-5415429893). Four suggestions from him, all four now acted on |

---

## Closed out — loop already closed

| Contributor | What they gave | Outcome | Told? |
|---|---|---|---|
| **@QuantumByteRider** | Reported POST 101 and the duplicate `(1988) i386SX` entry on 86Box PR #7626 | ✅ **Root-caused and fixed** — the 1982 ROM incompatibility. Duplicate entry had been fixed in-tree by someone else | ✅ closing comment on #7626 (2026-08-22) |
| **@Fenix770** | Reported #7638 — memory reported "BAD" regardless of `mem_size`; suggested dev-branching the machine | ✅ **Same 1982-ROM cause**; they were also on `i386dx`, hitting a second defect simultaneously | ✅ replied on #7638, asked for a re-test rather than leaving the dev-branch suggestion unanswered |
| **@Fenix770** | Follow-up 2026-08-23 (our issue #11): still no extended memory after the updates, “random Shadow RAM errors” — **and attached his VM as a zip** | ✅ **The attachment was the contribution.** Run directly, it root-caused the shadow-RAM failure: 86Box places the high BIOS-shadow alias at `0xF0000 + mem_size*1024`, but the driver targets a **fixed `0x5F0000`** — the two agree only at mem_size 5120, the size the original trace was taken on. He had already added `NODIAGS` himself, so his disk reproduced the *next* failure cleanly. It also exposed that two earlier in-house attempts at this question had recorded “no difference at any memory size” from runs that were stalled at the POST F1 prompt and had never booted DOS | ✅ **replied 2026-08-24** on [#11](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/11#issuecomment-5397684273). Gave him the full measurement, said plainly there is **no fix yet** and why the one-line change is not shippable, confirmed his `NODIAGS` config is the right one to stay on, and **warned him off `mem_size = 5120`** — the obvious-looking workaround is the one path that crashes the emulator |
| **@OBattler** | The `mem_size` 3072/5120 granularity answer on #7638 | ✅ Correct — that part was expected behaviour, not the failure | ✅ acknowledged in the #7638 reply |
| **Al Williams** (Dr. Dobb's, Hackaday) | His real 1990 `a20()` code | ✅ **Ports match this project exactly** — saved as `docs/al_williams_inboard_a20_correspondence_2023.md` | ✅ credited; worth a further note if the A20 path ever produces a new finding |
| **Wim Osterholt** | "XT, AT and PS/2 I/O port addresses" (1994) | ✅ Already produced a lead — port `0x00A0` is the **NMI mask register** on an XT, and our Inboard device also claims `0xA0` | n/a — 1994 document, credited in README |

---

## Foundational — credited, no open loop

| Contributor | Contribution |
|---|---|
| **SuperFury** (UniPCemu) | The entire Inboard 386/PC hardware model is a direct port of `hardware/inboard.c` |
| **Stynx & Harrison Frazier** (VCFed) | Designed the 4 MB Inboard daughterboard, without which Win95 wouldn't run at all |
| **CimonVg** | Ongoing work pushing the Inboard to its limits; inspiration and support |
| **RonnyRoy** | Reproducing the Inboard as cloned hardware — possibly the path past the 4 MB ceiling |
| **Kevin Moonlight** | Original author of COMrade |
| **Ahmad Byagowi** | Ported COMrade to Windows 95 (`COMR95.EXE`) — the live real-hardware introspection path |
| **viti95** (FastDoom) | Real-hardware-validated XT keyboard ISR reference |
| **Microsoft Windows 95 DDK** | The genuine period source and toolchain behind the custom `VKD.VXD` |

---

## Community write-ups relied on

| Contributor | What they gave | Where it landed |
|---|---|---|
| **Feipoa** (Vogons) | Extensive write-up on driving 486 upgrade-chip registers directly, and the pointer to the **CTCHIP / KTCHIP34** tool. Source: <https://www.vogons.org/viewtopic.php?t=45756> | **Issue #9, closed.** `CTCHIP34` from `AUTOEXEC.BAT` replaces `revto486.sys` entirely on the real 5160 + Inboard - confirmed booting to Windows, stable, and faster. Win95 had been running 1:1 clock with no cache at all; this recovered both the 2x multiplier and 640 KB of cacheable memory. The same post also carried the V86-mode clue that shaped the earlier Revto486 investigation |

Their account of ditching a vendor driver that "would even crash my machine" and going at the
registers directly with `KTChip34` is what made the register-level route look viable here at all.
Two things it saved us from: trusting `revto486.sys` on hardware it was never validated against, and
guessing at the register set - the values we shipped came from the real machine's own `CTCHIP34`
screens and were then independently corroborated by `REVTO486`'s own MSR dump.

**Not yet told.** Owed a reply on the thread saying it worked on a 5160 + Inboard 386/PC, which is
not a configuration that post was written for. Attribution of post `p1392798` to Feipoa is per the
project owner; the doc that recorded the quote did not capture an author.

---

## When reporting back, include

What makes a reply worth sending — and worth replying *to*:

1. **The specific evidence.** "Verified — `CF` at file offset `0x7F53` in both 1986 ROMs" beats
   "you were right, thanks."
2. **What it replaced, and why theirs is better.** Shows the suggestion was understood, not just
   applied.
3. **Where it landed.** A PR or commit link makes it real and lets them see it in context.
4. **An honest negative result if that's what happened.** "We tried it, here's what we saw, it
   wasn't that" is genuinely useful to the person and keeps the exchange open. Silence after a
   failed suggestion is what stops people offering.
5. **Their full name or handle**, correctly.
