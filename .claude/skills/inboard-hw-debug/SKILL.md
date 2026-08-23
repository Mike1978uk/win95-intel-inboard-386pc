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

## Complete Windows 95 boot fix inventory (XT + Inboard 386/PC), consolidated 2026-08-22

This is THE canonical list of everything required to get Windows 95 (OSR1) booting to a working
desktop on the `ibmxt_inboard386` machine profile in 86Box. It exists because this exact list was
scattered across ~15 dated memory files, which cost real time to re-derive when porting fixes into
a fresh upstream clone for a PR. **Read this before re-diagnosing a Win95 boot hang from scratch —
check whether the symptom matches something already solved here first.** Update this table itself
(don't just add another dated memory file) whenever a new required fix is found — this list should
always be the current source of truth, not an archive.

Two categories matter, because they interact with a fresh clone/PR differently:
- **Emulator/code fixes** — live in 86Box's own C source (`386_dynarec.c` etc.). These must be
  manually re-applied to any fresh clone of upstream 86Box; they will NOT be present just because
  you have the right disk image.
- **Disk-image fixes** — patches baked into the Windows 95 `.img` file itself (VxD/DRV binary
  patches, a flipped byte in `INBRDPC.SYS`). These travel WITH the disk image regardless of which
  86Box build boots it — if two builds share the same `.img` file, both already have these; they
  are never a source of difference between builds and never need "porting" into emulator code.

### Emulator/code fixes (must exist in `386_dynarec.c` unless noted)
1. **Base PIC-IMR/DMA-refresh timing fix** — `dma_force_xt`/`pic_set_force_xt_imr_timing` wiring
   in `device/inboard386.c`'s init. Already part of upstream 86Box mainline since PR #7626 merged —
   a fresh clone has this for free, nothing to port.
2. **`E362-E3AC` IRQ1-suppression self-test fix**, with the 2026-08-22 correction: exit on
   `E3AE`/`E38E`/**`E3AD`** (not just the first two), and `in_negative_test` set ONLY on `E3AE`. See
   the "RESOLVED, 2026-08-22" section below for the full story — this is the fix that closed `301`.
3. **`E507` DMA-refresh status-flag force** (`AL |= 0x01` at `F000:E507`).
4. **Mach8 PIT-readback delay-loop fix** — force the elapsed-ticks register past target at
   `C000:0x7B37`/`0x7B23`/`0x7B16` (three known ROM-revision addresses; add a new one here if a
   fourth ROM revision is ever encountered, following the same live-CS:PC-trace method).
5. **Mach8 option-ROM waitstate exemption** (dated 2026-07-26 — easy to miss, it's NOT near the fix
   above in the file's history even though both are Mach8-related): zero out
   `io_waitstates`/`reg_op_waitstates`/`cpu_prefetch_cycles`/`cpu_mem_prefetch_cycles`/
   `cpu_rom_prefetch_cycles`/`cpu_cycles_read(_l)`/`cpu_cycles_write(_l)`/`isa_cycles` for the
   duration of `CS==0xC000` execution, restoring the real values the instant CS leaves 0xC000.
   Without this, the Mach8 option ROM's self-test stretches from real-hardware-instant into
   65-100+ real seconds, because this project's system-BIOS waitstate inflation gets wrongly
   applied to the option ROM's own hundreds of I/O operations too.
6. **Segment-650B / `INT 68h` wild-jump fix** (`patchint68`, dated 2026-08-04; **revised
   2026-08-23**): the first time `CS==0x0EAF`, point the `INT 68h` IVT vector (`0x1A0`) at the
   BIOS's own IRET at **`F000:FF53`** (write `53 FF 00 F0`). **No stub is injected.** The original
   version wrote a `0xCF` byte into INT F0h's own vector slot (physical `0x3C0`) and pointed there;
   Michal Necasek asked in review of PR #7749 why it didn't just use the BIOS's existing IRET, and
   he was right. Verified: byte at file offset `0x7F53` of the U18/F800 chip is `0xCF` in **both**
   1986 revisions (09MAY86 and 10JAN86), the only BIOSes this machine accepts. Strictly better - no
   injected code, and no assumption that INT F0h is unused this early in boot. **The real-hardware
   twin `ivt68fix/IVT68FIX.ASM` must be kept in step** (now 20 bytes / 4 writes, was 26 / 5). Without this, VMM32's real-mode VxD loader's uninitialized `INT 68h` call walks
   off into the raw IVT as code and eventually wild-jumps into segment `650B` with bogus calling
   context. **This one is easy to forget when porting** — it belongs to a completely different
   investigation (Win95 keyboard/boot, not the 101/301 POST-error family), so it's not in the same
   part of the file as the other fixes and won't turn up if you only grep for "301" or "101".
7. **`kbc_xt.c` `blockedtimeout` self-heal** — bounded auto-clear of `kbd->blocked` (~50 poll ticks)
   if nothing ever acknowledges the XT keyboard strobe via port 0x61 bit 7. Needed because some
   guest keyboard handlers (written assuming AT-style hardware) never perform this XT-only
   acknowledgment.
8. **`cpu_table.c` 83.5 MHz CPU speed table entry** (`486BL3`/`83.5`) and **`m_xt.c` LPT device**
   addition — both omitted from PR #7626's file list, straightforward to re-port (small, self-
   contained diffs).
9. **`snow_enabled = 0`** in the video device's config section — NOT a code fix, a config
   recommendation, only relevant for plain-CGA setups (Mach8/VGA setups don't need it). 86Box's CGA
   snow simulation is accurate on its own; it just desyncs with this project's timing overrides.

### Disk-image fixes (already baked into any `osr1_XT_customvkd_test.img`-family file — never port these into emulator code)
1. **`INBRDPC.SYS` self-test-skip patch** (`vxd-patches/osr1/INBRDPC_selftest_skip.SYS`, file offset
   `0x6BA` flipped `3C00`→`FFFF`) — without this, boot stalls at "Please wait while Setup updates
   your configuration files..." in a redundant loop in the `0206:06xx` self-test wrapper. WITH it,
   that same region is visited only briefly (~1 real second, a single legitimate ~2000-iteration
   A20-toggle burst) and boot proceeds to the Startup Menu / GUI.
2. **Custom `VKD.VXD`** built from Microsoft's 1995 Windows 95 DDK sample source — removes
   `VKD_Int_09`'s check of port 0x64's AT-only "data available" bit before reading port 0x60 (the
   Inboard has no real 8042, so that bit never sets, silently discarding every keystroke otherwise).
3. **`KEYBOARD.DRV`** patch (file offset `0xf14`) — an independent, separate port-0x64 check inside
   Windows' own keyboard driver, needed alongside the VKD fix (Windows 3.11 needed both `IBKBD.DRV`
   and `IBVKD.386` for the same underlying reason — same pattern here).
4. **Headless keystroke-injection capture gate** — not a boot requirement per se, but needed for
   *unattended/scripted* testing specifically: see Technique 9 (`inject_key.txt`) for how to send
   real keystrokes into a running VM without a physical keyboard.

### Known OPEN issue — do not confuse this with "the boot recipe isn't fully known"
There is a genuinely unresolved, separate problem, first found 2026-08-22 (this project's OWN fork
`local` vs. a fresh upstream clone, NOT anything to do with Windows 95 specifically): the two builds
execute a different number of raw CPU instructions through an early BIOS timing-calibration loop on
byte-identical ROM, traced to a register (`AX`) already differing only ~30 instructions after CPU
reset, for a reason never found. This is NOT part of the boot recipe above — every item in that list
is fully solved and, as of 2026-08-22, confirmed present in both `local` and a from-scratch upstream
clone. This open issue is a CPU-core-timing mystery that can make ANY sufficiently timing-sensitive
self-test loop (not just the ones already fixed above) take a different number of iterations between
the two builds — up to and including looking like a hang when it's actually just taking far longer
than expected. Concrete, never-executed next step: dump `pit_const` (the actual cycle-to-tick
conversion value, `dev->pit_const` in `pit.c`) for both PIT counters, at the same early instruction
index, in both builds — if it differs despite identical `cpu_busspeed`/waitstate inputs, that pins
the mechanism.

### For a future OSR2 attempt
The disk-image fixes above (self-test-skip, VKD.VXD, KEYBOARD.DRV) were derived specifically against
OSR1's files/offsets — OSR2 will very likely need its own equivalent patches at different file
offsets (OSR2 uses a different `VKD.VXD`/`KEYBOARD.DRV` build), even though the underlying root
causes (missing port-0x64 "data available" bit, uninitialized `INT 68h` vector, INBRDPC.SYS self-
test redundancy) should be identical in kind. The emulator/code fixes above are OS-version-
independent and should apply unchanged. Technique 35 (read the real DDK source rather than guessing
at binary offsets) is the proven path for re-deriving the VKD/KEYBOARD.DRV patches against OSR2's
actual files.

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

## Technique 13: offline segment-confinement analysis from an existing per-second modecheck log — no rebuild needed

When a boot looks stuck but you already have a `[modecheck]` heartbeat log running (Technique 3),
check for genuine forward progress *before* writing any new instrumentation: `grep -o
"CS:PC=[0-9A-F]*:" run.log | sed 's/CS:PC=//' | uniq` lists distinct CS segments in order of first
appearance — real boot progress moves through many segments (BIOS → IO.SYS → MSDOS.SYS →
COMMAND.COM → ...); if the segment value stops changing and stays fixed for minutes while the
`[vramdump]` screen text also stops changing, that's your hang window, found for free from data you
already collected. Then check *within* that segment for a repeating cycle vs. genuine (if slow)
work: `grep -o "CS:PC=SEGVAL:[0-9A-F]*" run.log | sed 's/.*://' | sort | uniq -c | sort -rn | head`
— if most offsets appear only once each and span a wide range, it's not a 2-3 instruction spin
loop (rules out the simplest hang class immediately, no new hooks needed). A near-100%-unique,
wide-ranging offset spread that *still* never leaves the segment for minutes is a strong signature
of a genuine non-terminating loop in whatever routine owns that segment (e.g. a relocation/retry
loop that keeps advancing through fresh table/buffer addresses without ever reaching a terminating
condition) — worth a live Technique 1/12-style trace centered on that segment next, but this
offline pass already tells you where to point it without burning a rebuild cycle first.

**2026-08-01 case**: booting a real-hardware-lineage Windows 95 image (see the CF-card note under
"real hardware bridge" below) reached `Starting Windows 95...` normally, then CS:PC became confined
to segment `650B` (highly plausibly the real-mode VMM32 monolithic-builder, based on file layout —
`vmm32/` folder + a pre-combined `VMM32_combined.VXD` both present on the source card) for 300+
continuous seconds — 389 of 390 one-per-second samples were unique offsets spanning the segment's
full range, never returning to any BIOS/DOS segment, while the text-mode *and* graphics-plane VRAM
both stayed completely blank (matches the real 5160's own described "blank flashing cursor" symptom
— a hardware-drawn text cursor blink leaves the underlying character cell itself blank, so a raw
VRAM dump of a real flashing-cursor hang and a "nothing ever got drawn" hang look identical this
way) and the PIT channel-0 timer kept ticking throughout (not a total system freeze, same shape as
the earlier SB Pro VMM hang investigation's "one context permanently stops, everything else still
runs" finding). This is a real, reproducible emulator-side hang at the same boot stage the user
independently described from prior real-hardware/local testing ("Windows 95 has completed updating
files, continuing to load Windows 95... hangs at a flashing cursor") — strong first-time evidence
the emulator does reproduce the real machine's core blocker. Not yet root-caused to a specific
instruction; next step is a live Technique 1/12 trace scoped to segment `650B` specifically (arm
on first entry to that segment, log next N *distinct* offsets with full register dump) to find
whether it's a genuine infinite loop in the VMM32 combiner or something slower but still finite that
just needs more real time than tested so far.

## Technique 14: identify a mystery CF-card/disk-image by physically matching against the source device, not by filename

When several same-named-family disk image backups exist (e.g. `*_golden*.img`, `*_ready_for_real_hw*.img`,
multiple `*_combined_patched*.img`) and it's unclear which one matches current real-hardware state,
check whether the actual physical source media is still mounted before trusting any filename/date
heuristic. `Get-Partition -DriveLetter X | Get-Disk` (PowerShell) reports the real physical disk's
`FriendlyName`/`Size`/`BusType` — matching that exact byte size against candidate `.img` files'
`ls -la` sizes is a hard, unambiguous filter (multiple checkpoint images from the same card will
share its exact physical byte count, e.g. `1968390144` bytes for this project's Trantor/FNK CF
reader+card combo — a value that recurs across `Golden_win95_vm_installed.img`,
`5160_combined_patched_real_hw_test.img`, `Prestaged_pre_vmm.img`, etc., confirming they're all
snapshots of the same physical card, not independent images). **Don't try to raw-`dd` the physical
device itself from an unprivileged shell** — opening `\\.\PhysicalDriveN` for even read-only access
needs admin and will fail cleanly (safe to attempt once, not worth escalating for). Use an existing
`.img` snapshot from the same lineage instead, and reconcile specific changed files (a newer
`CONFIG.SYS`, a `INBRDPC3.SYS` replacing `INBRDPC.SYS`, etc.) by direct comparison against the live
mounted card rather than assuming any one backup is fully current.

**Known quirk of this specific CF card**: its MBR partition table and FAT16 BPB both describe a
larger partition (~2,045,772,288 bytes / 3,995,649 sectors) than the card's actual addressable
physical capacity (1,968,390,144 bytes, per `Get-Disk`) — consistently, across every image ever
captured from it. This is **not corruption** (verified: MBR and BPB agree with each other, just not
with the physical device) — it's an inherent property of this reader/card combination. **Implication
for `86box.cfg`**: don't trust the image's own baked-in MBR/BPB geometry for `hdd_01_parameters`
(Technique 8's normal approach) when working with this specific card's images — it will describe a
disk larger than the actual file, which can misbehave. Instead compute geometry from the *actual
file's byte size* via 86Box's own `hdd_image_calc_chs()` algorithm (`disk/hdd_image.c` ~line 150,
size argument in MB, `spt`/`heads`/`cyl` derived per the documented VHD-spec formula) — this
guarantees the computed disk size stays at or under the real file size, so 86Box never tries to
read past EOF. For the 1,968,390,144-byte family: `hdd_01_parameters = 63, 16, 3813, 0, ide`.

## Technique 15: when forcing a Win95 VMM32 recombine, preserve the existing `VMM32.VXD`, don't delete it

To make Windows 95 pick up new/patched override VxDs from `WINDOWS\SYSTEM\VMM32\`, it's tempting to
delete the existing `WINDOWS\SYSTEM\VMM32.VXD` (the combined blob) so Windows is forced to rebuild
from scratch. **This is wrong and produces a new, misleading failure mode.** A genuinely
pre-combine Windows 95 image (e.g. this project's `Prestaged_pre_vmm.img`) still has a `VMM32.VXD`
present — a small, generic, CD-shipped placeholder (~411KB vs. ~690KB for a real full combine,
FAT-timestamped to the OS build date rather than any local session) — and Windows expects to read
and *update* this file in place, not find it missing. Deleting it outright causes Windows to
explicitly report `Windows could not combine VxDs into a monolithic file before starting` and stop
at a "Press any key to continue" prompt — a real, on-screen diagnostic, but a different bug than
the one being investigated, and confusing if you don't already know to expect it. **Fix**: only
replace the individual override VxD files inside `VMM32\` (delete-then-recreate each one — see the
pyfatfs note below), and leave the top-level `VMM32.VXD` alone. Windows' own hardware-detection
logic decides when to actually recombine; forcing that decision by deleting its input isn't
necessary and isn't how the real reimage-and-boot workflow this project mirrors ever does it.

**2026-08-01 case**: exactly this mistake, caught by the user, cost one full boot-and-diagnose
cycle. The corrected retry (same 3 patched VxDs, `VMM32.VXD` left in place) combined cleanly
(`Please wait while Setup updates your configuration files...` → `Completed updating files,
continuing to load Windows...`, no error) and reached a stall the user confirmed live matches
where the real 5160 permanently gets stuck — the strongest reproduction of the project's core
Win95 blocker so far. See `memory/win95_emulator_repro_2026_08_01.md` for the full trace.

**Related pyfatfs note**: in-place overwrite of an *existing* file entry can fail with
`PyFATException: FREE_CLUSTER mark found in FAT cluster chain` if that specific file's on-disk
cluster chain is unusual (seen on a VxD that turned out to be an unidentified third-party variant,
not a clean stock/patched copy). Workaround: `fs.remove(path)` then `fs.writebytes(path, data)` —
delete and recreate rather than overwrite in place — worked cleanly where direct overwrite didn't.
**Always copy the target image before any pyfatfs write** (this mistake was caught mid-session:
briefly wrote directly to a OneDrive source-of-truth file, hit the FREE_CLUSTER error partway
through, had to restore from a pre-emptive backup) — never operate on the only copy of anything.

## Technique 16: dump live guest memory through the real page-table translation, not a physical-address guess

When a CS:PC address needs identifying and static disassembly of the "obvious" source file
(BIOS ROM, a driver's own disk file) doesn't explain it, dump the actual guest memory the CPU is
executing from and disassemble *that* instead of guessing. Use `readmemb(base, offset)` (defined
in `cpu/386_common.h`, already visible from `cpu/386_dynarec.c`), not `mem_readb_phys()`, whenever
paging might be active (`CR0.PG`, bit 31) - `readmemb()` goes through the real page-table lookup
(`readlookup2`), so it reflects what the CPU genuinely sees; `mem_readb_phys()` reads a raw
physical address and will silently return the wrong bytes if the linear-to-physical mapping isn't
what you assumed (e.g. inside a V86 box that isn't identity-mapped the way you expect). For a V86
or real-mode segment, the linear base is simply `segment << 4` (no descriptor table involved) - so
`readmemb(((uint32_t)segment) << 4, offset)` walking `offset` from 0 to 0xFFFF and writing each
byte to a file (same pattern the existing `[vramdump]` hook already uses for `0xB8000`) gives a
clean 64KB dump to `objdump -D -b binary -m i8086` offline. **Once dumped, extract readable ASCII
strings first** (`re.findall(rb'[\x20-\x7e]{5,}', data)` in Python) before disassembling anything -
recognizable strings (error messages, filenames, component names) very often identify what's
actually at an address far faster and more reliably than trying to disassemble blind, especially
in low DOS memory where multiple unrelated resident drivers get loaded contiguously and an address
that looks like "one component" from its segment number alone can actually span several.

**2026-08-01 case**: a CS:PC trigger address in segment `0048` turned out, once dumped and
string-extracted, to span `INBRDPC.SYS`, `HIMEM.SYS`, `EMM386.EXE`, `IFSHLP`, and `WININIT.INI`
error/component strings all in one contiguous block - simply the normal `CONFIG.SYS` driver load
order in low memory, not evidence of one specific "owner" component. The `WININIT` string in
particular reframed the whole investigation: Windows' file-commit/setup-finalization mechanism
processes a *list* of pending operations, which is a fundamentally different failure shape ("long
but finite, might just need more real time at this interpreted CPU's effective speed") than "stuck
forever" - a distinction that would have been easy to miss from CS:PC address ranges alone.

**Caveat on exact trigger addresses from exception dispatch**: `ring_last_cs`/`ring_last_pc` (the
Technique 1 "previous unique CS:PC" ring-buffer state) is proven reliable for normal
`call`-based control flow ([e0abtrap]), but a CPU *exception* (e.g. `INT 06h` Invalid Opcode,
raised by the processor itself mid-fetch, not by a deliberate `call`/`int` the ring buffer's
per-instruction check necessarily saw first) may not route through the exact same point in the
per-instruction loop - treat an exception-triggered "caller" address as approximate, not exact,
until cross-checked (e.g. does the reported offset actually decode as plausible code, or does it
land inside a data/string region as happened here).

**Caveat on `objdump`'s linear disassembly desyncing across embedded data**: a ROM/driver dump
almost always mixes real code with embedded data (option-ROM tables, error strings, DDBs) that
`objdump -D -b binary` cannot tell apart from code - it disassembles every byte linearly from
whatever start address you give it, so one misinterpreted data blob upstream of the address you
actually care about can desync every subsequent instruction boundary, silently producing plausible-
looking but wrong decodes from that point on. **2026-08-02 case**: disassembling a known-correct
call site (`F000:AC00`, confirmed live via `x86_int_sw`'s own `CS:oldpc`) showed a garbage 4-byte
instruction spanning what should have been a clean 2-byte `int 0x10` - `objdump` had desynced
somewhere earlier in the 64KB dump and never resynchronized. **Fix**: when you already know the
exact byte offset that matters (from a live trace, not a guess), read the raw bytes directly
(`open(path,'rb').read()[offset-N:offset+M]` in Python, or equivalent) and hand-decode or spot-
check against the opcode table yourself, rather than trusting a linear `objdump` pass through a
file you know contains non-code bytes upstream of your target.

**Caveat on `cpu_use_dynarec`**: every debug hook this project's investigations rely on
([modecheck], [vramdump], [e0abtrap], [vmmhang], [int6entry], etc.) lives inside `exec386()`,
`cpu/386_dynarec.c`'s *interpreter* loop. Compiled dynarec blocks don't run through that function,
so switching `cpu_use_dynarec` to `1` for a speed boost would very likely make all of them go
silent. Fine for a plain "does this ever finish" endurance test where no tracing is needed: not
fine for any session actively relying on this project's hook infrastructure.

## Driving the VM without any live guest agent (Technique 9 + Technique 2)

For keyboard input and output readback, the project has its own host-native channel — no COMrade,
no serial bridge, nothing guest-side to keep alive across VM restarts: Technique 9's
`inject_key.txt` polling (wired into `keyboard_input()` in `386_dynarec.c`) for input, Technique
2's `vram_dump.txt` for text-mode output readback. Prefer this over any live-agent bridge for
*internal* emulator debugging (reproducing a hang, driving DOS/Windows menus, reading a screen) —
it has no external dependencies (HHD, named pipes, a TSR that has to survive every VM relaunch) and
nothing to reconnect when the VM is killed and restarted, which happens constantly during this kind
of investigation.

## Technique 17: an exception's dispatch function being confirmed silent doesn't rule out that vector - check for alternate dispatch paths into the same lower-level function

If a hook on the "obvious" raise function (e.g. `x86_int()` in `386_common.c`) is properly
time-gated (Technique 11 style, no cap-exhaustion false negative) and still shows zero calls for a
vector you know is firing (confirmed some other way - e.g. a handler's own entry address being
reached repeatedly), don't conclude "it's not really that vector." Check for *alternate callers*
into the same lower-level dispatch function first. **2026-08-01 case**: hardware IRQ delivery in
`exec386()` (`386_dynarec.c`, the interpreter loop actually active when `cpu_use_dynarec=0`) calls
`pmodeint()` (`x86seg.c`) **directly**, bypassing `x86_int()` entirely - `x86_int()` is only one of
at least two callers into `pmodeint()`, which is the true single choke point for all
protected-mode interrupt/exception dispatch regardless of origin. Hook the lowest common function,
not the first plausible-looking one. (This same session also independently re-confirmed the
opposite lesson still applies too: after fixing an *unrelated* bug, the *original* "obvious"
hook - `x86_int()` - started firing again where it hadn't before, because the fix changed what
code ran at the trigger address. Don't assume a prior "never called" finding stays true forever;
re-verify after any code change that touches the relevant address.)

## Technique 18: a 4-line vector-formula check in the emulator's own PIC source can rule out (or in) a whole theory in minutes

Before building a live trace to test a "wrong hardware IRQ vector" theory, check whether the
emulator's PIC vectoring formula makes the theory even *possible* from the source alone.
`pic_irq_ack_read()` (`pic.c`) computes `vector = IRQ_line + (ICW2 & 0xf8)` - since `IRQ_line` is
0-7 and `ICW2 & 0xf8` is always a multiple of 8, a specific target vector often has only one or two
possible `(base, IRQ_line)` decompositions, decidable by inspection. **2026-08-01 case**: this
correctly predicted vector 6 requires `ICW2 base=0` + `IRQ6` - a real, checkable claim, not a
hand-wave - which then got *disproven* by direct tracing (see Technique 19), saving a great deal
of time that would otherwise have gone into building a fix for the wrong theory first.

## Technique 19: gate hides-the-real-window bugs can recur one layer above where you just fixed one - always double check by removing the gate first, not tightening it further

Technique 11's lesson (fixed-cap hooks get exhausted by boot noise before the interesting window)
has a mirror-image failure mode: a *time*-gated hook can equally hide the one write/call that
actually matters, if it happens to land before the gate opens. **2026-08-01 case**: a
`[picICW]`/`[picICW1]` trace gated `>=60s` (chosen to skip "ordinary BIOS POST noise") showed zero
PIC vector-base writes for the rest of a 300+s run - seemingly confirming a "PIC never
reprogrammed, stuck wrong" theory. Removing the gate entirely (not adjusting it - removing it)
revealed the *one* write that mattered happened at **t+0s**, correctly setting the standard value,
and was never touched again - completely reversing the conclusion. **Rule of thumb**: when a
gated hook shows *zero* hits and that itself is the interesting result (not just "nothing relevant
happened yet"), always re-run once with the gate fully removed (small hit-cap only) before trusting
the negative - a "confirmed" absence that's actually a gate artifact is worse than an honest "don't
know yet", because it looks authoritative.

## Technique 20: real-mode DOS device driver (.SYS) append-and-detour needs the declared resident size grown too, unlike 32-bit VxD patches

This project's established append-and-detour technique (relocate/extend code past a file's
original end, detour via a short/near jump) works differently depending on what kind of file is
being patched. For **32-bit protected-mode VxDs** loaded by Windows 95's VMM32 (e.g. this
project's `VPICD_INBOARD.VXD`/`VDMAD_INBOARD.VXD`/`VKD_INBOARD.VXD` patches), appending past the
original end is safe as-is - VMM32's loader has no equivalent shrink step. For a **real-mode DOS
character-device driver** (`.SYS`, loaded via `CONFIG.SYS`), it is *not* automatically safe: DOS
frees everything past whatever "end of resident code" address the driver's own `INIT` routine
reports back via the standard request-header fields (`ES:[BX+0xE]`=offset, `ES:[BX+0x10]`=segment)
once `INIT` returns. **2026-08-01 case**: `INBRDPC.SYS`'s `INIT` declared a resident size of only
`0xEA0` bytes via `LEA AX,[0xEA0]` (found by disassembling the `INIT` handler with
`objdump -m i8086`, the same instrumentation-free static-analysis approach useful once a function's
rough location is already known from other tracing) - appended code at file offset `0xc695` (far
past that boundary) was being silently discarded from memory after `INIT` completed, with no error
at patch-build or patch-deploy time; the failure only showed up later, as a *second*,
seemingly-unrelated fault when something eventually jumped into the reclaimed memory and executed
whatever had since overwritten it. **Fix pattern**: find the `LEA`/immediate-load instruction that
sets the resident-size value in the request-header-fill code (search near the driver's documented
`INIT` entry point), grow the immediate to cover the new appended-code end address (with a small
margin), and if there's a conditional alternate-size computation gated on a runtime flag you don't
want to depend on, force the unconditional path (e.g. flip a `JNE`/`JE` to `JMP` - same operand
byte, same target, now unconditional) rather than trying to also patch the alternate branch's
logic. **Always check this before appending to any real-mode `.SYS` driver in this project again**
- it would have saved a full round of "the mechanical fix works but doesn't resolve anything" this
session had it been checked first.

## Technique 21: a resolved bug's own diagnostic hook can start causing the next bug's symptoms - audit "one-shot" hooks for whether they're actually gated on the failure, not just elapsed time

A hook built to catch a specific historical bug can keep firing on every future boot if its
trigger condition isn't actually "this specific failure is happening" but something weaker that's
merely correlated with it (e.g. "N seconds after reaching segment X" instead of "N seconds after
reaching segment X **with no further progress**"). Once the original bug is fixed, a hook like
this doesn't go quietly inert - it keeps firing on the code path that's now perfectly normal, and
if its payload is expensive (a large dump, many syscalls), it can itself become a new, confusing
symptom: it blocks the interpreter loop for real, disk-speed-dependent wall-clock time, which looks
exactly like a fresh hang (frozen window, silent heartbeat, no visible POST progress) to anyone
watching the actual screen - a false "regression" report with a completely different, no-longer-
relevant root cause.

**2026-08-02 case**: the `[optionrom]` hook (Technique 13's originating investigation, 2026-07-26)
fired unconditionally 8 real seconds after CS first became `0xC000` and dumped 1,048,576 ring-buffer
lines in one burst - its comment claimed this was gated on "no further progress," but the code never
actually checked for that, so it fired on every single boot through the Mach8 option ROM, hang or
not. This had presumably been quietly eating real time in every run since 2026-07-26, but on
2026-08-02 it was mistaken live for a genuine new boot failure ("no Mach8 BIOS banner, black
screen, doesn't clear") - checked config/ROMs/disk image first (all correct, per Technique 4/8's
"verify before guessing" spirit) before the real cause was found by directly correlating the
`[modecheck]` heartbeat going silent against this hook's dump progress in the same log file at the
same moment. **Fix pattern**: once a hook's target bug is confirmed fixed (check for the actual
fix code nearby, e.g. an `AX = BX + 1`-style force resolving the original loop), remove the hook
entirely rather than leaving it "just in case" - a diagnostic hook with no remaining diagnostic
value and a real per-boot cost is a liability, not a harmless leftover. Before concluding a fresh
"it doesn't boot anymore," grep for large/expensive fprintf loops or one-shot dumps gated near the
same CS:PC/segment the boot is currently stuck at - a still-in-progress multi-million-line write
(check the log file's growth and whether its last line is torn/incomplete) is a distinguishable,
checkable signature, not a guess.

## Technique 22: a repeatedly-called shared entry point can be an innocent pass-through link in a hook chain - verify the segment is a protected-mode/real-mode match before assuming causality, and be ready to trace one more hop back

When a live trace (Technique 1/12 CS-transition-into-segment style) finds something calling a
shared, well-known entry point (a driver's resident API, an interrupt multiplex handler) over and
over with unchanging parameters, don't stop at "found the caller" - verify two things before
committing to a fix location:
1. **Does the calling/called segment's addressing mode actually match what you think it is?** A
   plain `segment:offset` CS value seen in a live trace is inherently real-mode/V86-mode code - a
   protected-mode component (a VxD, a flat 32-bit selector) *cannot* be the code executing there,
   no matter how plausible the theory connecting them seemed from static analysis alone. **2026-08-02
   case**: a live `OUT 0x60,0xDD/0xDF` A20-toggle loop was initially attributed to Windows 95's
   `VKD.VXD` (a protected-mode VxD) based on a strong, well-sourced hypothesis (Al Williams'
   primary-source A20 code, a project research doc explicitly indicting stock VKD's readback logic)
   - but the loop's actual `CS` value (`0E77`) was a real-mode segment, which `VKD.VXD` structurally
   cannot produce. Dumping and identifying the segment (Technique 16) found it was `HIMEM.SYS`
   instead - a completely different, correctly-behaving component.
2. **A shared entry point being called doesn't mean the caller wanted *that* entry point
   specifically** - many resident TSRs/drivers install hooks on the same shared interrupt (`INT
   2Fh`'s multiplex interface is the classic example: dozens of drivers can each claim a different
   `AH` "multiplex ID" on the same vector, chaining to the next hook for anything not theirs).
   Tracing "who calls segment X" with the CS-transition technique will faithfully report the
   *immediately preceding* link in that chain, which may itself just be relaying a request meant for
   someone else entirely. **2026-08-02 case**: traced a tight 300-hit identical retry loop
   (`AX=0x1213`) to a caller in segment `0F86`; disassembly showed `0F86`'s own code at that address
   is the tail of *its* `INT 2Fh AH=0x16` ("Windows Init/Exit Broadcast") hook, chaining onward via a
   stored vector to `HIMEM.SYS`'s `INT 2Fh` handler (`AH=0x43`) because neither wanted `AH=0x12`.
   Both `0F86` and `HIMEM` were innocent pass-through links, not the real source or destination.
   **When this happens, apply the exact same CS-transition-into-segment trick one hop further back**
   (trace who calls into the first "caller" you found) rather than assuming the first hit is the
   end of the chain - shared multiplex/dispatch points often need walking multiple links deep before
   reaching a component that actually initiated or resolves the request.

## Technique 23: repeated forced VM kills poison the next boot's own recovery logic - detect and auto-clear it, and beware stale content in ever-growing log/dump files

Iterating on a boot-hang investigation necessarily means killing the VM mid-boot, over and over, to
apply new instrumentation. This has a real side effect on Windows 95 specifically: it sets the OS's
own "did not finish loading on the previous attempt" dirty flag, so **every subsequent boot shows
the Startup Menu** (Normal/Logged/Safe Mode/etc.) with a real-time countdown (as short as ~13s) that
defaults to **Safe Mode** once it expires - and Safe Mode is independently known-broken on this
project's hardware (unloads `INBRDPC.SYS`, losing the waitstate/memory-mapping setup). **Reacting to
a chat notification and manually pressing a key is not fast enough to reliably beat this timer** -
confirmed by losing two consecutive test runs to Safe Mode this way before switching to an automated
fix: a tight (~2s) polling loop watching for the menu's own on-screen text, then writing the `1`
key's scancode to `inject_key.txt` (Technique 9) the moment it's detected, with no dependency on
notification round-trip latency.

**A related trap when building that detector**: `vram_dump.txt` (Technique 2) is a single file that
keeps growing across every relaunch this session, never truncated - a naive `grep "target text"
vram_dump.txt` matches *any* occurrence in the file's entire history, including stale content from a
previous, already-killed run. This produced a real false positive (the startup-menu detector fired
at t+18s, absurdly early, because the text was leftover from the prior run). **Always restrict the
check to the latest snapshot** (`tail -n <lines-per-snapshot> vram_dump.txt | grep ...`) before
trusting a text-based detection against any file that accumulates across runs rather than being
truncated at process start.

**2026-08-02 addendum: the auto-clear script itself had a real bug for two full runs before being
caught** - it wrote the digit's scancode (`1` = Normal mode) to `inject_key.txt` but never followed
up with **Enter**, so it correctly typed `1` into the `Enter a choice:` prompt and then sat there
re-typing the same unsubmitted digit every polling cycle forever, never advancing. The VM was
found live-stuck at this exact menu for 60+ real seconds (screen showing `Enter a choice: 1` with
no progress) before the missing Enter was noticed. **A one-shot keystroke automation script needs
the same scrutiny as any other new code - test that the target screen actually changes state after
firing, don't just confirm the trigger condition was detected.** Fixed by sending scancode 2 (`1`)
then, after a short delay, scancode 28 (Enter), with a `handled` state flag so it fires once per
menu appearance (re-arms only once the menu text is confirmed gone from the latest snapshot)
rather than spamming keystrokes at a screen that already moved on.

## Technique 24: when a guest-OS component's expected behavior is documented but its actual failure mode isn't, run the same guest on a stock reference machine profile with the same instrumentation, and diff the traces

Technique 6 (A/B against a stock, non-Inboard machine) is normally used to *rule out* an
Inboard-specific explanation for a bug. It has a second, more targeted use: when a guest OS
component (a driver, a VxD, a protocol handshake) is suspected of expecting hardware behavior this
project's XT+Inboard target doesn't provide, but static analysis/documentation only tells you *that*
a mismatch might exist, not the *exact* sequence of calls/responses involved - run the identical
guest OS build on a stock reference machine profile 86Box already ships (a plain AT/386, not the
Inboard XT), with the *same* live trace instrumentation already built for the XT investigation left
active. Most instrumentation keyed to the guest OS's own internal file structure (a specific driver's
disassembled offsets, a documented interrupt/multiplex protocol) will fire identically on either
machine, since it's tied to the Windows/DOS build, not the hardware - only genuinely
hardware-dependent behavior (e.g. a real 8042 correctly answering a keyboard-controller command
sequence) will differ. The resulting two traces turn "guess what's missing on the XT+Inboard side"
into a direct diff against a real, successful reference execution - much more tractable than
continuing to reason from documentation or partial static analysis alone.

**Setup caveat**: don't reuse a disk image whose `CONFIG.SYS`/`SYSTEM.INI` were prepared for the
Inboard-specific target (loads `INBRDPC.SYS`, Inboard-patched VxDs, etc.) - those lines don't belong
on the reference hardware and will produce unrelated noise, defeating the comparison. Use a genuinely
vanilla install of the same guest OS build for the reference run, or a copy of the working image
with the Inboard-specific `CONFIG.SYS`/`SYSTEM.INI` lines stripped out first.

**2026-08-02 case (planned, not yet executed)**: after tracing a Windows 95 boot stall through
`HIMEM.SYS`, `IFSHLP.SYS`, and an `INT 2Fh` multiplex-hook chain without finding a definitive root
cause, and independently observing a BIOS keyboard-buffer-full beep suggesting something isn't
draining keyboard input via `INT 16h`, the natural next step identified was: boot the same Windows
95 build on a stock AT/386 86Box profile with the same `[a20trace]`/`[himemcaller]`-style
instrumentation active, and diff which `INT 2Fh`/keyboard-controller calls a genuinely successful AT
boot makes (and gets correctly answered) against what the XT+Inboard boot does - directly filling
the gap between "what Windows 95 expects" and "what this hardware actually provides."

**2026-08-02 case (executed, decisive)**: for the segment-650B wild-jump investigation (see
`memory/xt_650B_root_cause_null_far_call_2026_08_02.md`), after a specific patch failed to fix the
crash and the exact triggering mechanism remained unclear, ran the **same compiled debug build**
(not just the same guest OS - the identical binary with all trace hooks already compiled in)
against the AT success recipe's own profile (`vm_win95_at_gap/86box.cfg`, same
`Golden_win95_stage1_copy.img`-lineage disk image). Key design choice: the detector hook
(`[nulljump]`) was written to fire on the *generic symptom* (`CS:PC==0000:0000`, wherever that
happens) rather than a hardcoded address specific to the XT investigation - this mattered, because
if the AT run's boot-logo code lives at a different segment (plausible, given a different
CONFIG.SYS-driven low-memory layout), an address-specific hook would silently never fire and give
a false "AT doesn't have this bug" result for the wrong reason (never even reaching the check),
indistinguishable from a true negative. A generic symptom-based hook doesn't have this blind spot.
Result: zero hits across an entire run through to a confirmed working desktop - a clean, direct,
decisive negative that conclusively reframed the bug as Inboard/XT-specific rather than a general
Windows 95 boot-logo or emulator-interpreter issue. **Housekeeping**: the AT profile directory
lacked its own `roms/` folder (only the XT profile directory had one) - symlinked rather than
copied (`ln -s ../vm_win311/roms vm_win95_at_gap/roms`) to avoid duplicating a 106MB tree.

## Addendum to Technique 29: a live-fresh-read hook can rule out self-modifying code cheaply, separate from the underlying PC-bookkeeping question

When a static byte capture (a one-shot dump taken after the fact) seems to contradict live
execution behavior (Technique 29's core scenario), don't assume self-modifying code is the
explanation just because it's a plausible sounding one for old real-mode boot code - test it
directly and cheaply: add a hook that reads the bytes **fresh, live, every single time** the
CPU's PC reaches the address in question (not one-shot), and compare across visits. Identical
bytes across many visits rules out self-modification cleanly, narrowing the remaining
possibilities to the tracing/interpreter's own PC-advancement bookkeeping - a completely
different, and for this project novel, class of question (see Technique 29's case for the
specific unresolved discrepancy: a real disassembler proves the static bytes at `0048:0619` can
only ever decode as a 2-byte instruction, yet the live raw-ring trace proves the real execution
step there was 3 bytes - confirmed NOT self-modifying, 10 identical live reads across one boot,
still unexplained as of this session's end). A cheap, one-hook test is worth running before
spending more time reasoning about which explanation is more plausible.

## Technique 25: tally counts at a shared choke point instead of logging every hit, when you need "what's being called and how often" rather than "the exact sequence"

Technique 11's trigger-armed uncapped logging solves "show me every distinct address visited"
but is the wrong shape for a different, equally common question: "which services (interrupts,
ports, function codes) does the guest request during this window, and roughly how often" -
answering that with one `fprintf` per call either explodes log volume at a hot choke point (any
polling loop calls the same `INT`/port dozens of times a second) or needs a cap that gets
exhausted by boot noise (Technique 11's original problem) long before the interesting window.
Instead, keep a small in-memory table (a short linear-scan array of `(key, count)` pairs is fine
- these hot paths are already running through a slow interpreter, so a scan of a few dozen
entries is noise next to that) keyed on whatever identifies "the same kind of call" (e.g.
`(int_num << 8) | AH` for software interrupts), increment it on every hit with zero I/O, and only
`fprintf` the whole table on a coarse timer (every 5-10 real seconds, gated well past the boot-
noise period the same way Technique 11's arming flag is). This gives a live, low-volume, at-a-
glance answer to "what does the guest actually keep asking for" without needing to know the
interesting address/window in advance - useful as a first-pass survey before reaching for
Technique 1/12's address-specific tracing.

**2026-08-02 case**: added `[intcalltally]`, gated to start dumping at t+100s (past ordinary
POST/CONFIG.SYS noise) and every 10s thereafter - a direct, low-cost way to see INT 15h/16h/21h
call volumes during the CONFIG.SYS-stage stall, motivated by the still-unexplored "BIOS keyboard-
buffer-full beep" lead (a strong hint nothing is calling `INT 16h` to drain input). First placed at
`x86_int()` (see Technique 26 below for why that was wrong and had to be moved to `x86_int_sw()`).
Paired with `[kbdbuf]`, a per-second read of the BIOS Data Area's own keyboard-buffer head/tail
pointers (`0000:041A`/`0000:041C`) added to the existing `[modecheck]` per-second block - real-mode
low memory is always identity-mapped, so `mem_readb_phys()` is the right tool here (same as the
existing `B8000` VRAM dump), not `readmemb()` (Technique 16's page-table-aware reader is for
segments that might be paged, which BIOS Data Area addresses never are). Result at the actual
stall this session: `head`/`tail` sat equal (buffer empty) throughout, so the buffer-full theory
didn't hold for *this* particular stall instance - the beep from the prior session was a real,
but apparently not-always-present, symptom.

## Technique 26: two functions can both plausibly be "the INT dispatch choke point" - verify which one the specific instruction you care about actually reaches, by checking a real hit first

Technique 17 established that hardware-IRQ delivery and CPU-raised exceptions share a single choke
point (`pmodeint()`) regardless of caller. It's tempting to assume the *same* function is also the
choke point for the software `INT n` instruction - but this codebase actually has two separate
top-level functions, `x86_int(num)` (CPU-raised exceptions only: divide error, BOUND, GP, double
fault, etc - grep its callers, all are exception sites) and `x86_int_sw(num)` (the real `INT n`
opcode's own handler, in `x86_ops_int.h`) - both eventually call into `pmodeint()`, but a hook
placed in `x86_int()` will **never** see a guest's `INT 21h`/`INT 16h`/`INT 2Fh` calls, only
CPU exceptions. **2026-08-02 case**: `[intcalltally]` was first added to `x86_int()` (a plausible-
looking choke point - Technique 17's own writeup already documents it as receiving both real-mode
and V86 dispatch) and immediately "worked": it started producing a suspiciously clean, steady,
exactly-once-per-second tally entry (`INT05h/AH4Fh`). That regularity was itself the tell that
something was wrong - real guest interrupt traffic is bursty, not metronomic - and cross-checking
`num=5`'s callers found it's the `BOUND` range-check exception, not a real interrupt, confirming
the hook was on the wrong function entirely. **Lesson**: before trusting a new hook's first "hit,"
check whether the specific call/value it reports is even *possible* to originate from where you
placed it (grep the hooked function's own other callers/call sites) - a hook on the wrong-but-
adjacent function can still produce plausible-looking, non-empty output and pass a superficial
sanity check.

## Technique 27: a steadily-growing counter can be a red herring if its growth rate matches a known, universal periodic event (the ~18.2Hz PC timer tick) rather than being unique to the bug under investigation

A hit-count that climbs smoothly and linearly for the entire observation window looks exactly like
"the one thing still happening in an otherwise-frozen stall" - but before treating it as the bug's
own active loop, check its growth rate against the standard PC timer tick (IRQ0, ~18.2 ticks/
second, the classic `1193182/65536` PIT-divisor rate every DOS/BIOS system runs at from power-on).
If the observed rate is in that neighborhood, the counted call is very likely part of ordinary,
universal BIOS timer-tick housekeeping (cursor blink state, a `INT 1Ch` user-hook tail, etc) that
fires at that rate on *every* boot, hang or not - not a symptom specific to the stall. **2026-08-02
case**: `[int10Fcaller]`'s per-call-site tally found a single site, `F000:AC00` (disassembled via a
direct Python byte-read at the dump file's own offset, not `objdump`'s linear scan, which desyncs
across the ROM's embedded data blobs and silently produces wrong instruction boundaries - see the
caveat this added to Technique 16's own live-dump approach), growing at a measured ~19/s across a
230-second window (188 hits at t+20s -> 4551 at t+250s) - close enough to 18.2Hz, once wall-clock
sampling noise is accounted for, to identify it as the BIOS's own periodic `mov ah,0Fh` / `int 10h`
video-state check, not a bug-specific spin. **Still a useful negative result**, though: with this
one explained away, literally nothing else in the same tally advanced during the stall - meaning
the actual stuck loop makes zero application-level software-interrupt calls of any kind, reinforcing
(at a deeper level than the existing CS:PC-segment evidence) that this is a pure CPU-level spin with
no OS-service dependency, consistent with the SHLD-based bitstream-decoder theory from earlier this
session (`memory/win95_emulator_repro_2026_08_02.md`).

## Technique 28: a binary "patch" script must assert it actually found and changed something, or a build-mismatch can silently produce an unpatched file with a misleading name

When a patch script searches a binary for a specific raw byte pattern (an opcode, an immediate
operand) and rewrites matching sites, it must refuse to write output if it found zero matches -
otherwise a stock/reference file from a different build than the one the search pattern was
originally derived against will silently produce a "patched" output file that is actually
byte-identical (or near-identical) to plain unpatched stock, with no error, no warning, and a
filename that confidently claims otherwise. This is exactly the kind of silent-failure trap
Technique 7 warns about for memory-file claims, but it applies just as sharply to tooling output.

**2026-08-02 case**: `vxd-patches/patch_vpicd.py` and `patch_vdmad.py` both had this bug -
`open(OUT, "wb").write(data)` ran unconditionally, with no check on the `patched` counter first.
Confirmed by re-running both scripts fresh against this project's own actual disk image's real
stock `VPICD.VXD`/`VDMAD.VXD` (extracted directly from
`image_backups/Prestaged_pre_vmm.ORIGINAL_2026-08-01.img`, not assumed): both reported "Found 0 raw
candidates" and then wrote an output file anyway. Direct comparison proved `vxd-patches/
VPICD_INBOARD.VXD` and `VDMAD_INBOARD.VXD` - deployed and relied on across every prior XT
investigation session - were genuinely unpatched stock the entire time (VDMAD: exact md5 match to
stock; VPICD: identical except a 16-byte internal project-watermark region). `patch_vkd.py` was
unaffected because it already had an unconditional `assert actual == ORIGINAL_BYTES` before
touching anything - the fix applied to the other two scripts (both now `raise SystemExit(1)` on
`patched == 0`) brings them to the same safety standard. **Lesson for any future patch/codegen
script in this project**: always assert a nonzero, expected-shape result before trusting or writing
its output, the same way a live trace hook's first "hit" needs sanity-checking (Technique 26) - a
script that "succeeds" by doing nothing is far more dangerous than one that errors loudly, because
its output looks identical to a real fix at every layer above it (file exists, right name, right
size in the lucky cases) until someone directly diffs the bytes.

## Technique 29: when a ring-buffer "predecessor" address looks nonsensical, widen the byte capture and hand-decode carefully before concluding it's just an approximation artifact - it can be a symptom of a wild jump, one hop further upstream

Technique 17 warns that an exception-triggered "caller" from the ring-buffer predecessor
technique can be approximate. It's tempting to stop there whenever a predecessor address looks
too low/implausible to be real code ("it's just an artifact of the dispatch mechanism"). Before
accepting that explanation, systematically rule out every dispatch mechanism first (a short,
cheap list: hook the interrupt/trap-gate CS-assignment point for the specific destination
selector; hook the same function's *entry* unconditionally for the specific source segment,
since a zero-hit result there is a strong, direct negative per Technique 19's "always test the
gate-removed case" spirit; hook the V86-mode IRET branch filtered on the specific bad value).
If all of those come back clean/negative, the nonsensical address is very likely a genuine
*symptom* worth explaining on its own terms, not noise - it can be the CPU's actual, if
accidental, decode position after a real (but different) wild jump happened even further
upstream.

**Capture more bytes than seems necessary, and hand-decode opcode-by-opcode, tracking exact
instruction length at every step** - a short window (10 bytes) can end mid-instruction-stream
without ever reaching the actual control-transfer opcode, producing a false "this looks like
ordinary ALU code, not a jump" read that stalls the investigation. Widening to 50+ bytes and
decoding forward strictly (never skip/guess an instruction boundary) is cheap compared to
another live-hook rebuild cycle. Once a plausible transferring instruction is found (e.g. an
indirect `CALL FAR [mem]`/`JMP FAR [mem]`), resolve its actual operand (the displacement,
the memory location it reads from) and read that location's *live* contents directly with one
more hook - don't stop at "this instruction shape could explain it," confirm the specific data.

**2026-08-02 case**: `[seg650Bcaller]`'s ring-buffer predecessor for the segment-650B stall
reported `caller=0000:0038` - implausibly low, "looks like an artifact." Ruling out `pmodeint()`
interrupt/trap-gate dispatch (both for the `650B` destination and the `0000:0000` intermediate
step), `pmodeint()` entry from the source segment at all (zero hits, a clean negative), and
`pmodeiret()`'s V86-IRET branch (filtered on the destination segment, zero hits) left only a
direct instruction-level far transfer. A first 10-byte capture from the true predecessor address
decoded as ordinary `OR`/`ADD`/`ROR` opcodes with no visible jump - widening to 56 bytes and
decoding through an intervening `RET` (a real subroutine return, not the actual bug) found the
real culprit: `CS: CALL FAR [0144]` (`2E FF 1E 44 01`), an indirect far call. Resolving `0144`'s
linear address and reading it directly confirmed a genuinely uninitialized (all-zero) far
pointer - the true root cause, three full hops upstream of where the ring buffer's raw
predecessor value first pointed, and the reason `0000:0038` looked nonsensical in the first
place: it wasn't a caller at all, it was the CPU already several steps into decoding raw IVT
table bytes as an accidental instruction stream after the *real* wild jump had already happened.
Full writeup: `memory/xt_650B_root_cause_null_far_call_2026_08_02.md`.

**2026-08-02 correction, same session, added after live-testing the fix**: patching the
`0048:0144` pointer (redirecting it to a safe `RETF` stub, confirmed via a live read that the
patch genuinely took effect and persisted) produced **the identical crash** - proving the
hand-decoded `CS: CALL FAR [0144]` instruction, despite reading a genuinely-zero pointer, was
**not actually the live-executed trigger**. This is the sharpest possible restatement of this
technique's core warning: a plausible-looking hand-decoded instruction that reads confirmed-bad
data is still only a hypothesis until the fix is tested end-to-end - reading confirmatory data
is not the same as confirming causality. **Always test the fix, not just the diagnosis, before
updating memory to say "root cause found."**

The follow-up investigation added two tools worth reusing directly: (1) a small **always-append,
never-deduplicated** per-instruction ring buffer (separate from the main dedup-on-change ring
buffer already described above) - the main ring buffer is correct for "which distinct addresses
ran" but a backward hand-decode from its last recorded predecessor can still desync if the
hand-decode itself has an error, since the main ring never proves your decoded instruction
*lengths* were right, only that the addresses it happened to record were real. An undeduped ring
directly gives ground-truth instruction boundaries (the delta between consecutive raw entries
*is* the real length of whatever executed there) with zero decoding involved. (2) Feed the
reconstructed byte stream to a **real disassembler** (`capstone`, `CS_MODE_16` for real/V86-mode
16-bit code) instead of continuing to hand-decode - and cross-check its output against the raw
ring's ground-truth boundaries opcode-by-opcode, not just at the end. This combination caught a
second, deeper anomaly by itself: capstone's linear decode from a confirmed-correct starting
point (`0617`) desyncs from the raw ring's own ground truth almost immediately (`0619` onward) -
the bytes there (`00 EC`) can only ever decode as a 2-byte `ADD AH,CH` under standard x86 rules,
yet the raw trace proves the real execution step was 3 bytes. That contradiction remains
unresolved as of this session's end (candidate causes: self-modifying code, an interrupt-boundary
PC-advancement artifact, or a bug in the hook's own `cpu_state.pc` read timing) - a good example
of a case where the disciplined tooling (raw ring + real disassembler) surfaced a genuine anomaly
that pure hand-analysis had been silently overwriting with a plausible-but-wrong story.

## Technique 30: when the exact mechanism resists identification, a blind "neutralize this address" patch can unblock progress without ever finding it

Techniques 29's mechanism-hunt for the segment-650B wild jump hit a genuine wall: a real
disassembler contradicted the live execution step length at a nearby address, and no amount of
further hand-analysis or live-fresh-byte-reading resolved it. **Don't let an unresolved mechanism
question block trying the fix anyway**, once you have one fact nailed down with certainty (here:
raw undeduped tracing proved `0048:0637` is the *exact*, single, zero-intermediate-steps
instruction that transfers control to the bad destination, even though *what it decodes to*
remained unclear). A one-shot hook that fires the moment `CS:PC` reaches that exact address and
forcibly sets `cpu_state.pc` past it (skip execution entirely, don't try to emulate what "should"
happen) is cheap to try and immediately falsifiable - either the downstream symptom stops
recurring (strong evidence the address really was the trigger, regardless of why) or it doesn't
(back to the mechanism hunt, but you've lost almost nothing).

**2026-08-02 case**: `[skip0637]` (`386_dynarec.c`) - `if (CS==0x0048 && cpu_state.pc==0x0637) { cpu_state.pc = 0x0639; }`,
one-shot - completely eliminated the segment-650B stall that had resisted a specific, verified-but-
insufficient data patch (`[patch0144]`, Technique 29) earlier the same session. The CPU proceeded
into genuinely new territory (segment `020B`, confirmed INBRDPC.SYS's own resident code) that this
project had never reached on the real XT+Inboard profile with genuinely-patched VxDs, all without
ever resolving what the `0637` instruction actually was. **This doesn't mean the mechanism question
stops mattering** - a blind skip is a debugging tool to find out whether an address is truly load-
bearing for the bug, and to make forward progress while the real question stays open, not
necessarily a deployable end-state fix (skipping unknown guest code has obvious correctness risk
for real use) - but for an emulator-side investigation trying to find out "how far past this point
does the system actually work," it's a legitimate, high-value experiment to run before continuing
to sink time into full mechanism identification.

**Compounding finding, same session**: once past the `0637` block, combining this skip with an
*already proven, previously undeployed-on-this-profile* fix (`inbrdpc_fixed_v2.bin` from
`memory/win95_emulator_repro_2026_08_01.md` - an INT06h fault-loop + resident-size fix for
INBRDPC.SYS, confirmed effective back on 2026-08-01 but never tested with genuinely-patched VxDs
until now) pushed boot further still, through HIMEM/EMM386 territory, before hitting a new stall.
**Lesson**: when a session finds a new way past an old blocker, always check whether previously-
built-but-shelved fixes for *later* blockers are still sitting unused and worth re-deploying on
the newly-reachable configuration - this project has accumulated several such fixes
(`inbrdpc_fixed_v2.bin`, the VxD patches, etc.) across sessions that only pay off once whatever
was blocking progress *before* them is separately resolved.

## Technique 31: a near CALL/JMP through a computed pointer can be the last *visible* step before a wild far-jump - the actual transfer is hiding in already-visited code the dedup ring buffer won't show

When backward-tracing a wild jump (Technique 1/29 style) lands on a **near** CALL/JMP (opcode
shape confirms it - e.g. `FF /2` = `CALL r/m16`, same segment only) as the "immediate predecessor"
of a segment change, don't conclude the trace is wrong or the mechanism is exotic. The main ring
buffer only records *distinct* (CS,PC) pairs - it silently skips any instruction whose address was
already logged earlier in the same run. A near call's target is very often code that ran earlier
in boot (a subroutine, a shared handler) and is therefore invisible to a "predecessor" query even
though it executed again, for real, in between the near call and the eventual far transfer. The
apparent contradiction ("this near call can't explain a CS change") is the tell that real
execution happened in a gap the ring buffer can't see, not a decoding error.

**Fix**: don't try to hand-wave past the near call - capture the operand register values (the
base/index registers the indirect addressing mode actually uses, e.g. `BX`+`SI` for a `[BX+SI+
disp8]` near call) so the exact target address is known, then arm a **raw, undeduped** per-
instruction trace (the same tool Technique 29's addendum built for the 650B bug's `0619`
discrepancy) starting the moment that near call is taken, running forward with no dedup until `CS`
actually changes - this shows every instruction in the gap with zero blind spots, including
repeated visits to old code, and will contain the real `CALL FAR`/`JMP FAR`/bad `RETF` responsible.

**2026-08-03 case**: EMM386 halted with "error #04 in an application at memory address 0128:009B"
on the XT+Inboard profile (post-[skip0637]+`inbrdpc_fixed_v2.bin`, a new stall past the
segment-650B fix). A [seg650Bbackward]-style 40-step backward dump found the predecessor of
`0128:0017` (first entry into the faulting segment) was `0048:00A8`, decoding unambiguously as
`CALL WORD PTR [BX+SI-0x0A]` - a near call that structurally cannot change `CS`. Segment `0128`
itself, once dumped and string-extracted (Technique 16), turned out to be a live FAT
directory-entry buffer (a `BOOTLOG.TXT` entry visible in the bytes) - pure data, not code - and the
whole `0128:0017-009B` span the CPU "executed" through it was later confirmed (via a wider
40-step-back dump) to be garbage-decoded data bytes the entire way, the same "wild jump into blank/
data memory" shape as the segment-650B null-far-call bug
(`memory/xt_650B_root_cause_null_far_call_2026_08_02.md`). Segment `0048` itself was already
established (that same prior investigation) to be `IO.SYS`'s own low-memory boot-time code, not a
loadable driver - meaning this is very likely a second, still-not-fully-explained symptom of the
same unresolved `IO.SYS`/XT-hardware-detection mystery that produced `0637`, not an INBRDPC.SYS or
EMM386 bug specifically. Not yet resolved as of this session's end: the exact instruction inside
the near call's target that performs the actual far transfer - next step is capturing `BX`/`SI` at
`0048:00A8` and running the raw-trace forward from there, per this technique.

## Live hardware bridge (COMrade / COMR95) — real hardware only, not the VM

For cross-checking emulator behavior against the **real 5160**, `COMRADE`/`COMR95` (Windows
95-compatible build, `COMR95 /com1 /baud 115200`) is the live bridge — HELLO/status, text screen,
desktop thumbnails/screenshot, file read/write, directory listing, CRC hashing, keyboard input.
See `memory/comrade_bridge.md` for current capability details. Use it to settle "does real
hardware show this too" questions directly instead of guessing from a photo or a stale note —
this was flagged as the fastest way to resolve the "ROM BIOS shadow RAM failed" system-BIOS
mismatch (`mem_dump` of physical `0xF0000-0xFFFFF` on real hardware) but wasn't available in the
session that hit it. Check whether it's connected before assuming it isn't.

### Addendum 2026-08-23: COMR95 and COMRADE are NOT interchangeable - check which one is running

Confirmed by reading the Win95 agent's own dispatch table (`win95/comr95.c`, `dispatch_frame`):

| Capability | DOS `COMRADE.EXE` | Win95 `COMR95.EXE` |
|---|---|---|
| file read/write, dir list, hash | yes | yes |
| screen text, keystrokes, reboot | yes | yes |
| **`mem_read` / `mem_write`** | **yes** | **no** - falls through to `ST_BAD_ARGS` |
| **`io_in` / `io_out`** | **yes** | **no** |
| `bus_stim` / `idx_*` / `io_rmw` / `pic_snapshot` | yes | no |
| `desktop_screenshot` | no | yes (but see below) |

**Plan around this: DOS mode is a superset for nearly all diagnostic work.** Any port or memory
probe - the `02E8` Mach8 check, DMA register reads, PIC state - needs a DOS boot, not the Windows
session. Everything else in Windows is reachable as files.

**`desktop_screenshot` times out at the default 8s op-timeout, and asking for a smaller thumbnail
does not help.** `handle_screenshot()` does a `StretchBlt` of the **entire** screen into a 32bpp DIB
before downscaling, so cost is set by source resolution and the 8-bit ISA bus, not output size.
Raise `--op-timeout` in the MCP server args if it is needed.

**Reading a register does not mean the register is readable.** `io_in 0x83` (DMA page, channel 1)
returns `0xFF` on the real 5160 - the page latch is write-only and the bus floats. Any experiment
built on writing a page register and reading it back to observe 4-bit truncation **cannot work on
this hardware**. Establish readability before designing a probe around it.

**MCP server paths (as of 2026-08-23):** both `comrade` (COM2, real hardware) and `comrade86box`
(COM3) now run Ahmad's tree at `Open-Source-PC110\Software\COMrade` under the pythoncore 3.14
interpreter, which is a strict protocol superset of Kevin's. `MSDOS.SYS` on the card has
`BootGUI=1`, so **a reboot goes straight to Windows** - and COMrade autostart is commented out in
`AUTOEXEC.BAT`, so rebooting unattended loses the bridge until someone starts the agent by hand.

**Do not use this bridge (or the `comrade86box` MCP variant) to introspect the *VM* itself —
decided against 2026-07-31 after a long session chasing it.** The architecture is fundamentally
mismatched for that: the MCP server's process lifecycle is tied to the Claude Code session, while
the VM gets killed and relaunched constantly during debugging, and nothing in the chain (86Box's
named-pipe server, HHD's COM-port-to-pipe client bridge, the python MCP client holding the COM
handle open) reconnects when one side restarts — whichever side started first just holds a dead
handle. It also duplicates capability the project already has natively and more reliably
(Technique 9/2 above, plus Technique 1/10/11/12's source-level tracing) for anything that's about
the emulator's own internal state. `comrade`/COMrade proper stays real-hardware-only.

## Technique 32: when you need a calling convention/data-structure format for a NEW patch, live-capture it from a reliable profile running the identical code, don't guess from the problematic profile's own uncertain disassembly

When building a patch that needs to correctly speak an existing protocol (a BIOS service's
calling convention, a driver's internal data layout) and the code implementing/using that protocol
is byte-identical across a working profile and the problematic one (e.g. `HIMEM.SYS` on AT vs XT
Inboard), don't try to reverse-engineer the format from the problematic profile's own disassembly
if that profile is also the one with unresolved non-determinism or unreliable static analysis —
add a generic capture hook (keyed on the *symptom*, e.g. `INT 15h` with a specific `AH`, not a
hardcoded address) and run it on the *reliable* profile instead, where you already know execution
will complete cleanly. The resulting live-captured data (register values, memory structures) is
then directly reusable when building the new patch for the problematic profile, since the code
generating it is the same binary either way.

**2026-08-03 case**: needed the exact 48-byte GDT descriptor-table format `HIMEM.SYS` builds for
`INT 15h AH=87h` to write a new extended-memory probe in `INBRDPC.SYS`. A first attempt tried to
find a reusable "build a descriptor" helper by disassembling `INBRDPC.SYS` itself around a
plausible-looking `call` target — the target address didn't even exist in the file (a disassembly
desync, the same class of mistake Technique 29 already warns about). Instead, added `[int1587gdt]`
to `x86_int_sw()` (`386_common.c`, generic on `num==0x15 && (AH==0x87||AH==0x88)`, not tied to any
address) and ran it on the AT profile — which boots reliably — capturing the exact live descriptor
bytes `HIMEM.SYS` builds there. This is standard, real-world-proven Microsoft code, not a guess;
the same hook fired identically on the XT profile immediately afterward, confirming the exact same
calling pattern reproduces there too (same binary, same behavior) — the AT capture wasn't just
analogous, it was ground truth for both profiles at once.

## Technique 33: a new hook installed at a driver's INIT very first instruction can trip the driver's own residency/duplicate-load self-check — install after any early self-check logic runs, not before it

DOS TSRs/device drivers commonly self-check "am I already loaded" near the very start of their own
`INIT` routine, often by inspecting whether some interrupt vector they care about already points
into their own code segment. If a new patch installs an interrupt hook by detouring `INIT`'s first
instruction, that hook's install runs *before* the driver's own self-check — which can then see its
own hook already installed and incorrectly conclude a second copy of itself is loading, even on a
genuinely single, fresh load. The fix is mechanical once suspected: move the detour to a *later*
same-length-replaceable instruction in `INIT`, positioned after whatever early self-check logic the
driver does (a `call`/`self-test-table loop` a little further in is usually a safe, easy landing
spot), and re-test — no need to fully prove the self-check's exact mechanism first (same spirit as
Technique 30's "blind fix, verify by testing" approach).

**2026-08-03 case**: `INBRDPC.SYS`'s own new `INT 15h` hook, first installed via a detour at
`INIT`'s literal first instruction (`0xA701`), caused it to immediately show its own genuine,
pre-existing stock error — *"There's more than one DEVICE=iNBRDPC.SYS command in the CONFIG.SYS
file"* — on a verified single-line, freshly-cloned, non-dirty `CONFIG.SYS`/image. Moving the same
hook-install call to a same-length-replaceable instruction at `0xA731` (after the driver's own
16-entry self-test-table loop) eliminated the false error completely, with the hook's actual
functionality unaffected. The mechanism (does `INBRDPC.SYS`'s duplicate check specifically inspect
`INT 15h`'s vector?) was never fully proven, only strongly suspected and successfully worked around
— consistent with Technique 30, a working fix doesn't require a fully proven mechanism first.

## Technique 34: a stack-pointer delta of exactly +2 with BP unchanged, right before a "wild jump," means it's a plain RET into a missing-code gap, not a corrupted pointer — the fix target is different

When backward-tracing a CS-segment change that looks like a wild jump (Technique 1/29/31 style),
check the stack pointer's own delta across the transfer before assuming a bad table/pointer is
involved. If `SP` increases by exactly 2 and `BP` is unchanged, that's the unmistakable signature
of a plain, ordinary `RET` (near call/return) — the transfer target is not a corrupted jump table
entry or a bad computed address at all, it's a completely legitimate *return address* from some
earlier, correctly-functioning `CALL` elsewhere in the code. This reframes where the bug actually
is: not "what wrote a bad pointer here," but "why is the code that's supposed to exist at this
known-legitimate return address missing/zeroed" — a different, narrower question, and the next
step is finding what calls into the routine that returns here (to learn what's upstream) and what
should occupy the return address itself (to learn what's missing), not chasing a pointer-table
theory that doesn't apply.

**2026-08-03 case**: a live raw register trace (`[seg1278trace]`/`[seg1278raw]`, armed a few
instructions before the observed transfer, avoiding the ambiguous static-disassembly trap Technique
29 already warns about) showed `SP` going from `0FE2` to `0FE4` and `BP` staying at `0FE4` across
the `0048:1278` → `0048:00B6` transfer — confirming a plain `RET`, not a wild jump. This reframed a
second, previously-mysterious wild-jump-into-segment-0128 crash as "IO.SYS's own low-memory
workspace has ANOTHER location where expected resident code is missing on this hardware," the same
underlying disease as the already-known `0048:00A8` bug, not a new or different bug class.

## Technique 35: when patching binary VxDs/DRVs stalls out on ambiguous disassembly, read the real DDK source instead of guessing at more offsets

This project spent many sessions finding and patching individual `IN AL,64h`-style byte patterns in
stock `VKD.VXD`/`KEYBOARD.DRV` — each one a real, necessary fix, but each also just one instance of
a whole *class* of AT-only assumption scattered across a file with no map of where the rest might
be. The actual breakthrough came from abandoning further binary-patch guessing and building the
target VxD **from Microsoft's own period DDK sample source** instead (`Windows95_ddk\KEYB\SAMPLES\
VKD\`), reading the real procedure (`VKD_Int_09`) in readable assembly, and fixing the logic
directly. This is strictly more reliable than pattern-scanning a stripped binary once DDK source
exists for the target file, and works for the *whole* function's logic in one look, not just the
one byte sequence you happened to search for.

**Toolchain, confirmed to run natively on modern Windows 11, no DOSBox needed**: genuine 32-bit PE
MASM 6.11c (`Windows95_ddk\MASM611C\ML.EXE`) via WOW64, linked with the bundled VC++ 2.0-era
`Windows95_ddk\MSVC20\LINK.EXE` (modern `link.exe` dropped the `/VXD` flag this needs). `$env:INCLUDE`
must include **both** `INC32` (most VMM headers) and `INC16` (`PIF.INC`/`CMACROS.INC` only live
here) — the sample `MAKEFILE`'s `INC16`-only path fails. Working build script: `custom_vkd/build.ps1`.

**Escalation rule of thumb**: if a second or third binary patch attempt at the same general class of
bug (same file, same kind of port/API assumption) is needed, that's the signal to stop guessing at
more offsets and check whether DDK source exists for the file first.

## Technique 36: prove a bug is real and hardware-specific (not a red herring) by cloning the *unpatched* base onto a known-good reference profile, never an already-patched image

When a fix doesn't fully resolve a symptom and you're not sure whether the remaining gap is a real,
narrower bug or a fundamentally wrong theory, get a clean differential comparison: clone the exact
same disk state onto a machine profile known to have correct, standard hardware for the subsystem in
question (e.g. `deskpro386`, a real dual-8042/dual-PIC AT, as the reference for anything XT+Inboard
is suspected of getting wrong), and confirm the same UI/behavior works normally there.

**Critical gotcha, caught by the user mid-session**: clone from the **truly unpatched** base image,
never from an image that's already had XT/Inboard-specific patches applied. An XT-patched image's
fixes (e.g. "always report port 64h ready") assume no real 8042 exists — deploying them onto a real-
8042 AT profile is testing something that was never a fair comparison to begin with, and any result
is uninterpretable. Re-clone from source and remove only the hardware-specific driver (e.g.
`INBRDPC.SYS`) that doesn't apply to the reference machine.

## Technique 37: an emulator-side self-heal/timeout workaround can silently mask an incomplete fix — check whether it's modeling real hardware behavior (must be ported to real HW) or emulator-only forgiveness (won't exist on real silicon) before declaring victory

`kbc_xt.c`'s `[blockedtimeout]` (a bounded auto-clear of a stuck keyboard-latch flag) let a real
keystroke-delivery bug look fully fixed in emulator testing, because the timeout quietly compensated
for a missing acknowledgment the guest software should have performed itself. The distinction that
matters: is the workaround modeling something a **real chip** would also eventually recover from
(then it's fine, it's accurate emulation), or is it emulator-only leniency for a genuinely broken
guest-side assumption (then real hardware will hit the original bug in full, with no recovery)? Here
it was the latter — real XT keyboard hardware, per FastDoom's real-hardware-validated ISR, requires
an explicit port-0x61-bit-7 acknowledgment per keystroke or it never presents the next one, with no
timeout escape at all. Caught by directly comparing the emulator's own self-heal condition against
independently-sourced real-hardware reference code (FastDoom), not by testing on real hardware and
finding out the hard way. **Before trusting an emulator-confirmed guest-software fix as ready for
real hardware, explicitly ask what happens if every emulator-side leniency/self-heal this session
relied on is removed.**

## Technique 38: an uninitialized-memory-poke fix proven only inside the emulator's CPU core needs a *timing-equivalent* real-mode translation, not just a byte-identical one

A fix implemented as a live physical-memory write inside the emulator's dynarec (`[patchint68]`,
pre-filling an uninitialized interrupt vector) has no real-hardware form by default — there's no
"CPU hook" to deploy to real silicon. The translation has to be a real, running program on the
target, and **timing matters as much as the bytes do**: the same write, tried at boot start, was
already proven to get clobbered by ordinary BIOS POST/DOS low-memory init before the vector was ever
used — only firing it right before the specific code path that needs it (empirically, "the moment
segment 0EAF starts running") survived. The real-hardware translation (`IVT68FIX.COM`, a 26-byte
hand-assembled `.COM`) is called from the **very last line of `AUTOEXEC.BAT`**, as late as possible
before Windows auto-launches — the closest real DOS boot gets to matching the only timing already
proven to work, not an arbitrary convenient place to put it.

## Technique 39: once real hardware is available and behaving consistently with the emulator, prefer it for verification over another emulator rebuild/boot cycle — it's free in token budget, an emulator cycle isn't

Once an emulator-confirmed fix chain has real-hardware deployment ready, and token budget is tight,
the highest-value move is often to skip a "let's verify once more in the emulator first" cycle and
deploy straight to real hardware instead. A full rebuild-clone-boot-monitor cycle in the emulator
costs real assistant budget; a real-hardware boot costs the user's time, not tokens, and gives
strictly stronger evidence (COMrade/COMR95 can query live state) than another simulated pass. This
is specifically the right call when: the remaining unknown is a genuine timing/environment question
(not a logic bug you can reason about from source), and a wrong guess on real hardware is cheaply
recoverable (a card image, not a one-way action).

## Technique 40: a black-screen/stall late in first-time Setup completion may just need a reboot — don't assume a fresh, deeper bug before trying the cheapest recovery step

Real-hardware Setup hit a black screen at the same "About to show Help files" point the emulator had
previously hit (before the display-driver fix). Rather than immediately assuming a new, undiagnosed
bug, a plain reboot was tried first — and it went straight through to a fully working desktop. Not
yet root-caused *why* the reboot helped (worth investigating if it recurs), but the practical lesson
for this specific class of "everything traces correctly, then the boot just doesn't visibly proceed"
symptom late in first-boot Setup completion: try the cheap, reversible recovery step (reboot) before
spending investigation budget on a fresh live-tracing session.

## Technique 41: when a PR/submission strips "debug hooks" by excluding whole files, it can silently drop real fixes tangled inside them — diff every file with this project's fingerprints, not just the files the submission's own file list says it touched

A submission built by hand-selecting which files to include (because the rest "looked like debug
scaffolding") is a different, riskier operation than stripping debug lines from files that *are*
included. A file full of `fprintf`/trace blocks can also contain a handful of genuinely necessary
behavioral fixes woven in between them - excluding the whole file because it reads as debug-heavy
silently drops those too, with no error, no warning, and a merged PR that looks complete.

**2026-08-21/22 case**: `86Box/86Box#7626` (this project's own Inboard 386/PC submission) was
scoped by an earlier session to "the device model and the small number of core-file timing/PIC/DMA
fixes it depends on," explicitly excluding "debug/tracing hooks" - see `upstream-submission/
README.md`'s own description. This wholesale exclusion silently dropped three files containing real,
load-bearing fixes that happened to also carry heavy trace instrumentation: `cpu/386_dynarec.c` (the
101 self-test's two follow-up fixes - IRQ1 suppression across a specific BIOS self-test window, and
forcing a DMA-refresh status bit the guest's own read-and-clear check would otherwise miss - plus a
whole family of Mach8/ATI option-ROM PIT-readback delay-loop fixes), `cpu/cpu_table.c` (the
hand-calibrated 83.5MHz Blue Lightning speed grade, with its own tuned `mem_read_cycles`/
`atclk_div`, interpolated between the stock 75/100 entries - omitting it makes 86Box silently snap
any request for 83.5MHz to the untuned 100MHz entry instead), and `device/kbc_xt.c` (a bounded
self-heal for a stuck XT keyboard-latch flag). **Diagnostic method that found all three**: `grep -rl
"2026-07-2[0-9]\|2026-08-0[0-9]"` across the whole local source tree for this project's own
dated-comment convention, cross-referenced against `gh pr diff <PR#> --name-only` - any file with
project fingerprints that *isn't* in the PR's file list is a candidate for exactly this failure
mode, regardless of how debug-heavy it looks.

**Practical fix pattern once found**: extract just the non-`fprintf`/non-`static ... _hits`-counter
lines (a `grep "^[<>]" diff_output | grep -v "fprintf\|fflush\|static.*_hits\|..."` filter works
well for a first pass), verify each surviving hunk actually mutates guest-visible state (register
writes, `picintc()`/`device_add()` calls, table entries) rather than just observing it, and port
only those hunks - not the whole file - into a clean patch.

## Technique 42: don't assume upstream drift explains a regression just because a shared subsystem has a large diff — check whether the specific mechanism you depend on is still wired correctly before writing off the whole subsystem as "moved on without us"

A large diff in a shared core file (hundreds+ lines) looks alarming and invites a "upstream rewrote
this, our fixes don't apply to the new reality" conclusion. Before accepting that, check the *exact*
call chain your own fix depends on, function by function, in the current upstream version - a large
diff is very often dominated by an alternate code path gated behind a build flag (dead weight, not
active), or general unrelated improvement work, with the actual integration point your fix needs
either untouched or (better still) already accounted for by an upstream maintainer.

**2026-08-21 case**: `src/pit.c` showed a 1189-line diff against this project's fork - an entire
`#ifdef NEW_PIT` alternate PIT implementation (`ctr_set_out`/`ctr_decrease_count`/`ctr_tick`, a
different counter model) that doesn't exist in the older fork at all. This looked like a serious
architectural divergence worth treating as a probable root cause for a boot failure under
investigation. Checking the actual commit history (`gh api repos/86Box/86Box/commits -f
path=src/pit.c`) instead of just the diff size found the real story: upstream introduced that
overhaul on 2026-07-18, found it broke too much, and reverted it back close to the old behavior on
2026-08-04 - and on 2026-08-06, the exact day this project's PR merged, a maintainer commit titled
"Make the PIT use the correct DMA refresh function depending on the DMA type in use and fix a
warning in the InBoard 386 code" applied a compatibility fix to *both* PIT code paths, keyed
directly off this project's own `dma_xt8237_active()` (which the same commit changed from `static`
to exported specifically so `pit.c` could call it). The large diff was mostly inert alternate-path
code plus general unrelated work; the one thing that mattered for this project was not only intact,
it had been proactively kept compatible by upstream. **Lesson**: `gh api .../commits -f
path=<file>` (commit history, not just a diff) is cheap and can turn "this looks scary" into either
a confirmed real regression or a confirmed non-issue in a couple of minutes - check it before
spending a debugging session on a subsystem-level theory.

## Technique 43: when testing two builds against the same disk image, give each its own config file - config auto-normalization can silently corrupt the *other* build's settings

86Box rewrites `.cfg` files on every load, filling in normalized/default values including, for a
machine's own `device_config_t` section, a header matching the *currently loaded binary's* exact
device `.name` string (Technique 4). If two builds with different internal device names (e.g. after
an upstream rename) are run alternately against the same physical config file, each run's
auto-rewrite can silently break the *other* build's next run - a device-specific key like
`enable_5161` that was correctly set stops resolving the moment the section header no longer matches
whichever binary is about to read it, and 86Box falls back to that key's compiled-in default with no
error. This reproduces a previously-fixed bug (see the `enable_5161`/section-name case under
Technique 4) for a completely different, self-inflicted reason - not a code regression at all, just
shared test-harness state.

**2026-08-21/22 case**: alternating a local build (`ibmxt_inboard386_device.name = "IBM XT (1982) w/
Intel Inboard 386/PC"`, pre-rename) and a fresh upstream clone (renamed by a maintainer to `"IBM XT
(Inboard 386/PC)"`) against one shared `86box.cfg` produced a confusing, seemingly-random pair of
POST errors (`1801` then `301`) on *both* builds at different points, including the previously
"confirmed working" local build - purely because whichever build ran last had rewritten the section
header to its own name, silently defaulting `enable_5161` back to enabled (`1`) for the other one on
its next run. **Fix**: maintain one config file per build/binary under test (a cheap `cp` + `sed` on
the section header line), never share one file across builds with different internal device names -
and if a "working" build suddenly shows a previously-fixed symptom, check whether something *else*
touched the same config file since its last known-good run before assuming a real regression.

## Technique 44: static ROM-file disassembly can desync from live execution in ways that look like a completely different, coherent-but-wrong routine - not just garbage bytes - re-verify with a live dump before trusting a "this address is X" conclusion built on the static file

Technique 16 already warns that static disassembly can desync across embedded data and produce
plausible-but-wrong decodes. It's tempting to assume this only produces obviously-broken output
(garbage opcodes, decode errors) that's easy to catch. It can instead produce a *different, entirely
self-consistent, plausible-looking* routine at the same address - passing every sanity check a static
read alone could apply, and only contradicted by direct comparison against live CS:PC behavior (e.g.
a live trace showing the address participating in a real multi-address loop, when the static decode
at that address shows straight-line/padding code that couldn't produce a loop at all).

**2026-08-22 case**: two addresses found via live `[modecheck]` heartbeat tracing (`F000:E69F`,
`F000:E842`, both part of a live-observed keyboard-wait/RAM-test investigation) decoded, from the
*static* ROM file at the address math's own file offset, as pure `int3` (`0xCC`) padding - unused
filler, not real code. But the live CS:PC trace showed genuine branching behavior at exactly those
addresses (`E842`→`E843`→`E845`→`E849`→`E84D`→ back to `E842`, not a monotonic int3-by-int3 crawl),
which is structurally impossible if the live bytes really were padding. A live one-shot dump
(`mem_readb_phys()` loop around the target address, written to a file, same pattern as the existing
`vram_dump.txt`/Technique 16 tooling) at the *exact* moment `CS:PC` reached each address, disassembled
with capstone, revealed the real live content - a completely different, coherent, correct BIOS
routine (the standard `INT 16h`-style keyboard-buffer poll at `E842`; a `AAAA`/`5555`-pattern RAM
verify loop at `E69F`) that the static file's bytes at the same nominal offset never matched at all.
**Root cause not yet determined** (why the static U18 file and the live-mapped content disagree this
severely at this address - candidates: wrong file-offset math for this specific BIOS revision's
chip-select boundary, a shadow-copy/relocation this project's own `bios_load_aux_linear` two-chip
handling does that the naive offset math doesn't account for, or the "two 32KB chip" split boundary
being different than assumed). **Lesson**: any time a live CS:PC address's *static* disassembly is
used to justify a conclusion (a branch is "the error path," a loop's shape, anything beyond "there is
some byte here"), re-verify with an address-triggered live dump before trusting it, even when the
static decode looks perfectly plausible on its own - "plausible" is not the same as "correct" here,
and this file/BIOS combination has now demonstrated it can fail silently in both directions.

## Open investigation, checkpoint 2026-08-22: PR #7626's submission gap, "301" POST message, and the real ROM-offset mismatch

**Status**: not resolved this session. Real, validated progress made (see Techniques 41-44 above),
but the specific trigger for a `301` POST message (appearing on a fresh 86Box/86Box upstream clone
plus the three files from Technique 41, using this project's own real hardware configuration -
Mach8, Trantor T130B, `pristine.img`, `cpu_speed=83500000` - but confirmed **absent** on this
project's own full local build under an identically-matched config) was not found.

**What's confirmed so far**:
- The three-file gap (Technique 41) is real and independently worth submitting upstream regardless
  of `301` - it demonstrably fixes the original PIC-IMR/DMA-refresh POST-101 region (confirmed via
  live CS:PC tracing showing clean progression through the exact addresses that region's fixes
  target) and a genuine, previously-unknown third Mach8 PIT-delay-loop ROM-revision address
  (`0x7B16`, alongside the already-known `0x7B37`/`0x7B23`) - found and fixed live this session,
  confirmed to unblock what had been a genuine, non-progressing freeze (`[modecheck]` showed
  `CS:PC` completely unchanged for 28+ real seconds before the fix, cycling normally after).
- `301` is **not** the standard "keyboard hardware self-test failed" code taken at face value - the
  address it leads to (`F000:E842`) is a completely generic, shared "wait for any keystroke" BIOS
  routine (`INT 16h`-style buffer poll), not something specific to a keyboard fault. Something
  upstream of it decided to print an error and call this shared routine; that decision point is not
  yet found.
- Every XT-keyboard-specific source file (`kbc_xt.c`, `keyboard.c`, `keyboard_xt.c`) is confirmed,
  via full diff against the fresh clone, functionally identical (Technique 41's `blockedtimeout`
  aside) - so this is not a missing keyboard-emulation fix in the same shape as the other three.
- The `enable_5161`/config-section bug (Technique 43) is a real, independently-confirmed recurrence
  of an already-fixed historical bug, fully explains a *different* symptom (`1801`) that was
  initially conflated with `301`, and is fully resolved by using correctly-matched per-build config
  files - not the remaining `301` question.
- Static disassembly at the two live-traced addresses (`E69F`, `E842`) does not match live content
  at all (Technique 44) - any further static-file-based reasoning about this specific BIOS ROM
  should be treated as unreliable until the offset mismatch itself is understood.

**Next step, not yet taken**: the actual decision point that leads to printing `301` and jumping to
the shared wait-for-key routine has not been located. The live-dump-and-disassemble technique
(Technique 44) works and should be repeated backward from `E842`'s known live entry point (a
[seg650Bcaller]/Technique 29-style backward trace, live-dumping the *actual* predecessor rather than
trusting the static file) rather than continuing to guess forward from static-file addresses that
have already been shown unreliable for this ROM.

**Later same session - important correction and a new technique (45) below**: confirmed
`enable_5161=0` genuinely resolves correctly and the device is genuinely not added (direct
`fprintf` inside `machine_ibmxt_inboard386_init()` itself, not inference from config content) -
Technique 43's fix is real and holds. A first attempt at pinpointing `301`'s exact trigger via a
literal-text-in-VRAM scan (mirroring the historical `1801` technique) landed on `F000:E418` - the
5161 test's own entry point - but this was a false positive caused by the scan's own throttle
(checking video RAM only every 500 real instructions is far too coarse a resolution for a
backward-trace window of only ~200 raw ring-buffer entries; by the time the coarse scan noticed
"301" was already on screen, live execution had moved 0-500 instructions further on, so the ring
buffer's tail no longer reached back to the real trigger at all). Tightened to every-instruction,
then throttled to every-20-instructions as a compromise - and hit Technique 45 below before getting
a clean capture.

## Technique 45: an added detection/trace hook can itself be heavy enough to change which bug reproduces - don't trust a trace captured under instrumentation load until reproduced with a light one too

A live trace hook that's cheap in isolation (Technique 1/29's guidance) can still become expensive
enough, once several are stacked together in the same per-instruction hot path (a raw undeduped
ring-buffer append, a multi-hundred-cell VRAM literal-text scan, a modecheck heartbeat, prior
fix code all sharing the same loop), to measurably slow real-time-to-emulated-time throughput on an
interpreted CPU core. Since several of this project's own fixes (Technique 41's PIC-IMR/DMA-refresh
timing fix chain in particular) are themselves calibrated against real-time-vs-instruction-count
ratios, sufficiently heavy added instrumentation can push that ratio back into the failure regime
the fix was built to correct - resurfacing the *original* bug the instrumentation was added to
investigate a *different, downstream* symptom of, not because the fix is wrong, but because the act
of observing it closely enough disturbed the exact timing condition it depends on.

**2026-08-22 case**: after tightening a VRAM-literal-scan hook (added to backward-trace `301`) from
a 500-instruction throttle to every-instruction (to fix Technique 44's coarse-resolution false
positive), the *same* build, same config, produced a plain `101` on the next run instead of `301`
or a clean boot - the original PIC-IMR/DMA-refresh self-test failure this session's Technique 41 fix
was built to resolve, reappearing intermittently under heavier-than-normal per-instruction overhead.
**Lesson**: when stacking multiple live-trace hooks for a deep investigation, periodically verify
the target machine still exhibits its *expected* baseline behavior (with lighter/throttled
instrumentation, or none) before trusting a trace captured under the heaviest instrumentation load -
if the symptom itself changes between instrumentation levels, the heavier instrumentation is now
part of the experiment, not a neutral observer of it, and any conclusion drawn purely from that run
needs re-verification under lighter load before it's trusted.

## Technique 46: hook the single shared choke point for a whole category of runtime behavior (not the guest CS:PC) to compare two builds' *emulator-side* behavior directly, when guest-code tracing has stopped narrowing things down

When exhaustive source-file diffing between a working and a non-working build comes back clean
(every relevant `.c`/`.h` file, every config value, every ROM checksum identical) and guest-side
CS:PC tracing (Technique 1/29/44) keeps landing on plausible-but-inconclusive addresses, step back
one level: hook the single shared *emulator-side* function that some whole category of runtime
behavior funnels through (e.g. `device_add_common()` in `device.c`, the one function every
`device_add*()` variant calls internally) and log its arguments across a full startup, for both
builds, to the same file format - then a plain `diff` of the two logs is a direct, unambiguous
answer to "does this build take a different code path here at all," with none of the interpretation
burden that guest-instruction tracing carries (Technique 44's static-vs-live gotcha doesn't apply -
this is host C code, not guest ROM bytes).

**2026-08-22 case**: after config, ROMs, and every plausibly-relevant source file were confirmed
identical between a working local build and a non-working upstream-clone-plus-fixes build, and two
rounds of guest CS:PC backward-tracing (Technique 44) failed to pin down `301`'s trigger, a single
`fprintf` added to `device_add_common()`'s entry (logs `dev->name` + `inst`) on both builds,
diffed directly, showed **zero differences** - identical 19-device list, identical order, identical
instance numbers. This is a strong, cheap, unambiguous negative: no device is being silently
skipped, added out of order, or added with different parameters between the two builds. Combined
with the exhaustive file-diff work, this shifts the most likely remaining explanation away from "a
missing code path" entirely and toward Technique 45's timing-margin hypothesis (identical code,
different real-time performance characteristics due to accumulated debug-hook overhead in one
build and not the other) - not confirmed by direct measurement yet, but now the best-supported
remaining explanation after ruling out every discrete code-path difference this technique and
Technique 41-44 could check.

**General lesson**: this "hook the shared host-side choke point, diff the log" pattern generalizes
well beyond device init - the same approach would work for I/O port registration order
(`io_sethandler`), timer registration (`timer_add`), or any other startup-time API with a single
funnel function, whenever the open question is "do these two builds do the same *host-side* setup
work," as distinct from "do they execute the same *guest* code" (which needs Technique 1/29/44's
CS:PC-based tools instead).

## Checkpoint, end of 2026-08-22 session: `301` is deterministic and has a strong, specific, dated lead - `vid_ati_mach8.c`'s independent upstream evolution

**Confirmed 100% reproducible**: three consecutive clean runs (light instrumentation, `86box_clone.cfg`,
real hardware config) all produced `301` at effectively the same point in boot. This is a
deterministic bug at this instrumentation level, not a race condition - a much more tractable
target for a focused session than an intermittent one would have been.

**Exhaustively ruled out** (all confirmed identical or fixed, see Techniques 41-46 above): every
config value, every ROM checksum (system BIOS, Mach8 ROM - both `roms/video/mach8/BIOS.BIN` and
`roms/video/ATI_MACH8.bin` match the real `113-11504-002` dump), the single shared disk image file
(not a copy), `machine.c`, `machine_xt_common_init`, the full device-init call order/names/instances
(Technique 46 - zero diff across 19 devices), and every XT-keyboard-specific file (`kbc_xt.c`,
`keyboard.c`, `keyboard_xt.c`). `inboard386.c`, `dma.c`, `pic.c`, `cpu.c`, `cpu.h`,
`x86_ops_io.h`/`x86_ops_jump.h` all functionally identical. `hdc_xtide.c` byte-identical.

**Best lead, not yet pursued**: `src/video/vid_ati_mach8.c` was never touched by this project (no
project fingerprints, not in PR #7626's file list) but has had substantial, *targeted* upstream
development since this project's fork point - not generic churn. Most notable:
`e19b15a7` (2026-08-17, five days before this session): "Actually enable the 8514/A/XGA side when
prompted to when going to port 0x3c3 of the VGA" - a change to exactly when/how the card's
accelerator side activates, in the same subsystem this session's `0x7B16` PIT-delay-loop fix (self-
test entry, self-test exit into 8514/A mode) directly interacts with. Also relevant: `5f77486b`
(2026-04-24, "Large overhaul in the mode switches of the Mach8/32") and `fc7a7bcd` (2026-05-24,
"9001st fix for mode switches"). **Next step for a future session**: pull `e19b15a7`'s actual diff
(`gh api -H "Accept: application/vnd.github.v3.diff" repos/86Box/86Box/commits/e19b15a7`) and check
whether it changes behavior around the exact `0x7B16`-family self-test window, using the same
backward-CS:PC-trace (Technique 44) and device-choke-point (Technique 46) tools already proven this
session - this is a concrete, bounded starting point, not a fresh open-ended search.

**Timing-margin hypothesis (Technique 45) status**: still plausible but now secondary to the
`vid_ati_mach8.c` lead above, since that lead is a specific, dated, targeted code change in the
directly-relevant subsystem, while the timing-margin theory remains unconfirmed by direct
measurement. Worth keeping in mind but not the first thing to chase next.

**BREAKTHROUGH, same session, after the checkpoint above**: `301` is confirmed to have nothing to
do with Mach8/8514A at all - reproduced identically with plain CGA (`gfxcard = cga`, everything
else in the config unchanged), ruling out the `vid_ati_mach8.c` lead above entirely. This actually
simplified the trace enormously (no Mach8 option-ROM code interleaved).

**Root string/call-chain found, via the technique this session kept re-deriving (hook the actual
print subroutine, not the symptom-in-VRAM)**: the BIOS's generic "print CS:SI until 0x0A" routine
lives at `F9CA-F9D7` (loops: read `cs:[si]`, `inc si`, `push ax`, `call 0xF99C` (the true character-
output primitive), `pop ax`, `cmp al,0xA`, loop until linefeed). Hooking this routine's *entry*
(not the delayed VRAM-scan symptom, which kept landing on unrelated later code - Technique 44's
lesson) with a small always-append raw ring buffer gave a clean, exact backward trace to the true
first entry: `E3B7 -> E3D7 -> E3DA -> E3DB -> F9A9 -> F9CA`, with `SI=0xEC4C` pointing at a literal
`" 301\r"` string embedded in the ROM at that exact offset (confirmed live, not from the static
file - matches character-by-character with the actual `f99c_hits` print log: ' ','3','0','1',CR).

**This is the exact same E3A6-E3DE self-test region already investigated in this project's own
history** (the port-0x61 pulse / port-0x60 readback "keyboard click-type test", previously
concluded to be a false lead for `1801` because that investigation's specific run showed the JE at
`E3D2` being taken, i.e. passing). This session's run reaches the print via `E3D7`/`E3DB` instead -
consistent with the *other*, previously-undiagnosed branch of the same test actually firing this
time. **Not yet fully closed**: the exact register/state difference that makes this branch trigger
now (vs. passing historically) hasn't been isolated - next step is a register dump right at `E3D2`
(the actual `JE` decision point) on this exact config, comparable against the historical live-traced
"AL=0, JE taken" result, to see what differs. Given the extensive Technique 41-46 elimination work
already done (config/ROMs/every plausible file confirmed identical), the remaining candidate is
almost certainly *upstream state* at the exact moment this check runs - e.g. residual keyboard
controller/PPI state left over from something earlier in POST behaving subtly differently - not a
missing fix in a file this session hasn't already checked.

**Immediate next step for a future session**: hook `CS:PC==0xF000:0xE3D2` (the `JE` itself) and log
`AL`/flags there directly, on this exact `86box_clone_cga.cfg` config, then compare against what a
clean run of the local working build shows at the same address (same technique, same config,
already-proven infrastructure - `rawring`-style small ring buffer + one address-triggered dump, no
new methodology needed, just point it at `0xE3D2` instead of `0xF9CA`).

## RESOLVED, 2026-08-22 (later the same day): `301` was a genuine bug in this project's OWN ported
## fix, not a missing upstream fix or CPU-emulation mystery - full root cause below

Everything above this point in the `301` investigation (the `E3D2`/`E080`/`CX`-residual-value
chase, the timing-ratio hypotheses, the exhaustive file-by-file diffing) was real, valid
elimination work, but it was chasing the wrong layer. The actual bug was much simpler and was
found by going back to first principles: **is the ported self-test fix's own exit-condition
address list actually complete for this exact ROM path?**

**Root cause**: the `E362-E3AC` IRQ1-suppression fix (`in_irq_selftest`) only treated landing on
`F000:E3AE` or `F000:E38E` as "self-test passed, stop suppressing IRQ1". A raw, non-deduped,
per-instruction trace (Technique 49 below) showed this exact run's real passing path lands on
`F000:E3AD` instead - one byte adjacent, almost certainly a data-dependent micro-branch a few
instructions earlier resolving to a slightly different but equally-valid "passed" address. Since
`0xE3AD` was never in the check, `in_irq_selftest` (and its `picintc(2)` IRQ1 suppression) **never
cleared for the rest of execution** - silently breaking keyboard/IRQ1 delivery from that point on,
eventually surfacing as the `301` keyboard-error POST code much later in boot. A second, identical-
class bug was immediately hiding behind the first: once `E3AD` was accepted, the follow-on
`in_negative_test` phase was being wrongly entered too (original logic: enter it whenever exiting
via anything other than `E38E`) - live trace showed `E3AD`'s own continuation never touches the
negative-test's own exit address (`E3C6`) at all before calling into an unrelated subroutine, so
`E3AD` needed to be treated like `E38E` (no negative phase), not like `E3AE`.

**The fix** (in `386_dynarec.c`'s `E362-E3AC` self-test block):
```c
if ((CS == 0xF000) && ((cpu_state.pc == 0xE3AE) || (cpu_state.pc == 0xE38E)
                        || (cpu_state.pc == 0xE3AD))) {
    in_irq_selftest  = 0;
    in_negative_test = (cpu_state.pc == 0xE3AE);   /* NOT "!= 0xE38E" */
}
```

**Confirmed working live**: clone build (fresh upstream + all ported fixes + this correction) no
longer shows `301`. Execution proceeds from what had been a permanent stall (previously capped at
~21M instructions, stuck forever) out to 68M+ instructions, reaching realistic late-boot device
ports (COM2, keyboard controller command port 0x64, DMA page register) never touched before, and a
POST-completion beep was heard. A separate, pre-existing, already-documented CGA text-rendering
glitch (garbled/interleaved on-screen text, data confirmed fine underneath) remains and is
unrelated to this fix.

**Why this class of bug is worth remembering**: when re-deriving/porting an address-gated self-test
fix from a working reference build, the reference build's own behavior can silently blind you to
alternate-but-equally-valid landing addresses your reference never happened to take. The original
author's local build always lands on `E3AE` (confirmed via its own log), so the fix was written to
check only that - never anticipating an adjacent address on a differently-built binary. **Don't
assume an address-gated fix's exit-condition list is complete just because it was "confirmed
working" on one specific build/config** - verify it against a live raw trace on every build/config
combination you actually intend to ship for, especially after a big upstream rebase.

## Technique 47: "the Holmes method" - resolve which exact device source files are touched during
## boot by hooking the I/O dispatch layer, not by guessing from a whole-tree diff

When a whole-tree diff between two builds turns up dozens of differing files and it's unclear which
ones are even reachable during the boot phase in question, don't keep guessing address-by-address -
directly instrument the shared I/O dispatch choke point (`io.c`'s `inb()`/`outb()`, all byte/word/
dword call sites) to log every distinct device-handler **function pointer** the first time it's
invoked. Since the running process's load address is ASLR-relocated and won't match what `nm`/
`addr2line` know from the static `.exe`, also log the *runtime* address of the logging function
itself once - `nm <exe> | grep <that function>` gives its file-static address, and
`runtime - static = slide`; subtract that slide from every other logged address before calling
`addr2line -f -C -e <exe> <adjusted-address>`, which resolves straight to `function_name
file.c:line`. This needs only the debug symbols already present in a normal `-g` build - no
coverage/profiling rebuild required - and gives a precise, unguessed map from "this port got
touched" to "this exact source file is involved."

**Caveat proven this session**: comparing the resulting touch-lists by *wall-clock time* is
misleading if the two builds carry different amounts of per-instruction debug overhead (one much
heavier than the other runs slower in real seconds for the same amount of guest progress) - a
"only touched in build X" conclusion from a fixed real-time window can be pure timing skew, not a
real behavioral difference (confirmed by re-testing with a much longer window and watching the
"only in X" file eventually appear in the other build too). Always tag entries with an instruction-
count index (Technique 48) instead of wall-clock time before trusting a "file only touched in one
build" conclusion.

## Technique 48: a global, unconditional per-instruction counter makes cross-build comparisons
## immune to relative execution-speed differences

Add one `uint64_t` global (e.g. `holmes_instr_count`), incremented once per guest instruction at a
single fixed point in the interpreter's hot loop, declared in the same file as the interpreter and
`extern`'d wherever else needs it (e.g. `io.c` for Technique 47's touch log). Tag every diagnostic
log line with its current value instead of (or alongside) wall-clock time. Two builds with wildly
different real-time execution speed (due to different accumulated debug-hook overhead, different
optimization levels, whatever) can then be compared at genuinely equal amounts of guest progress -
this is what actually made Technique 47's file-touch comparison trustworthy, and what let this
session directly measure where two builds' instruction counts diverge (and by how much) at any
given milestone, independent of how many real seconds each build took to get there.

## Technique 49: a RAW (non-deduped), index-aligned per-instruction trace starting from a *known-
## identical* entry point finds the exact first divergent instruction - dedup'd traces can't

The existing dedup-on-change ring-buffer/forward-trace techniques (Technique 1/29 and this
session's `[forwardtrace]`) are excellent for surveying a large instruction range cheaply, but they
throw away exactly the information needed to find a single divergent instruction: consecutive
repeats of the same address collapse to one entry, so two builds' dedup'd traces can drift out of
alignment the moment ANY loop runs a different number of iterations, making a line-by-line diff
meaningless past that point. When two builds are confirmed to have byte-identical register/flags
state at some fixed entry address (verified separately first), instead log every single instruction
UN-deduped, indexed by a simple incrementing counter (not `holmes_instr_count`, a fresh per-capture
one), for a fixed bounded window (a few thousand is usually enough) starting at that entry point, in
both builds, and diff the two logs directly by index. The first index where `CS:PC` (or any register
you're tracking) differs is the exact, unambiguous first divergent instruction - no guessing, no
address-offset math. This is what found both the `E3AD`-vs-`E3AE` landing-address mismatch (Technique
above) and the `F9CA`-vs-`F40F` wrong-print-routine-address false assumption earlier the same
session. A repeated single address in this raw trace (e.g. `E37C` or `FA31` appearing hundreds of
times in a row) is the unmistakable signature of a `REP`- or `LOOP`-based instruction whose `PC`
doesn't advance until its internal counter reaches zero - don't mistake it for a hang without
checking whether it eventually terminates within your capture window.

## Technique 50: forcing register/CX values at a fix's own entry point to "known-working" values is
## a fast, cheap way to falsify (not just support) a "wrong input state" hypothesis

Before spending more time hunting for *why* a register differs between builds, just force it (and
anything else suspected) to the known-good value at the exact address in question, address-gated,
same pattern as this project's other self-test fixes, and rerun. If the downstream outcome doesn't
change AT ALL (same instruction count reached at the next milestone, same failure mode), that's
strong, cheap, direct evidence the forced register(s) were never actually causally relevant to the
downstream divergence - redirecting effort away from a plausible-looking but wrong lead much faster
than continued static reasoning would. This session forced `AX` at `F000:E080` and separately forced
`AX`/`CX`/`SI` at `F000:E362` to match the known-working build exactly; neither changed the outcome
at all, which correctly redirected the investigation away from "wrong register content" and toward
"wrong exit-condition address" (Technique 49's finding) - the actual root cause.


## ⚠️ CRITICAL, 2026-08-22: the IBM XT 1982 BIOS ROM is INCOMPATIBLE with the Inboard 386/PC —
## and the wrong ROM can be selected SILENTLY. Check this FIRST on any Inboard boot failure.

**This one root cause burned an entire multi-hour session** in which a fresh upstream clone build
"mysteriously" ran POST faster than this project's own working build, then hung booting Windows 95,
while every timing/config/device audit came back byte-identical. It was not a timing bug at all.

### The incompatibility (must be documented for 86Box users)
`INBRDPC.SYS` v1.1 (02/17/89) — the Inboard's own required DOS driver — hardcodes a 3-byte reference
signature at a fixed BIOS offset (`F000:E05B`) as part of its ROM-shadow self-verification. **The
1982-dated 5160 ROMs do not contain that signature at that offset.** This is a genuine ROM-revision
mismatch, not an emulation bug (originally documented in `INBOARD_86BOX_PORT_PLAN.md`, 2026-07-26).
Real Inboard installations from the 1989 driver era used a later ROM revision.

**Therefore: the `ibmxt_inboard386` machine MUST be run with a 1986 ROM revision**
(`ibm5160_050986` = "1501512 (05/09/86)", or `ibm5160_011086` = "5000026 (01/10/86)").
With a 1982 ROM (`ibm5160_1501512_5000027` etc.) the machine will POST at visibly the wrong speed,
produce spurious POST errors (`301` among them), and **cannot boot Windows 95** — it hangs at the
splash screen with the CPU still executing. Every timing fix in this project is calibrated against
the 1986 ROM's POST code; the 1982 ROM's POST is *different code doing a different memory test*,
which is why its RAM count visibly races compared to a correct run.

### The silent-failure mechanism — this is what made it so hard to see
The 1986 ROM entries are **not part of stock upstream 86Box's `ibmxt_config` BIOS list**; this
project *added* them (see the dated comment block in `m_xt.c`). The `ibmxt_inboard386_device`'s
`.config` field points at `ibmxt_config`, which it *shares with the plain `ibmxt` machine*.
So on any tree where those two entries are missing:
- `bios = ibm5160_050986` in the `.cfg` is **not a valid option**, so it is **silently ignored**
- selection falls back to `.default_string = "ibm5160_1501512_5000027"` → **the 1982 ROM**
- there is **no warning, no log line, and no visible error** — the machine boots and looks plausible

This is this file's own **Technique 4** ("config value silently ignored") in its most expensive form
to date, and the fix is to ensure the 1986 entries exist in whatever BIOS list the Inboard machine's
`.config` actually points to.

### How to check this in under a minute, before investigating anything else
Two independent checks, either of which catches it immediately:
1. **Window title.** 86Box puts the machine's own `.name` string in the title bar. A build whose
   device definition differs will say something different — e.g. `IBM XT (Inboard 386/PC)` vs
   `IBM XT (1982) w/ Intel Inboard 386/PC`, and `[386DX]` vs `[386SX]`. **A differing title bar
   between two builds you believe are equivalent is a red flag that the machine/device definition
   itself differs, and therefore its BIOS list may differ too.** This was visible in every single
   screenshot for an entire session and was never questioned.
2. **Live ROM-content check (decisive, no guessing).** Read the actual bytes the CPU is executing
   and compare against the raw ROM file. The 1986 and 1982 ROMs diverge at a convenient, early,
   already-known spot — physical `0xFE07E` (i.e. `CS:PC = F000:E07E`; note the address math is
   `CS*16+PC`, NOT `0xF0000+PC`):
   - **1986 ROM (correct):** `D2 EC 72 29 D0 E4 70 25` (`SHR AH,CL` …)
   - **1982 ROM (wrong):**  `B1 05 D2 EC 72 29 D0 E4` (an extra `MOV CL,05h` first)
   Verify ground truth straight from the file with
   `xxd -s 0x607E -l 8 roms/machines/ibmxt86/BIOS_5160_09MAY86_U18_*_F800.BIN`
   (file offset `0x607E` = `0xFE07E - 0xF8000`, since U18 loads at `0xF8000`).

**Do NOT conclude "same ROMs" from matching file hashes.** Both builds had byte-identical ROM *files*
on disk (verified MD5) — that says nothing about **which one the machine actually selected and
loaded**. Verify the loaded content, not the available files.

### Knock-on consequence for the PR — re-examine the "E3AD" self-test fix
The `301` investigation that produced the `E3AD` addition to the `E362-E3AC` IRQ1-suppression fix
was carried out **on a clone build that was unknowingly running the 1982 ROM**. The observation that
"this run's passing path lands on `F000:E3AD` instead of `E3AE`" was almost certainly an artifact of
executing *a different ROM revision's code*, not a real alternate landing point in the 1986 ROM.
With the correct 1986 ROM the build reaches the Windows 95 desktop. **Before submitting: re-verify
whether the `E3AD` branch is reached at all on the 1986 ROM.** It is additive and harmless if it
never fires, but shipping it without checking would add misleading dead code to the PR — and the
`301` symptom it was written to fix was itself a consequence of the wrong ROM.

### The fix that shipped: give the Inboard machine its OWN BIOS list
Rather than adding the 1986 entries to the shared `ibmxt_config` (which would also change the plain
`ibmxt` machine's options - out of scope for an Inboard PR), `ibmxt_inboard386_device` now points at
a dedicated `ibmxt_inboard386_config[]` containing **only** the two compatible 1986 revisions
(`ibm5160_050986` as default, plus `ibm5160_011086`), along with the same `enable_5161`/`enable_basic`
options. **This makes the incompatible 1982 ROMs unselectable by construction** - the silent-fallback
failure mode becomes impossible rather than merely documented. Verified: clone build boots to a full
Windows 95 desktop with this change, reproducibly.

**For the 86Box submission**: call this incompatibility out explicitly in the PR description, so
maintainers understand why this machine does not share `ibmxt_config` - it is a hardware-fidelity
constraint (INBRDPC.SYS's own ROM-signature check), not an arbitrary restriction.

## Companion finding, same session: "the clone runs too fast" was actually "local runs too slow"

A large amount of session time went into chasing an apparent wall-clock timing discrepancy - the
clone build's POST/RAM-count visibly racing compared to this project's own long-standing build. Part
of it was genuinely the wrong ROM (above; the 1982 ROM's memory test is different code). But the
*residual* difference after the ROM fix has a much more mundane cause, and it is the **opposite** of
how it presents:

Measured directly (`awk` the body of `exec386()` in each tree, count debug I/O sites):
- **local's `exec386()`: ~2310 lines, ~148 `fprintf`/`fopen`/ring-buffer-write sites**
- **clone's `exec386()`: ~470 lines, ~12 sites** (nearly all temporary session diagnostics)

`exec386()` is the emulator's innermost per-instruction loop, so ~148 instrumentation points there is
enormous overhead. Consequences to remember:
- **86Box's title-bar percentage is the ground truth for this.** It reports how well the host is
  keeping up with the *configured* emulated speed. A build showing 100% is running the machine at its
  configured rate; a build dipping to 1%/20%/94% is failing to keep up and therefore running the
  guest **slower than it should in wall-clock terms**. Local routinely dips; clone holds 100%.
- Therefore **clone is the timing-accurate build and local is the artificially slow one.** Do not
  treat the heavily-instrumented local build as a wall-clock speed reference - it never was one.
  Both builds were verified to have byte-identical `cpu_busspeed` (27833333.333), `cpu_waitstates`
  (31), `cpu_multi` (3), `cpu_dmulti` (3.000), `is386`, and `cpu_16bitbus` at runtime.
- If the *configured* speed itself is felt to be wrong versus real hardware (e.g. the RAM count still
  looking too quick on a clean 100% run), that is a **calibration question about `cpu_speed`/
  waitstate values that affects BOTH builds equally** - it is not a difference between them, and
  chasing it as a build-vs-build divergence will waste time. Measure it against real hardware with a
  stopwatch on a clean build only.

**Also cosmetic, don't mistake it for an emulation difference**: the two builds render at different
window sizes (local 640x400 video area with menu+tool bars and a `<vmpath> - 86Box 7.0` title; clone
426x266 with no chrome and a `86Box vX - N% - [CPU] <machine>` title). That is 86Box UI mode/scale/DPI
handling, not video emulation - the guest's actual video mode is identical.

## Technique 51: never gate a CPU-core fix on an exact exit ADDRESS — always add a range-based
## safety net, because the exact address varies with POST timing

This project has now been bitten by the same failure shape **four separate times** (`E3D2`, `F9CA`,
`E3AD`, and the generic-VGA keyboard error below). The pattern:

An address-gated fix in `exec386()` **arms** a state flag at a known entry address, then **disarms**
it on a small set of enumerated exit addresses. The exit addresses are reached via a *data-dependent
micro-branch*, so **which one is taken varies with POST timing** — and POST timing varies with
things that have nothing to do with the fix, e.g. **whether a large video option ROM ran first**.
Concretely: the `F000:E362-E3AC` IRQ1-suppression fix boots clean with `gfxcard=mach8_vga_isa` (big
option ROM, lots of POST time burned before the keyboard test) but produces a **keyboard POST error
requiring F1** with `gfxcard=vga` (no comparable option ROM, so the keyboard controller's own
~1ms-after-start self-test IRQ1 lands at a different point relative to the test window). The exit
address taken differs, doesn't match the enumerated set, **the gate never disarms, and `picintc(2)`
fires on every instruction for the rest of the session** — silently killing the keyboard.

**A stuck gate is far worse than an early disarm**: an early disarm merely risks re-exposing the
original (intermittent, POST-only) bug; a stuck gate breaks a whole subsystem for the entire run and
presents as a completely unrelated symptom much later. So always bound it:

```c
/* Safety net - disarm if execution leaves the self-test's own address range by ANY path we
   didn't enumerate. Can only ever shorten suppression, never extend it. */
