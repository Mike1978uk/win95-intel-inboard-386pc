---
name: inboard-hw-debug
description: Diagnostic methodology for 86Box-Inboard hardware/timing/boot bugs - live tracing, data-vs-render separation, config gotchas, bisection against real hardware. Use whenever debugging a new boot hang, black screen, device-not-detected, or timing mismatch in this project, instead of re-deriving an approach from scratch.
---

# Inboard hardware-debug methodology

This is a living document. Every time one of these techniques resolves (or rules out) a real
bug in this project, update this file with what actually worked — a new technique, a
refinement of an existing one, or a note that something didn't pan out. Don't let it go stale;
a technique that isn't kept current is worse than no technique, because it looks authoritative.

Source: distilled from `INBOARD_86BOX_PORT_PLAN.md` (2026-07-24 → 2026-07-29 investigation) and
the 2026-07-31 repo-recovery session. See `memory/recovery_plan_2026_07_31.md` for the incident
this was extracted from.

## Core principle: live evidence over static disassembly or guessing

Every fix in the port plan that stuck was found by tracing what the emulator actually did at
runtime, not by reading disassembly and reasoning about what it *should* do. Static analysis
repeatedly produced confident, wrong conclusions in this codebase (see "Two contradictory
static-analysis conclusions" and the double-shifted-address diagnostic bug, both in the port
plan). If a fix is based on reading code rather than watching it execute, treat it as a
hypothesis, not a finding, until traced.

## Technique 1: CS:PC ring-buffer tracing to find a one-shot trigger address

When something fails once, deep in a boot sequence, and you need the *exact* instruction that
caused it: `386_dynarec.c` already has a ring-buffer CS:PC history mechanism used throughout the
port plan investigation. Log `ring_last_cs`/`ring_last_pc` (the ring's own dedup-tracking
variables) *before* the current instruction writes into the ring — this avoids wraparound-order
ambiguity that makes a full ring dump unreliable for finding "the instruction right before X."
Add a real-address-gated hook (fires only on the specific CS:PC of interest), not a blanket trace
— blanket traces produce too much volume to read and can themselves perturb timing.

## Technique 2: separate "data" from "pixels" before chasing a rendering bug

