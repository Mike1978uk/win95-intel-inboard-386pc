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
| **@andrew-hoffman** | XT DMA page register is only 4 bits → the SB Pro DMA buffer must live under 1 MB (#5, 2026-08-20) | 🟡 Untested on hardware, but the **strongest live lead** on the project. Fits a page fault in VDMAD's buffer handling well | ❌ not yet — report either way after the `DMABufferIn1MB=Yes` test |
| **@andrew-hoffman** | Follow-up 2026-08-23: primary sources (os2museum ×2, martypc book) **and** the pointer at the 386MAX source | ✅ **Paid off twice over.** 386MAX's `MARK_XT` sets `DMA_MAX = 0A0000h` — the XT ceiling is **640 KB, not 1 MB**, stricter than we assumed; and it independently confirms port `0xA0` as the XT NMI register (plan step 5). Checking it also surfaced an emulator fidelity bug: `dma_at = is286` makes 86Box give the Inboard a full 8-bit DMA page register where real hardware latches 4 — a concrete mechanism for why the old `vmad` patch passed in emulation and failed on hardware | ✅ **replied 2026-08-23** on [#5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5#issuecomment-5385997588) — gave him the 640 KB `MARK_XT` evidence, the `dma_at` fidelity bug his pointer surfaced, and an explicit "nothing is fixed yet"; asked him to keep the leads coming |
| **@TC1995** | Mach8/Graphics Ultra EEPROM "power up configuration" angle (#7) — independently matched #4's own finding | 🟡 Untested. Unblocking action is `C-INFO.EXE` at a DOS prompt | ❌ not yet |
| **@TC1995** | Asked us to implement the Mach8 **RAM banks** so the self-test reports "Ok." rather than "RAM addressing" (#4) | 🟠 Not started. Note this is a *different* defect from the PIT delay-loop hack we ship — would be a second upstream PR | ❌ not yet — worth saying so explicitly rather than leaving them thinking it's covered |

---

## Closed out — loop already closed

| Contributor | What they gave | Outcome | Told? |
|---|---|---|---|
| **@QuantumByteRider** | Reported POST 101 and the duplicate `(1988) i386SX` entry on 86Box PR #7626 | ✅ **Root-caused and fixed** — the 1982 ROM incompatibility. Duplicate entry had been fixed in-tree by someone else | ✅ closing comment on #7626 (2026-08-22) |
| **@Fenix770** | Reported #7638 — memory reported "BAD" regardless of `mem_size`; suggested dev-branching the machine | ✅ **Same 1982-ROM cause**; they were also on `i386dx`, hitting a second defect simultaneously | ✅ replied on #7638, asked for a re-test rather than leaving the dev-branch suggestion unanswered |
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