if ((in_irq_selftest || in_negative_test) &&
    ((CS != 0xF000) || (cpu_state.pc < 0xE362) || (cpu_state.pc > 0xE3C6))) {
    in_irq_selftest  = 0;
    in_negative_test = 0;
}
```

**Generalised rule for any future address-gated fix in this project**: enumerate the known exits for
precision, but *always* pair them with "or execution left the region at all" as a backstop. Write
the backstop at the same time as the fix, not after a mystery bug surfaces.

**Corollary for testing**: a fix validated on only ONE video card is not validated. Video card
choice changes POST timing enough to change which micro-branch a BIOS self-test takes. Always
re-test address-gated POST fixes across at least one card WITH a big option ROM (Mach8) and one
WITHOUT (generic `vga`).

## Technique 55: read the guest OS's OWN detection/setup logs off the real machine before writing any instrumentation

Windows 95 writes a detailed record of what it thought your hardware was, and on this project that
file has been sitting on the CF card the whole time. `C:\DETLOG.TXT` (hardware detection),
`C:\SETUPLOG.TXT`, `C:\BOOTLOG.TXT` and their `.PRV`/`.OLD` predecessors are plain text, pulled in
seconds over COMrade's `file_read` with `dest_path` (bytes bypass the model context, CRC-verified).

**This is a primary source describing the exact machine under investigation** - not a manual, not a
theory, not an emulator approximation - and it costs nothing. Check it before building a trace hook
for anything that smells like "why does Windows think X".

**2026-08-23 case - root-caused two open issues in minutes, with no instrumentation at all:**
- Issue #6 ("PS/2 mouse in Device Manager"): `DETECTPS2MOUSE` returned a positive and registered
  `*PNP0F0E\0000 = Standard PS/2 Port Mouse` on **IRQ 12**. That device cannot exist here twice
  over - there is no 8042 (ports `0060-0063` are PPI, `0x64` does not decode), and IRQ 12 lives on
  an AT's slave PIC, which this machine does not have.
- Issue #2 (`#` instead of backslash): the same log shows
  `Detected: *PNP030B\0000 = PC/AT Enhanced Keyboard (101/102-Key)` on a machine with an **83-key XT
  (Model F)** keyboard.

