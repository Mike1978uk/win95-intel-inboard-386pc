# Outstanding work plan — as of 2026-08-23

Captures every open thread from correspondence, external clues, and the GitHub issue tracker, with
a recommended order of attack. Written after the 86Box POST 101 work closed out
([#7749](https://github.com/86Box/86Box/pull/7749) open, `86box_full/` ported, release rebuilt).

**Ordering principle:** time-sensitive first, then infrastructure that makes everything else cheaper
to debug, then real bugs ordered by *strength of the lead* and *cost of the next test*, then
cleanup. Within each item, the cheapest falsifying test comes before any code change — the SB Pro
`vmad.vxd` patch was written and failed on real hardware without a cheap test first, and that is the
mistake to avoid repeating.

**Source-quality warning:** several clues below came from an AI assistant and carry citation markers
but no primary source. They are recorded as **hypotheses to test**, not facts. Each is marked.

---

## ▶ EXECUTION ORDER — start here

> **Companion doc:** [`issue_tiered_action_plan.md`](issue_tiered_action_plan.md) maps the same work
> **by GitHub issue** — current status, next concrete action, and what each unblocks. This file is
> ordered by *execution sequence*; that one by *issue*. Read both before starting.
>
> Two corrections it records that supersede assumptions here: **#5's** earlier byte-patch fix was
> already made *and still failed on real hardware*, so the DMA-buffer lead is genuinely the live
> one; and **#8's** actual request from @TC1995 is to implement the Mach8 **RAM banks** in
> `vid_ati_mach8.c` — a different defect from the PIT delay-loop hack we currently ship, and a
> second upstream PR if pursued. Per the user, #8 is a **low-priority nice-to-have** likely to fall
> out of #4.

Each step names its **first concrete action** and **what result would falsify it**, so no step turns
into open-ended poking. Confidence is stated honestly: three of these are verified facts, the rest
are leads of varying strength.

### Confidence key
- 🟢 **Verified** — checked at byte/code level this session, will work
- 🟡 **Strong lead** — specific mechanism, cheap decisive test, outcome genuinely unknown
- 🟠 **Hypothesis** — plausible, unverified source, test in cost order
- 🔴 **Blocked** — precondition fails; needs its blocker cleared first

---

### 1. 🟢 Move the MCP servers off the legacy `COMrade_Latest` path
**Why first:** costs minutes, needs a session restart anyway, unblocks step 6 — and both MCP servers
are currently pointed at **`RiderProjects\COMrade_Latest`, which is an old path for this project**.
That is a tidy-up owed regardless of the capability argument.

**Action:** in `~/.claude.json`, point `comrade` (COM2, real hardware) and `comrade86box` (COM3,
emulator) at the current tree — **`RiderProjects\Open-Source-PC110\Software\COMrade`** — and
collapse the **two duplicate `comrade` entries** into one (a global/project scope overlap: one uses
the `COMrade_Latest` venv python, the other `python3` + `PYTHONPATH`).

**Verified safe:** Ahmad's protocol is a strict superset of Kevin's — all 44 opcodes present at
identical values, +13 added; no MCP tool removed, +10 added. **No DOS capability is lost.**

**Two things NOT to delete:**
- **`COMrade_Latest` must stay on disk.** It also holds
  `XT_5160_rework_claude\INBOARD_86BOX_PORT_PLAN.md`, the 4,227-line source-of-truth port plan.
  Only the *MCP paths* move; the directory itself is still referenced.
- The card's `COMRADE.EXE` — it is byte-identical to Kevin's upstream and works. Leave it.

**Also tidy:** `RiderProjects\COMrade-yyzkevin` was cloned this session purely to compare trees. It
is identical to `COMrade_Latest`'s upstream and can be removed once the comparison is no longer
wanted.

**Falsified if:** a base DOS op (`mem_read`, `io_in`) errors after the switch. Revert is one edit.

### 2. 🟢 Adopt Michal Necasek's `F000:FF53`, in the emulator *and* on real hardware
**Why now:** PR #7749 is in review; better to land it there than as a later follow-up.
**Verified:** byte at `F000:FF53` is `CF` (IRET) in *both* 1986 ROMs (offset `0x7F53`, U18/F800).
**Action, both halves — they must stay in step:**
1. `386_dynarec.c` → point `0000:01A0` at `F000:FF53` (`53 FF 00 F0`), delete the `0x3C0` stub write
2. `ivt68fix/IVT68FIX.ASM` → same change, rebuild, redeploy to `D:\IVT68FIX.COM`
Rebuild, one boot-to-desktop each, push to #7749.
**Falsified if:** Win95 no longer reaches the desktop — then the `0x3C0` stub was doing something
beyond providing an IRET, which would itself be worth knowing.

### 3. 🟢 Reply to Michal Necasek — step 2 is not done without this
Byte evidence, what the fix looked like before and why his is better, PR link. Plus his actual
question about whether the Inboard could ever have held enough RAM for Win9x. Use his full name.

### 4. 🟡 Sound Blaster Pro / `vmad` BSOD — issue #5
**The strongest lead of the whole list**, and the one most likely to convert into a fix.
**Mechanism:** XT DMA page register is 4 bits → 20-bit reach → the DMA buffer **must** be under
1 MB. Confirmed adjacent detail: SB Pro is on **DMA 1**, so its page register is **`0x83`** (the
channel→port mapping is not numeric). The port list's `0080-008F` section documents the **AT**
`74612` width — do not read 8 bits into it for this machine.
**Action, in this order — do not skip to the code:**
1. `SYSTEM.INI` `[386Enh]` → `DMABufferIn1MB=Yes`, `DMABufferSize=32`. Reboot. *(Zero code. Same
   edit is the real-hardware equivalent.)*
2. If unchanged: force a Win3.1-era 8-bit `.drv` via `[drivers]`, bypassing the protected-mode VxD
   audio stack. Lower fidelity, but "sound at all" is the milestone.
3. Only then redo the `vmad.vxd` disassembly — **from scratch**, not patching the failed one.
**Falsified if:** step 1 changes nothing *and* the BSOD stack shows no DMA-address involvement.

### 5. 🟡 NEW — trace port `0x00A0` in the emulator
**Why:** on an XT, `0xA0` is the **NMI mask register**; it is PIC 2 only on an AT. Our
`inboard386.c` *also* claims `0xA0` (`port_a0`). Two consumers of one port on the real machine.
**Action:** I/O-dispatch hook (skill Technique 47) logging every `0xA0` access through POST — who
writes what, when, and whether the Inboard shadow swallows a BIOS NMI-mask write or vice versa.
**Falsified if:** the BIOS never touches `0xA0`, or the accesses are cleanly interleaved.
**Emulator-side and cheap** — can run alongside the real-hardware steps.

> ### ✅ RESULT, 2026-08-23 — **falsified. No conflict. Close this step.**
> Came for free as a by-product of the step 2 boot verification (`a0a1trace.txt`), plus a code read.
> Three independent reasons the "two consumers fight over `0xA0`" worry does not hold:
>
> 1. **Both handlers are registered and both see every write.** 86Box chains I/O handlers, so
>    `nmi_init()`'s `nmi_write` on `0x00A0`–`0x00AF` (`src/nmi.c`) and `inboard386_write_a0()`
>    (`src/device/inboard386.c:530`) each receive the byte. Nothing is swallowed by either side.
>    This also matches real hardware, where the Inboard's decode and the motherboard NMI latch both
>    sit on the same bus and both latch the write.
> 2. **The Inboard's copy is inert.** `inboard386_write_a0()` stores into `dev->port_a0`, but
>    `inboard386_apply_waitstates()` computes its value from `dev->speed` — it never reads
>    `port_a0`. So an NMI-mask write cannot perturb memory timing.
> 3. **The live trace shows pure NMI-mask semantics, cleanly paired** — the step's own stated
>    falsification condition. Only ever `0x80` and `0x00`, matching 386MAX's `NMIENA equ 80h` /
>    `NMIDIS equ 00h` exactly; never a PIC ICW/OCW pattern:
>
> ```
> [a0a1trace] t=114.3 WR_A0 val=80 CS:PC=F000:0000E694   <- BIOS POST, NMI enable
> [a0a1trace] t=122.9 RD_A1 val=FF CS:PC=0070:00000923   <- PIC-2 probe, correctly floats to FF
> [a0a1trace] t=184.6 WR_A0 val=00 CS:PC=0206:0000A62B   <- driver, NMI disable
> [a0a1trace] t=185.3 WR_A0 val=80 CS:PC=0206:0000A66A   <- ...matching re-enable
> ```
>
> The `RD_A1 = FF` is also correct-by-accident confirmation that there is no second PIC here.
>
> **Honest limit:** the trace hook is gated on `clock() > 90.0` s, so writes in the first 90 s of
> wall-clock are not logged. The `F000:E694` entry proves the BIOS does touch `0xA0`, but an
> exhaustive early-POST census would need the gate lowered. Nothing seen suggests it would change
> the conclusion.

### 6. 🟠 GUI-stage issues — #3, #4, #6, #7
**Gated on step 1**, which is what makes `desktop_screenshot` work. Until then these can only be
debugged by photographing the screen.
Once live: triage each, comment the lead on the issue, and **close the dead ends outright** — #7
(setup hangs before Help files, reboot works around it) is the likely close-as-documented candidate.

### 7. 🟠 Mach8 boot RAM test — issue #8
Currently **worked around, not root-caused** (`AX = BX+1` at `C000:7B16/7B23/7B37`, shipped in #7749
and labelled as such).
**Test in cost order:** declared VRAM size mismatch → EEPROM/`SETMACH.EXE` "video test on boot"
toggle not persisting → emulator's uninitialised-VRAM fill pattern.
**Concrete first probe:** `02E8` read = **8514/A display status**. Compare emulator vs real hardware
via COMrade's DOS port I/O (Technique 6). Note the 8514/A registers are sparse across four ranges:
`02E8`, `06E8`, `0AE8`, `0EE8`.
**Hex-patching the option ROM is LAST RESORT** — it diverges emulator from hardware, against the
project's fidelity goal.

### 8. 🔴 Revto486 — issue #9 (blocked, do not treat as quick)
The vogons clue says the driver needs the CPU already in 386 protected mode with DOS in V86.
`CONFIG.SYS` has it commented out and **no EMM386 at all** — so there is no V86 provider, which fits
the clue exactly. **But** adding EMM386 walks straight into the unresolved `0128:009B` halt
([[xt-emm386-halt-0128-wildjump-2026-08-03]]).
**So the real first task is that EMM386 bug, not the driver.** Either clear it or find another V86
provider. Budget accordingly.

### 9. Cleanup
Issue **#2** (`#` in place of `\`): the port list says where *not* to look — `0060-0063` are PPI and
port `0x64` does not exist here, so this is a **layout/scancode** problem, not a controller one.
`AUTOEXEC.BAT` already runs `keyb uk`. Start there.

---

## Honest assessment of what the new input actually buys

**Likely to produce real fixes:** #5 (sound) — genuinely new mechanism with a free test. The
`F000:FF53` change is a certain improvement, though it fixes nothing that was broken.

**Newly *investigable* rather than fixed:** #3, #4, #6, #7 — COMR95 turns "photograph the screen"
into live introspection, but does not itself diagnose anything.

**New lead that did not exist before:** the `0x00A0` NMI-mask collision. Could be nothing; could
explain emulator-vs-hardware divergence.

**Not moved:** #9 is now understood to be *harder* than it looked, not easier. #8 has three
hypotheses but no verified mechanism. Being clear about that is the point of the confidence key —
the plan should not read as if everything is about to fall over.

---

## Findings from inspecting the real CF card (mounted at `D:`, 2026-08-23)

The card is the live real-hardware disk — `MS-DOS_6`, FAT, 2 GB, ~1.59 GB free, with `INBRDPC.SYS`,
`REVTO486.SYS`, `IVT68FIX.COM`, `SBPRO\`, and the Win95 install. Three things it changed about the
plan below:

### A. `COMR95.EXE` is now deployed — Phase 1a is done
Copied to `D:\COMR95.EXE` and MD5-verified (`1de004db…`). `COMRADE.EXE` (DOS) was already at the
root.

⚠️ **Version mismatch to resolve:** the card's `COMRADE.EXE` (`dfb2cab4…`, dated 2 Jul) is **not**
the same binary as the local `dist\COMRADE.EXE` (`f1bee5ea…`). `COMR95.EXE` came from the newer
tree. If the two ends of the protocol disagree, that will look like a COMrade bug rather than a
version skew. Decide whether to refresh the card's DOS binary from the same tree — **not** done
unprompted, since it means overwriting a known-working file on real hardware.

### B. Phase 0 has a real-hardware counterpart that must change with it
`D:\IVT68FIX.COM` (26 bytes, run from `AUTOEXEC.BAT`) is the real-hardware equivalent of the
emulator's `[patchint68]` fix, and it uses **the identical `0x3C0` stub approach**:

```
33 C0              xor ax, ax
8E C0              mov es, ax
26 C6 06 C0 03 CF  mov byte [es:03C0], 0CFh     ; IRET stub
26 C7 06 A0 01 C0 03   mov word [es:01A0], 03C0h
26 C7 06 A2 01 00 00   mov word [es:01A2], 0000h
CD 20              int 20h
```

So adopting `F000:FF53` in the emulator **also means rebuilding `ivt68fix/IVT68FIX.ASM`** and
redeploying to the card, or emulator and hardware diverge — which is exactly what the project's
real-hardware-equivalence convention exists to prevent. The new version is *simpler*: no stub write
at all, just `mov word [es:01A0], 0FF53h` / `mov word [es:01A2], 0F000h`.

### C. Phase 2b is more concrete — and probably blocked
`CONFIG.SYS` currently has the driver **commented out** and, critically, **no EMM386 at all** —
only `HIMEM.SYS`:

```
DEVICE=c:\INBRDPC.SYS NODIAGS NOPAUSE
DEVICE=C:\WINDOWS\HIMEM.SYS
DOS=HIGH,UMB
...
REM DEVICE=C:\revto486.sys /BL /CN /CCM /2
```

There is therefore **no V86 provider on this machine**, which is precisely the precondition the
vogons clue says `revto486.sys` requires. That makes the test concrete — but it runs straight into
[[xt-emm386-halt-0128-wildjump-2026-08-03]]: **EMM386 is itself an unresolved blocker here**, halting
with `error #04 in an application at memory address 0128:009B`, a wild jump into a FAT
directory-entry buffer entered from IO.SYS's own low-memory boot code. So Phase 2b likely cannot be
tested until either that bug is resolved or a different V86 provider is found. Budget for it
accordingly, and do not treat 2b as the "quick one".

---

## Findings from the COMrade sources and the XT port list (2026-08-23, late)

### D. COMrade: the version question resolved, and a real gap found
Three trees exist, and they are **not** interchangeable:

| Tree | What it is | `COMRADE.EXE` | `COMR95.EXE` |
|---|---|---|---|
| `COMrade_Latest` | clone of **Kevin's** `yyzkevin/COMrade` — **this is the host bridge the MCP servers actually run** | `dfb2cab4` | — |
| `COMrade-yyzkevin` | fresh clone of Kevin's upstream (new, 2026-08-23) | `dfb2cab4` | — |
| `Open-Source-PC110/Software/COMrade` | **Ahmad's** fork — adds the Win95 agent | `f1bee5ea` (57 KB) | `1de004db` (21 KB) |

**My earlier "version skew" warning pointed the wrong way.** The card's `COMRADE.EXE` is byte-identical
to Kevin's, which is the host we run — so **the DOS side is correctly matched and should be left
alone.** Do *not* copy Ahmad's `COMRADE.EXE` onto the card; that would break the match.

`COMrade_Latest` is **0 commits behind** Kevin's upstream (6 local-only commits ahead), so nothing to
pull there.

**The real gap:** Ahmad's fork is a protocol **superset**, and the host bridge we run does not
implement the additions. Ahmad adds:
- `0x30 WIN_SCREENSHOT` / `0xB0 WIN_SCREENSHOT_DATA` — the Win95 desktop thumbnail
- `0x11–0x15` `BUS_STIM`, `IDX`, `IO_RMW`, `PIC`, `SAFE` (+ `0x91–0x95` replies) — repeated/composed
  bus cycles for logic-analyzer-style work, behind a compiled-in **write-guard** deny-list of ports
  where a stray write can hang or power off the box

Confirmed by grep: `WIN_SCREENSHOT` exists in Ahmad's `comrade/protocol.py` and `connection.py`, and
is **absent from the host bridge we run**.

**So the `COMR95.EXE` now on the card will connect and do the base ops — but `desktop_screenshot`
will not work**, which was the main reason Phase 1 came before Phase 3. **Action: point the MCP
`comrade` server at Ahmad's tree** (`Open-Source-PC110/Software/COMrade`) rather than
`COMrade_Latest`. Being a superset it should still speak to Kevin's DOS agent for base ops. Not
changed unprompted — it edits `~/.claude.json` and affects live tooling.

The `BUS_STIM` / `IO_RMW` / `PIC` ops are independently interesting for this project — they are
exactly the "what does real hardware actually do at this port" primitives that skill Technique 6
wants, with a safety guard already built in.

### E. Wim Osterholt's XT/AT/PS-2 port list is genuinely useful — curated in a new doc
See **[`xt_io_port_reference_annotated.md`](xt_io_port_reference_annotated.md)**. Its value is not
the port numbers but the **`(XT)` / `(XT only)` markings**, since most of our Win95 problems are the
OS assuming AT hardware this machine lacks. Highlights:

- **⚠️ NEW LEAD — port `0x00A0` is the NMI mask register on an XT** (it is PIC 2 only on an AT). Our
  Inboard device *also* claims `0xA0` (`port_a0` in `inboard386.c`). Two consumers of one port on
  real hardware; worth an I/O trace to see whether BIOS NMI-mask writes and the Inboard shadow
  interfere. Not investigated yet.
- **SB Pro is on DMA 1 → page register `0x83`** (the channel→port mapping is not in numeric order).
  And the list's `0080-008F` section documents the **AT** `74612` width — the XT's 4-bit page latch
  is the actual constraint, which is exactly the basis for the `DMABufferIn1MB=Yes` test in 2a.
- **8514/A registers are sparse across four ranges** (`02E8`, `06E8`, `0AE8`, `0EE8`). `02E8` read =
  display status, the natural first emulator-vs-hardware comparison for the Mach8 self-test question
  in 2c.
- **`0060`–`0063` are PPI, and port `0x64` does not exist here** — primary-source confirmation of the
  whole `VKD.VXD` story, and a signpost that issue #2 (`#` vs `\`) is a layout problem, not a
  controller one.
- **⚠️ `0340-0357` is the primary XT RTC range** — and our Trantor T130B is configured at `0x340`.
  Not causing a problem today, but a latent conflict if any RTC driver is added.

---

## Phase 0 — while PR #7749 is still in review (time-sensitive)

### 0a. Adopt Michal Necasek's `F000:FF53` suggestion — VERIFIED, do this first

Michal Necasek (2026-08-09) asked: *"if you need an IRET instruction for an interrupt vector to point to,
why not use the one at F000:FF53?"*

**Checked, and he is right.** Byte at `F000:FF53` is `CF` (IRET) in **both** supported ROMs:

| ROM | file offset `0x7F53` in the U18/F800 chip | bytes |
|---|---|---|
| 09MAY86 | `BIOS_5160_09MAY86_U18_…_F800.BIN` | `cf 1e e8 b9 fa …` |
| 10JAN86 | `BIOS_5160_10JAN86_U18_…_F800.BIN` | `cf 1e e8 ba fa …` |

Our current `[patchint68]` fix writes an IRET stub to physical `0x3C0` — **borrowing INT F0h's own
4-byte vector-table slot as scratch code space** — then points INT 68h at it. Michal Necasek's version is
strictly better on every axis:

- no injection of executable bytes into guest memory at all, just a vector write
- no assumption that INT F0h is unused (that assumption was always the weakest part of the fix)
- points at the BIOS's own canonical dummy-interrupt handler, which is what the BIOS itself uses
- 4 bytes written instead of 5

**Action:** change the vector to `0000:01A0 = F000:FF53` (`53 FF 00 F0`), drop the `0x3C0` write,
update the comment to credit Michal Necasek, rebuild, re-verify one Win95 boot, push to #7749. Do it while
the PR is under review rather than as a later follow-up.

**Caveat to check during the rebuild:** the fix currently fires when `CS` first becomes `0x0EAF`.
That timing logic is unchanged — only the vector target changes — so a single boot-to-desktop run is
sufficient re-validation.

### 0b. Reply to Michal Necasek — treat as a deliverable of 0a, not an optional courtesy

He gave us a correct, checkable improvement to a fix that was already public in a PR. Closing the
loop with the actual outcome is the whole point of the exchange, so **0a is not done until 0b is
sent.** Include the concrete evidence, not just "thanks, done":

- the byte check (`CF` at `0x7F53` in the U18/F800 chip of both 1986 ROMs)
- what the fix looked like before (IRET stub written into INT F0h's vector slot at `0x3C0`) and why
  his version is better (no injected code, no assumption about INT F0h being unused)
- the PR link, so he can see it in context

Two things owed in total:
1. **Credit + confirmation** on `F000:FF53`, once 0a is pushed.
2. **His actual question**, still unanswered: *"back in the day Win9x could never support the
   Inboard 386/PC because there was not enough RAM to run Win9x in the first place?"* — worth
   answering properly. The card tops out well below what Win95 wants, and our own working setup runs
   5 MB, which is not a period-plausible Inboard configuration. His framing is probably right and
   it is a genuinely interesting point about *why* nobody hit these bugs in 1995.

He also observed that the keyboard being the biggest blocker is unsurprising, since the 8042 was
baked into the PC/AT design and Win9x had no reason to abstract it — which matches exactly what the
custom `VKD.VXD` work found.

---

## Phase 1 — infrastructure that unblocks the rest

### 1a. Get COMrade onto the CF card — ✅ DONE 2026-08-23

`COMR95.EXE` copied to `D:\` and verified; `COMRADE.EXE` was already there (see finding A above for
the outstanding version-skew question). Remaining optional step: `AUTOEXEC.BAT` has
`REM c:\Comrade.exe` commented out, so autostart is off — leave it manual unless a boot-time bridge
turns out to be useful.

Both binaries are also **present locally**, if the card ever needs re-imaging:

```
%USERPROFILE%\RiderProjects\Open-Source-PC110\Software\COMrade\dist\COMRADE.EXE   (DOS)
%USERPROFILE%\RiderProjects\Open-Source-PC110\Software\COMrade\dist\COMR95.EXE    (Windows 95)
```

Put **both** at the root of `C:` on the CF card for easy invocation. On Win95:

```
COMR95 /com1 /baud 115200
```

COMR95 gives HELLO/status, text screen, **desktop thumbnails**, file read/write, directory listing,
attributes, CRC hashing, and keyboard input — i.e. it makes the GUI-stage bugs (issues #3, #4, #5,
#6, #7) debuggable *in place* on real hardware instead of by photographing the screen. That is why
it comes before them.

Reminder: COMrade is **real-hardware-only** — it is not a route for emulator debugging.

---

## Phase 2 — the real bugs, strongest lead first

### 2a. Sound Blaster Pro 2 (CT1600) → `vmad` BSOD — issue #5

**Status:** a `vmad.vxd` patch was written on the theory that Win95 was touching the non-existent
secondary 8237 (ports `0xC0`–`0xDF`). It **failed on real hardware** (photo evidence). Do not
re-patch on the same theory.

**The stronger new lead**, from andrew-hoffman:

> The PC DMA controller only has a 4-bit page register so you will need to make sure any buffer the
> sound blaster DMAs from is in the first 1mb of RAM that the DMA controller can see.

This is a much better fit for the symptom than the secondary-controller theory, and — importantly —
**Windows already has a documented setting for exactly this**, so it can be tested with no code at
all. Try, in `SYSTEM.INI` `[386Enh]`:

```ini
DMABufferIn1MB=Yes
DMABufferSize=32
```

**Do this before touching `vmad.vxd` again.** If the buffer placement is the cause, this either
fixes it or changes the failure mode, and either outcome is informative for the cost of an INI edit.
Requires the real-hardware-equivalence note per project convention: this is a `SYSTEM.INI` edit, so
the real-hardware equivalent is identical — the same edit on the CF card.

**Fallback if that does not resolve it** — drop out of the protected-mode VxD audio stack entirely
and force the Windows 3.1-era 8-bit driver via `SYSTEM.INI` `[drivers]` (Sound Blaster 1.5/2.0 style
`.drv`). *(Hypothesis, AI-sourced, unverified.)* This yields simple unmixed 8-bit audio that respects
native XT interrupt routing. Lower fidelity, but "sound works at all" is the milestone.

**Only after both cheap routes fail** should the `vmad.vxd` disassembly be redone — and per the
existing triage note, redone from scratch rather than patched further.

Card choice is not the problem: the CT1600 is natively 8-bit ISA and is the right part for this bus.

### 2b. Revto486.sys TSR halts during boot — issue #9

**The clue** (vogons, Evergreen `revto486.sys` on a Model 30):

> It only works when I make sure the CPU is in 386 protected mode before loading the Evergreen
> driver and DOS is running in virtual x86 mode.

So the driver expects to be loaded *into* an already-established V86 environment, not on bare real
mode. **Cheap test:** change CONFIG.SYS load order so a V86 provider (EMM386 or equivalent) is
active *before* `revto486.sys` loads, rather than after. No patching, one reboot.

If that works on DOS, the Win95 question becomes how to reproduce that ordering during Win95's own
boot — which is a second, separate step, and should not be attempted until the DOS-level ordering is
confirmed to matter.

### 2c. ATI Mach8 ROM RAM test on boot in emulator — issue #8

**Current state:** worked around, not root-caused. `inboard_post_fixups()` forces `AX` past `BX` at
`C000:7B16` / `7B23` / `7B37` so the option ROM's PIT-readback delay loop resolves. That hack is now
in upstream #7749, honestly labelled as such.

**Hypotheses to test** *(all AI-sourced and unverified — treat with suspicion, especially the
architecture claims)*:

1. **Video memory size mismatch.** If the emulator profile declares a different VRAM size than the
   ROM scans for, the ROM may loop trying to isolate "missing" banks. Cheapest test of the three:
   compare the declared size in the 86Box Mach8 device against what the ROM expects.
2. **EEPROM / config override.** The real card stores config in an onboard EEPROM written by ATI's
   `INSTALL.EXE` / `SETMACH.EXE`. If there is a "Video Test on Boot" / "Co-processor Test" toggle,
   and 86Box is not persisting the virtual EEPROM cleanly, the ROM would re-run the test every boot.
   Testable by running the ATI DOS utility under the emulator and seeing whether the setting sticks
   across a restart.
3. **Uninitialized VRAM pattern.** Claim is that emulators may map the framebuffer with a different
   fill pattern than a real cold boot, so the ROM sees "changed/corrupt" hardware. Plausible, and it
   would explain why real hardware slips past the test while the emulator does not.
4. **Last resort:** hex-patch the dumped Mach8 option ROM to NOP the diagnostic call. This is
   explicitly the *worst* option — it diverges the emulator from real hardware, which cuts against
   the project's hardware-fidelity goal. Only if 1–3 are exhausted.

**Reference material to mine first:**
- <https://www.ardent-tool.com/video/ATI_8514_Ultra.html> — detailed Mach8 documentation
- `%USERPROFILE%\OneDrive\Desktop\XT_project\inboard_files\win95_attempts_files\XT, AT and PS2 IO port addresses.txt`
  — has Mach8 ROM addresses plus the I/O and ROM addresses Windows 95 uses

Note the architecture description in the source ("28800 VGA core + 38800 coprocessor") should be
checked against the ardent-tool page before being relied on.

---

## Phase 3 — remaining issue triage and cleanup

Work these once COMrade is on the CF card (Phase 1), since most are GUI-stage and become far cheaper
to diagnose with a live bridge.

| Issue | Title | Notes |
|---|---|---|
| #2 | Keyboard mapping, `#` in place of `\` | UK vs US layout; likely a scancode-map issue, not hardware |
| #3 | Browse button in "Have Disk" causes a fault | GUI-stage — needs COMrade |
| #4 | ATI Mach8 stock driver not working | Related to #8; the display-driver address ranges were offered as a place to trace |
| #6 | PS/2 mouse appearing in Device Manager | Cosmetic-ish but wrong — no 8042 on this machine |
| #7 | Setup hangs to black screen before Help files | Known workaround: reboot. May be a dead end worth closing as "documented workaround" |

**For every issue:** post a comment with whatever lead exists, even partial. **Close the dead ends
explicitly** rather than leaving them open — #7 is the obvious candidate if the reboot workaround is
the honest answer.

---

## Not forgotten, lower priority

### 💡 IDEA (logged 2026-08-23, user-raised, LOW priority) — a BIOS-extension TSR to make older ROMs usable

**Tracked as [issue #10](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/10).**

**The thought:** the POST 101 work established *why* the 1982 ROMs fail. If the reason is understood,
could a loadable shim supply what the old ROM lacks — the way **2M** (Ciriaco García de Celis, 1995)
adds 1.44/1.68 MB support to machines whose BIOS never had it? Reference copy on the user's disk:
`%USERPROFILE%\OneDrive\Desktop\XT_project\Windows_311_working_build\2M30`, which contains
`2M.COM` / `2MX.COM` (TSR), `2M.SYS` (driver) and — the interesting part — `2M-XBIOS.EXE` /
`2M-ABIOS.EXE`, i.e. *BIOS-service replacements loaded from disk*. That is exactly the shape of shim
being proposed. Target would be 5150/5160-class machines with 1982-era ROMs, for Windows 3.11 and
possibly Windows 95.

**⚠️ These are TWO different questions and must not be conflated:**

**(a) The Inboard-specific one — INBRDPC.SYS vs the 1982 ROM.**
The short form used in the READMEs ("INBRDPC.SYS checks a signature at `F000:E05B` that the 1982 ROMs
don't carry") is **true but incomplete**, and the fuller version matters here. Per
`docs/INBOARD_86BOX_PORT_PLAN.md` (~lines 2340-2380), a later investigation found that
`INBRDPC.SYS`'s `CS:0x2C6` is **not a compile-time reference constant** — it is a *destination
buffer*, filled at runtime by an `INT 15h AH=87h` GDT-descriptor block copy whose **source** is built
dynamically from `word[0xC24]:word[0xC22]`. Two fixed-address patch attempts (one-shot, and
intercept-every-write) both **failed** for exactly this reason.

Crucially, `F000:E05B` is **not arbitrary** — it is the target of the standard reset vector at
`FFFF0h` (`EA 5B E0 00 F0` = `JMP F000:E05B`). So the comparison is very likely
**reset-vector-relative**, not a magic signature.

**Why that's good news for this idea:** a reset-vector-relative check is far more shimmable than a
literal signature would be. A shim doesn't need to forge a byte pattern IBM never wrote; it needs to
make the boot-continuation entry point present the expected content. That is a much more tractable
target.

**Why it's still not free:** the mechanism is only *partially* root-caused. The named next step,
already written down, is to find the caller of the routine at `INBRDPC.SYS` file offset
`~0xA59x-0xA5A1` and see what `AX:BX` actually resolve to — and it needs a **live CS:PC-at-entry
trace**, not another static scan (no static `E8` call reference to it was ever found, so it is
reached indirectly or by fallthrough). **Do that before designing any shim.**

**(b) The general one — old ROMs and Windows generally.** "Why can't a 5150/1982-5160 BIOS run
Windows 3.11/95 fully" is a *separate* question with its own causes (missing INT 15h services, the
system-configuration table, extended-memory calls, etc.) and is **not** answered by the `E05B`
finding. Anything learned about (a) does not automatically transfer to (b). Scope them separately or
this turns into an open-ended hunt.

**Cheapest first step if picked up:** enumerate which BIOS services Windows actually demands that a
1982 ROM lacks, before writing a line of shim code — a shim can only be specified once the gap list
exists. The 2M utilities are worth disassembling as a *structural* model (how they hook and replace
BIOS services from a loaded TSR), independently of what they do to floppies.

**Fidelity caveat:** per [[feedback-hardware-fidelity-priority]] and
[[feedback-real-hardware-reproducibility-2026-08-03]], any such shim must be a **real loadable
artifact that runs on real hardware**, not an emulator-side hack. That is the whole point of the 2M
analogy, and it is what makes this idea worth logging rather than dismissing.


- **`machine_table.c` naming not synced with upstream.** Upstream renamed the machine to
  `[386DX] IBM XT (Inboard 386/PC)` and trimmed `.cpu.package` in commit `f8a10398`. Deliberately
  not ported into `86box_full/`, because the package trim would invalidate local test configs that
  use 386SX-family CPUs. Cosmetic; revisit only if title-bar consistency starts costing time.
- **Dirty run artifacts in the repo:** `bios_f000_dump_int15.bin`, `vram_dump.txt`, and a deleted
  `vm_win311/stderr_log.txt`. Decide whether these should be tracked at all — they are regenerated
  by every run and mostly add noise to `git status`.
- **~99 untracked `live_*.txt` trace files** in the repo root from this investigation. Worth a
  `.gitignore` rule rather than repeated manual filtering at commit time.