If a screen looks corrupted, don't assume the underlying data is wrong. Dump the actual text-mode
VRAM content directly (physical `B8000` read, not a screenshot) and compare against what's on
screen at the *same* logical moment. The port plan's `vram_dump.txt` technique — append a
timestamped snapshot every few real seconds — solves the "which screenshot corresponds to which
dump" correlation problem that caused a false lead earlier in the same investigation (comparing a
stale one-shot dump against a screenshot taken much later). If the dumped text is clean while the
screen shows garbage, the bug is in the render path (e.g. `vid_cga.c`'s `charbuffer` latch), not
in CPU/BIOS logic — a completely different fix location.
**2026-07-31 addition**: this same mechanism (already wired into the current build, writes to
`<exe_dir>/vram_dump.txt` continuously) is also the most reliable way to check DOS-level boot
progress when a `PrintWindow` screenshot looks static — a small/cropped emulator window can hide
new text below the visible area even though the VM is actively progressing. Check the log file
tail for `[vramdump] appended snapshot` and read the dump file itself before assuming a hang.

## Technique 3: distinguish a real freeze from "slow but progressing"

Before concluding something is hung, confirm the CPU is genuinely not executing. A `[modecheck]`-style
heartbeat (dump CS:PC once a second) that shows the same few addresses cycling means the CPU is
alive and polling/looping, not deadlocked — this ruled out several false "hang" reports in the
port plan (the Mach8 self-test's 65-170s real-time duration looked like a hang until this check
was added) and confirmed at least one genuine blocking wait (the SB Pro digitized-sound hang,
where DSP port activity stopped completely and stayed stopped for 60+ seconds with zero log
growth — a real difference from the polling-loop pattern).

## Technique 4: config value silently ignored → check the section name, not the key name

If a config value seems to have no effect, don't assume the underlying mechanism is broken —
check where 86Box is actually looking for it first. Device-specific `device_config_t` options
(e.g. `enable_5161`, `bios`, `bus_width`, T130B's `irq`/`zero_wait`) are read from an INI section
named after the *owning device's* `.name` field (`dev->name` in `device.c`'s
`machine_get_config_int()`), not `[Machine]` and not the device's `internal_name`. A key in the
wrong section is silently accepted as "not found" and the compiled-in default applies — no error,
no warning. This bit the project badly once already (`enable_5161` misplacement, port plan lines
~1060-1090) and would have caused another false lead in the 2026-07-31 recovery if not checked
directly against the C source. **Always grep the actual `device_t` struct for `.name` before
writing a new section header** — don't infer it from the machine-table display name (which can
differ, e.g. `machine_table.c`'s `"[386SX] IBM XT (1982) w/ Intel Inboard 386/PC"` vs the actual
config-owning device's `"IBM XT (1982) w/ Intel Inboard 386/PC"`, no prefix).

## Technique 5: bisect against a real-hardware-measured target, not a theoretical one

When a timing constant needs tuning (e.g. `isa_cycles`), don't compute it from first principles
and stop — get a real-hardware reference measurement (the user's own physical 5160, timed via
video or a benchmark utility) and bisect the emulator constant against *that* number specifically.
The port plan's CPU-speed grade (`83.5MHz`) was only trusted once five independent real-hardware
tools converged on the same figure — a single measurement (or a "performs like X" inflated
figure from a benchmark utility, e.g. LANDMARK's "233MHz" claim) is not enough on its own.

## Technique 6: A/B against a stock, non-Inboard machine to isolate scope

When something looks Inboard-specific, build a disposable comparison VM (or reuse `ibmxt86`, a
genuine stock 8088 machine profile already in 86Box) with the same peripheral card and no project
code involved. If the bug reproduces there too, it's a general 86Box bug (report upstream
eventually, don't keep chasing an Inboard-specific explanation) — this is exactly how the SB Pro
digitized-sound hang was correctly re-scoped from "Inboard IRQ bug" to "general `dma_xt8237`
bug," and how `enable_5161`/`io_waitstates` were correctly ruled out as complete explanations for
POST `101`.

## Technique 7: don't trust a memory file's specific claim without re-deriving it once

This project's own condensed memory files have been wrong before — not vague drift, but specific,
citable claims (a `dma16`-disable "fix" that never appears anywhere in the port plan; a
black-screen "regression" that was actually introduced by the session that wrote the memory file
claiming it). Before acting on a memory claim that names a specific config key, function, or fix,
grep the primary source (port plan) or the actual C source for it. If it's not there, the claim is
unverified — correct the memory file rather than propagating it forward again.

## Technique 8: don't guess hardware geometry/config that can be read directly

If a value can be read off the actual asset instead of computed or guessed (disk CHS geometry
from the image's own MBR partition table and BPB, ROM offsets from the actual binary's contents,
real I/O addresses from `CHECKCPU`/`COMRADE`-style real-hardware readouts), read it directly
rather than deriving it from a formula or an example config for a *different* disk image.
**2026-07-31 case**: a first attempt computed HDD CHS from file size using 86Box's own auto-calc
formula, in the wrong field order (`cyl, heads, spt` instead of the actual `spt, hpc, tracks`
order `config.c` expects) — produced sector errors on boot. Parsing the image's own MBR partition
table directly (`heads`/`spt` decoded from the CHS-end field, `1 partition-end LBA / (heads*spt)`
for cylinder count) gave the real geometry and fixed it immediately. When in doubt about a
config's exact field order, grep `config.c`'s `sscanf`/`ini_section_get_string` call for that key
rather than trusting an example from a different context.

## Addendum to Technique 4: numbered-instance sections auto-migrate from bare names

Multi-instance devices (a card that could have more than one, e.g. `scsicard_1`) get a section
name of `"%s #%i"` (`dev->name` + instance number) once instance > 0 — confirmed in
`device.c`'s `device_set_context()`. This looked like it might repeat the `enable_5161` trap
(writing `[Trantor T130B]` when the real section is `[Trantor T130B #1]`), but it's actually
safe: `device_set_context()` (`device.c` ~line 277-283) auto-renames a bare non-numbered section
to the numbered one on load if the numbered one doesn't exist yet, and 86Box's own config-save
persists the renamed result — confirmed 2026-08-01 by writing a bare `[Trantor T130B]` section by
hand, booting, and finding the values correctly present (not defaulted) under
`[Trantor T130B #1]` after 86Box rewrote the file. Still always grep for `.name` and check
whether the owning device is single- or multi-instance before assuming a bare name is wrong, but
don't panic if 86Box "corrects" your section header on the next save — that's this migration
path working as intended, not silent data loss.

## Technique 9: real keystroke injection into the running VM via `inject_key.txt`

OS-level `SendKeys`/`SendInput` do not reach 86Box's SDL/Qt keyboard handling (confirmed
repeatedly in the port plan, 2026-07-24/25 — a known limitation, not root-caused, possibly
synthetic input being filtered). This project has its own file-based channel instead, wired
directly into `keyboard_input()` in `386_dynarec.c` (~line 1654, present and compiled in as of
2026-07-31): once per second it polls for a file named `inject_key.txt` in the **process's own
current working directory** (not the `-P` profile dir, not the exe's own dir unless that's also
the CWD at launch — whatever directory 86Box.exe was actually launched from), reads a decimal XT
Set-1 scancode (optionally prefixed with a bare `s` to hold Shift around it, for `:` and other
shifted characters the plain make/break protocol can't reach otherwise), injects a make+break
pair, logs `[keyinject] sent scancode N (0xXX)` to stderr, and deletes the file so the next
keystroke can be dropped. Confirmed working live 2026-08-01: single test character appeared at
a `C:\>` prompt in `vram_dump.txt` within ~1s of writing the file.

**Practical driver script**: write the scancode, wait for the file to disappear (consumption),
then write the next one — don't burst multiple files, only one is read per poll. A small
char→scancode table covering lowercase a-z/0-9/space/`.`/`/`/`\`/`-`/`,`/shifted `:` is enough
for typical DOS command-line testing (FORMAT, COPY, DIR, CD, etc.) A working version of this
driver lives in scratch form each session — rebuild it from this description rather than
searching for a committed copy, none is checked in (it's throwaway tooling, not project code).

Combine with Technique 2's `vram_dump.txt` polling to read command output back without needing
to look at the actual window.

## Technique 10: check for pre-existing gated trace hooks before writing new ones

Before adding a new Technique-1-style CS:PC/register hook for a bug that's been investigated
before, grep the relevant source file for the bug's old name first (e.g. `grep -rn "sb.*hang\|sbprov2-hang"` in `.c` files, not just this memory dir). The 2026-07-27 port-plan
investigation into the SB Pro digitized-sound hang left exactly the hooks needed to root-cause
it — `[picimr5]` in `pic.c` (logs every PIC1 IMR write that flips the IRQ5 mask bit, capped at
50 hits) and `[dspC]`/`[dspE]` in `snd_sb_dsp.c` (logs write-buffer-status and IRQ-ack port
reads, capped at 80 hits each) — each with a `"Remove once root-caused"` comment, still present
because the bug was never actually root-caused before. **2026-07-31 reproduction session**:
these hooks, read together with the ring-buffer-adjacent `[sbcmd]` DSP-command log, gave the
exact trigger instant for free with zero new instrumentation - see `recovery_plan_2026_07_31.md`
punch-list item 6 for the full sequence (`0x40` Set Time Constant issued twice, IRQ5 masked
immediately after, then total silence). Writing a fresh hook from scratch would have reproduced
work that was already done and just never followed up on.

## Technique 11: trigger-armed uncapped tracing when generic hooks are exhausted before the interesting window

Fixed-hit-cap hooks (Technique 1/10 style) get silently exhausted by ordinary boot-time noise
(PIT reprogramming, keyboard polling, PIC IMR churn) long before execution ever reaches the
actual bug — checking a capped hook's hit count against real elapsed time (or line number) is
the tell. Don't just raise the cap globally (it explodes log volume with irrelevant boot noise).
Instead add one small global flag (e.g. `int foo_trace_armed = 0;`) set by whichever earlier
event marks "we're now in the interesting window" (found via Technique 1), and gate new,
*uncapped* logging in every function you want visibility into on that flag. 2026-07-31 SB Pro
session: the generic `[iotrace]`/`[dspC]`/`[dspE]` hooks were all capped-out during ordinary boot
long before the fatal `imr 00->AC` IRQ5 mask; a flag armed by that exact PIC transition, checked
in `dma.c`'s `dma_write`/`dma_channel_read` and `snd_sb_dsp.c`'s `sb_write`, gave clean, complete,
uncapped visibility into the post-mask window with zero boot-noise pollution — proving in one
capture that zero further DMA-controller or DSP-port I/O happens at all after the mask (ruling
out a DMA-arbitration bug and pointing instead at a VMM-level scheduling/wait primitive).

## Technique 12: CS:PC-triggered execution trace for protected-mode/VMM hangs, same pattern one layer up

When Technique 11's device-level tracing proves a hang *isn't* a hardware-register bug (e.g. zero
further DMA/DSP I/O after the trigger point), the next layer up is the CPU's own instruction
stream, not another device. Add the same trigger-armed hook (Technique 1/11 style) directly in
`cpu/386_dynarec.c`'s `exec386()` per-instruction loop (next to the existing `F000:E0AB`/`F8C8`-
style one-shot hooks, right before the ring-buffer update) — arm on the specific CS:PC the fault
was already pinned to (a flat/protected-mode selector like `0028` means VMM32/VxD ring-0 code, not
BIOS real-mode), then trace the next N *distinct* (CS,PC) values with the same consecutive-dedup
approach the ring buffer uses (a raw uncapped per-instruction log floods on any spin/wait loop). A
short, non-growing set of unique addresses in the post-trace confirms a genuine spin/wait at the
emulator level; a long, non-repeating trace shows the real code path taken first. **2026-07-31 SB
Pro session**: added as `[vmmhang]`/`[vmmhangpost]` targeting `0028:80051271` (the VxD context that
stops being scheduled, pinned via real-hardware comparison — see `recovery_plan_2026_07_31.md`
item 6). Register macros for a 32-bit dump: `EAX`/`EBX`/`ECX`/`EDX`/`ESI`/`EDI`/`ESP`/`EBP` in
`cpu/cpu.h` (`cpu_state.regs[n].l`) — note `CS` is the raw selector (`cpu_state.seg_cs.seg`), not a
real-mode-style base; don't `<<4` it for protected-mode addresses, use `cpu_state.seg_cs.base` if a
linear/physical address is ever needed instead of just the selector:offset pair.

## Driving the VM without any live guest agent (Technique 9 + Technique 2)

For keyboard input and output readback, the project has its own host-native channel — no COMrade,
no serial bridge, nothing guest-side to keep alive across VM restarts: Technique 9's
`inject_key.txt` polling (wired into `keyboard_input()` in `386_dynarec.c`) for input, Technique
2's `vram_dump.txt` for text-mode output readback. Prefer this over any live-agent bridge for
*internal* emulator debugging (reproducing a hang, driving DOS/Windows menus, reading a screen) —
it has no external dependencies (HHD, named pipes, a TSR that has to survive every VM relaunch) and
nothing to reconnect when the VM is killed and restarted, which happens constantly during this kind
of investigation.

## Live hardware bridge (COMrade / COMR95) — real hardware only, not the VM

For cross-checking emulator behavior against the **real 5160**, `COMRADE`/`COMR95` (Windows
95-compatible build, `COMR95 /com1 /baud 115200`) is the live bridge — HELLO/status, text screen,
desktop thumbnails/screenshot, file read/write, directory listing, CRC hashing, keyboard input.
See `memory/comrade_bridge.md` for current capability details. Use it to settle "does real
hardware show this too" questions directly instead of guessing from a photo or a stale note —
this was flagged as the fastest way to resolve the "ROM BIOS shadow RAM failed" system-BIOS
mismatch (`mem_dump` of physical `0xF0000-0xFFFFF` on real hardware) but wasn't available in the
session that hit it. Check whether it's connected before assuming it isn't.

**Do not use this bridge (or the `comrade86box` MCP variant) to introspect the *VM* itself —
decided against 2026-07-31 after a long session chasing it.** The architecture is fundamentally
mismatched for that: the MCP server's process lifecycle is tied to the Claude Code session, while
the VM gets killed and relaunched constantly during debugging, and nothing in the chain (86Box's
named-pipe server, HHD's COM-port-to-pipe client bridge, the python MCP client holding the COM
handle open) reconnects when one side restarts — whichever side started first just holds a dead
handle. It also duplicates capability the project already has natively and more reliably
(Technique 9/2 above, plus Technique 1/10/11/12's source-level tracing) for anything that's about
the emulator's own internal state. `comrade`/COMrade proper stays real-hardware-only.