**The unifying insight, which neither issue revealed on its own:** Windows 95's detection routines
false-positive AT-class hardware on this XT, in more than one subsystem. Two issues previously
tracked separately are two symptoms of one condition. Reading the log is what made that visible -
each issue in isolation looked like its own unrelated bug.

**Corollary:** `DETLOG.TXT`'s resource lists are also worth reading for what they *omit*. The Mach8
is detected correctly but claims only the VGA ranges (`3b0-3bb`, `3c0-3df`) - none of the sparse
8514/A accelerator ranges (`02E8`/`06E8`/`0AE8`/`0EE8`). That is a lead for #4, though **not proof**:
a 16-bit display driver can legitimately program registers that were never enumerated as PnP
resources, so absence here is not a defect by itself.

**Issue #2's own root cause, for the record** (derived from `keyboard_xt.c` plus the live
`AUTOEXEC.BAT`): on a UK 102-key layout the backslash lives on scancode `0x56`, the extra key that
exists only on 102-key European keyboards. 86Box's XT table (`scancode_xt`, used for the default
`KBD_83_KEY`) has `{ .mk = { 0 }, .brk = { 0 } }, /* 056 */` - it emits **nothing**. Scancode `0x2B`
(left of Enter) does exist and is `#` under `KEYB UK`, which is correct UK behaviour. So the key is
not mis-mapped; the key that would produce backslash does not physically exist. `ALT`+`9`+`2` on the
numpad is the practical workaround. Not a project bug.

## Technique 56: an emulator flag meaning "is this an AT?" derived from CPU type is WRONG for an accelerator card in an XT - grep every consumer of it

86Box derives `dma_at` from `is286`, which is `cpu_type >= CPU_286`. That is a reasonable proxy on
ordinary hardware and **wrong by construction** for this project's entire premise: a 386-class CPU
bolted onto a genuine XT board. The CPU is 386; the motherboard's DMA page latches are still the
XT's 4-bit ones.

Consequence found 2026-08-23 in `src/dma.c`: `dma[addr].page = dma_at ? val : val & 0xf` gave the
Inboard an **8-bit** page register - 24-bit DMA reach where the real 5160 has 20-bit. A DMA buffer
above 1 MB therefore *works under emulation and silently truncates on real hardware*. That is
precisely the shape of bug that lets a fix pass in the emulator and fail on the bench, which had
already happened once on issue #5's `vmad` byte-patch.

**The fix pattern already existed in this codebase** - `dma_force_xt`, set by `inboard386_init()`
and never touched by `dma_reset()` (unlike `dma_set_at()`, which loses a race against a reset that
runs after `device_reset_all()`). It fed only `dma_xt8237_active()`. Added `dma_page_is_xt()`
(`dma_force_xt || !dma_at`) and routed both the page width and the XT reset mask through it.

**General lesson: when you add a force-flag for one consumer of a wrong global, grep every other
consumer of that global in the same file before moving on.** `dma_at` had five consumers; only one
had been corrected, and an uncorrected one was the one that mattered for a real open bug.

Still unreviewed: `_dma_writeb`'s `mem_invalidate_range` and the `refreshread()` call are also
`dma_at`-gated. Believed benign, but nobody has actually checked.

## Technique 57: verify a fidelity fix actually CHANGES behaviour, then delete the hook that proved it

Technique 28 says a patch script must assert it changed something. The same applies to a conditional
fix in emulator source, and it is easy to skip because "the boot still works" feels like evidence.
It is not: if the new condition never fires, the boot works *identically*, so a silent no-op is
indistinguishable from a working fix.

Cheap procedure: add a capped `fprintf` at the changed line logging **both the inputs and the branch
taken**, run only as far as the first few hits (POST is usually enough - no need for a full boot),
confirm, then **remove the hook in the same session**.

2026-08-23, `dma_page_write`: `[xtpage] ch=1 val=00 force_xt=1 dma_at=1 is_xt_path=1` - proving in
one line that the device really does set the flag, that `dma_at` really is wrongly 1 (confirming the
diagnosis live rather than from source reading), and that the new XT branch is really taken.

**Then remove it, per Technique 21** - and take that rule seriously, because the same session
produced a concrete example of the cost. The `[a0a1trace]` hook (added 2026-08-02, uncapped until
20,000 hits, `fflush` to disk on **every** port-0xA0 access, arming at t+90s) was still live long
after its question was answerable, and the user independently noticed the emulator had become slower
to initialise. Its question - "do the Inboard's `port_a0` shadow and the NMI mask conflict?" - was
resolved the same day (they do not: 86Box chains I/O handlers so both receive every write, and
`apply_waitstates()` computes from `dev->speed` and never reads `port_a0`). Removed.

## Technique 58: establish provenance BEFORE repeating a claim - the PRIMARY/AI-SOURCED tags only work at intake

This project already tags sources `[PRIMARY]` / `[AI-SOURCED]`. 2026-08-23 showed the tagging is
worthless if applied retroactively. Finding `DMABufferIn1MB=True` / `DMABufferSize=64` in the user's
*working* Windows 3.11 `SYSTEM.INI` was reported (by Claude) as "empirical real-hardware
confirmation" that the Inboard requires the setting - a strong claim that shaped the plan for issue
#5. The user then pointed out those lines were **Google AI suggestions added as performance tweaks**,
not part of the original working build, which had merely never broken anything.

What the file actually establishes is much narrower: the setting is **tolerated** on this hardware.
Not that it is needed, and not that it fixes anything.

**The test to apply before repeating any config value found in a working build: does this file record
a decision someone made *because it was necessary*, or a change someone made *and kept because
nothing broke*?** Those look identical on disk. If you cannot answer, ask, or mark it unverified.

The same file *did* contain genuinely primary material, which is the useful contrast: Intel's own
shipped choices - `keyboard.drv=ibkbd.drv`, `keyboard=ibvkd.386`, and notably `device=*vdmad` (the
**stock** virtual DMA device, *not* Intel's own `IBVDMAD.386`, even though that file ships in the
same bundle). That last one is real evidence and it *lowers* the priority of disassembling
`IBVDMAD.386`, because the configuration that actually works on this hardware does not load it.

**Derive values from evidence instead of inheriting them.** For `DMABufferSize`, 386MAX's own XT
path (`MARK_XT`) selects `@DMA_DSK` = **64 KB** over its own 16 KB default (`@DMA_DEF`), bounded by
min 8 / max 128 - and `QMAX_EVM.ASM` gives the reason not to exceed it: a buffer <= 64 KB need only
sit within a 64 KB boundary, while > 64 KB must land on a 128 KB boundary. So 64 is the largest
value avoiding the harder alignment rule. Same number the AI suggested, now for a defensible reason.
`HardDiskDMABuffer` was **dropped** rather than copied across: this machine's disk path is XT-IDE
(PIO) and a polling T130B, so a hard-disk DMA buffer buys nothing.

## Technique 59: a POST that stops with no video at all is often a deliberate `HLT`, not a hang - find the byte before theorising

A machine that shows *nothing* and makes beeping noises looks like a catastrophic failure. It is
frequently the BIOS working perfectly: reaching a failed test and executing a deliberate `HLT` with
interrupts already disabled. That is a *stop*, not a *hang*, and it is far easier to diagnose than a
loop - because the address is exact and it never moves.

**Signature to recognise (all four together):**
- `[modecheck]`-style heartbeat shows **CS:PC completely unchanged** for minutes (Technique 3)
- **`IF=0`** in the same heartbeat - interrupts disabled, so `HLT` can never be woken
- `imr`/`irr`/`isr` all `00` with nothing pending - the PIC is idle because nothing can be serviced
- The PIT keeps ticking (`clocks=` advancing) - the machine is alive, the CPU just is not fetching

**Then read the single byte at that address.** `F4` is `HLT`. One byte is a far safer static read
than a disassembly window (Technique 16/44 warn this project's ROM desyncs), and it is decisive.

**2026-08-23 case - the Sound Blaster Pro POST halt.** Fitting `sbprov2` (0x220 / IRQ 5 / DMA 1 -
the real machine's exact settings) to `ibmxt_inboard386` stopped POST dead at `F000:E12B` with a
black screen and repeated beeps. The byte at `E12B` is `F4` = `HLT`, and the surrounding code is an
I/O port scan (`mov dx,0` / `out dx,al` / `in al,dx` / `inc dx` / `cmp` / `je` past the `HLT`) -
**ports 0-0F are the 8237**, so this is the BIOS's DMA controller register test writing values and
reading them back. The readback does not match with the SB Pro present, so the BIOS halts on purpose.
The halt is *before* video init, which is why nothing is ever drawn - the black screen is the
symptom of *where* it stopped, not of a video problem.

**Do not confuse "no video" with "video broken".** Check where POST stopped first.

### The A/B discipline that made this usable (do this every time)
Four runs, one variable each, ~100 s apiece:

| Run | Result |
|---|---|
| fork, no SB Pro | boots through to Windows |
| fork, SB Pro fitted | HLT at `F000:E12B` |
| fork, SB Pro + **own recent DMA change reverted** | **still** HLT - clears the change |
| **clean upstream build**, SB Pro fitted | **also halts** - upstream defect |
| real hardware, same card, same 0x220/IRQ5/DMA1 | **POSTs fine** |

The third run matters most and is the one most often skipped: **when you have recently touched the
subsystem a failure points at, rule yourself out before blaming anything else.** Here `dma.c` had
been edited hours earlier and the failing test was the DMA controller test - a coincidence that
would have been very easy to mistake for causation.

The fourth run is what turns "our fork is broken" into a **reportable upstream bug**. Keep a clean
upstream build to hand for exactly this (see the note below on when to prefer it).

**Status:** the SB Pro POST halt is an **open upstream 86Box defect** and it blocks reproducing
issue #5 (the `vmad` BSOD) in the emulator at all - the card cannot even be fitted. Consistent with
the long-standing note that the SB Pro hang lives in `dma_xt8237`.

## Prefer a CLEAN UPSTREAM build for anything except active tracing

Both Inboard PRs are merged (#7626 2026-08-06, #7749 2026-08-23), so **upstream master now contains
every fix this project depends on**. The instrumented fork's only remaining advantage is its debug
scaffolding - roughly 148 trace sites in `exec386()` alone, which this project's own measurements put
at **~3.45x slower** than a clean build.

- **Clean build** - daily use, and the **only** valid reference for wall-clock timing against real
  hardware. Built at `86Box-upstream-work/upstream/build/clean/`.
- **Instrumented fork** - active investigations only, where `vram_dump` / `inject_key` / new hooks
  are needed.
- **Any "is this us or is this 86Box?" question** - run the clean build. It isolates the fork's
  patches completely, which stepping back through fork commits does not.

**Not yet upstream:** `dma_page_is_xt()` (Technique 56). A clean build lacks the 4-bit DMA page fix.

### io.c traces are now gated - re-enable them when you need them
Every `[tag]` trace in `io.c` sits in `inb()`/`outb()` and `fflush`es per hit, with caps up to 20000.
Together they emitted ~45,000 lines (3.5 MB) in two minutes of boot and were a large, user-visible
part of the slowness. All nine sites are now gated on `io_dbg_on()`, **default OFF**:

    set INBOARD_IO_TRACE=1

Measured effect of gating, identical 120 s boot: **3.5 MB / ~45,000 lines -> 764 KB / 9,413 lines.**
The gate is why the `E12B` investigation above could re-enable full port tracing instantly with no
rebuild - which is the argument for gating spent hooks rather than deleting them.

## Forward notes for a future OSR2 attempt
- **Emulator/code fixes are OS-version-independent** — the whole "Complete Windows 95 boot fix
  inventory" section above applies unchanged to OSR2. Start from that list, don't re-derive it.
- **The BIOS constraint is absolute and OS-independent**: 1986 ROM only (`ibm5160_050986` /
  `ibm5160_011086`). The 1982 ROM incompatibility is an `INBRDPC.SYS` property, not a Windows one,
  so it will bite OSR2 identically. This is now enforced in code (dedicated BIOS list).
- **Disk-image fixes will need re-deriving for OSR2's own binaries**: the self-test-skip byte in
  `INBRDPC.SYS` should be identical (same driver), but `VKD.VXD` and `KEYBOARD.DRV` are different
  builds in OSR2, so their patch offsets will differ. Use Technique 35 (read the real DDK source
  rather than guessing at binary offsets) and Technique 28 (assert the patch actually changed
  something — `Patched: N` with N>0).
- **Don't repeat the OSR2 track's old mistake**: the archived 2026-07-31→08-03 OSR2 work chased a
  wild-jump chain (`650B` → `0128`/EMM386 → `0048:00A8` → `0048:1278`) that was never resolved. The
  `INT 68h`/`patchint68` fix found later on OSR1 addresses the `650B` link in that same chain — try
  that fix FIRST on OSR2 before re-opening that investigation.

## Technique 52: a CPU-core fix placed in only ONE of 86Box's two interpreters silently does
## nothing for half the CPU families — check `cpu_set()`'s dispatch before assuming it applies

86Box has **two** interpreter loops and `cpu_set()` (`cpu.c` ~line 1885) picks between them:

```c
if ((cpu_s->cpu_type == CPU_IBM486SLC) || (cpu_s->cpu_type == CPU_IBM486BL) ||
    cpu_iscyrix || (cpu_s->cpu_type > CPU_486DLC) || cpu_override_interpreter) {
    cpu_exec = exec386;        /* src/cpu/386_dynarec.c */
} else
    cpu_exec = exec386_2386;   /* src/cpu/386.c:225     */
```

Every Inboard POST fix originally lived only in `exec386()`. Consequence: selecting a plain
**386DX/386SX — the CPU this accelerator card was actually sold to pair with —** routed execution to
`exec386_2386()`, where **none of the fixes existed**, so POST hung in the Mach8 option ROM's PIT
delay loop *before even reaching the RAM count*. It presented as "beep, then black screen", which
looks like a video or speed problem and is neither: the fix code simply never executed.

**Diagnostic tell**: a failure that appears/disappears purely on CPU *family* (not speed) and shows
up *earlier* in POST than any of your fix addresses. Check the dispatch before anything else.

**Do NOT try to fix it with `cpu_override_interpreter = 1` in the config** — tested, does not work
(the flag is consulted at `cpu_set()` time, before/independently of that config path).

**The correct fix (implemented, validated to a Win95 desktop on `i386dx`/25):** extract the whole
fix block into ONE shared function and call it from BOTH loops.
- `void inboard_post_fixups(void)` — defined in `src/cpu/386_dynarec.c` immediately before
  `exec386()`, prototype in `src/cpu/cpu.h` next to the `exec386_2386` declaration.
- Called from `exec386()` and from `exec386_2386()` (`src/cpu/386.c`), in both cases immediately
  after `cpu_state.ssegs = 0;` — the identical anchor point in each loop.
- Contents: C000 wait-state exemption, Mach8 PIT delay-loop fix (`0x7B37/0x7B23/0x7B16`),
  `E362-E3AC` IRQ1 self-test + range safety net, `E507` DMA-refresh force, `patchint68`.

## Technique 53: don't set `cpu_waitstates` if you already override the variables it feeds
`cpu_update_waitstates()` only honours `cpu_waitstates` for
`cpu_type >= CPU_286 && cpu_type <= CPU_386DX`. `inboard386_apply_waitstates()` used to set it — dead
on 486BL/486DLC, but **live on 386DX/386SX**, where it stacked on top of
`inboard386_apply_mem_timing()`'s own bus-speed-scaled override of the very same variables
(`cpu_cycles_read/write/prefetch`), compounding with `io_waitstates`/`reg_op_waitstates`/
`cpu_rom_prefetch_cycles`. Net effect: the 386-class parts ran an order of magnitude too slow.
Fixed by zeroing `cpu_waitstates` there so exactly one memory-timing mechanism is ever in play.
General rule: if you override the consumed variables directly, do not also drive the upstream knob
that writes them — especially when that knob is CPU-type-gated.

## Technique 54: a per-instruction hook is machine-wide by default — gate it on the device being
## present, or "this is address-gated so it's inert elsewhere" quietly stops being true

Found while preparing the upstream PR (2026-08-22), *after* the fix set below was already validated.

Extracting the Inboard POST fix-ups into `inboard_post_fixups()` and calling it from both
interpreter loops (Technique 52) made it run for **every 386/486 machine in 86Box**, not just the
Inboard. The function's own comment claimed it was inert elsewhere because "every branch is gated on
a specific CS:PC". That was **half true, and the wrong half was the dangerous one**:

| Gate style | Example | Inert on other machines? |
|---|---|---|
| Address-gated (`CS==0xF000 && pc==0xE507`) | DMA refresh flag | Effectively yes — a specific BIOS's own byte offset |
| **Segment-scoped** (`CS==0xC000`) | Mach8 option-ROM waitstate zeroing | **No** — 0xC000 is *the* option-ROM segment on every PC |
| **Segment-scoped** (`CS==0x0EAF`) | VMM32 `INT 68h` vector patch | **No** — an ordinary segment value any guest can load, and it *writes guest memory* at 0x1A0/0x3C0 |

So an unrelated machine executing its own option ROM would silently get `io_waitstates`,
`isa_cycles` and `cpu_cycles_*` zeroed, and any guest that happened to load `CS=0x0EAF` would get
four IVT bytes rewritten underneath it.

**The rule:** when you move a machine-specific fix into shared CPU-core code, add an explicit
presence flag — here `int inboard386_present`, set in `inboard386_init()`, cleared in
`inboard386_close()` — and gate the *call site* on it, not just the body (keeps it to one
well-predicted branch in the hottest loop in the emulator).

**How to audit your own hook in one pass:** list each condition and ask *"could a machine that has
never heard of this project satisfy this?"* Anything scoped to a segment, a port number, or a
register value alone is a yes. Only a specific address *within a specific ROM you also pin* is a no —
and even then, the presence gate costs one branch and removes the argument entirely.

**Reviewer-facing corollary:** this is exactly the question a maintainer asks first about a diff that
touches `exec386()`. Answering it in the PR body before it is asked is cheaper than answering it
after.

## Technique 60: a binary patch is only "verified" against the file's REAL instruction stream —
## and on Win9x a VxD replaced after the VMM32 combine never loads at all

Two separate traps, both hit on the same bug (issue #5's Sound Blaster Pro BSOD), both of which made
a wrong answer look confirmed for weeks.

**(a) Never verify an instruction by re-disassembling from a guessed start.** `patch_vdmad.py`
scanned for raw `E4 xx` / `E6 xx` opcodes, then "confirmed" each hit with a helper that tried start
offsets until one decoded an `IN`/`OUT` at the target. Such a start *always* exists — x86 is a
variable-length encoding, so any byte can be made to look like the head of an instruction. Fed

```
OBJ1:0x1660   80 E4 C0        AND AH, 0C0h
```

it matched the `E4 C0` in the middle, overwrote two bytes, and produced

```
OBJ1:0x1660   80 B0 00 A8 20 74 06     XOR byte ptr [EAX+7420A800h], 6
```

a wild write to an unmapped address. The BSOD said `VDMAD(01) + 00001660` — the fault offset was a
byte-exact match to the corruption, and never moved across attempts, which should itself have been
the clue. **A fault offset that is stable across unrelated "fixes" is pointing at a constant, not a
race.**

Do it this way instead (`vxd-patches/vxdstream.py`): decode each executable object's stream **once**,
linearly from the object's own start, and require the candidate to sit on a boundary in it.

**The Win9x wrinkle that breaks naive disassembly:** a VxD service call is `CD 20` followed by an
**inline 4-byte service ID** — 6 bytes total, not a 2-byte `INT 20h`. Any disassembler that doesn't
know this desyncs after *every* service call and stays desynced until it happens to resynchronise.
A first pass at this audit wrongly flagged `VPICD` file `0x616a` as corrupt for exactly that reason;
the bytes were `CD 20 F3 00 01 00` four bytes earlier. Handle it explicitly, or every verdict in a
VxD is noise.

**(b) A patched VxD dropped into `WINDOWS\SYSTEM\VMM32\` after Setup's combine step does nothing.**
Windows 95 Setup combines the staged VxDs into a single `VMM32.VXD`. That combined file has a **`W4`**
signature at `e_lfanew` — it is *compressed*, so it cannot be byte-patched in place and searching it
for opcodes finds nothing whether or not the code is in there.

`BOOTLOG.TXT` tells you which copy actually loaded, and it is the only thing that does:

| Log line | Meaning |
|---|---|
| `Loading Vxd = VDMAD` | came from the bundled, combined `VMM32.VXD` |
| `Loading Device = C:\WINDOWS\SYSTEM\VMM32\VDMAD.VXD` | came from the file |

For eighteen days this project believed a fix had been "tested on real hardware and failed". The
fixed file was sitting in `\patched_files\` on the card, had never been placed anywhere Windows
looks, and `BOOTLOG` had been saying `Loading Vxd = VDMAD` the whole time. **Before drawing any
conclusion from a VxD change, prove the new file loaded.** The reliable route is to apply patches to
a *pre-monolith* image — one where `WINDOWS\SYSTEM\VMM32\` still holds the individual VxDs and
`VMM32.VXD` is still the stock 411,132-byte copy — and let Setup's own combine bake them in.
`vxd-patches/deploy_premonolith.sh` does this and refuses to run against a post-combine image.

**The generalisation:** both halves are the same failure — *a step that silently succeeds at doing
nothing*. The project already had this rule for patch scripts that write a byte-identical output
(memory: "always confirm `Patched: N` with N>0"). Extend it: confirm the patch is **correct**, and
confirm the patched artefact is **the one actually loaded**. A negative result from an unverified
deployment is not a negative result — it is no result, and it will send you off chasing theories.
Consequence here: the `DMABufferIn1MB=Yes` test was **void, not negative**, so the contributor lead
it was meant to settle stayed untested while everyone believed it had been ruled out.

## ✅ FINAL VALIDATED FIX SET (2026-08-22) — 5 files, all needed
1. `machine/m_xt.c` — dedicated `ibmxt_inboard386_config[]`, 1986 ROMs only, default
   `ibm5160_050986`. **Fixes the reported POST 101.**
2. `device/inboard386.c` — `cpu_waitstates = 0` (Technique 53).
3. `cpu/386_dynarec.c` — fix block extracted into `inboard_post_fixups()`; `E3AD` exit + range
   safety net (Technique 51).
4. `cpu/cpu.h` — prototype for the shared helper.
5. `cpu/386.c` — call the helper from `exec386_2386()` (Technique 52).
6. **`device/inboard386.c` + `cpu/cpu.h` + both call sites — `inboard386_present` gate (Technique
   54).** Added last, after validation; keeps the shared helper from touching every other machine.
Plus already-ported: `cpu/cpu_table.c` (83.5 MHz entry), `device/kbc_xt.c` (blockedtimeout), and the
standard parallel port in `machine_ibmxt_inboard386_init()`.

**Validated to a Windows 95 desktop**: 09MAY86+Mach8; 10JAN86+Mach8; **default BIOS (no `bios=`
line — the PR reporter's exact scenario)**; **`i386dx`/25 MHz+Mach8**. Generic `vga` boots but stops
for F1. Untested: `am386dx`, `ibm486bl2`, `ibm486slc3`, ET4000/ATI28800/Trident.

**Re-validated after the Technique 54 gate was added**: rebuilt and booted to a Windows 95 desktop,
title bar steady at `100%` — the gate is a no-op on the Inboard machine itself (flag is always 1
there), so the earlier validation carries over.

## ✅ MERGED UPSTREAM, 2026-08-23 — 86Box/86Box PR #7749 (was: submitted 2026-08-22)
Branch `inboard386-fix-post101` on `Mike1978uk/86Box`, one commit, 7 files. PR body kept in
`docs/PR_description_inboard_post101_fix.md` (edit that file, then `gh pr edit 7749 --body-file`).
PR #7626 (the original submission) is **merged**, so this is a follow-up against `master`, not an
update to it. The reporter's two items — POST 101 and the duplicate `(1988) i386SX` entry — are
answered in a closing comment on #7626; the duplicate was already fixed in-tree by someone else.

**MERGED 2026-08-23T14:43Z**, merge commit `3fedf529de1a`. It carries one extra commit beyond the
original submission: `d6fbb9e88`, adopting **Michal Necasek's `F000:FF53`** in place of the
`0x3C0` IRET stub (see boot-fix inventory item 6). **Both Inboard PRs are now upstream** - #7626
(the machine itself) merged 2026-08-06, #7749 (POST 101 + non-486BL CPUs + F000:FF53) merged
2026-08-23. Anything further is a NEW PR against master, not an update to either.

**Not yet upstream**: the `dma_page_is_xt()` DMA page-width fidelity fix (Technique 56) is
local-only and is the obvious candidate for a third PR.

**Still open from earlier**: upstream issue #7638 (RAM configuration) has had no reply yet.
