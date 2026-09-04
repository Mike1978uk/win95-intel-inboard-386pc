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

## Technique 59: before calling a POST stop a *stop*, prove the CPU is not moving - and read the
## address it is actually at, not one you inferred

**This technique was rewritten on 2026-08-23 because its original case study was wrong in every
particular.** The corrected version is more useful than the original, and the way it was wrong is
the real lesson.

### What was claimed, and what was true

The original said: fitting `sbprov2` (0x220 / IRQ 5 / DMA 1) to `ibmxt_inboard386` stopped POST dead
at `F000:E12B`, the byte there is `F4` = `HLT`, so the BIOS was deliberately halting on a failed
8237 register test - and a clean upstream build did it too, so it was an upstream defect.

Measured properly the next day, on a matched disk image:

| Claim | Reality |
|---|---|
| POST stops dead | POST **completes**. Windows ring-0 code first appears at **t+113s** |
| It is halted at `F000:E12B` | It is cycling `F000:ECAC`-`ED4A`. `E12B` was never observed - it was inferred from a theory and then "confirmed" by reading the ROM at that address |
| A clean upstream build halts too, so upstream is at fault | **Invalid control.** PR #7626 is merged, so upstream *contains* this machine. A clean upstream build was never a control for our machine code |
| A recent local `dma.c` change was ruled out | It was ruled out for the right reason but on a false premise, since there was no halt to explain |

The byte at `F000:E12B` really is `F4`. That is what made the wrong answer so convincing: a
verifiable fact about an address the machine was never executing.

### What `F000:ECAC` actually is - worth knowing for its own sake

```
ECA8: out 0xc, al      ; clear the DMA byte-pointer flip-flop
ECAA: pushf / cli
ECAC: in  al, 0        ; DMA channel 0 CURRENT ADDRESS, low byte
ECAE: and al, 0feh
ECB0: cmp ah, al
ECB2: mov ah, al
ECB4: in  al, 0        ; read it again
ECB6: je  ECAC         ; unchanged? spin here.
ECB8: popf / ECB9: loop ECAA
```

On an XT, **DMA channel 0 is the DRAM refresh channel** and its address counter cycles continuously.
This loop spins until it sees the counter move, i.e. it is the BIOS waiting for refresh to be
running. Any machine that wedges here has stopped refresh - a genuinely useful thing to recognise.
It takes ~9 s of wall time on an instrumented build and is passed through normally.

### The discipline that would have caught it

1. **A single sample is not a state.** 0% CPU in the 86Box title bar, or one `[modecheck]` line, is
   an instant. Sample it repeatedly and see whether CS:PC *moves*. A stop means the same address
   twice, minutes apart - not one reading.
2. **Never read a byte at an address you inferred.** Read the address the heartbeat actually
   reported. Confirming a theory's address against the ROM feels like evidence and is not.
3. **Know what a slow build looks like.** This tree's `exec386()` carries ~148 `fprintf`/`fopen`
   sites and runs **3.45x fewer guest instructions/sec** than a clean build. A POST that takes ~110 s
   instead of ~30 s looks exactly like a hang if you only wait 45 s. Both Inboard PRs are merged, so
   **build clean unless you specifically need a hook.**
4. **Check that your control is a control.** Once your work is upstream, "clean upstream build" stops
   being a control for it. The valid control here is a build from *before* the change, or a
   different machine type in the same build.

### The A/B discipline is still right - it was just applied to a phantom
Four runs, one variable each. In particular: **when you have recently touched the subsystem a failure
points at, rule yourself out before blaming anything else.** That instinct was correct. Just
establish that the failure exists first.

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

## Technique 61: "screen black, machine alive" on a Graphics Ultra is a display-PATH switch, not a crash

Issue #7's stall - Windows 95 Setup going black around the "Windows Help" step - has this signature:

- CPU executing **varied** ring-0 addresses (`0028:C0003xxx`, `C0036Exx`, `C0037xxx`)
- PIT channel 0 in **mode 2** with `clocks=` advancing - Windows' own VTD timer is installed and running
- `A0000` checksum **frozen** - nothing writing to VGA memory
- **Ctrl+Alt+Del is handled** (reported on real hardware) - input path, VKD and Windows' handler all alive
- A plain **reboot clears it**
- The point of failure **moves between runs on hardware**, but is deterministic in the emulator
  (two independent twins froze with the identical checksum `00003E8C`)

Ruled out by direct experiment, not inference: **it is not waiting for a mouse.** A twin with
`mouse_type = msserial` fitted and COM1 free stalls with the identical signature. (The hypothesis was
attractive because the real machine has a single serial port shared with COMrade, so the mouse is
usually unplugged - a real shared variable, and still wrong.)

**The mechanism to check instead.** An ATI Graphics Ultra is a mach8 accelerator *and* a VGA core,
two output paths sharing one monitor. `vid_8514a.c`:

```c
case 0x4ae8:
    WRITE8(port, dev->accel.advfunc_cntl, val);
    dev->on = dev->accel.advfunc_cntl & 0x01;   /* <- who drives the screen */
```

Port **`0x4AE8`** is Advanced Function Control; bit 0 hands the display to the 8514. Enable it
without valid 8514 timings and you get every symptom above: black screen, live machine, working
Ctrl+Alt+Del, and a reboot that fixes it because the card resets to the VGA path. The 8514 registers
are **sparse across `02E8`/`06E8`/`0AE8`/`0EE8`/`4AE8`** - easy to miss when tracing.

**Not yet proven** - stated as a mechanism that fits every observed symptom, not a verified cause.
Proving it needs a trace build: `ibm8514_log()` compiles to nothing unless the log is enabled.

## Technique 62: on a 4-bit page latch, audit every driver's `_PageAllocate` - a DMA buffer above
## 1 MB does not crash, it transfers against the wrong address

**The class of bug.** An Inboard 386/PC in a 5160 keeps the XT's **4-bit DMA page latch**: 20-bit
reach, 1 MB. Every ISA-era driver assumes **24-bit**, 16 MB. Allocate a DMA buffer above 1 MB and
there is no fault - the page register drops the high bits and the 8237 runs against a different
physical address entirely. **Silent wrong data, not a crash**, which is why it survives normal
testing and looks like something else (for issue #5 it looked like a video-driver problem, because
the same machine's Windows 3.11 install sounded fine).

**How to see it.** `dma.c` carries a gated trace (`INBOARD_DMA_TRACE=1`, capped, inert otherwise)
that logs page-register writes and flags truncation:

```
[dmapage] ch=1 val=4E -> page=0E *** TRUNCATED, buffer is above 1MB ***
     intended 0x4E0000 = 4.875 MB (top of RAM)   actual 0x0E0000 = 896 KB (adapter ROM)
```

**Where the address comes from - and where it does NOT.** `DMABufferIn1MB=Yes` will not fix this,
and measuring that saved a wrong conclusion. In stock `VDMAD.VXD` at `OBJ3:0x162` it clamps
`[0x2100]`, the **size** of VDMAD's own bounce buffer. VDMAD is not naive - it already switches masks
on machine class (`cmp byte ptr [0x211a], 2`). A Windows 95 32-bit driver allocates its **own**
buffer and hands VDMAD the physical address, so nothing bounces. (Windows 3.11 works on the same
hardware precisely because its 16-bit `.DRV` does go through the bounce buffer.)

**The real lever.** `_PageAllocate` = VMM service **`0x00010053`**, eight dwords pushed right to
left, caller balances `add esp, 0x20`:

```
_PageAllocate(nPages, pType, hVM, AlignMask, minPhys, maxPhys, PhysAddrPtr, flags)
```

`maxPhys` is a maximum **page number**. `0xFFF`/`0x1000` = 16 MB = the ISA assumption. Change it to
`0xFF` = 1 MB. `push 0xfff` and `push 0xff` are both `68 imm32`, so **one byte moves and no
instruction boundary shifts** - Technique 60's corruption mode cannot occur here.

**Read `maxPhys` as intent, not just a number:**

| `maxPhys` | Meaning | Action |
|---|---|---|
| `~0xFFF`/`0x1000` (16 MB) | driver deliberately declaring an ISA DMA constraint | **patch to `0xFF`** |
| `0xFFFFF` (4 GB) | "anywhere" - an ordinary allocation that never touches DMA | **leave alone** - forcing it low wastes the only DMA-capable RAM |
| `<= 0xFF` | already XT-safe | nothing to do |

A blanket patch gets that middle row wrong.

**Tools:** `tools/vxd_dma_audit.py` (read-only, any VxD), `vxd-patches/patch_vxd_dma_maxphys.py`
(refuses a no-op, post-checks its output), `tools/fatls.py` (pull files out of a raw image when the
CF is in the machine). **Caveat:** VxDs bundled inside `VMM32.VXD` cannot be audited - it is `W4`
compressed. Use their pre-monolith staging copies.

**CONFIRMED ON REAL HARDWARE 2026-08-24.** Emulator twin first, then the 5160. Clean audio on both:

```
before:  [dmapage] ch=1 val=4E -> page=0E  *** TRUNCATED, buffer is above 1MB ***
after:   [dmapage] ch=1 val=09 -> page=09  ok
```

**Sweep the WHOLE install, and sweep the PRE-MONOLITH image.** `tools/sweep_image_dma.py <image>`
does the walk. Two lessons from doing it properly on 2026-08-24, both of which a narrower pass had
missed:

- sweeping the whole volume rather than the drivers you suspect found **`VFINTD.386`** and
  **`SCSIPORT.PDR`** as well;
- sweeping the **pre-monolith** image found three more that are *invisible* on a combined install
  because `VMM32.VXD` is `W4` compressed: **`IOS.VXD`** (the I/O Supervisor), **`VFAT.VXD`**,
  **`VFBACKUP.VXD`**.

These are *Microsoft's own* drivers, so this is a machine-class bug, not one install's misconfiguration.

**Then do NOT patch everything the sweep lists.** Tier by deployment cost and blast radius, and let
a measurement decide the expensive tier:

| Tier | Files | Why |
|---|---|---|
| patch freely | plain files in `WINDOWS\SYSTEM` (`MSSBLST`, `LPT`, `QIC117`) | dynamically loaded, deploy by file copy, revert by file copy |
| check relevance first | `IOSUBSYS\SCSIPORT.PDR`, `C:\DOS\VFINTD.386` | only load under conditions this machine may never meet |
| leave alone unless measured | anything inside `VMM32` (`IOS`, `VFAT`, `VFBACKUP`) | needs a pre-monolith rebuild to deploy, and the boot path is in the blast radius |

On this machine the runtime trace across a full verified boot showed the page register programmed
**six times, all channel 1** - nothing but the SB Pro does ISA DMA here at all. That is the argument
for not touching `IOS.VXD`: it fixes something that is not happening, at real risk. Note also that
~20 files supply `maxPhys` **in a register** (`VDMAD`, `V86MMGR`, `IFSMGR`, `NDIS`, `HSFLOP`,
`DOSMGR`); those are not statically decidable and need tracing, not disassembly.

**A refinement deliberately not made:** `0xFF` is a 1 MB ceiling, but 386MAX's `MARK_XT` sets
`@DMA_PA_XT = 0x000A0000` - the true XT ceiling is **640 KB**. `0xFF` is safe in practice because
pages `0xA0`-`0xFF` are not RAM so `_PageAllocate` cannot return them. `0x9F` is strictly correct.
It was left at `0xFF` because that build is verified on hardware, and re-cutting a proven artefact
needs a re-test rather than a rationale.

## Technique 64: `NODIAGS` fixes the memory diagnostic and then exposes the shadow-RAM failure

**Verified 2026-08-24 on the reporter's own image** (upstream #7638), converted from their dynamic
VHD with `qemu-img convert -f vpc -O raw`, their `CONFIG.SYS` with one flag added:

```
DEVICE=INBRDPC.SYS NOPAUSE EGACACHE NODIAGS
```

| | before | after |
|---|---|---|
| functional extended memory | 0k | **2048k** |
| bad extended memory | 128k | **0k** |

So `NODIAGS` genuinely works as a workaround, and the RAM was never the problem. **But it then
surfaces a second panel** - *"The Inboard 386/PC's ROM BIOS shadow RAM failed"* - so it is not a
clean answer to give a reporter. Their `INBRDPC.SYS` is byte-identical to our stock
(`c25b951a0a6093dcfa5138d89159cbf6`); the difference was only ever the missing flag.

**New measurement, and it matters:** with `INBOARD_MEM_TRACE=1` on their config -

- **0** accesses to our high BIOS-shadow alias at or above 1 MB, and
- **0** `INT 15h AH=87h` calls (`[int1587]`), where the port plan's 2026-07-26 trace found **two**
  on our own 5120 image.

**Our emulator-side shadow fix is not engaging on their machine at all.** The alias is placed at
`0xF0000 + mem_size*1024` - `0x3F0000` for their 3072. If the driver derives its alias target from
its *own* notion of installed RAM rather than the configured `mem_size`, our mapping is simply at the
wrong address for any configuration that is not the one it was derived on (5120). That is the next
thing to test, and it is cheap: run the same image at 1024 / 3072 / 5120 and see whether the alias
is ever hit.

Related but **not the same root cause**: SuperFury hit the identical message in UniPCemu and
attributed it to `F0000+` landing in a memory hole, needing the ROM mirrored at both `FFFF0000` and
`F0000` (vogons t=52651). The port plan's own conclusion was different again - a genuine
`INT 15h AH=87h` emulation gap, with an all-zero GDT descriptor table at the call so no copy happens.
Three explanations, one symptom; do not assume any of them without re-measuring on the config in
front of you.

## Technique 63: "bad extended memory: 128k" - CONCLUSION CORRECTED 2026-08-24, see Technique 67
##
## > **The measurements below are all correct. The conclusion drawn from them was wrong.**
## > This is NOT "the driver's own preliminary check" and NOT something to work around with
## > `NODIAGS`. **It is our bug.** The two mark-bad hits at offset `9D2E` with `EBP=FFFFFFFF` are
## > the driver probing the card's 128 KB reserved block through its two high windows -
## > `0x5E0000` and `0x5F0000` - and reading `0xFF` because we never mapped them. Read
## > **Technique 67** instead. Keep this section only for its measurement technique.

**Recognise it instantly.** `INBRDPC.SYS` shows a scare screen - *"Some extended memory on the
Inboard 386/PC or Piggyback board has failed"* - with:

```
extended memory detected:  2048k     <- correct
extended memory diagnosed: 2048k     <- it tested all of it
functional extended memory:   0k     <- does not add up
bad extended memory:        128k     <- CONSTANT, whatever the RAM size
```

**The tells that it is not a real memory fault, and not a config problem:**

- the numbers do not reconcile - `diagnosed != functional + bad`. The driver aborts and zeroes the
  total rather than reporting 128 KB bad and using the rest;
- `bad` is **exactly 128k regardless of `mem_size`**. A genuine fault scales with the array;
- it is **independent of BIOS revision, CPU family and RAM size**. Confirmed 2026-08-24 by A/B on a
  reporter's own disk image: i386dx/16 MHz vs ibm486bl3/83.5 MHz, superega vs mach8 - identical.

**Root cause, already traced** (port plan lines 1302-1307): the only mark-bad hits are **2 hits at
INBRDPC.SYS code offset `9D2E`**, the very first block/pair, both a *complete* mismatch
(`EBP=FFFFFFFF`, every bit wrong). Exactly **2 x 64 KB = 128k**. That block runs twice, not in the
64-iteration per-chip loop - a one-time preliminary sanity check, separate from the real per-board
test, which passes cleanly on its own.

**The fix is Intel's own, documented in their manual (Table D-2):**

```
DEVICE=C:\INBRDPC.SYS NODIAGS NOPAUSE
```

`NODIAGS` turns off the extended memory diagnostics. At 3072K the result is then
`detected 2048k / functional 2048k / bad 0k`.

**Why this keeps coming back:** every image this project builds already carries `NODIAGS`, so the
screen is invisible here - but any *user-built* image will show it, and it looks exactly like a
port bug. Reported as one on upstream issue [#7638](https://github.com/86Box/86Box/issues/7638).
**Check `CONFIG.SYS` before touching `86box.cfg`** - the diagnostic running at all proves `NODIAGS`
is absent, so there is nothing to bisect in the machine config.

### CORRECTION 2026-08-24: `NODIAGS` is a workaround, and the driver is RIGHT

The section above was written believing the check was simply buggy. It is not. **Real hardware
passes this diagnostic** - the user runs `NODIAGS` only to speed up boot, on hardware already known
good. So a failure here is *our* gap, and the driver is correctly reporting missing memory.

Confirmed by reproducing it on this project's own reference image: put stock `INBRDPC.SYS` back
(ours is `INBRDPC_selftest_skip.SYS`) and drop `NODIAGS`, and we fail identically. **Our images hide
this two ways at once** - that is why it stayed invisible for so long, and why neither workaround
should be treated as a convenience that can be dropped.

**Root cause, quantified.** Same `mem_size = 5120` config:

| | conventional | reserved | extended | total |
|---|---|---|---|---|
| real 5160 + Inboard | 640K | **128K** | **4352K** | 5120K |
| this emulator | 640K | **384K** | **4096K** | 5120K |

128K is exactly **two 64 KB shadow windows** - system BIOS at `F0000`, video BIOS at `C0000`. That is
all the real card reserves. We reserve the whole 384 KB `A0000-FFFFF` hole and return 256 KB less
extended memory. Independently corroborated by 386MAX: `Available Shared memory` 3840 KB real vs
3568 KB ours.

**The missing feature:** the card remaps the unused part of its 1 MB base RAM into extended space -
UniPCemu's `MoveLowMemoryHigh` / `inboard_remapVideoAndBIOSROMhigh`, gated on port `0xA0` bit 7.
This port implements none of it and ignores `0xA0`'s value for memory purposes entirely.

**Two theories killed by measurement**, recorded so nobody re-runs them (`INBOARD_MEM_TRACE=1`):
- *A20 desync* - no. The driver's first A20 op is `DF` (enable), so `mem_a20_state` syncs at once and
  all 40 later `DD`/`DF` transitions flip `rammask` correctly between `FFFFFFFF` and `FFEFFFFF`.
- *The high BIOS-shadow alias being hit during the probe* - no. Zero accesses at or above 1 MB.

**TWO FIXES ATTEMPTED 2026-08-24. NEITHER FIXES THE 128k. Read before trying either again.**

**Result first: the 256 KB accounting gap and the `bad 128k` report are SEPARATE problems.** I
assumed one caused the other. It does not. Reclaiming the hole changed the diagnostic not at all -
still `detected 2048k / functional 0k / bad 128k`. The driver's *detection* number never moved
either, because it comes from the A20-wrap probe, not from what the emulator maps. Whatever the
preliminary check is comparing, it is not "is there RAM at the top".

**Attempt 1 - `ram_remapped_mapping` (86Box's own 256 KB window at `mem_size*1024`, backed by
`ram + 0xa0000`). SEGFAULTS** during the A20 probe: with `rammask = FFEFFFFF` an address inside the
window is pulled below the window's own base while that mapping is still selected, so the callback
indexes `ram + 0xa0000` negatively. AT chipsets never hit this because their BIOS settles A20 long
before such a window exists. **Do not put a RAM window at the top of memory on this machine.**

**Attempt 2 - UniPCemu's actual approach, and it is address TRANSLATION, not a window**
(`mmu/mmuhandler.c`, `MMU_calcmaplocpatch`):

```c
if ((MoveLowMemoryHigh & 1) && (memloc))
    address += (LOW_MEMORYHOLE_END - LOW_MEMORYHOLE_START);   /* 0x100000 - 0xA0000 = 384 KB */
```

Everything above the hole indexes RAM 384 KB lower, so the RAM behind `A0000-FFFFF` is used instead
of wasted and extended becomes `mem_size - 640K`. In 86Box that is re-pointing `ram_high_mapping` at
`ram + 0xa0000` and growing it by 384 KB. **Stable, no crash** (no top-of-memory window, so no A20
interaction) - and it correctly reclaims the memory: `extended now 2432 KB (was 2048 KB)`. It just
does not touch the diagnostic. Shipped opt-in behind `INBOARD_BACKFILL=1`, default off.

**Where to start next time:** stop theorising about the memory map and hook the driver's own compare.
The port plan pins the mark-bad site at `INBRDPC.SYS` code offset `9D2E`, and live traces show the
driver running at `CS:PC = 0020:00009Dxx`, so a Technique 12 CS:PC hook there dumping the operands
gives the failing address directly instead of another inferred mechanism. That is the one thing
nobody has actually measured.

---

**Original note, superseded in its conclusions but kept for the mechanism:** 86Box already
carries the exact mechanism: `mem_init()` creates `ram_remapped_mapping`, a **256 KB** window at
`mem_size * 1024` backed by `ram + 0xa0000`, then disables it - the standard "relocate the shadow
hole to the top of memory" feature. Right size, right place, right backing. Enabling it for the
Inboard (`INBOARD_BACKFILL=1`, opt-in, off by default) **segfaults 86Box during the A20 probe**.

Mechanism of the crash: with A20 masked (`rammask = FFEFFFFF`) an address inside the remapped window
is pulled *below* the window's own base while that mapping is still selected, so the remap callback
indexes `ram + 0xa0000` at a negative offset. The AT chipsets that use this mapping never hit it
because their BIOS manages A20 before any such window exists. **Placing a RAM window exactly at the
top of memory on a machine that actively toggles A20 needs the masking handled explicitly** - that
is the real work, and it is not a switch flip.

Corroboration that this is the right area, from SuperFury's own Vogons thread
(https://www.vogons.org/viewtopic.php?t=52651, 3 pages): UniPCemu hit the *same class* of symptom
while developing this - "5120K extended memory with 704K assumed bad", later "5504K with 960K bad
RAM" - explicitly attributed to memory-hole calculation during the diagnostic. The feature is
`MoveLowMemoryHigh` / `applyMemoryHoles()`, set by `initInboard()` and by port `0x670` writes
(`case 0x670: MoveLowMemoryHigh = 1;`). **Our `inboard386_write_670` handles speed, ROM shadow,
wait states and prefetch, but not the memory move at all.**

**Do not test this by raising `mem_size`.** `INBRDPC.SYS` validates the board configuration and
refuses anything that is not 1 MB, 1+2 MB or 1+4 MB - a `3328` config makes it bail before probing,
giving a silent false negative. The fix has to come from remapping *within* the existing total.

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

**MERGED 2026-08-25 as [#7771](https://github.com/86Box/86Box/pull/7771)**: the `dma_page_is_xt()`
DMA page-width fidelity fix (Technique 56). Nothing emulator-side remains local-only.

**Upstream issues raised from this project, both now closed**: #7638 (RAM configuration, closed
2026-08-08 by our merged PRs) and #7805 (settings dialog rounded RAM to an invalid size — reported
by @andrew-hoffman, diagnosed here, fixed by OBattler in `9ee5197` and closed 2026-08-28).

## Technique 65: on this XT, a Win95 device failure is usually an UNCONFIGURED device node, not a
## wrong driver - read the hive for `ForcedConfig` vs `BootConfig` before touching any driver file

**Established 2026-08-24, on real hardware, and it resolved issue #4 (Mach8) while explaining #6
(phantom PS/2 mouse) as the same bug one step behind.**

Windows 95 assigns resources to a non-PnP ISA device by *automatic configuration*. On this machine
that frequently has nothing to work from, so the node is left with no usable resources and whatever
driver you bind to it fails - looking exactly like a missing or wrong driver. The fix is
Device Manager -> the device -> Resources -> uncheck **Use automatic settings** -> pick a
configuration -> reboot. Users call this "set configuration manually".

**It leaves a fingerprint in the registry, so you can tell which devices have had it done.**

| Value on the `Enum\Root\...\0000` node | Means |
|---|---|
| **`ForcedConfig`** | a human set the configuration manually - this device has had the fix |
| **`BootConfig`** | the configuration came from a **detection routine** at setup time |
| **`DetFunc = *:DETECTxxx`** | which detection routine produced it |

A `BootConfig` with no `ForcedConfig` is the failure state, and **Device Manager greys the resources
out** in that state - which is what "IRQ 12 isn't changeable" means. It is not a locked setting, it
is a device Windows believes it detected.

**Working Mach8 config, for reference** (`Services\Class\Display\0000`, verified against a live card
running video + sound + network simultaneously):

```
InfPath     msdisp.inf          <- MICROSOFT's bundled driver, section [ATI8]
InfSection  ATI8
DriverDesc  ATI Graphics Ultra (mach8)
DevLoader   *vdd                ->  ATIM8.DRV + ATI.VXD
Settings\   BitsPerPixel 8  |  Resolution 1024,768
node:       HardwareID *PNP090B, CompatibleIDs *PNP090A, Mfg ATI Technologies, + ForcedConfig
```

Two traps this kills:

- **There IS a Windows 95 driver for the mach8.** ATI never wrote one, but Microsoft shipped
  `ATIM8.DRV`/`ATI.VXD` in the retail box. Do **not** install the Windows 3.1x driver
  (`MACHW3.DRV`/`.3GR`/`MACHW3V.386`) under Win95 - it was tried first and failed. Leftover MACHW3
  files in `WINDOWS\SYSTEM` and a stale `[boot.description] display.drv=ATI mach8: ...` line in
  `SYSTEM.INI` are cosmetic; `[boot] display.drv=pnpdrvr.drv` is normal and the real driver comes
  from the hive.
- **Changing a device's driver does not clear its configuration.** Re-driver'ing the phantom PS/2
  mouse to "Standard Serial Mouse" rewrote `DeviceDesc` and `HardwareID` (`*PNP0F0E` -> `*PNP0F0C`)
  and left `DetFunc = *:DETECTPS2MOUSE` and the PS/2 `BootConfig` (IRQ 12, I/O 310-311 - neither
  exists on a 5160) in place. **Remove** the node and reinstall via Add New Hardware **declining
  autodetect**, or `DETECTPS2MOUSE` re-runs and the phantom returns (Technique 55 / issue #6).

**Reading the hive without booting the guest.** `SYSTEM.DAT` is `CREG`, not NT `regf`, so libregf and
friends will not open it. A plain printable-run scan is enough for this job and needs no parser -
value records are stored as `<name><data>` back to back, so grepping for a value name and printing
~400 bytes of context reads out the whole node. `strings` is absent from this environment; use Python
`re.finditer(rb'[\x20-\x7e]{5,}', data)`. Beware that adjacent records bleed together in the output
(`MouseTypeSerialial0` is `MouseType`=`Serial` plus the head of the next record) - do not read a
value off a run boundary. The 12 bytes before a name are its header, ending
`type(2) namelen(2) datalen(2)`, if you need to delimit exactly.

## Technique 66: the high BIOS-shadow alias belongs at a FIXED 0x5F0000 - and proving it needs a
## config where the formula and the constant DISAGREE

**Established 2026-08-24 on Fenix770's own disk (issue #11 / upstream #7638).**

`inboard386.c` places the alias at `0xF0000 + mem_size*1024`. That formula was fitted to a **single**
live trace taken at `mem_size = 5120`, where it evaluates to `0x5F0000` - so "derived from installed
RAM" and "a fixed constant" are **indistinguishable at that one size**. Every later reading inherited
the wrong one.

| `mem_size` | formula gives | driver writes there | driver's own shadow self-test |
|---|---|---|---|
| 2688 | `0x390000` | **0** | fails |
| 3072 | `0x3F0000` | **0** | fails |
| 4096 | `0x4F0000` | **0** | fails |
| 5120 | `0x5F0000` | **40**, ROM content from offset 0 | **passes** |
| 4096 + `INBOARD_ALIAS_BASE=0x5F0000` | `0x5F0000` | **41** | **passes** |

That last row is the whole experiment: it is the only configuration where the two candidate readings
predict **different** addresses, and the driver goes to `0x5F0000`. Al Williams' driver for this card
hard-codes the same number - `MINBRDPC.ASM`: `inbrd_romram equ 5f0h ; high speed ram to replace the
pc's [ROM]`, a "starting physical bank number", and `0x5F0 * 4 KB = 0x5F0000`.

**The crash you will hit first - and it is the prerequisite, not a side quest.** Whenever the
alias is actually *used*, 86Box dies, and it dies **before the driver prints its verdict**, so
the alias fix looks like it changed nothing:

```
exit code 0xC0000005 (ACCESS_VIOLATION)
#0 mmutranslatereal_normal (addr=3963, rw=0)  mem/mem.c:291   temp = temp2 = rammap(addr2);
#1 mmutranslatereal   #2 readmembl   #3 exec386
#define rammap(x) ((uint32_t *)(_mem_exec[(x) >> MEM_GRANULARITY_BITS]))[...]
```

`_mem_exec[]` is NULL for physical pages with no RAM-backed mapping, and `rammap()` dereferences it
unchecked. With the self-test satisfied the driver gets **further than it ever had**, enables paging,
and lands `CR3` above the top of installed RAM. **This is also a general 86Box bug worth reporting on
its own: a guest can kill the emulator by pointing CR3 at unbacked physical memory.**

### RESOLVED the same day - both fixed, and one conclusion recorded here was WRONG

Fixed by pinning the alias (`#define INBOARD_SHADOW_ALIAS 0x5f0000` in `inboard386.c`) **after**
fixing `rammap()` in `mem.c` to substitute a shared all-`0xFF` page when `_mem_exec[]` is NULL.
Keeping `rammap()` a macro preserves its use as an lvalue, which matters because nine callers do
`rammap(x) |= ...` for accessed/dirty bits.

| `mem_size` | 1024 | 2688 | 3072 | 4096 | 5120 |
|---|---|---|---|---|---|
| before | - | fail | fail | fail | pass |
| after | pass | pass | pass | pass | pass |

Stock `INBRDPC.SYS` at 3072 now ends `The iNBRDPC.SYS device driver is installed.`

**Retracted:** the claim that a bare 64 KB window is the wrong model because the driver's sizing
probe "sees the island" (`detected: 2752k` vs `3072k` at 4096). **It does not.** `detected` and
`functional` are **byte-identical** before and after the fix (`2048k` at 3072 both ways). The `2752k`
was a mid-count snapshot - which is a trap in its own right:

### Technique 66b: that panel's memory figure is a COUNTER, and it stops pausing once the test passes

`extended memory detected:` **counts up while the driver probes**. While the self-test was failing the
panel paused, so any screenshot caught a settled value. The moment it passes, the panel is on screen
for about **four seconds** and boot moves on - so every slow capture samples the counter mid-flight
and reads like a regression:

- one frame per 10 s -> caught `512k` at 3072
- a frame frozen by the crash -> caught `2752k` at 4096
- both were wrong, and each produced a confident wrong conclusion before being re-measured

Spawning a fresh `pwsh` per screenshot costs ~1 s and cannot sample a 4 s window. `tools/shadow_burst.ps1`
does the `PrintWindow` capture **in-process at 4 fps**. Take the *last* full-panel frame, and confirm it
has settled by looking for `Press any key to continue:` or the driver's own success line.

### Two methodology traps this cost real time on, both about inferring from indirect signals

1. **A null result from a run that never reached the guest code is not a null result.** The previous
   session's `vm_alias_1024/3072/5120` recorded "0 alias hits at every size" - and every one of those
   runs was sitting at the POST `ERROR. (RESUME = "F1" KEY)` prompt, having never booted DOS. Check a
   screenshot before believing a zero. Drive F1 with the build's own `inject_key.txt` channel
   (`386_dynarec.c`, `[injectfix]`; XT scancode 59) **on a timer** - the moment POST stops varies with
   `mem_size`, so a single injection at a guessed time misses.
2. **Do not diagnose a crash from stderr size, a missing screenshot, or an absent log tag.** All three
   lied here, in both directions: the failing runs emit a ~1M-line `[ringshadow]` ring dump so
   *smaller* stderr looked like a crash; `shot.ps1`'s single probe reported "no window" while the VM
   was alive; and `ringshadow=0` looked like "the test passed" when it actually meant "crashed before
   the verdict was printed - the panel's `diagnosed`/`functional`/`bad` lines were still blank".
   Get the exit code (`Start-Process -PassThru` → `$p.ExitCode`) and a `gdb` backtrace.

Harness: `tools/shadow_run.ps1` (one config-controlled run), `tools/shadow_exit.ps1` (exit code +
live frames), `tools/shadow_gdb.ps1` (backtrace), `tools/vhd2raw.py` (dynamic VHD → raw; there is no
`qemu-img` on this box, and reporters attach VHDs).


---

## Technique 67: the Inboard reserves **128 KB**, not 64 - and UniPCemu's memory model lives in
## `mmuhandler.c`, NOT in `inboard.c`

**The single most expensive mistake in this port**: treating `UniPCemu/hardware/inboard.c` (257
lines) as "the reference implementation". It is only the *ports and wait-states* half. The **memory
map** - where every shadow/alias/reserved-block behaviour actually lives - is in
`UniPCemu/mmu/mmuhandler.c`. Two sessions burned on `bad extended memory: 128k` would have ended in
minutes with one grep of that second file.

**Grep the whole UniPCemu tree for the flag, not the device file:**

```bash
grep -rn "inboard_remapVideoAndBIOSROMhigh\|MoveLowMemoryHigh" "$U" --include=*.c --include=*.h
```

### What it says

```c
/* mmuhandler.c ~910 */
translatedaddr += 0x20000; // Apply reserved memory as well (this is placed at the beginning) of 128KB!

/* mmuhandler.c ~824, ~841 */
if (((precalcposvirt == 0x5F) && inboard_remapVideoAndBIOSROMhigh) || (precalcposvirt == 0xF)) // BIOS ROM
if (((precalcposvirt == 0x5E) && inboard_remapVideoAndBIOSROMhigh) || (precalcposvirt == 0xC)) // "Second 64K of reserved memory (EGA ROM as documented)"
```

The card reserves **128 KB at the bottom of RAM**, normal RAM is shifted up past it, and the block is
visible through **two** 64 KB windows: `0x5F0000` (BIOS ROM) and `0x5E0000` (video/EGA ROM). The low
views are `0xF0000` and `0xC0000`. `memoryhole = 0xFF` marks it "manually managed".

**This port only ever implemented `0x5F0000`.** Reads of `0x5E0000` hit no mapping and returned
`0xFF`. **That is the 128k.** Adding a plain-RAM window at `0x5E0000` takes it `128k -> 64k`,
measured on a reporter's disk at 3072 with diagnostics enabled. Shipped: fork `f0480f9`, upstream
PR #7761.

Note this is NOT the same thing as caching the video ROM at `0xC0000` - Intel's opt-in `EGACACHE`,
which this file correctly leaves alone. The card reserves the window regardless, which is also why
the panel prints `EGA BIOS: 32-bit RAM` on machines that never enable EGA caching.

### `0xFF` / "every bit wrong" is an UNMAPPED signature, not a memory-fault signature

Technique 63 recorded the mark-bad hits as `EBP=FFFFFFFF`, "a *complete* mismatch, every bit wrong",
and read that as a driver quirk. All-ones is what 86Box returns for **no mapping at all**. When a
guest's memory test reports a region bad and the readback is all-ones, **stop looking at the test and
go find what should be mapped there.** A real failing DRAM array gives you *some* bits wrong, not all
of them, and not the same byte count at every RAM size.

### Two things our own source comments get wrong - do not trust them

1. `inboard386.c`'s note blames gating on **port 0xA0 bit 7**. In UniPCemu's actual source **that
   block is commented out**. The live gate is **port 0x670 bit 0** (the cache/shadow-enable bit),
   inverted: bit clear -> ROMs mapped **high** (the `0x5E`/`0x5F` windows exist); bit set -> mapped
   **low only**. That is the same bit our port already reads as
   `dev->rom_shadow_enabled = dev->speed & 1;`, so our high alias is permanently enabled where
   UniPCemu's is conditional.
2. UniPCemu also clamps `MMU.maxsize = MIN(MMU.size, 0x500000)` (XT) / `0x300000` (AT) and recomputes
   it on every `0xA0` / `0x670` / `0x674` write. We never do.

### Recorded dead end - do not re-attempt blind

Making the high BIOS window plain RAM (matching UniPCemu, whose ROM flags apply only to the low
`0xF0000` view) **does** give `bad: 0k`. It also **resets the guest** a few seconds after the driver
loads. Reproduced on **two consecutive runs** - resample before blaming the intermittent POST flake
this machine has at 2688/3072 (issue #14), but do not stop at one clean run either.

**Working hypothesis for why** (untested): the diagnostic's test patterns land in `bios_shadow_ram`
and destroy the ROM copy this port *pre-populates at init*, after which the low `0xF0000` window
serves that garbage as executable code. On real hardware the driver tests the block and *then* copies
the ROM in; our pre-population may be letting it skip the copy. **Measure before coding** - the fork
already has `INBOARD_MEM_TRACE` with `[inbmem] ALIAS read/write` logging; use it to establish the
*order* of the driver's test writes vs its shadow copy rather than guessing again.

### Practical notes that each cost a run

- **You cannot drive 86Box's keyboard from the host. All three methods fail SILENTLY.**
  Corrected 2026-08-24 after I claimed `keybd_event` worked - it does not. Tested and failed:
  `WScript.Shell.SendKeys`, `WM_KEYDOWN`, and `keybd_event` via P/Invoke (with
  `SetForegroundWindow` first). Each returns success and changes nothing, so the guest sits at a
  prompt looking exactly like a hang. The fork's `inject_key.txt` hook did not advance an F1 prompt
  either, and it does not exist at all in an upstream build.
  **What actually works:**
  1. **Ask the user to press the key** - the only method confirmed to work here.
  2. **Engineer the prompt away**: `NOPAUSE` on `INBRDPC.SYS`, and **clear `<vm>/nvr/`** - a stale
     NVR makes GLaTICK stop at `RTC [...] ERROR. (RESUME = "F1" KEY)` *before* `CONFIG.SYS`, which
     silently costs you the entire run and looks like the driver never ran.
  `tools/sendkey86.ps1` exists but should be assumed non-functional until someone proves otherwise.
- **The "press any key to display the location" page is a CHIP MAP, not addresses** - *"Failed chips
  are highlighted and labeled 'BAD'"*, a physical package layout. It will not give you a failing
  address. Do not spend runs on it.
- **`NODIAGS` blanks two fields.** `conventional memory initialized:` and `extended memory
  diagnosed:` render **empty** under `NODIAGS`. They are real value fields (`640k` / `2048k`); the
  flag just skips the pass that fills them. Never read a `NODIAGS` screenshot as evidence that those
  values are missing.
- **`functional 0k` with `bad 128k` is not a contradiction** - the driver rejects the **entire** pool
  over any bad block. A 64 KB gap costs the user *all* extended memory, which is why this matters
  more than the number suggests.
- **A pre-#7749 `86box.cfg` boots to a black screen, no POST, no message.** The machine now carries
  its own BIOS list with only `ibm5160_050986` / `ibm5160_011086`; an older
  `bios = ibm5160_1501512_5000027` matches nothing. Both upstream reporters' attached VMs hit this.
  Check `bios =` before diagnosing anything else.

---

## Technique 68: the accelerator's CPU is an **IBM Blue Lightning**, so Cyrix tools do not apply

Recurring dead end when chasing the cache / clock-multiplier work (issue #9). The real machine's
`revto486.sys /BL /CN /CCM /2` line and our own `cpu_family = ibm486bl3` both say **IBM BL3**.

- **Cyrix `CX486DRX` disk** (`CX486.EXE`, `CXID.EXE`) - **Cx486DRx/SRx only.** `CXID.EXE` returns
  errorlevel **255** on anything else and Cyrix's own installer aborts on it. Wrong family.
- **Gortmaker `cyrix.exe`** - DLC/SXL only. Wrong family.
- **BL3 needs** `revto486.sys` / `lght486.sys` (multiplier `/1` `/2` `/3`) or **`CTCHIP34 IBM486`**
  for the registers. CTCHIP34 runs from `AUTOEXEC.BAT` and exits (needs `%%` there, `%` at the
  prompt).
- The two Cyrix disk versions in circulation are **identical for DOS**: `HD/CX486.EXE` is
  byte-identical (MD5) between the "3.0" and "3.3" downloads; only `CX486WIN.EXE` differs.

**Reference**: feipoa, *"Register settings for various CPUs"*,
<https://www.vogons.org/viewtopic.php?t=45756> - all 4 pages read 2026-08-24. Load-bearing points:

- **`-cd` does NOT clock-double a plain DLC.** On SXL2 it enables 2x; on Cyrix/TI **486DLC the same
  register selects direct-mapped vs two-way L1 instead**. An easy way to think you enabled doubling.
- **BARB vs FLUSH with a DMA controller**: *"If you are using a DMA-capable SCSI controller ... you
  may need to use the BARB method ... If using BARB, and your motherboard has enabled FLUSH, disable
  flush."* An XT planar never drives `FLUSH#`, so the Cyrix disk's shipped default `CC_0=13`
  (FLUSH on, BARB off) is an enabled cache with **no DMA invalidation** - silent corruption, not
  errors. Relevant to the T130B.
- **The Win95-only halt has a documented match** (replies 64-67): cache enabled -> Win95 will not
  boot, GPF in text mode then shutdown; disabling L1 lets it boot; **`win /d:x` boots it with the
  cache on**. `/d:x` == `EMMExclude=A000-FFFF` in `SYSTEM.INI`. Same signature as issue #9 - fine in
  DOS 6.22 and Win3.11, dies only on the Win95 boot.
- **HIMEM for BL3**: feipoa - *"Normally I only need to experiment with `/machine:1` or `/machine:3`
  when using a BL3 chip"*, plus `/CPUCLOCK:ON` and `/TESTMEM:off` if it errors. Two people cleared
  "unable to control A20" with `/machine:13`.

  > ### DO NOT APPLY THE `/MACHINE:` PART ON THIS MACHINE
  >
  > It is good advice generally and **wrong here**, and we have already paid for it once.
  > `INBOARD_86BOX_PORT_PLAN.md:1290`: *"`HIMEM.SYS /MACHINE:12` was tried as a faster workaround
  > before finding the real cause - had no effect (still failed identically), and was reverted...
  > **`HIMEM.SYS` should stay switchless in the working config.**"* That entry also retracts an
  > earlier memory note claiming `/MACHINE:12` means "A20 always on, hardware never touched" -
  > that characterisation is **wrong**, do not reuse it.
  >
  > **Why switchless wins here:** HIMEM's *default* AT-method auto-detect computes the correct
  > value on the real 5160+Inboard "by a fortunate accident of the flag logic", but only because
  > port `0x64` reads back `0x00` on this board - measured on real hardware via COMrade's `io_in`.
  > A `/MACHINE:` override replaces a detect that already happens to work. (The emulator-side
  > counterpart of the same fact: `kbc_xt.c` claims only `0x60-0x63`, so `0x64` fell through to
  > 86Box's open-bus `0xFF` instead of the `0x00` the real board shows - that port-`0x64` fix is
  > the real, verified fix, not a HIMEM switch.)
  >
  > `/CPUCLOCK:ON` and `/TESTMEM:off` are unaffected by this and remain fair game.
  >
  > **Consequence for issue #9:** prefer `CTCHIP34` from `AUTOEXEC.BAT` (next section) over any
  > `CONFIG.SYS`/HIMEM change. It sets the same silicon without touching a working A20 path.
- Load `revto486.sys` / `lght486.sys` **before** HIMEM, and use `DOS=HIGH,UMB` - plain `DOS=UMB`
  causes soft-reset failures and hangs that disappear with `HIGH,UMB`.

### The clock multiplier IS reachable without `revto486.sys` - `CTCHIP34` + `IBM486.CFG`

This is the answer to "how do we get clock doubling back when the resident driver halts under
Win95". `CTCHIP34`'s own `IBM486.CFG` (`NAME=486DLC3 ; (oder 486SLC2) IBMs Blue Lightning`,
dated 1993-12-07) documents the register directly:

```
INDEX=1002:3     ; Clock Control Register Byte 3
BIT=5            ; EDFS:   External Dynamic Frequency Shift
BIT=4            ; DFSRDY: Dynamic Frequency Shift Ready
BIT=3            ; DFSREQ: Dynamic Frequency Shift Request
BIT=210          ; Clock Mode Selection
                   000= 1:1 Clock Mode
                   011= 2:1 Clock Mode
                   100= 3:1 Clock Mode
```

So:

```
CTCHIP34 IBM486 /1002h:3:=%%xxxxx011     REM 2x   (%% in AUTOEXEC.BAT, single % at the prompt)
CTCHIP34 IBM486 /1002h:3:=%%xxxxx100     REM 3x
CTCHIP34 IBM486 /1002h:3:=%%xxxxx000     REM back to 1x
```

**Why this matters for issue #9:** `CTCHIP34` is a real-mode EXE that runs from `AUTOEXEC.BAT` and
exits. It is not a `CONFIG.SYS` resident driver, so it sidesteps the Win95 load-time halt that both
`revto486.sys` and `lght486.sys` hit, while still setting the same silicon.

Other BL3 registers worth knowing from the same file:

| Index | Bit | Meaning |
|---|---|---|
| `1000h:0` | 7 | `CE` - **Internal Cache enable** |
| `1000h:0` | 2 | `A20M` - A20 mask. Relevant: this card does A20 its own way (port `0x60`, `0xDF`/`0xDD`) |
| `1000h:0` | 1 | `CPC` - Cache Parity Checking |
| `1000h:1` | 7 | `CNPX` - Cachability of NPX operands (feipoa: set **1**, FPU perf) |
| `1000h:1` | 4 | `XTOUT` - Extended Out instruction (feipoa: set **0**, costs DOOM realtics if 1) |
| `1001h:0/1` | all | `LMCR` - **1 MB Cacheable** region mask, lo/hi byte |
| `1001h:2/3` | all | `LMROR` - **1 MB Read-Only** region mask, lo/hi byte |
| `1001h:4` | all | `CMLR` - 1..16 MB cache memory limit |
| `1001h:5/6` | all | `ECMLR` - extended (to 4 GB) cache memory limit |
| `1004h:3` | 4 | `CLP` - Cache Low Power (disable cache when not in use) |

`1001h:0-3` are the adapter-region cacheability bits - i.e. the same `A000-FFFF` range that Win95's
boot-time memory scan trips over. That is the register-level equivalent of the `EMMExclude=A000-FFFF`
/ `win /d:x` workaround in Technique 68, and the more precise fix if the workaround proves too broad.

Pack location on this machine: `C:\Users\lycet\Downloads\CPU_Register_Control_Apps\CTChip34\`
(`CTCHIP34.EXE`, `IBM486.CFG`, `CTCHIP_English.doc`). Sourced from feipoa's
`CPU_Register_Control_Apps.rar` attached to the vogons thread above.


### Technique 67b: the last 64 KB is a TRADE-OFF, not a missing line of code

Measured 2026-08-24, three states, same disk, `mem_size = 3072`, diagnostics enabled:

| high BIOS window at `0x5F0000` | `bad` | `functional` | shadow self-test | stable |
|---|---|---|---|---|
| ROM-steered, shared buffer **(shipped)** | `64k` | `0k` | **pass** | yes |
| plain RAM, **shared** `bios_shadow_ram` | `0k` | `2048k` | (not reached) | **NO - guest resets** |
| plain RAM, **own** buffer | `0k` | **`2048k`** | **FAIL** | yes |

> **CORRECTED, same session.** I first wrote this up as an unavoidable trade-off. It is not.
> A fourth state - **shared `bios_shadow_ram` + plain-RAM reads on the HIGH window only** - passes
> **both** checks: `bad 0k`, `functional 2048k`, no shadow-failure box, boot continues past the
> driver into ILIM386. It is an 11-line change (one extra read handler, one mapping argument).
> **But it resets the guest intermittently** - roughly half of runs, a few seconds after the driver
> loads, always after the panel has printed. That is a *separate* bug newly exposed by the driver
> getting further, exactly the pattern that produced #7760. Kept as
> `docs/patches/inboard_shared_plainram_alias.patch`. **Root-cause the reset, then ship it** - do
> not treat the tension below as fundamental.

The naive readings of the two checks look like they conflict:

- the **memory diagnostic** needs this window to behave as plain RAM (write a pattern, read it back);
- the **shadow self-test** needs it to share storage with the low `0xF0000` window;
- sharing it *and* making it plain RAM lets the diagnostic's test patterns overwrite the shadowed
  BIOS, which the low window then serves as executable code - hence the reset.

**Do not try to solve this with another buffer** - the own-buffer variant breaks the shadow test,
because the low `0xF0000` view and the high window genuinely must share storage: this file's own
comment records the mechanism as *"Write RAM, Read=PCI/ROM"* - writes always land in the underlying
RAM so the driver can copy the ROM in *before* flipping reads over. Share the buffer, and steer only
the READ. The remaining gap is that this port collapses two
distinct concepts into one flag: `dev->rom_shadow_enabled = dev->speed & 1;` conflates "cache/shadow
enable" with "what the low `0xF0000` view serves". UniPCemu keeps them separate - `inboard_mapF0000`
is tri-state (`0` unmapped / `1` RAM / `2` ROM) and is what decides the low view, independently of
the high windows. Model that, and both checks can pass at once.

**Which state to ship meanwhile:** the shipped one. With Intel's documented `NODIAGS` - the
configuration this project and both reporters actually run - the memory diagnostic never executes, so
`bad`/`functional` are moot and only the shadow test is visible. Without `NODIAGS`, the own-buffer
variant is better for the user (a non-fatal, `NOPAUSE`-suppressible warning beats losing all extended
memory). Judgement call, not a clear win either way - which is exactly why it should not be shipped
silently.

The own-buffer variant is kept as `docs/patches/inboard_split_alias_buffer.patch` so it does not have
to be re-derived.


### Technique 67c: the reset is a PMODE triple fault - and the real bug is the missing 128 KB shift

Root-caused 2026-08-24 by making the reset name itself. There are five distinct paths into
`softresetx86()` in 86Box and guessing between them is hopeless, so tag them:

| file | site | meaning |
|---|---|---|
| `src/cpu/386_common.c` (~1619) | `idt.limit < 35` | real-mode triple fault |
| `src/cpu/386.c` (~341) | double fault aborts again | **protected-mode triple fault** |
| `src/cpu/x86seg.c` (~1553) | `pmodeint` vector > IDT limit | pmode triple fault (other path) |
| `src/cpu/x86_ops_misc.h` (~724) | `hlt_reset_pending` | reset deferred to the next `HLT` |
| chipset/kbc files | port writes | deliberate guest-requested reset |

With the shared+plain-RAM alias fix applied, the answer is unambiguous and repeatable:

```
[resetdiag] PMODE triple fault pc=00000C7D
[resetdiag] softresetx86 pc=00000C7D mask=0
```

Same `pc` on both captures. Not real-mode, not a deferred HLT reset, not guest-requested.

**What this most likely means.** #7760's write-up says the driver "enables paging and lands CR3
above the top of installed RAM". That framing is probably wrong. UniPCemu puts the card's reserved
128 KB at the **bottom** of RAM and adds `0x20000` to every normal RAM access
(`mmuhandler.c`: `translatedaddr += 0x20000;`). This port never implemented that shift, so **every
guest physical address is offset from what the driver expects by 128 KB**. The driver's page tables
then land where we have no RAM, and #7760's all-ones fill makes a bogus page-directory entry read as
`0xFFFFFFFF` - present bit **set**, frame `0xFFFFF000` - so the walk succeeds into garbage, faults,
double-faults, and triple-faults.

If that holds, the 128 KB reservation shift is the single proper fix for the whole family: the
memory diagnostic, the shadow windows, and the CR3 placement all fall out of it. It also means
#7760's scratch page should arguably fault rather than return all-ones - a page of `0x00` (present
bit clear) would surface this class of bug instead of hiding it. Worth raising if a maintainer asks.

**Not yet implemented or verified.** Next step is to model the reservation properly rather than
keep bolting windows on: allocate the 128 KB, shift normal RAM past it, and map the two halves at
`0x5E0000`/`0x5F0000` and `0xC0000`/`0xF0000` as views onto it - which is what UniPCemu does and
what this port has been approximating one window at a time.

### CONFIRMED ON REAL HARDWARE 2026-08-25 - issue #9 CLOSED

CTCHIP34 from AUTOEXEC.BAT replaces revto486.sys/lght486.sys. Booted to Windows, stable, speed
visibly improved. Applied set: `1000h:0=92` `1000h:1=9C` `1001h:0=FF` `1001h:1=03` `1002h:3=03`.
Win95 had been at **1:1 clock with LMCR=0000** - half clock, no cache, nothing cacheable.

Retires the vogons "needs protected mode / V86" theory: REVTO486 1.04 dumps `CR0=0000FFF0` (PE
clear), `CR3=0`, and hooks no INTs or IRQs. It runs in **real mode** - which is exactly why a
run-and-exit EXE can replace it.

Gotchas that each cost a run: `CD` to the CTCHIP dir first (IBM486.CFG is looked up in the CURRENT
directory); run from real-mode DOS not a Win95 DOS box (port 22h/23h is V86-trapped); `&` is the
batch-safe binary prefix.

---

## Technique 69: a config key in a section 86Box does not read is SILENTLY IGNORED - and machine
## config lives under the machine DEVICE's `.name`, not the machine-table name

**This has now produced two confidently-wrong conclusions on this project. Check it FIRST whenever
a config-driven test "rules something out".**

Machine device config is read here (`src/device.c`, `machine_get_config_int`):

```c
const device_t *dev = machine_get_device(machine);
...
return config_get_int((char *) dev->name, str, cfg->default_int);
```

So the `86box.cfg` section header must match **`ibmxt_inboard386_device.name`** -
`[IBM XT (Inboard 386/PC)]`. It is **not** the machine-table `.name`
(`[386DX] IBM XT (Inboard 386/PC)`), and it is not the internal name.

If the header does not match, **every key in that section is ignored and the compiled-in
`default_int` is used instead** - with no warning, no log line, nothing on screen.

### The two times this has bitten

1. **POST 101 / the 1982 ROM.** `bios = ibm5160_050986` was ignored; the machine used the list
   default, which was then the 1982 ROM. See the 1982-ROM section.
2. **POST 1801 / the 5161.** In July 2026 the "phantom 5161" theory was tested by setting
   `enable_5161 = 0`, got an identical result, and was written into
   `INBOARD_86BOX_PORT_PLAN.md` as *"fully ruled out [...] Do not re-test this theory in a future
   session; it's closed."* The key was in a stale section. **The variable was never changed** -
   every "5161 disabled" run still had the 5161 attached. It was the cause all along
   (commit `9ace07f`).

### How to check, in one line

Do not trust the config file. Trust the runtime:

```
grep "\[m_xt\]" stderr.txt
[m_xt] machine_ibmxt_inboard386_init: bios=ibm5160_050986 enable_5161=1   <-- says 0 in the cfg
```

That `fprintf` exists precisely because of #1. **Print the value the code actually received.**

### The trap in general form

A config-driven negative result is only as good as proof the config took effect. "I set X=0 and the
symptom persisted" is worthless without evidence X was read as 0. Prefer changing the **default in
code** for a test - it cannot be silently ignored.

---

## Technique 70: verify the BINARY, not the source tree, before trusting any measurement

`stat` the exe against `git log` on the file you changed. If the exe predates the commit, every
number from that run is from a different program.

```bash
stat -c '%y' 86box_full/build/phase1_mingw/src/86Box.exe
git log -1 --format='%h %cd' --date=iso -- 86box_full/src/device/inboard386.c
```

**2026-08-25:** an entire five-memory-size sweep, plus the claim "#7638 fully fixed", came from a
binary built at 22:50 from `f0480f9` - before the A31 commit `4e28733` at 00:20. The results were
internally consistent and completely meaningless. Cost: most of a morning.

A ninja/cmake build that says `[2/2] Linking` is *not* proof either - confirm which sources it
recompiled.

---

## Technique 71: an auto-keypress harness HIDES POST errors - log what you pressed F1 *for*

Every unattended harness here presses F1 on a timer to clear
`ERROR. (RESUME = "F1" KEY)`. That prompt only appears **because POST reported an error**. For
weeks the harness cleared `1801` before anyone read it, and it was found only when the human
watched an un-harnessed boot.

Rules:

- A harness that clears prompts must **log the screen it cleared**, not just clear it.
- The clean (uninstrumented) build has **no `inject_key.txt` hook** - it does not exist in the
  source. Host-side keys need `AttachThreadInput` to the current foreground thread *before*
  `SetForegroundWindow`; the latter alone is refused across processes and the guest never sees the
  key. See `tools/quiet_run.ps1`.
- **`INBRDPC.SYS`'s memory diagnostic is abortable by a keystroke.** A harness that keeps tapping
  reads out a stopped counter - `functional + bad = 128k` at *every* board size - which looks
  exactly like a memory map and is not one. Send keys only until the BIOS prompt clears, then stop.
- When picking a frame from a capture run, take the **last** frame showing the panel, never the one
  with the most ink: the counters tick upward and "most ink" catches a mid-count value that reads
  like a regression. (See Technique 66b.)

---

## Technique 72: one shared read handler cannot serve two windows with different semantics

`inboard386_bios_shadow_read()` served both the low `F0000-FFFFF` BIOS code-fetch window and the
high `0x5F0000` bank-alias off a single `rom_shadow_enabled` flag. They need opposite answers:

| window | shadowing off | why |
|---|---|---|
| low `F0000-FFFFF` | **ROM** | the CPU fetches BIOS code here |
| high `0x5F0000` | **shadow RAM** | not a code-fetch window - it is the card's bank-alias, used to *load* the shadow and probed by the diagnostic |

Writes were never gated, only reads - so the diagnostic wrote its pattern through the alias, read
it straight back, got ROM, and marked the block bad. `INBRDPC.SYS` then rejects the **entire**
extended pool over one bad block: `functional extended memory: 64k` on a 2048k board.

Fix: branch on `addr >= 0x100000` **before** consulting the flag (commit `4b570de`, upstream
[#7765](https://github.com/86Box/86Box/pull/7765)).

The earlier attempt made the window plain RAM in **both** directions - which also clears the
diagnostic, but breaks the code-fetch path and resets the guest. **When one handler serves two
address ranges, ask what each range is FOR before making them agree.**

### Downstream symptoms worth recognising

A starved extended pool shows up far from the driver:

- `ICACHE /S:256` - *"There is not enough free memory in your system"*
- `ILIM386`/386MAX - *"Total available = 0 KB [...] Memory manager NOT installed"*
- Windows 3.0 [Inboard 386] enhanced mode faulting after the splash screen (`WIN386.EXE` has
  nowhere to build page tables)

If a guest memory manager refuses to install, read the INBRDPC panel before suspecting the manager.

## Technique 73: the Mach8 accelerator has no bank concept in 86Box (open)

**Status: hypothesis, blocked on information.** Recorded so it is not re-derived.

Issue #8 — the Graphics Ultra option ROM prints `14207 7B6A 0007  RAM Addressing` in 86Box where
the real card prints `Testing........Ok`. Reproduced on **stock upstream 86Box**, so it is not
this fork's doing.

**What is known.** @TC1995 (86Box's Mach8 maintainer) clarified that the self-test exercises the
**Mach8 accelerator's** memory, not the VGA side: bitblt tests, then a rectangle fill, then a
PIXTRANS read expecting `0xAAAA` / `0x5555` — an alternating pattern, i.e. an address-line test.

Checking `vid_ati_mach8.c` against that: `mach->bank_r` / `mach->bank_w` are computed from regs
`0xB2` / `0xAE` and used in **exactly one place in the file** —

```c
svga->read_bank  = mach->bank_r << 16;
svga->write_bank = mach->bank_w << 16;
```

— the **VGA aperture**. The accelerator path addresses `dev->vram` directly
(`dev->accel.dest + offset & dev->vram_mask`) with **no bank applied**. If the self-test walks
banks, they would all alias in emulation. That is what "RAM Addressing" detects.

**What is NOT known**, and could not be answered from any source checked:

| Question | Status |
|---|---|
| Which register selects the accelerator-side bank | unknown |
| Bank-relative addressing vs a 64 KB window on `dest` | unknown |
| The exact self-test sequence between stages | unknown |
| Meaning of `0007` in the diagnostic | unknown |

**Sources checked, and what they yielded — do not re-check these expecting more:**

- **Wim Osterholt, *XT, AT and PS/2 I/O port addresses*** — documents the 8514/A port block
  (`02E8`…`E2E8`) but at one line per port. `E2E8` is listed **write-only** ("pixel data
  transfer"); the read side the ROM uses is undocumented. **The word "bank" does not appear in
  the file at all.**
- **dosdays, ATI Graphics Ultra** — confirms the dual-memory architecture ("The VGA controller had
  its own memory, completely separate from the Mach8 accelerator's memory") and the BIOS part
  numbers (`113-11504-002`, Aug 1992 — matches our card). **No aperture, banking or self-test
  detail.**
- **ardent-tool 8514/A page** — states plainly that *"IBM never published the hardware register
  information for the 8514/A"*. **Lead not yet followed:**
  [`8514A_Registers.pdf`](https://www.ardent-tool.com/video/8514A_Registers.pdf), from
  *Harnessing the 8514/A*, MIPS, January 1990 — the one primary register reference identified.

**The rule this illustrates:** when a maintainer says "not that one, this one", re-read the
original wording before checking anything. Two wrong turns here — checking whether the memories
were *allocated* separately (they are; the request said "banks", not "size"), then guessing at
`svga->read_bank`/`write_bank` (still the VGA side). Both were answerable from TC1995's own
sentence.

## Technique 74: prove a driver LOADS before measuring what it does

**Cost of not doing this: three discarded probe runs and a shipped patch that does nothing.**

Issue #3. `HSFLOP.PDR` was patched (`maxPhys 0x1000 -> 0xFF`), deployed to the CF, and md5-verified
on the card. The controller was installed (`PNP0700` in `SYSTEM.DAT`). Three attempts to measure
the DMA page register produced nothing and were each written off as "the probe run was invalid".

The probe was fine. **The driver was never loaded.** One grep would have said so:

```bash
grep -ic hsflop /d/BOOTLOG.TXT        # 0
```

### The check, in order

```bash
grep -i "Dynamic load" BOOTLOG.TXT     # what IOS actually loaded
ls WINDOWS/SYSTEM/IOSUBSYS/            # what was available to load
```

Diff the two. Anything present but absent from the log never ran, and **nothing you measure about
it means anything**.

### The specific signature to look for: a real-mode storage stack

```
Dynamic load success C:\WINDOWS\system\IOSUBSYS
mm.pdr
INITCOMPLETESUCCESS = RMM
```

`RMM.PDR` is the **Real Mode Mapper**. If it loads — especially with `ESDI_506.PDR` *not* loading —
Windows is driving storage through **real-mode BIOS**, not 32-bit drivers. On this machine that is
true for the hard disk **and** the floppy.

Consequences, all of which were being missed:

- Every `.PDR`/`.MPD` patch in `IOSUBSYS` is inert. Patch the real-mode path instead.
- A DMA-reach bug still applies, but to the *real-mode* transfer buffer, not the PDR's
  `_PageAllocate`.
- The absence of `IOS.LOG` is **not** reassurance. IOS writes it for failures it considers
  notable; declining to load a driver whose hardware it does not claim is not one.

**@andrew-hoffman predicted this from the symptom alone** before any of it was measured, and named
the cheaper test: *"If Windows is using real mode access for any disk drives there will be an error
message in the Performance tab of the System control panel."* That is a GUI check taking seconds.
Prefer it to reading `BOOTLOG` when the machine is in front of you.

**Generalises to:** any "the fix didn't work" on this project. Before theorising about why a patch
had no effect, prove the patched code executed. `Patched: N>0` says the file changed; `BOOTLOG`
says the file *ran*. They are different claims.

### Technique 73, addendum 2026-08-25 — architecture settled, register names still open

**[PRIMARY] Michal Necasek, OS/2 Museum, *The 8514/A Graphics Accelerator*:**
<https://www.os2museum.com/wp/the-8514a-graphics-accelerator/>

Two facts that change the shape of issue #8:

1. **The 8514/A is not memory-mapped at all.** *"Unlike MDA/CGA/EGA/VGA, the 8514/A was not mapped
   into the host system's memory space at all and the framebuffer could not be accessed directly."*
   VRAM is reachable **only** through I/O, via `PIX_TRANS`. So @TC1995's "Mach8 RAM banks" cannot be
   a VGA-style memory window, and `mach->bank_r`/`bank_w` -> `svga->read_bank`/`write_bank` is
   definitively the wrong layer. **Do not look for an aperture; there isn't one.**
2. **Memory is plane-organised, and the engine processes 4 or 8 bits at a time depending on whether
   512 KB or 1 MB of VRAM is fitted.** 86Box models this (`config1 |= 0x20` at >=1024; `dev->bpp`
   branches in the `PIX_TRANS` macros).

**FREE UNTRIED EXPERIMENT that follows directly:** run the Mach8 at **512 KB** instead of the 1 MB
default. If the self-test passes at one size and fails at the other, the defect is the 4/8-bit width
handling and **no register documentation is needed at all**. Config change only.

**[PRIMARY] Best remaining sources** (Necasek cites them; IBM never published 8514/A register docs):
- **Chips & Technologies 82C480 datasheet, August 1991** — an 8514/A-compatible chip, so a real
  register-level document. Best lead by some distance.
- *Graphics Programming for the 8514/A*, Richter & Smith, M&T Books 1990.

**Observed directly in our own source** (not from any secondary claim): the accelerator path
already uses `mach->accel.dst_ge_offset` and `mach->accel.dst_pitch` in the `PIX_TRANS` read macros.
Whatever the correct addressing turns out to be, it runs through those, not through the VGA banks.

**Open questions for @TC1995 or the 82C480 datasheet** — unchanged, and still the way to settle it:
which register selects the accelerator-side bank; how it combines with the address; the self-test
stage transitions; and what `0007` means in `14207 7B6A 0007`.

## Technique 75: a stock driver's chipset probe is a destructive write on an XT - audit
## fixed-port I/O before installing anything

**CONFIRMED on real hardware 2026-08-28**, root-causing issue #22 (LS-120 install killed keyboard
input). Write-ups: `docs/xt_io_aliasing_gotcha.md` (shareable) and
`docs/ls120_keyboard_root_cause.md` (detail).

### The measurement - this is the load-bearing fact

COMrade (Kevin Moonlight; Win95 port by Ahmad Byagowi), read-only, at the DOS prompt on the 5160.
**This measurement is only possible because that tool exists** - no emulator models the alias:

| port | `0x21` | `0x23` | `0x25` | `0x31` | `0x3F` |
|---|---|---|---|---|---|
| value | `0xAC` | `0xAC` | `0xAC` | `0xAC` | `0xAC` |

### CONFIRMED BY PRIMARY SOURCES 2026-08-29 - this is documented, not a discovery

The measurement stands, but it did not need to be a measurement. Two sources, both from
@andrew-hoffman:

- **IBM PC/XT Technical Reference (6280089, MAR86)** - its own I/O Address Map lists *ranges*:
  8237A `000-01F`, **8259A `020-03F`**, 8253 `040-05F`, 8255 `060-06F`, DMA page `080-09F`.
  Eight 32-byte blocks covering the `000-0FF` system-board reservation.
  <https://archive.org/details/IBMPCXTIBM51555160TechnicalReference6280089MAR86>
- **DubaiXTClone** KiCAD schematics - the I/O decoder's nets are `XA5 XA6 XA7 XA8 XA9` + `~AEN`
  in, `~DMACS ~INTRCS ~T/CCS ~PPICS ~WRTDMAPGREG ~WRTNMIREG` out. **`XA0`-`XA4` never reach it.**
  <https://github.com/spencer-uk/DubaiXTClone/>

Consequence for 86Box: `pic.c`'s `io_sethandler(0x0020, 0x0002, ...)` contradicts IBM's published
map. That makes the upstream fidelity report a documentation citation, not a claim about our board.

Checked and unhelpful for this question: **NuXT** uses the Faraday FE2010A, so the decode is inside
an ASIC - no discrete decoder to read.

**The 8259 answers across the whole `0x20`-`0x3F` block**, `A0` alone selecting command vs data.
`out 23h,al` *is* `out 21h,al`. The other XT blocks alias the same way (8237, PIT, PPI, DMA page).
**The DMA page registers are write-only** - reading returns `0xFF`, so that alias cannot be measured
non-destructively. Do not test it by writing.

### The bug class

AT-era drivers probe for host chipsets by writing a known value to a config port pair and reading
it back. On an XT the alias returns what was written, so **the probe always succeeds**, the driver
concludes a chipset is present, and its configuration writes reprogram the interrupt controller.

`SD120PPD.MPD` does this three separate ways:

- **`0x22`/`0x23`** - config writes leaving the mask at `0x06` (IRQ 1 + IRQ 2 masked), no restore.
  `push eax`/`pop eax` preserves the *register*, not the *port*.
- **`0x24`/`0x25`** - a second pair; `mov al,61h / out 24h / in 25h / or al,01h / out 25h` masks
  **IRQ 0, the timer**. Another site ORs `0x08` (IRQ 3).
- **`0x94`** - PS/2 system-board setup register, written `0x7F` then `0xFF`; on an XT that is inside
  the DMA page block.

**Diagnostic signature: one interrupt-driven device dies and the others do not.** Keyboard gone,
serial mouse fine, no error anywhere. Read the 8259 mask before touching the driver stack.

### Three traps that cost time here

1. **A raw byte scan is not evidence.** `E4`-`E7` occur constantly in data and mid-instruction. The
   first scan reported 283 candidates; 29 were real. Confirm each by its idiom (`cli`, `mov al,imm8`,
   `eb 00` I/O delays, `push`/`pop`).
2. **Do not only look for `E6`.** 19 of the writes were **word-width** `E7` (`66 e7 22` =
   `out 22h,ax`), which on an 8-bit bus splits into two byte writes and hits *both* registers of the
   pair. Worse, not lesser.
3. **86Box cannot reproduce any of this.** `src/pic.c` maps the XT PIC as
   `io_sethandler(0x0020, 0x0002, ...)` - two ports, no alias; only `pic_init_pcjr()` maps eight. A
   clean emulated run proves nothing. Inverts this project's usual "test in emulation first" rule,
   and is itself a fidelity gap worth reporting upstream - but measure on real hardware first.

### The tools

- **`dist/post-install-fixes/scripts/xt_port_audit.py`** - scan any `.MPD`/`.VXD`/`.PDR`/`.SYS` for
  fixed-port writes landing in the XT device blocks. Confidence-scored. Run it **before** installing
  anything. Cannot see `DX`-addressed I/O; nothing static can.
- **`dist/post-install-fixes/scripts/patch_sd120ppd_chipset.py`** - NOPs all 98 offending writes
  across `0x22`/`0x23`/`0x24`/`0x25`/`0x94`, byte and word forms. Leaves reads (harmless, and
  detection then finds nothing - what `/ni` does in the DOS build). Leaves `out 21h` alone: those
  are paired save/restore and already neutral.

### Deploying to the CF: two traps that both produce a SILENT no-op

**2026-08-29. Between them these voided a real test result** - a "v4 didn't fix it" was reported to
the owner and written up, when the card still held v3. Check both before believing any negative.

**1. A `.BAT` written from bash is LF, and `COMMAND.COM` cannot parse it.** The symptom is the
batch printing `OFF` (from `@ECHO OFF`) and then doing nothing. `REGEDIT` tolerates LF, so a `.REG`
written the same way imports fine - **do not infer one from the other.** The repo's
`.gitattributes` already forces CRLF for `*.BAT` (Andrew's suggestion, issue #3); writing straight
to the card bypasses it entirely. Convert explicitly:

```python
raw = open(p,'rb').read()
if b'
' not in raw:
    open(p,'wb').write(raw.replace(b'
', b'
'))
```

**2. A driver in `IOSUBSYS` is open and locked while Windows is running.** `COPY` over a loaded
`.MPD`/`.PDR`/`.VXD` fails with a sharing violation, and a batch window closes before anyone reads
the error. Two routes that work:

- **from the host**, CF in the card reader - the reliable one, and it lets you verify the md5 in
  the same breath;
- **from real-mode DOS** - `F8` -> *Command prompt only*. On this machine that is available even
  when the fault under investigation has killed the keyboard under Windows, because the keyboard
  works until the miniport loads.

**The rule that catches both:** after any deployment, `md5sum` the file *in its destination* and
compare against the artefact you meant to ship. Not the staging copy - the destination. This is
Technique 74 applied to your own tooling, and the tooling is exactly where it is easiest to skip.

### Always round-trip a binary patch before deploying it

`--revert` must reproduce the original md5 **exactly**. Two attempts here did not, and both bugs
would have been invisible on inspection: the site map recorded the offset but not the original
opcode (so byte vs word forms were guessed), and then the revert restored the opcode byte while
leaving the port byte as `0x90`. Record every byte you change, and prove the round-trip.

### Read the vendor's README before disassembling the vendor's binary

**2026-08-29, and it cost two patch rounds.** The Imation package ships a 166-line `README.TXT`.
It had been on the CF at `D:\SD120PPD\README.TXT` since 2026-08-23 - **six days before anyone
opened it** - and the installer drops a byte-identical copy into
`C:\Program Files\Imation\SD120PPD\`. In that time this project disassembled the miniport three
times, wrote two binary patches, and deployed both to real hardware.

The README documents the supported way to do what the patches were trying to force:

> On machines like the NEC 9800 series, the MPD requires to be explicitly supplied with the parallel
> port IRQ and port base values. In the Device Manager [...] select the Settings tab, and type the
> port base and IRQ values as follows: `PORT=0xYYY IRQ=Z`. The 0x prefix is mandatory.

It also documents how to stop the driver loading at all (Device Manager -> Properties -> uncheck
**Original Configuration [Current]** -> reboot, red X appears) - the exact bisection test that was
otherwise going to be improvised.

**Two conclusions I had already published were wrong because of this**, both stated as fact:
`[DriverSettings] Flags=""` dismissed as dead boilerplate, and "the miniport binary contains no
switch-parsing strings at all". The second came from grepping for `/ni`-style switch text; the real
format is `PORT=`/`IRQ=`, the strings `'port'` and `'irq'` are right there at `0x010674`/`0x0106b4`,
and the device node already carried `AdapterSettings = " /W95"` put there by the installer.

**The rule:** a vendor `README.TXT`, `.HLP` or `.DOC` shipped alongside a driver is a **primary
source about that driver**, in the same category as `BOOTLOG.TXT` under Technique 55. Read it before
any static analysis. Grepping a binary for the strings you *expect* is not a substitute - it only
finds the mechanism you already guessed at, and a confident negative from that search is worse than
no search, because it closes the question.

Corollary that also held here: check whether the package you are about to reverse-engineer is
**already staged somewhere on the machine** from an earlier session. It was.

### And check the DOS driver for switches first

A DOS driver often has a documented escape the Windows driver lacks. `SD120PPD.SYS` carries its own
help text in its strings: `/ni` "Skip chipset initialization", `/de`, `/db`, `/sf`, `/dp`, `/fp` -
all in use on this machine. The miniport exposes none of them, no `AdapterSettings`, nothing. Pull
the strings from the DOS binary: it documents which probes are optional and what safe looks like.

## Technique 76: two diagnostic routes that do not exist on this machine - stop proposing them

Both were proposed on 2026-08-28 and both are dead ends. They look obvious, which is why they keep
coming back.

**1. Safe Mode is unavailable.** Windows 95 Safe Mode skips `CONFIG.SYS`, so `INBRDPC.SYS` never
loads and the Inboard card is not initialised. `CLAUDE.md` already says never to treat
`INBRDPC.SYS` as an optional test variable; **Safe Mode is that rule in disguise.** So the standard
"boot clean and bisect" move is not available here.

*What works instead:* **F8 -> Step-by-step confirmation.** It walks `CONFIG.SYS` line by line, so
`INBRDPC.SYS` can be accepted and everything else declined. It only bisects the DOS side, so it is
useless for a Windows-side fault - but it is the one bisection route that exists.

**2. COMrade and the mouse compete for the serial port.** Running COMrade means the serial mouse is
disconnected. So under Windows you have **either input or introspection, never both**:

- Keyboard broken + COMrade attached = no way to drive the GUI at all.
- Keyboard broken + mouse attached = can drive Device Manager, but cannot read a port.

This is why "just launch `COMR95.EXE` with the mouse and read the PIC mask" is not a plan. Any
Windows-side measurement has to be arranged **before** the fault is triggered, or captured to a file
that can be read later from DOS.

**Consequence for planning:** on this machine, a Windows-side fault that costs you keyboard input is
close to undiagnosable in place. Get the evidence written to disk during the failing boot - `IOS.LOG`
and `BOOTLOG.TXT` survive a reboot and can be read from DOS afterwards - or accept that reverting to
a known-good image is cheaper than diagnosing. That is a legitimate outcome, not a failure.

**And bisect installs.** The 2026-08-28 keyboard loss was unattributable because DirectX 7.0a,
WinZip, InfoPro, SIV and the LS-120 driver all went on between working boots. One install, one boot.
## Technique 77: when a bisect proves WHICH component, stop patching and cost the target

**2026-08-29, issue #22. Six binary patches, six nulls, on a convenience feature.** The record is
worth keeping because every individual step was defensible and the sequence as a whole was not.

### The one test that worked, and why it took all day to reach

The decisive bisect was **renaming the driver out of `IOSUBSYS`**. `SD120PPD.MPD` -> `SD120PPD.MP_`,
nothing else touched: keyboard returned instantly, drive gone. Device node, registry, `Program
Files` payload and IOS binding all exonerated in one boot, with a one-character rename and no
reinstall.

It took all day because the two *obvious* routes were both blocked - Device Manager had no "Original
Configuration [Current]" checkbox for this device, and *Remove* costs a reinstall. **A file rename
from the card reader was the third route and nobody thought of it.** When the GUI will not let you
disable a component, take its file away instead.

### What six patches bought, and what they cost

Eliminated by measurement, which is real value: chipset/PIC config writes; PS/2 DMA arbitration at
`0x94`; a **Micro Channel double-registration** (`DriverEntry` calls `ScsiPortInitialize` twice, the
second with `AdapterInterfaceType=3` on a machine with no MCA bus); SCSIPORT **polling**;
`AdapterSettings` (`HwFindAdapter` hardcodes and probes all three LPT bases, so `PORT=` is a hint,
not an override); VMM32/VKD replacement; registry changes; IRQ conflicts; and a second VxD that
ships 27 destructive-write candidates and **never loads**.

The cost was a day on a parallel-port floppy that already worked under DOS. **Both the owner and
@andrew-hoffman independently asked "why so much focus on this driver" within an hour of each
other.** They were right. The rule that was missing:

> **Once a bisect tells you WHICH component is at fault, price the component before hunting the
> mechanism inside it.** "We know exactly where the bug is" is the moment to ask whether the fix is
> worth having - not a licence to keep going. A confirmed cause on a low-value target is a good
> place to stop.

### Amendment: "cost the target" is not "stop at the feature's value"

The rule above is about **making the choice explicitly**, not about always stopping. The project
owner's own framing, on [#22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22#issuecomment-5464167418),
and it is the better statement of it:

> It's not explicitly about getting the ls120 working. I know it does and DOS real mode driver is
> sufficient [...] But understanding why it broke, this can lead to better understanding break
> points, weak areas in the build and things that could bite us with other driver attempts [...]
> if I can build a none real mode driver later using the compare between the driver and knowing the
> differences I have potentially the capability to fix it properly.

So a low-value **feature** can still be a high-value **probe**. The LS-120 hunt produced the DX-addressed
I/O gap in our own audit tool, the CREG checksum rule, two silent deployment traps, the vendor-README
rule, and the proof that polling is not what kills the keyboard - which is what de-risked `T130.MPD`
for #19. None of that came from the feature.

#### The receipt, 2026-08-30 - one day later, on a different driver

The paragraph above was a claim when it was written. It is now evidenced. Phase 0 of the XT-IDE
port driver (#21) reached "IOS loads and calls our code on real hardware" in a single session, and
almost every step that went right came out of the LS-120 diversion:

| from #22 | used in #21, one day later |
|---|---|
| Renaming `SD120PPD.MPD` to `.MP_` | The free A/B that proved **IOS binds port drivers to device nodes, not by scanning `IOSUBSYS`**. `SCSIPORT` left the `INITCOMPLETE` list when its miniport did. This killed a null result that would otherwise have looked like "our driver does not load" |
| IRQ 1 lost to a driver that had no business touching it | Why the DDK sample's `Port_Set_IRQ_Handler` was **removed before the file went near the machine** - on a devnode with no IRQ it registers zero, i.e. the system timer |
| The LS-120's `OEM0.INF` as a binding pattern | Copying it was **wrong** (SCSIAdapter is for `.MPD` miniports under SCSIPORT) - but having it to compare against `MSHDC.INF` is what got the class right, `hdc`, before install rather than after |
| `AdapterSettings` on the LS-120 node | One of the two precedents for making the new driver's **transport selectable by registry override** instead of hardcoded |
| Technique 75, port aliasing | Why the COMrade pass stayed **read-only**, and why re-measuring the `0x21`/`0x23`/`0x25` alias was worth doing |
| md5 at the destination; CRLF on `.INF`/`.BAT` | Both applied to tonight's deployment. The md5 check is what proved the INF's own `CopyFiles` installed the driver rather than an earlier hand-drop |
| "Polling is not what kills the keyboard" | De-risked `T130.MPD` (#19) *and* the XT-IDE driver, which must poll - the card is jumpered without an interrupt |

**The lesson to carry forward is not "always keep digging".** It is that the owner named the switch
out loud - *this is a probe, I am buying knowledge* - and that made a day on a parallel-port floppy
a deliberate purchase rather than a sunk cost. Name which one you are doing, and a diversion can be
the cheapest infrastructure you ever build.

**What the rule actually asks for:** say out loud which one you are doing. *"This is a probe, I expect
no fix, I am buying knowledge"* is a legitimate answer and changes nothing about the work. What is not
legitimate is drifting into a sixth patch while implicitly believing you are still chasing a fix. The
failure on 2026-08-29 was not the investigating - it was never naming the switch.

Corollary the owner also makes, and it is right: **real hardware yields data the emulator structurally
cannot.** This exact bug is invisible in 86Box because `kbc_xt.c` auto-clears the XT keyboard latch
after ~50 ticks when nothing acknowledges it (see Technique 37) - real XT hardware has no such escape.
A session that only ran in emulation would have concluded there was no bug at all.

### Measure whether a patch changed anything, not just whether it deployed

A logged boot (`F8` -> Logged) gives per-driver init cost straight from `BOOTLOG.TXT` timestamps:

```
Initing hsflop.pdr    -> Init Success     7 ticks
Initing sd120ppd.mpd  -> Init Success   895 ticks     (whole Windows load ~1790)
```

That one driver was **half the Windows load**. The final patch aimed at exactly that, and re-measuring
gave **896 ticks** - verifiably deployed, and inert. Without the re-measurement it would have been
filed as "tried it, did not help", implying the theory was tested. It was not; the patch never
altered the behaviour it targeted. **Diff the measurement across the patch, not just the md5.**

### Cheap eliminations that should come first next time

All of these are minutes, need no patch, and each killed a live theory:

- **Diff the config files against a known-good image.** `CONFIG.SYS`/`AUTOEXEC.BAT`/`SYSTEM.INI`/
  `WIN.INI` came back identical bar two REM'd lines, collapsing the problem to one variable.
- **String-diff `SYSTEM.DAT` against the same image.** 70 additions, none keyboard-related - killed
  every "the installer changed something else" theory at once.
- **md5 `VMM32.VXD` against the pre-install image.** Killed the best-fitting theory of the day (a
  recombine replacing our custom `VKD.VXD`) in two minutes.
- **Read the Device Manager IRQ list.** Free, and it showed IRQ 1 correctly held with no conflict.


---

## Technique 78: time the failure before explaining it, and re-read every port address after a call

Phase 1 of the XT-IDE port driver (issue #21) returned `Init Failure port.pdr` on the real 5160.
Three causes were written down in advance, each with its own check. **The bootlog settled it before
the machine was touched again**, and the answer was none of the three.

### The measurement that did it

```
[00109829] Initing port.pdr
[0010982B] Init Failure port.pdr      <- 2 ticks
```

The driver's status polls are `XT_SPIN = 400000` iterations of `in al,dx`. On a 4.77 MHz 8-bit bus
that is roughly 0.6 s. The same log calibrates a tick: `sd120ppd.mpd` took 895 ticks and was half the
Windows load (~1790 ticks total). **One timeout would have cost tens of ticks; the whole failure cost
two.** No spin loop ran. That eliminates "the drive never went ready" outright, and it also
eliminates every explanation in which the code reached a transfer.

Cost a loop in ticks before accepting a timeout as the story. It is arithmetic, not machine time.

### The actual bug: EDX survived a call it was documented not to survive

```asm
	movzx	edx, bx
	add	edx, XT_DRVHD		; edx = base+6, Drive/Head
	out	dx, al			; select master

	call	XTIDE_WaitNotBusy	; "Uses: EAX, ECX, EDX"
	jc	xti_fail

	in	al, dx			; <-- reads base+0Eh. Alternate status.
	test	al, 10h			; "DEV bit as selected?"
	jnz	xti_fail
```

`XTIDE_WaitNotBusy` returns with `EDX = base + XT_ALTSTAT`. The read-back sampled alternate status,
**measured at `50h` the day before**. `50h XOR A0h = F0h`, bit 4 set, fail - on both transports, in
microseconds.

What made it survive review is that the wrong port returns a *plausible* value: **DSC in alternate
status occupies the same bit as DEV in Drive/Head.** A garbage read would have been obvious. A
neighbouring register that happens to alias the exact bit under test is not.

- A helper's "Uses:" line is a contract you have to honour at every call site, not documentation.
- Reload the port address after any call, even one that looks like it only reads a status bit.
- When a bit test fails, print the whole byte before theorising. `50h` names its own port.

### A reference driver proves the part it proves, and no more

`drivers/xtide_cdrom/xtidecd.asm` (OBattler's XT-IDE port, via @andrew-hoffman) gave three things:

| what | verdict |
|---|---|
| Port map `dw 320h..327h, 32eh` - alt status at **base+0Eh** | Confirms ours. Use it. |
| Write order: `+8` (high) before `+0` (low, commits the word) | Confirms ours. Phase 2 depends on it. |
| `ATARepInswXT` / `ATARepOutswXT` - the transfer loops | **Broken. Do not copy.** |

The read loop does `in al,dx` / `add dx,8` / `mov es:[di+1],al` - it advances DX to the high latch and
never issues the second `in`, so every high byte is a copy of the low. The write loop has the mirror
bug (`mov ds:[si+1],al` stores instead of loads). That is the byte-duplication signature, and it is
why the shipped binary is the broken one.

A source can be authoritative about a register map and wrong about the code around it. Say which half
you are relying on.

### Corollary: a check whose only outcome is a silent failure costs information

The Drive/Head read-back exists to tell an absent slave from one that mirrors the master. Applied to
the master, in a phase with no readback channel, it could only ever turn a working probe into an
unexplained `Init Failure`. It is now slave-only. **Before adding a test to code you cannot instrument,
ask what you will do with a failure you cannot see.**

## Technique 80: after TWO failed fix-and-run cycles, stop fixing and start bisecting

2026-09-01, phase 2c of the XT-IDE port driver. A Windows protection error the moment the driver
claimed a device. It took **six emulator runs**, of which one was a bisect and five were guesses.
The one bisect - build the same driver claiming nothing - answered the question the five guesses
were groping at, in a single run.

Each run here costs six minutes and a boot. A guess tests one hypothesis out of an unknown number.
A bisect halves the space whatever the answer is. **The moment a second fix-and-run fails, the
next run must be a bisect, not a third fix.** Write the switch that makes bisecting cheap - here it
was `build.ps1 -ClaimMask`, one integer - *before* you need it.

### The four things that were guessed at, and what each actually needed

| guess | what would have settled it, first time |
|---|---|
| INF rejected: line endings, then the install path | diffing against `MSHDC.INF` in the same install |
| protection error: stack args, sample's `mov esi,edi` | a claim-nothing bisect |
| DCB fields to fill at inquiry | `NEW95DOC/STORAGE.DOC`, which documents it in one paragraph |
| offsets of `DCB_apparent_*` | reading `dcb.inc` - they are in a **different structure** |

Three of those four are "read the thing that already exists". The DDK ships design guides
(`DOCS/DESGUIDE/`, `NEW95DOC/`) and a second sample (`BLOCK/SAMPLES/MINIPORT`) that went unopened
for a whole session. This is Technique 75's vendor-README rule again, one level up: **the DDK's own
documentation is a primary source about the DDK's own sample.**

### Two structures with a shared prefix will silently accept each other's offsets

`dcb.inc` defines `DCB_COMMON`, `DCB_BLOCKDEV` and `DCB`. `DCB_apparent_sector_cnt` belongs to
`DCB_BLOCKDEV` - the Int13h/BlockDev view - and `DCB_actual_sector_cnt` to `DCB`. MASM resolves
`[esi].DCB_apparent_sector_cnt` against whichever structure declared that name, applies it to
whatever `ESI` holds, and assembles cleanly. The write lands on `DCB_bus_type` / `DCB_scsi_*` /
`DCB_inquiry_flags` and the machine dies later, somewhere else.

`DCB_apparent_blk_shift` is safe only by luck: it is in `DCB_COMMON`, which is the first field of
`DCB`, so its offset happens to be right.

**Before writing any field of a DDK structure through a pointer, confirm which STRUC declares that
field.** `grep -n "STRUC|ENDS|<field>" the .inc` is one command and it is not optional.

### And do not hand-write stack-offset arithmetic

The same session wrote `push ebx / push edi / push eax` then read the pushed value back as
`[esp+8]` - which is the saved `EBX`. It then used it as an array index and copied from the
resulting wild pointer. That code existed *specifically* to avoid a register-clobber bug class
(technique 78), and introduced a worse one.

If a value is still live in a register, use the register. If it is not, spill it to a **named
module variable**, not to the stack. This file already holds every port address in its own
variable for exactly this reason; the rule applies to arguments too.

## Technique 79: a write/read-back self-test verifies the TRANSPORT, not the ADDRESS - check the
## disk from outside, for the exact pattern at the exact block on the exact device

Phase 2b of the XT-IDE port driver (#21) wrote a known pattern to LBA 100, read it back, compared
512 bytes, and reported success. The pattern had gone to **the boot disk**, not the scratch disk it
was aimed at.

`XTIDE_ProgramTaskfile` built Drive/Head from `DRVHD_LBA` (`0E0h`), which has the DEV bit **clear**.
`XTIDE_TryIdentify` had selected the slave; the taskfile programmer silently re-selected the
master. Both halves of the round trip went to the same wrong drive, so the comparison passed.

**A round trip cannot detect a wrong address, because the error is applied twice and cancels.** It
also cannot detect a wrong *device*, a wrong *partition offset*, or an off-by-N in a block
translation. What it does prove is the data path: bit order, latch order, transfer length.

The check that works costs one command and needs no guest cooperation:

```bash
python -c "
exp = <the pattern>
for name in ('scratch.img','boot.img'):
    d = open(name,'rb').read()
    i = d.find(exp)
    print(name, ('LBA %d' % (i//512)) if i>=0 else 'not present')"
```

Two images, not one - *absence* from the disk you did not mean to touch is half the result.

**Corollary for any self-test in this project**: state what the test cannot see, in the comment,
next to the test. The write self-test's own comment correctly claimed it would catch a high/low
latch swap - and it does, because the read path is separate code making a different mistake. It
said nothing about addressing, which is where the bug was.

**And keep the rollback.** `tools/pdr_loadtest.ps1 -Restore` copies the image back from
`xtide_base.img` before every run, so a driver that writes to the wrong disk costs one file copy
rather than a Windows reinstall. Build that before the first run that writes, not after.

### Technique 78, addendum 2026-08-31 — two registers that must alias, disagreeing, kills a map

The DOS probe (`drivers/xtide_pdr/tools/XTPROBE.BAS`) died on an unrelated QBASIC bug, but its first
line had already invalidated the driver:

```
taskfile sweep +1=0 +2=0 +3=0 +4=0 +5=0 +6=92 +7=92 +8=8F +9=8F +A=1 +B=1 +C=E0 +D=E0 +E=50 +F=50
```

Under the map the driver was written to, `+07` is **Status** and `+0E` is **Alternate Status**. Those
are the same register in every ATA device. They read `92h` and `50h`. **One contradiction, and the
whole map is wrong** - no further measurement needed to know that, whatever the truth turns out to be.

Look for a pair of addresses your map claims must agree, and read both. It is one `in` each, and it
falsifies an entire assumption rather than one register.

The pairing `(6,7) (8,9) (A,B) (C,D) (E,F)` and the sane values on a two-byte stride - `+0E=50h` is
`DRDY|DSC`, `+0C=E0h` is `LBA|master`, `+06/+08/+0A` are a plausible boot LBA - say A0 is not decoded.
**Inference, not yet measurement.** v2 of the probe settles it by writing distinct values to `+02..+05`
and reading the window back, which residue cannot fake.

### The crash site was the diagnosis

QBASIC raised `Overflow` on `FOR t = 1 TO 60000` - `DEFINT A-Z` had made the counter a 32767-max
integer. But the loop only ran to exhaustion **because the IDENTIFY opcode had gone to the sector
number register and been ignored**. A trivial language bug and the real fault surfaced at the same
line. Read where a crash happened before fixing why.

## Technique 81: when a DDK sample misbehaves, diff it against a driver that WORKS before
## theorising about the packet you are handing the OS

2026-09-01. Phase 2c of the XT-IDE port driver (#21) threw a Windows protection error the moment
it claimed a unit. A handover listed five suspects inside the ISP calldown-insert packet, ranked,
with the "most likely single cause and the cheapest to fix" first. **All five were wrong, and the
top one was wrong on a checkable fact** - it claimed the sample filled "only five fields" of
`ISP_calldown_insert` and left the rest as stack garbage. It fills all seven.

The whole space was closed by reading `zikolas/cfu1-win9x`'s `ios_config_dcb` - a **working**
Win9x IOS port driver doing the same `ISP_INSERT_CALLDOWN` - and comparing member by member.
Every value matched. That took minutes and would have cost five emulator runs to establish by
bisection.

The two real bugs were both visible in the same diff, as things the reference does and the sample
does not:

1. **The sample's request routine destroys the callee-saved registers its own header promises to
   preserve.** `Port_request`: *"Destroys: C call compatible, can only destroy eax, ecx, edx"* -
   and it clobbers EBX, ESI and EDI. `CFU1_IosReq` pushes and pops all of them. Harmless in a
   skeleton that is never called; a protection error the first time the OS calls it.
2. **It splices itself into every DCB the OS broadcasts**, with a header claiming it *"examines a
   DCB to determine if we want to work with this device"* and no examination anywhere in the body.
   The reference keeps a table of the DCBs it claimed and refuses a second insert on one, with a
   comment saying re-broadcasts loop back.

**The rules:**

- **A vendor sample is a skeleton, not a driver.** Its unexercised paths have never run, so its
  bugs are exactly the ones that surface the moment you make it do real work. Treat every comment
  in it as a specification the code may not meet - both bugs here are the header comment and the
  body disagreeing, in plain sight.
- **A working third-party implementation outranks the vendor sample and the vendor docs as
  evidence**, because it has been executed. Find one before designing an experiment.
- **A ranked suspect list from a previous session is a hypothesis, not a finding** (Technique 7).
  Check the cheapest falsifiable claim in it first - here, "only five fields are set" was
  refutable by counting `mov` instructions.
- **Read what a bisect actually isolates.** `-NoIo` was recorded as exonerating "the transport and
  the request path". It stubbed only the transport; the handler still walked the IOP and unwound
  the callback stack - which is where bug 1 lives. A bisect switch's scope belongs in the same
  note as its result.

### Technique 81, CORRECTION 2026-09-01 - the fix above was never exercised

**The claim "the protection error is fixed" was wrong, and the way it was wrong is the lesson.**

Both bugs in Technique 81 are real. But the clean boot that "confirmed" them came from a build in
which a *second* bug - introduced in the same commit - stopped the code path running at all. The
new `XTIDE_WantDcb` guard identified "our" DCB by comparing the pointer against the one seen at
`AEP_DEVICE_INQUIRY`. `NEW95DOC/STORAGE.DOC` says the IOS "creates a real DCB and fills it with
information supplied by the driver" once config completes - so the config-time DCB is a different
object at a different address, and the guard declined our own device every time.

The calldown was therefore never inserted. That build was, functionally, the `-NoCalldown` bisect
which was **already known** to boot clean. `Init Success` plus a desktop proved nothing new, and
was reported as a fix.

With the guard corrected to test `DCB_unit_on_ctl` instead of the pointer, the insert succeeds
(`ISP_result 0`) and the Windows protection error returns.

**The rule: a green result is only evidence if the thing you changed actually ran.** Technique 74
says prove a driver LOADS before measuring what it does; this is the same rule one level down -
**prove the specific code path executed before crediting a fix with the outcome.** The tell was
available and ignored: the passing run's tick count (523) was *identical* to the `-NoCalldown`
run's. Two builds that are supposed to differ, agreeing to the tick, is a warning, not a
reassurance.

**Corollary on guard clauses.** A guard added to make code safer is itself a new failure mode, and
its failures are silent by construction - it does nothing, quietly, which is indistinguishable
from having nothing to do. Any guard that can reject work needs a counter for how often it
rejected, wired at the same time as the guard. Here that was five minutes of work and it is what
eventually produced `cfg=1 claimed=0` and named the bug in one line.

### The measurement channel that settled it - build this FIRST, not third

A driver with no debug output can still report to the host: **have it write a marker sector to a
scratch disk, and read it from outside the guest** (technique 79's discipline, used for
instrumentation rather than verification). `-ReqMarker` in `drivers/xtide_pdr/build.ps1`, read back
by `tools/pdr_reqmarker.py`. One 512-byte sector on the claimed unit carries a counter per stage:

```
AEP_CONFIG_DCB calls   1     <- IOS offered us a DCB
...claimed by us       1     <- our guard accepted it
ISP_result of insert   0     <- IOS accepted the calldown
Port_request entries   0     <- the calldown was never called
XTIDE_StartRequest     0     <- ...so nothing reached the transport
```

Five stages, one boot. The first attempt instrumented only the last stage, got a null, and cost
two further runs to find out the null was three stages upstream. **Counters at every stage of the
path cost the same single run as a counter at the end of it.** Write the whole ladder first.

Two mechanics worth copying:

- **Address the marker at the unit YOU claimed, never at the unit named in the DCB you are
  inspecting.** A DCB you are about to decline belongs to someone else, and its `DCB_unit_on_ctl`
  will aim the write at their disk - which on this machine is the boot volume.
- **Clear the marker sector from the host before every run, and check every image afterwards, not
  just the one you aimed at.** Nothing in the harness resets a scratch disk, so a stale marker
  reads exactly like a fresh success (technique 23's stale-log trap, in a new place).

### Geometry belongs in AEP_CONFIG_DCB - and that is a separate change from the one above

`STORAGE.DOC`: the driver processes `AEP_CONFIG_DCB` "by setting the various members of the DCB
structure to values specifying geometry information, such as cylinder count and sector count".
Inquiry sets `DCB_product_id`, `DCB_vendor_id`, `DCB_rev_level` and nothing else. Written at
inquiry, geometry describes a DCB the IOS then replaces, so the disk TSD sees a zero-length device
and declines it - zikolas/cfu1-win9x carries a comment saying exactly that, from having hit it.
Moved to `XTIDE_ConfigDcb`. **It was not the blocker**, and was shipped in the same run as the
guard fix, so it has no independent evidence behind it yet - recorded as documented-correct, not
as measured.

## Technique 82: a macro-built stack frame can be silently omitted - check the LISTING, because
## the source looks correct either way

The Windows protection error that cost 2026-09-01 was this, in Microsoft's own DDK port-driver
sample:

```asm
BeginProc	Port_request
ArgVar	IOP_ptr
	EnterProc
        mov     ebx, IOP_Ptr         ; point to the iop.
```

That is textbook, and it is broken. `VMM.INC`'s `EnterProc` only emits `push ebp / mov ebp,esp`
when `??_pf_ArgsUsed` is **already** set - and that flag is set by *referencing* an ArgVar, which
happens on the next line, after the macro has expanded. Outside a `DEBUG` build the prologue is
therefore never emitted, and `IOP_Ptr` resolves to `[ebp+8]` against **whatever EBP the caller left
in the register**.

The listing says it in one line:

```
Port_request proc near
        EnterProc                        <- no bytes emitted
00000003  8B 5D 08    mov ebx, IOP_Ptr   <- mov ebx,[ebp+8]
```

Offset 3, and the only three bytes before it were this project's own `push ebx/esi/edi`. **A
prologue that is not there occupies no bytes, so its absence is invisible in the source and obvious
in the listing.** `ML -Fl<file>` costs one command and no machine time.

Downstream, the garbage pointer read as an IOP gave `IOP_callback_ptr = 0`, and the sample's
completion path does `sub eax, size IOP_CallBack_Entry` then `call dword ptr [eax]` - a call
through `[0FFFFFFF8h]`. That is the protection error, four layers from its cause.

**Rules:**

- **Read the listing for any routine an external caller enters.** Not the source. Prologue and
  epilogue are macro-generated in this codebase and both can vanish on a build-flag condition you
  did not know existed.
- **Prefer an explicit `[esp+N]` load over a macro ArgVar** in a routine called by the OS. It cannot
  be silently rewritten by a macro's internal state, and the offset is checkable by counting pushes.
- **The same sample uses `BeginProc ..., esp` for the routine that IS called** (`Port_Async_Request`)
  and the EBP form for the one that never was. Where a sample uses two conventions for the same job,
  the one on the exercised path is the one that works.
- **Near-null does not fault in Win95 ring 0.** Linear page 0 is the V86 low-memory mapping, so
  `mov edi,[edi.DCB_cd_ddb]` through a NULL pointer quietly returns BDA bytes instead of trapping.
  Do not expect a null dereference to announce itself; the fault surfaces later, somewhere else.

### The instrumentation lesson underneath it - dump raw bytes before interpreting them

Four runs were spent reading fields *through* the assumption that the pointer was an IOP:
`IOP_physical_dcb` said one thing, `IOP_calldown_ptr` said zero, `DCB_unit_on_ctl` said 0. Every one
of those was fiction, and each looked plausible enough to build a theory on. What ended it was
copying the first 32 bytes verbatim and printing them:

```
+00 IOP_physical      59504C52      <- 1.5 GB "physical address" on a 16 MB machine
+10 IOP_calldown_ptr  00000000
+14 IOP_callback_ptr  00000000
```

`+00` cannot be a physical address on this machine, so the structure is not an IOP, so **no field
read from it means anything**. One dump, one glance, question closed.

**When a structure's fields disagree with each other, stop reading fields and dump the bytes.** A
field read through a wrong base is not a wrong value, it is not a value at all - and a plausible
wrong value is far more expensive than an obviously wrong one, because it survives review.

## Technique 83: a dynamically registered IOS driver gets its own config pass and NOTHING else

2026-09-01, and it explains three separate dead ends at once. A Win9x port driver that must set
`DRP_FC_DYNALOAD` to load at all is registered *dynamically*, and IOS then runs **only that
driver's** `AEP_CONFIG_DCB`. Consequences, all measured:

- **Windows' own disk TSD never engages the DCB.** So nothing parses the partition table into a
  logical volume and no drive letter is ever assigned. Symptom: a freshly formatted FAT16 volume
  draws exactly three requests - LBA 0, then the partition's boot sector twice - and then silence.
  That looks exactly like "the driver returned bad data" and is not.
- **`AEP_BOOT_COMPLETE` never arrives.** A routine hung off it never runs. Instrument the *entry*
  of any handler before concluding anything about what it did.
- The fix is to **be your own TSD**: read the MBR in ring 0, `ISP_CREATE_DCB` a logical DCB, give
  it its **own** calldown node, fill `DCB_Partition_Start` and the actual geometry, then
  `ISP_ASSOCIATE_DCB` a drive number. zikolas/cfu1-win9x's `vol_create` is a working MIT-licensed
  implementation and its comments state the reason outright.
- **Do not share the physical DCB's calldown node with the logical one** (cfu1's warning):
  `ISP_DCB_DESTROY` frees a DCB's nodes, so a shared chain leaves the physical DCB pointing at
  freed memory.
- **And it cannot be done inline.** `ISP_ASSOCIATE_DCB` notifies IFSMGR and broadcasts device
  arrival; called from `AEP_CONFIG_DCB` - i.e. inside `IOS_Register`, inside
  `SYS_DYNAMIC_DEVICE_INIT` - it publishes the volume successfully and then kills the boot. It has
  to be deferred until registration has returned.

### The instrumentation rule this cost a run to learn

**Never let a status field's "not set" value collide with its success value.** Every ISP result in
the marker defaulted to 0, and 0 is also "the call succeeded" - so a run in which the routine never
executed was indistinguishable from one in which it executed perfectly. Both printed zeroes. A
separate `step` field whose 0 means *"this routine never ran"*, stamped at every bail-out, answered
in one boot what the result fields could not answer at all.

Corollary, and it generalises past this project: **a counter is worth more than a result code.**
Result codes report the last thing that happened; counters report whether anything happened.

### A raw surface is not a test fixture

Two runs were spent reading meaning into "Windows issued one request and stopped". The scratch disk
had no partition table, so one read and a stop was the **correct** behaviour and proved nothing
either way. `tools/mkfatimg.py` now builds a real partitioned FAT16 volume host-side, and the
project's own `fatls.py` verifies it before the run. **Build the fixture the OS expects before
concluding anything from how the OS treats it** - and keep the diagnostic marker outside the
partition, or a filesystem write will destroy the only channel the driver has.

### Technique 83, addendum: the ISP volume services are APPY-TIME ONLY

The reason publishing inline kills the boot is not that it is early - it is that it is the wrong
execution context. From zikolas/cfu1-win9x's `sched_volup`: *"the ISP volume services are
appy-time only; the supervisor tick is not."* `ISP_CREATE_DCB` / `ISP_ASSOCIATE_DCB` therefore
cannot be called from an AEP handler, an interrupt, or a timer tick.

The mechanism, which works in a shipping driver, is `_SHELL_CallAtAppyTime` with **`CAAFL_RING0`**
(flags = 1) so it still runs before the GUI is up during boot, guarded by a busy flag so only one
request is queued, and retried if queueing fails.

**The general rule: when a service kills the system rather than returning an error, ask what
context it requires before assuming the call itself is wrong.** Ours succeeded - every ISP result
was 0 and the drive letter was assigned - and *then* the machine died. A call that works and then
kills you is a context problem, not a parameter problem.

## Technique 84: residual findings, 2026-09-01 — test design, and one self-inflicted tax

### This machine is single-disk, and that is settled

A second drive would have to be another CF, and CF is the worst case for it: CF-to-IDE adapters
are commonly **master-only** in True IDE mode, and two CF cards on one channel are unreliable even
on ordinary hardware. Master/slave is a jumper on the device, not the cable (cable-select needs a
CSEL-wired cable *and* both devices set to CS, which 8-bit XT-IDE cabling generally is not), and
XTIDECFG would need reconfiguring on top. So a slave could cost power, mounting, cabling and a ROM
reflash and still not enumerate.

**The safety net is a host-side image, and for CF it is strictly better than a second drive**: the
card comes out, goes in the reader, and the exact tested state is captured and restorable. Design
tests around imaging, not around a scratch device.

Consequence for the port driver: the first hardware run targets the **boot** volume, because
there is no secondary-disk halfway house on this machine. That raises the stakes and is why the
boot path must be proven in emulation first.

### What emulation can and cannot prove here — split the risk deliberately

- **86Box proves the IOS-side logic**: registration, calldown, request path, volume publishing,
  the real-mode-mapper handoff. Every bug on 2026-09-01 lived here, and they present as protection
  errors four layers from their cause - the hard kind.
- **86Box cannot prove the transport on the real card.** Its `xtide` is **stride 1**, so a stride-2
  build has never executed at all; it does not model the card's high-byte latch, XT bus timing, or
  the `0x20-0x3F` port aliasing (Technique 75).

So expect hardware to surface transport faults, not logic faults - and that is the right way
round, because transport faults are legible: wrong bytes, a timeout, a status that will not clear.

### A timeout tuned to a failure hides a slower success

`pdr_loadtest.ps1` defaulted to 150 s, chosen when every run died early. The appy-time volume
callback fires later than that, so a **fully working** build reported
`XTIDE_VolCreate NEVER RAN`. Raising it to 260 s produced a mounted volume with no code change at
all. **When a harness timeout was set during a failing phase, re-examine it before trusting the
first null of a new phase.**

### Do not write regex-bearing Python inside a bash heredoc

Backslash escapes (`\n`, `\[`, `\r?\n`) get mangled between the heredoc and the interpreter, and
the result is a file that either fails to parse or - worse - silently contains a regex that cannot
match. This happened **five times** on 2026-09-01, and one instance produced a `\[` that matched a
literal backslash and failed an anchor assertion at build time.

Use the Edit/Write tools for anything containing backslashes, or `chr(92)`/`chr(10)` concatenation
if a script must generate it. Reserve heredocs for plain text.

## Technique 85: a pointer field can mean two different things depending on a flag - and the
## READ path can be the one that never sees the second meaning

2026-09-02, issue #21, and it is the first bug in this driver that reached the disk.

The Win95 IOS `IOR_buffer_ptr` is a data buffer **only while `IORF_SCATTER_GATHER` is clear**. The
DDK says so outright, in `NEW95DOC/STORAGE.DOC`'s own field table:

> *"IOR_buffer_ptr - Points to either a linear buffer space, unless IORF_SCATTER_GATHER is set, in
> which case it points to a list of linear (logical) scatter-gather descriptors."*

`BlockDev_Scatter_Gather` (`INC32/BLOCKDEV.INC`) is two dwords, **sector count first and buffer
pointer second** - the reverse of the order most people guess.

Our port driver read the field blind. VFAT then wrote through it and we transmitted the descriptor
list into the volume's root directory. The bytes on the disk were their own diagnosis:

```
LBA 126:  01 00 00 00  00 e4 69 c1  ...    BD_SG_Count = 1, BD_SG_Buffer_Ptr = C169E400
```

**Why it survived every earlier test, and this is the transferable part:** VFAT's mount-time reads
are single-buffer and its writes are scattered. So

- the read path exercised only the *first* meaning of the field and worked perfectly - it mounted a
  filesystem, enumerated a directory and read a file;
- the write path met the *second* meaning on its first real request;
- and technique 79's round-trip self-test could not catch it either, because a write/read-back
  applies the same misreading twice and cancels.

**The rule: when a structure field's meaning is switched by a flag, a path that never sets the flag
is not evidence about a path that does.** Enumerate the flags that change the meaning of anything
you dereference, and count how often each arrives - `scatter/gather requests 0 read, 3 write` is
one line of output and it is what turned this from a theory into a measurement.

### Corollaries this cost a run each

- **A counter beats a captured value for "does this ever happen".** The first snapshot caught the
  first write, which was single-buffer, and read as "scatter/gather never occurs". The scattered
  ones came later. Technique 83 already says this; it keeps being true.
- **Check the filesystem, not only the instrumentation.** The marker correctly reported a WRITE
  reaching the transport. The volume was destroyed. Both were true.
- **A green run of a non-deterministic bed is weak.** The corrupting binary, rebuilt byte-identical
  (`md5` compared, technique 70) and re-run on the same fixture, killed the boot at `Initing
  port.pdr` rather than corrupting anything - same input, different damage. Neither run alone
  characterises it. Repeat before concluding.
- **A wall-clock harness window is part of the experiment.** Two runs that differed only by a
  diagnostic counter published the volume and did not, because the volume is published from an
  appy-time callback near the end of the window. Technique 84 said this about 150 s; it was still
  true at 260 s. Raise it, and do not run builds on the host during a run.

### And the DDK's own docs outrank reasoning about the DDK's own sample

The whole answer was in `STORAGE.DOC` and `BLOCKDEV.INC`, and independently in a working
third-party driver (`zikolas/cfu1-win9x`, `rr_sg`/`rw_sg`, whose comment says *"logical SGD: ptr is
SECOND"*). Technique 80's "read the thing that already exists" and technique 81's "a working
implementation outranks the vendor sample" both applied, and both were reached only after guessing
first.

## Technique 86: `Initing foo.pdr` means IOS TRIED - and a real-mode driver elsewhere in
## `CONFIG.SYS` can be why it then refused

2026-09-01/02. This is the one that unblocked the XT-IDE port driver (#21) after it had looked
like a driver bug for weeks, and every part of it is cheap.

### `Initing` is not `ran`

`BOOTLOG.TXT` prints `Initing <name>` **before** IOS dispatches anything. A previous session read
that line as *"IOS loads and calls our .PDR"* and recorded phase 0 as passed. Nine hardware boots
then went `Initing port.pdr` -> `Init Failure port.pdr` in 1-4 log units, and eleven boots were
spent debugging driver logic - a register clobber, a transport, a register map - inside code that
**had never executed a single instruction on that machine.**

Technique 74 says prove a driver LOADS before measuring what it does. This is the same rule one
level deeper: **prove the driver's own code RAN.**

### How to prove it, with no readback channel and an unknown clock

Scale your instrumentation and see whether the answer moves. The probe reported its result by
burning `N x TIMEBASE` loop iterations, so a **tenfold** increase in `TIMEBASE` must show up in any
time base whatsoever:

| build | boot log gap |
|---|---|
| 1x | 4 units |
| 10x | 3 units |

It did not move, so the loop never ran. **A ratio test is immune to a clock you have not
calibrated** - which mattered, because the calibration I had taken from the emulator was wrong for
this machine by a large factor and I quoted it confidently before checking. Prefer a ratio to an
absolute whenever the unit is not yours.

The same trick decodes a value later: build a **calibration binary** that skips the work entirely
and reports a *known* code, run it, and compare gaps directly. Known-30 gave 506 units; the real
probe gave 506 units; the card's own partition type independently predicted 30. Three agreeing
lines, no arithmetic, no estimate of what a log unit is.

### `IOS.LOG` names the blocker, and the emulator gives you the control

`C:\WINDOWS\IOS.LOG` is written by IOS itself and is a primary source (technique 55). Hardware:

```
Unsafe driver     MODISK2  controlling unit 03
Monolithic driver MODISK2  controlling unit 03
Unit number 00..05 going through real mode drivers.        <- all six
```

The **same Windows install, in the emulator**: one real-mode unit, nothing flagged - because the
MO/Zip devices are not attached, so `MODISK2` controls nothing. REM that one line and IOS stops
refusing: `Init Failure` becomes the boot log ending *inside* our load, and after the whole
real-mode SCSI/ASPI chain came out, `Init Success` and 6 real-mode units -> **1**.

**An unrelated real-mode block driver can stop IOS initialising a port driver that has nothing to
do with it.** Read `IOS.LOG` before suspecting your own code. Note `hsflop.pdr` succeeded in the
same log throughout, so this is not IOS blanket-refusing - which is exactly why it was invisible.

### Build the emulator bed from the REAL disk on the REAL machine profile

The `.PDR` test bed was a Deskpro 386, on the reasoning that "whether a VxD loads is
machine-independent". That is false, and it cost the whole investigation its control. Copying the
card's own image into an `ibmxt_inboard386` profile (`vm_xtide_inboard/`, `86box.cfg` cloned from
`vm_golden`) takes ten minutes and splits every subsequent question in half: same binary, same
image, same `CONFIG.SYS` - if it works there and not on the bench, the difference is the hardware
or its resident drivers, and `IOS.LOG` will usually say which.

That bed is also where anything touching the boot volume gets developed, because a mistake costs a
file copy. It is the clone the runbook had been demanding since before it existed.

### Four setup traps met while building it, all of which look like driver results

- **86Box rewrites `86box.cfg` on every load, including after a failed one.** A run that died on a
  missing ROM was normalised down to a plain `ibmxt`, 8088, 256K - and *written back*. The next run
  booted a stock XT trying to start Windows 95 and showed a black screen. Keep an
  `86box.cfg.master`, restore it every run, and **assert the machine name before booting**
  (technique 43, in its most expensive form yet).
- **A new VM directory needs its own `roms/`.** `vm_golden/roms` is a symlink to the repo tree;
  a fresh directory without one fails with ROM errors and then poisons its own config as above.
- **The guest stops for prompts nobody is there to answer** - POST's `162-System Options Not Set`,
  the Windows 95 Startup Menu after repeated forced kills, and `TSLCD ... Press [return]` when no
  CD answers. Each produces *no* `BOOTLOG.TXT`, which reads exactly like a driver that killed the
  boot. Tap the key, and make the harness print "read the screenshot, this run is VOID".
- **The harness window is part of the experiment.** The volume is published from an appy-time
  callback near the end of the boot, so two builds differing only by a diagnostic counter published
  it and did not. Technique 84 said this at 150 s; it was still true at 260 s.

### A readback channel can be loud enough to look like a crash

The 10x delay build, once it finally got past IOS, sat burning its loop with interrupts masked -
Ctrl+Alt+Del dead, screen frozen, indistinguishable from a lockup, and reported as a hang. VxD
initialisation runs with interrupts off; a long delay there **is** a hang as far as anyone watching
can tell. Keep the diagnostic delay short enough to survive being watched, and say in the
instructions how long a healthy pause looks.

### And on a single-disk machine, remove the capability rather than trusting the guard

The probe's write self-test targets LBA 100 of whichever unit answered, gated only on a DEV-bit
read-back. On the 5160 that unit is the boot CF, and the gate had never executed on a stride-2 map.
`-NoWriteTest` skips the slave probe entirely, confirmed unreachable in the **listing** (`EB 48`
jumping the call) rather than asserted. Likewise `-ReadOnly` refuses every `IOR_WRITE` at the
request handler's door before any addressing is computed.

That pair is what made a claiming run on somebody's only disk a reasonable thing to attempt: a
wrong read costs a reboot, a wrong write costs the volume, and technique 79 already established
that a round trip cannot see a wrong address.


## Technique 87: a modal dialog at shell start makes every shell-dependent test
## impossible - and it presents as "the harness cannot send keystrokes"

2026-09-03. Cost two full emulator runs, and the second one produced a confident
wrong conclusion that was about to be written down as a finding.

### What happened

The `vm_xtide_inboard` bed had `mouse_type = none`. Windows 95 answers that with a
**modal** dialog at shell start - *"Windows did not detect a mouse attached to the
computer"*, one OK button. Explorer does not finish starting behind it, so:

- there is no taskbar, so `Ctrl+Esc` opens no Start menu;
- **nothing in the StartUp folder ever runs**;
- anything else that waits on the shell being up waits forever.

Every test on that bed that depended on the shell was silently impossible, and had
been for as long as the bed existed.

### The two tells, and both were on screen the whole time

1. **Desktop icons with NO TASKBAR.** That combination is not "still drawing" - it
   is Explorer blocked. Look for it before theorising about input.
2. Three screenshots taken ten seconds apart that are **byte-identical in size**.
   A live Windows desktop is not static to the byte.

### The wrong conclusion it nearly produced

A `Ctrl+Esc`/`U`/`Enter` sequence changed nothing, so the session concluded that
host-side keystroke injection does not reach 86Box - which is close enough to this
file's own long-standing warning about `SendKeys`/`WM_KEYDOWN`/`keybd_event` to
look confirmed.

It was wrong, and the disproof was already in hand: an EARLIER run of the same bed
had reached a desktop with icons. That run tapped Enter every 5 s for 300 s. Enter
is OK on that dialog. **The harness had been dismissing the blocker as a side
effect, and nobody knew.** Two runs differing only in whether Enter was tapped -
one past the dialog, one stuck on it - is a clean natural experiment proving the
keys DO land.

### The harness lesson underneath it, which is the transferable one

That Enter tap was added to clear `TSLCD`'s *"Driver aborting... Press [return]"*.
**`TSLCD` had been REM'd out of `CONFIG.SYS` since 2026-09-01.** So the tap's
documented job no longer existed, and its real job had silently become "dismiss the
mouse dialog" - load-bearing work nobody had written down, in a harness everyone
believed was a neutral observer.

Technique 71 says a harness that clears prompts must log the screen it cleared.
Extend it: **a prompt-clearing harness must be re-justified whenever the thing it
clears changes.** A tap that clears nothing is not harmless - it is an undocumented
dependency waiting to be removed, and when it is removed the failure lands
somewhere else entirely.

### What to do instead

**Configure the prompt out of existence; do not clear it with keystrokes.**
`mouse_type = msserial` removes this one at the source, and makes the bed MORE
faithful rather than less - the real 5160 has a serial mouse. Same family as this
file's existing advice to use `NOPAUSE` and to clear a stale `nvr/` rather than
racing an F1 prompt.

Then drive whatever the test needs **from inside the guest**, where no focus is
involved at all: a batch in the StartUp folder, with a `CHOICE /T` settle and a
stage ladder written to a file on C:. `SHUT.BAT` +
`tools/pdr_inboard_run.ps1 -Shutdown -InGuest` is the worked example -
`C:\SHUTLOG.TXT` says whether the trigger fired, and `BOOTLOG.TXT`'s
`Terminate=`/`EndTerminate=` pairs name the stage a shutdown died in.

### And do not take focus on a machine somebody is using

The harness grabbed focus every 5 s for 300 s so its taps would land. That makes
the host unusable for whoever is sitting at it, and the owner said so. The fix is
to grab focus **once**, at launch, when the VM has it anyway, and afterwards send a
key only if the VM **still** has it - counting and reporting the skips, so a run
that missed its prompts is recognisable as VOID rather than as a result.

### Bonus, cheap and real: TaskStop kills the emulator with the harness

86Box is started with `Start-Process` without `-Wait`, which reads as detached and
is not: stopping the background harness task killed the VM with it and threw away a
13-minute boot. If you want the guest to outlive the script, do not rely on
`Start-Process` alone.


## Technique 88: bisect by REMOVING the component before you theorise about it -
## and a polling Win9x port driver must not block

2026-09-03. The Win95 shutdown hang on the XT-IDE port driver (#21). The answer was found, but
the route to it wasted most of an evening, and the waste is the reusable part.

### The rule, first

**When a symptom might belong to your component, take the component away before you write
anything.** `PORT.PDR` renamed to `PORT.PD_` in `IOSUBSYS`, nothing else changed: Windows shut
down cleanly. Put it back: hangs. One character, one boot, and it converts "probably ours" into
"ours" with no code written.

That test was *set up twice and spent on neither* while two fixes were written and shipped on
theories. Both fixes were correct on their own terms and neither was the cause. Technique 77
already says a rename is the decisive move; this is the second time it has been the cheapest
thing available and the last thing tried.

**Then keep bisecting with switches, one variable per build.** Five builds settled it:

| build | claims unit | fills DCB | inserts calldown | publishes volume | shutdown |
|---|---|---|---|---|---|
| absent | - | - | - | - | clean |
| claim mask 0 | no | no | no | no | clean |
| no calldown | yes | yes | **no** | no | clean |
| no volume | yes | yes | **yes** | no | **HANGS** |
| no volume + stubbed request handler | yes | yes | yes | no | clean |

Four halvings, no guesswork in the chain. The claim, every DCB field, the published volume and
the whole completion path came out innocent; the transport body did not.

### Two bisects that were themselves void - both avoidable

- **A bisect switch must not break normal operation.** `-ReadOnly` (refuse every write at the
  request handler's door) was tried as "is it a write?". The guest never reached a desktop -
  Windows needs to write to boot. A switch that stops the system working cannot isolate a
  behaviour that only happens at shutdown. `-ReadOnly` is a *safety* flag for a hardware run;
  it is not a bisect.
- **Instrumentation must not share state with the thing it measures.** The on-disk request
  marker writes its sector through the same `XTIDE_ReqLba` / `ReqCount` / `ReqBuf` globals the
  in-flight request is using, so it clobbers the request and the boot hangs on the splash
  screen. Technique 45 in its purest form: the observer became the experiment. Give a diagnostic
  its own state, or it is not a diagnostic.

### And do not read meaning into where a log ENDS

`BOOTLOG.TXT` stops after `EndTerminate = KERNEL`. That was written up as "the hang is located
in the phase after KERNEL, which is where `AEP_SYSTEM_SHUTDOWN` is broadcast", and two theories
were built on it. **A run that shuts down cleanly logs the identical ladder** - the file just
ends there either way. **Before an end-of-file is evidence, check where the file ends on a
SUCCESSFUL run.**

### The actual cause, and a contract this project did not know about

From **Microsoft's I/O Supervisor Guide for Windows 9x/Me** (@andrew-hoffman, #21; archive.org,
originally a Microsoft download - it is NOT in the Win95 DDK, and the DDK's own `STORAGE.DOC`
says nothing about any of it):

> "If the hardware polling method is used (instead of waking up when an interrupt arrives), the
> driver then calls **Set_Global_Time_Out or Set_Async_Time_Out** so that the driver's timeout
> (polling handler) routine gets called back later, for example in 10 milliseconds. **Immediately
> after this Set_Global_Time_Out call, simply return (WITHOUT doing a JMP to the IOP_callback_ptr
> routine). This releases the system from your driver, so the system can run normally for a
> while.**"

The required shape:

1. start the I/O;
2. `Set_Global_Time_Out` ~10 ms;
3. **return without completing**;
4. poll from the timeout handler, re-arming while the device is busy;
5. when done, **CALL** (not JMP) `IOP_callback_ptr`;
6. `ILB_dequeue_iop` and start any queued IOP, else return.

**A card jumpered without an interrupt makes this contract yours, not optional.** The XT-CF has
no IRQ, so the driver polls - and it was spinning inline inside `Port_request`, 400000 `in al,dx`
holding the whole system, roughly 0.6 s per wait on a 4.77 MHz bus. It survives normal operation
because the drive answers; it wedges at `System_Exit`, where the scheduling and time-out context
a blocking driver leans on is being torn down.

⚠ **Correction, 2026-09-04: those spins are BOUNDED.** `XT_SPIN = 400000` with `dec ecx / jnz`,
in both `XTIDE_WaitNotBusy` and `XTIDE_WaitDrq`; each returns `CF=1` on expiry, and the driver
does not retry. So the driver **cannot** spin forever, and "hang" was assumed, not measured.
0.6 s per wait, several waits per sector, many sectors in a shutdown cache flush is *minutes* -
which is indistinguishable from a hang to anyone who did not wait. **Before designing a fix for
a hang, establish that it is one:** watch 86Box's XT-IDE access log during the freeze. Accesses
still arriving means a timeout storm, not a wedge, and the two want different fixes. Nobody has
recorded how long the machine was left.

Note also `Port_iop_timeout` (`AEP_IOP_TIMEOUT`) is an **empty stub** that returns the preset
`AEP_SUCCESS` - telling IOS we own the timed-out IOP and have handled it, having done nothing.
A second missed contract, independent of the polling one.

**The transferable point:** "it works in normal use" says nothing about whether the structure is
right. A blocking driver looks fine until the system needs to run something else while it waits.

### Where to look when the DDK is silent

The Win95 DDK is not the only vendor documentation for this layer, and for IOS it is not the
useful one. The **I/O Supervisor Guide** covers request flow, the calldown chain, polling, the
IOS data area and the `.IDUMP` / `.IDCB` / `.ILDCB` debugger commands. Read a binary `.DOC` with
a printable-run scan - the same trick used on `SYSTEM.DAT` (technique 65) - no parser needed.

### Confirming it from the binaries: diff the VxD service calls against Microsoft's own drivers

2026-09-04. The contract above was read out of a document. It can also be **measured**, in
minutes, off the CF card - and measurement is what turns "we appear not to meet a requirement"
into a fact.

Every `INT 20h` VxD service call in a `.PDR` is `CD 20` followed by `dw service, dw device_id`.
Scan for it, keep `device_id == 0001` (VMM), and resolve the ordinals by counting `VMM_Service`
declarations in the DDK's own `INC32/VMM.INC` in order from zero. Anchor the count on
`Get_VMM_Version` = 000 and `Get_Cur_VM_Handle` = 001 before trusting any other number.

Against the three Microsoft port drivers in `D:\WINDOWS\SYSTEM\IOSUBSYS`:

| driver | timeout / event services used |
|---|---|
| `ESDI_506.PDR` | `Set_Global_Time_Out` x1, `Set_Async_Time_Out` x2 |
| `SCSIPORT.PDR` | `Set_Global_Time_Out` x1, `Cancel_Time_Out` x3, `Set_Async_Time_Out` x1, `Schedule_Global_Event` x3 |
| `HSFLOP.PDR` | `Set_Global_Time_Out` **x5**, `Cancel_Time_Out` x1, `Schedule_Global_Event` x3, `Create_Semaphore` x2, `Wait_Semaphore` x4, `Signal_Semaphore_No_Switch` x1 |
| **our `PORT.PDR`** | **none - zero VMM service calls of any kind** |

Copies kept as `roms/xtcf_card/{ESDI_506,SCSIPORT,HSFLOP}_reference.PDR`; the scan is
`tools/pdr_vxd_services.py`, which self-checks its ordinal table against the two anchors and
refuses to run if they do not hold. None of the three are compressed - the service calls scan in
plaintext, so RLoew's IO8 decompressor is not needed here.

**`HSFLOP` is the better model than `ESDI_506`, because it shows blocking done legally:**

- `0x1853` / `0x1865` - two `Create_Semaphore` at init.
- `0x2D95` `Set_Global_Time_Out` ... `0x2DBE` `Wait_Semaphore`, 43 bytes apart, and exactly one
  `Signal_Semaphore_No_Switch` elsewhere in the file. **That closes the loop:** arm a timeout,
  block on a semaphore, and let the timeout handler signal it. A port driver *may* block, as long
  as something asynchronous can free it. That is a far smaller change than the full state-machine
  restructure, and it is the first thing to try.
- `0x2DE0` `Set_Global_Time_Out` immediately followed by `5E FF 67 0C` (`pop esi; jmp [edi+0Ch]`):
  arm, then tail-jump out. The "return without completing" shape, in situ.
- `ESDI_506` at `0x1B56` is the same idiom (`Set_Global_Time_Out`, then `jmp` back).

Those byte reads are from a scan, **not from a disassembler** - confirm them before building on
them. `ESDI_506` is also the weaker reference for our case: IDE has IRQ 14, so it completes from
an interrupt, and only the floppy driver has to survive a device that may simply not answer.

**The reusable move:** when a spec says your driver must do X, grep the vendor's own drivers for
X before designing anything. If they all call it and you call nothing, you have your answer
without a boot.

### Left-field reading has to be triaged, not followed

The same day, a batch of ten links was collected on XTIDE/Win9x. Nine were the wrong problem -
capacity patching (`137GB`, LBA48, the RLoew bundle), a jumpers-and-cables forum thread, general
protected-to-real-mode material. **But they named `ESDI_506.PDR`, and that pointer produced the
table above.** Scattering wide is cheap and occasionally lands; reading all ten in full is not.
Triage by *what problem is this about*, follow the one that names a file you can open, and say
plainly which ones were dead ends - see `docs/resources_and_sources.md`.

### The freeze is NOT in the transport - measured 2026-09-04, and it cost three re-derivations

**Read Techniques 3, 12 and 13 before instrumenting a hang. This session re-derived all three.**
Technique 3 already prescribes a CS:PC heartbeat to tell a freeze from slow progress; Technique 12
already gives the hook site (`exec386()`), the register macros and the `cpu_use_dynarec = 0`
caveat; Technique 13 already gives the offline distinct-address analysis. A session that built all
of it from scratch instead spent two rebuild-and-reproduce cycles arriving where the file already
was. **The skill is only cheap if it is loaded before the approach is chosen, not after.**

What the measurement found, on `vm_xtide_inboard` with the hang reproduced:

| question | answer |
|---|---|
| is our driver spinning? | **no** - 3,582 alternate-status reads across the whole shutdown, against `XT_SPIN` = 400000 for a single expiry |
| did it stall mid-transfer? | **no** - the last command (`READ SECTORS`, 8 sectors, LBA `0x0EC9A8`) drew exactly 2048 word-reads, its full length |
| was it doing real work? | **yes** - 262 commands, 1376 sectors read, 153 written, all after the shutdown began |
| is the guest alive? | **yes** - 86Box at 107-117% of a core, sustained |
| where is it? | **VMM**, ring 0, PM: `C0003178`-`C0003257` and `C0008E27`-`C0009077`. Never enters IOS or our driver |

So the timeout-storm reading in the correction above is **also wrong**, and the fast-fail clamp
built the same morning is inert for this bug - the clamp arms on an expiring wait, and none expire.

### Technique 88b: a device log's SILENCE is ambiguous - add a heartbeat, then diff it against a HEALTHY sample from the same run

Two additions to Technique 3 that made the above decidable rather than suggestive.

**1. `pclog` folds identical consecutive lines into `*** N repeats ***`.** So a per-instruction or
per-timeslice `CS:EIP` sample is cheap even in a tight spin - one line and an exact count - and the
fold itself is a signal. Sample from `exec386()` (`386_dynarec.c`), and dump the 16 bytes at
`cs + cpu_state.pc` via `readmembl()` so paging is honoured, saving and restoring `cpu_state.abrt`
around the reads. Verified correct by checking a known site: the sample at `F000:ECB6` returns
`74 F4` = `je ECAC`, matching the DMA-refresh loop disassembled in Technique 59.

**2. Take the control from the SAME run, before the failure.** Mark the log's byte offset while the
machine is healthy, then compare the distinct-EIP sets either side of the mark:

| | desktop idle | the freeze |
|---|---|---|
| distinct EIPs | 24 | 72 |
| spread | broad - our driver, several VxDs, `C0FCE1`, `C0380B`, `C03615` | 70 of 72 inside two VMM ranges |
| **addresses in common** | — | **1** |

A wedge and a healthy idle both look like "the same few addresses cycling", which is all Technique
13 asks for. **The discriminator is the overlap with a known-good sample, not the shape of the
freeze alone.** One address in common is a wedge; a large overlap would have meant Windows was
simply idle and waiting, and the whole investigation would have been aimed at the wrong layer.

Costs nothing: the same logfile, one `wc -c` at a known-good moment. Do it on every hang.

### Enabling a device log can be a silent no-op - `ENABLE_XTIDE_LOG` alone logs nothing

`hdc_xtide.c` gates its five `xtide_log()` sites on `ENABLE_XTIDE_LOG`, but calls
`log_open()` **only in `jride_init`**. The plain XT-IDE card's init never opens one, so with the
define set the driver emits a single `WARNING: Logging called with a NULL log pointer` and no
access lines at all. One `log_open("XTIDE")` in `xtide_init` fixes it.

Technique 28's rule for patch scripts applies to emulator diagnostics too: **an instrumentation
switch that appears to work and produces nothing is worse than one that fails loudly.** Check the
log has the lines you expect before spending a reproduction on it - here, `grep -c '\[R\]'` on a
12-second boot answers it.

Both edits are marked `DIAGNOSTIC, not for upstream` in `86box_upstream`, which is its own repo.

### RETRACTED 2026-09-04: the hang was in a binary nobody can rebuild

**Everything in technique 88 above about the polling contract is diagnosis of a defect in an
artefact that does not exist any more.** Read this before acting on any of it.

Four shutdowns on 2026-09-04, three binaries built from committed source, all **clean**:
`71a2620c` (clamp + counters), `a0032294` (counters only), `996f25b7` (neither). The binary
that hung - md5 `70298a8f`, 3/3 that morning - **cannot be rebuilt from any commit.** It came
from an uncommitted working tree on 2026-09-03, went straight into the emulator image, and that
source state is gone.

Eliminated before concluding it, in this order, each one cheap:

| candidate | test | result |
|---|---|---|
| non-deterministic toolchain | build the same source twice | same md5 - deterministic |
| line endings (`.gitattributes` normalises `.ASM` to CRLF) | build the same source as LF and as CRLF | **byte-identical** - not it |
| it was a bisect build | `-Release` refuses `-NoVolume`/`-NoCalldown`/`-NoDcb`/`-NoIo`; release is 20,619 bytes, bisect builds 20,675; `70298a8f` is 20,619 | not a bisect build |
| it matches the committed tree | `3b85848` + `-Release` | right SIZE, wrong content (`4e2dd46c`) |

So the hang was real - it reproduced three times out of three - and it is now **unattributable
and unbisectable**. A day went into diagnosing it, and the diagnosis cannot be checked.

**CORRECTED, same day, by the project owner:** the shutdown hang **was** also observed on the
real 5160, with the CF's own `0fe2431a` - and that binary IS tracked
(`dist/xtide_pdr/PORT_claim_master_stride2_rw.pdr`). So the hang is NOT merely an artefact of
the unreproducible `70298a8f`, and the retraction above must not be read as "there is no bug".

What the retraction does still establish: the diagnosis in technique 88 was derived from a
binary that cannot be rebuilt, so it is unverified; and three binaries built from committed
source shut down cleanly in the emulator, which `0fe2431a` has never been tested against there.

**That is the repair, and it costs one emulator run:** deploy `0fe2431a` - tracked, reproducible,
and known to hang on hardware - into the emulator bed. If it hangs there, the control that was
missing all along exists, and the bisect becomes possible for the first time. Do that before any
further theorising.

**The real machine was never running `70298a8f`.** The CF holds `0fe2431a` (2026-09-01,
`dist/xtide_pdr/PORT_claim_master_stride2_rw.pdr`), which is tracked and reproducible.

### Technique 89: an artefact under test must be traceable to a commit, or its results are not evidence

Technique 70 says verify the BINARY, not the source tree - `stat` the exe against `git log`.
That is necessary and it is not sufficient. The binary here was newer than every commit and
still corresponded to none of them, because the tree it was built from was dirty and the changes
were never committed. A timestamp check passes cleanly in that case.

**The rule: before you deploy a binary anywhere you will later draw a conclusion from, make it
rebuildable.** Commit first, or record the exact source state. The emulator-image loop is where
this slips, because deploying to an image feels like a test rather than a release - the md5 gets
checked against the *staging copy* (technique 75's rule, correctly applied) and never against a
commit.

Enforced rather than remembered, since remembering it did not work:

- `build.ps1` prints `commit <hash>  tree clean|DIRTY` next to the md5 on every build, and
  prints a loud block naming the dirty files when the tree is not clean;
- every build appends `utc, md5, bytes, commit, tree, flags` to `drivers/xtide_pdr/build_ledger.tsv`,
  which is tracked. **Any binary found later on a card or in an image can then be identified by
  its md5 alone** - which is precisely the question that could not be answered here.

The ledger is seeded with every binary this project knows about, including `70298a8f` marked
`UNREPRODUCIBLE`, so the next person who finds it does not repeat the archaeology.

**Corollary for a long debugging session:** commit at each verified iteration, which `CLAUDE.md`
already asks for. The reason is not tidiness. It is that a bisect needs a control, and a control
you cannot rebuild is not one.

## Technique 90: if a bug will not reproduce in the emulator, check the emulator models the CARD

2026-09-04, and it repairs the whole shutdown investigation. **REPRODUCED - read this before
acting on the retraction above.**

### The symptom that wasted the morning

The XT-IDE port driver's shutdown hang would not reproduce in 86Box. Four shutdowns, three
binaries built from committed source, all clean; the same binary hung 3/3 on the real 5160. The
natural readings were all wrong: a phantom binary, non-determinism, instrumentation timing.

**The real answer: 86Box was not modelling the card.** `hdc_xtide.c` decoded `port & 0xf` -
stride 1 - and did a 16-bit `ide_readw` per data access. The real Lo-tech XT-CF rev 3 does not
decode A0 (registers at `base+2N`, 32 ports) and its BIOS sends `SET FEATURES 01h` to put the CF
into **8-bit PIO**, where the data register is byte-wide with no high latch.

So in the emulator our driver silently fell back to the stride-1 path, could not properly claim
the disk, `RMM.PDR` stayed and served it, and **the defect never triggered because our driver was
never really doing the job.** On hardware it is the disk, so it does.

### The tell was in `BOOTLOG.TXT`, and it is free

| | emulator, stride-1 card | real 5160 |
|---|---|---|
| `INITCOMPLETE = RMM` | present | **absent** - RMM loads then unloads, having claimed nothing |
| `INITCOMPLETE = PORT` | - | **present** |
| `IOS.LOG` | present, names a real-mode unit | **file does not exist** |

**A driver that `Dynamic load success`es and then never reaches `INITCOMPLETE` found nothing to
claim.** Diff the `INITCOMPLETE` list between bed and hardware before trusting any bed - it is one
grep of a file both machines already write, and it would have shown the divergence on day one.

### What the fix was

`hdc_xtide.c`: a `Register stride` option (default 1, so every existing config is unchanged) with
`reg = (port >> shift) & 0xf` and a 32-port range; 8-bit PIO tracked from `SET FEATURES 01h/81h`
with the data register alternating low/high halves; and a BIOS entry for the XT+ r638 image read
back off the card. `Mike1978uk/86Box` branch `xtcf-lotech-stride2`.

Verify it took effect rather than assuming (technique 69) - the device now logs
`XTIDE: base 0300 stride 2 bios xtcf_lotech`, and the access log shows our driver reading
alternate status at **`031C`** (`base + 14*2`) where it previously used `0307`.

### The reproduction, and why it is the control everything else needed

With the faithful bed the hang appears immediately, on the binary that is actually on the CF
(`0fe2431a`, tracked as `dist/xtide_pdr/PORT_claim_master_stride2_rw.pdr`):

| | 2026-09-04 morning, `70298a8f`, stride-1 bed | faithful bed, `0fe2431a` |
|---|---|---|
| clusters | `C00031xx`-`C00032xx`, `C0008Exx`-`C00090xx` | identical distribution |
| `C0003178` / `C0003257` / `C0008E27` | present | **all three present** |
| shape | ring-0 VMM spin, driver idle, guest alive | same |

The duplicate volume reproduces too - `C:` and `D:` from one CF, on both machines.

**So the bug is real, consistent, and now bisectable at emulator cost.** Technique 89's retraction
stands on the *diagnosis* only: the polling-contract explanation was derived from a binary that
cannot be rebuilt, and is now testable for the first time.

### The transferable rule

**"It does not reproduce in the emulator" is a claim about the emulator, not about the bug.**
Before concluding a fault is hardware-specific, list what the emulator does *not* model about the
part in question - register decode, transfer width, timing - and check whether the code under test
takes the same path in both. Here it did not, and nothing measured in that bed was evidence about
the hardware. Technique 84 already said 86Box could not prove the transport on this card; what was
missed is that this also makes it unable to reproduce anything *downstream* of the transport.

### Technique 90b: the cause - our driver stops a VMM routine running at all

2026-09-04, on the faithful bed, and it is the first result on this bug that names a **cause**
rather than a location.

The freeze spins on two VMM globals, visible in the heartbeat's instruction bytes:

```
C0003184:  8B 6B 08              mov ebp,[ebx+8]
           39 05 F4 E9 00 C0     cmp [C000E9F4], eax
           75 ED                 jne  -19            <- ~19-byte spin, EAX never reloaded
C0008FA4:  66 83 3D F0 E9 00 C0 00   cmp word [C000E9F0], 0
```

A linear-write watchpoint on `C000E9F0-C000E9F7`, run twice on the same bed with one variable -
`PORT.PDR` present, then removed (technique 88's rename):

| | driver removed | driver present |
|---|---|---|
| shutdown | **clean** | **hangs** |
| writes to the range | **213** (141 during shutdown) | **zero, ever** |

```
79x [0028:C00032B4] w2 C000E9F0 = 0      35x = 1      6x = 2      2x = 4
76x [0028:C00032E1] w2 C000E9F2 = 0
 1x [0028:C0002F6F] w2 C000E9F0 = 0Ah
```

**The writers sit inside the cluster that spins** (`C00031xx`-`C00032xx`), so one VMM component
both advances this state and polls it; the values look like a state enum. With our driver loaded
that component **never executes at all** - not "runs and stalls", zero writes - and the shutdown
then waits forever for a transition nothing is alive to make.

**So the fault is not inside our transport.** It is whatever our driver's presence does to stop
VMM reaching `C00032B4`. That kills the polling-contract diagnosis on evidence, and it makes the
next question specific: what calls `C00032B4` on a clean shutdown, and why is that path not taken
when our calldown is inserted?

### Two mechanics worth reusing

- **Hook every width AND the pre-translated variants.** The first watchpoint covered only
  `writemembl` / `writememll` and reported zero - which read like a finding and was a hole: every
  one of these writes is **word** width. `mem.c` has six linear write entry points
  (`writemem{b,w,l}l` and their `_no_mmut` twins) and a watch is worthless until all six are
  hooked. A zero from a partial hook is technique 19's gate artefact in a new place.
- **A watchpoint needs a control run, and the control is the component removed.** "Nothing writes
  it" means nothing until you know what a *working* run does. The same bed with `PORT.PDR` deleted
  gave 213 writes and a clean shutdown, and that contrast is the entire result.

Both hooks are `DIAGNOSTIC, not for upstream` in `Mike1978uk/86Box`: the watchpoint in `mem.c`
(armed from `inboard386_init`, `INBOARD_MEMWATCH=0` disables) and the CS:EIP heartbeat in
`386_dynarec.c`.

### ⚠ 90c IS WRONG - CORRECTED BELOW, read 90d instead

The section that follows concluded "VMM enters the wait holding a null handle". It was built on
misreading a backward `jne` as a loop head. Kept only as a worked example of the mistake.

### Technique 90c: closed to one register - a NULL handle enters the wait

2026-09-04. The caller trace (a raw 96-entry ring dumped on reaching `C00032B4`) shows the whole
path on a **clean** shutdown:

```
C0002657..C0002665 -> C0002795 -> C000352E..C0003559
-> C0001300..C0001354                                   (dispatcher; C0001300 is the return addr)
-> C000317F C0003181 C0003182 C0003184 C0003187 C000318D C000318F   <- THE LOOP THAT HANGS
-> C0003190..C0003268 -> C0001359 C0001360 -> C000329A..C00032B4    (the state write)
```

**The loop that hangs is ordinary code that a healthy shutdown passes straight through**, and only
then reaches the state write. So it is not an exotic deadlock primitive - it is a wait whose exit
condition our driver prevents.

```
C0003184:  8B 6B 08            mov ebp,[ebx+8]
           39 05 F4 E9 00 C0   cmp [C000E9F4], eax
C000318D:  75 ED               jne -19        ; loop while they differ; EAX never reloaded
```

| | EAX | EBX | outcome |
|---|---|---|---|
| clean | `C0FCE28C` (a VMM pointer) | `C15200E8` | falls through; `C00032B4` runs; shutdown completes |
| hung | **`00000000`** | `C15200E8` - **the same object** | spins forever |

`[C000E9F4]` is **never written in either run**, so the global is not the variable - the clean run
exits because EAX already matches. **With our driver loaded, VMM enters this wait with a null
handle where a valid pointer belongs.** "The VMM routine never runs" (technique 90b) is the
consequence; this is the cause.

Next: EAX is set before `C000317F`, so it comes from the dispatcher at `C0001300`-`C0001354` or is
passed in. Trace backwards from there to find what returns 0, and the answer will be something our
calldown insert changes about an object VMM asks for at `System_Exit`.

**Method note worth keeping:** a raw undeduped ring (technique 49) dumped at a *target* address,
run on the **working** case, is what made this legible - it showed the loop is normal, which no
amount of staring at the failing case could establish. When a spin looks pathological, capture the
same addresses on a run that succeeds before concluding anything about them.

### Technique 90d: a backward `jne` is not a loop until you read its TARGET

2026-09-04, correcting 90c. Dumping the live bytes around the "spin" showed it is not one:

```
C000317C:  48 5E C3            dec eax / pop esi / ret        <- FAILURE RETURN
C000317F:  33 C0 56 33 F6      xor eax,eax / push esi / xor esi,esi
C0003184:  8B 6B 08            mov ebp,[ebx+8]
C0003187:  39 05 F4 E9 00 C0   cmp [C000E9F4], eax
C000318D:  75 ED               jne -19   ->  C000317C
```

`C000318F - 19 = C000317C`, which is `dec eax / pop esi / ret`. **The branch returns; it does not
loop.** And at the wedge `[C000E9F4] = 0` with `EAX = 0`, so the branch is not even taken - the
routine falls through and returns normally.

`C0003184` is a short leaf being **called ~2,000,000 times** by an outer loop. Every heartbeat
landed there because it is hot, not because it is stuck. Live globals at the wedge:
`C000E9F0 = 2`, `C000E9F2 = 1`, `C000E9F4 = 0`.

The real retry loop is in the callers, from the approach ring:
`C002BED7 -> C002EA48 -> C002CA56 -> C0001430..C0001444`.

**The rules, and this cost three reproduction runs:**

- **Compute a relative branch's target and read what is there before calling anything a loop.**
  A backward `jne` into a `ret` is a failure exit. Technique 44 already warns that a plausible
  reading of code you have not dumped can be self-consistent and wrong; this is that, on four
  bytes.
- **Registers sampled inside a hot leaf describe the leaf, not the bug.** `EAX = 0` was real and
  meant nothing - it is that routine's own `xor eax,eax` two instructions earlier.
- **A heartbeat concentrating in one address range means "hot", not "stuck".** To tell them apart,
  exclude that range from a raw ring (technique 49) and see what is still there. Doing so here
  produced the caller chain in one run - and note the first attempt failed because the counter was
  reset on leaving the range, so it never accumulated: the wedge cycles through *two* regions.
- **Dump the code at the address, from the running guest.** Everything above came from 80 bytes
  read live at the moment it wedged. Reading them earlier would have skipped 90b and 90c entirely.

## Technique 91: identify a list node by its SIGNATURE before deciding whose it is — and
## compute your own load base before calling an address yours

2026-09-04. The XT-IDE port driver's shutdown wedge was recorded in commit `8fe1609` as *"VMM
waits for a list our driver is still on"*, with the queued node `C1032D1C` described as *"inside
our driver's own address range"*. Both halves were wrong, and each was wrong in a way this file
already warns about.

### The node was never ours — dump the object, do not infer from the address

Extending the linear write watchpoint to dump 32 bytes at every pointer it saw queued gave the
answer in one boot:

```
+00  00510801
+04  C001080C   <- back to the list sentinel
+08  C0FD0F4C   <- prev
+0C  54 48 43 42  =  "THCB"
+14  C15200E8   <- the same value on EVERY node: the System VM handle
```

**`THCB` is the Windows 95 VMM Thread Control Block signature.** All 211 queued nodes carried it.
`C001080C` is not a driver queue at all — it is the **VMM thread list**, and `C0008F8A`'s
`cmp [C0010810], C001080C / jne` is VMM waiting for every thread in the System VM to exit before
shutdown can finish. `C15200E8`, the constant in `EBX` through every wedge heartbeat, is that VM.

A struct signature at a fixed offset settles ownership in one line where an address never can.
**Dump 32 bytes of any object you are about to attribute to something.** It costs one `readmembl`
loop in a hook you already have.

### The address looked like ours because heap blocks neighbour the image that was loading

Our object 1 loads at `C1030D40`. The queued node was `base+0x1FDC` in one run and `base-0xEC` in
the next — TCBs allocated either side of our module, because they were created around the time our
driver loaded. **Proximity in a heap is not ownership.**

### How to compute the load base of a Win9x VxD from a live log, exactly

Three independent families of anchor, and they must be reconciled because they disagree by one:

| anchor | what it gives |
|---|---|
| an I/O site: find the `in al,dx` / `out dx,al` byte inside a routine in the FILE, add the routine's `.map` offset | 86Box logs `pc` **after** the one-byte instruction, so this family reads base+1 |
| a heartbeat's 16 instruction bytes at some `CS:EIP` | matched against the file, this gives the offset **exactly** — `pc` is at the instruction start |
| register values in that same heartbeat | `ESI`/`EDI` holding your own buffers resolve straight to `.map` symbols |

Reconciling the off-by-one is what pins it. Object 1 offset O sits at file offset `datapages + O`
(`datapages` from the LE header, 0x1000 here) — `tools/ledump.py` prints it.

**Then check the offset against the object's own `vsize`.** Ours was `0x1B88`, the whole module
packed `0x1E21`; `0x1FDC` is past both. An offset outside every object is the tell that the address
is not in your image at all, before you go looking for a symbol that cannot exist.

### The service scan that said "zero VMM service calls" was true and misleading

Technique 88's table scanned `CD 20` records and kept `device_id == 0001`. Our driver makes no VMM
calls — and does make seven others, including the one that mattered:

```
0017:000E   _SHELL_CallAtAppyTime      <- one call site, publishes our volume
0010:0007   IOS
0003:xxxx x4,  0033:xxxx x2
```

**Filter a service scan by device only to answer a question about that device.** To answer "what
does this driver talk to", print every device ID and resolve afterwards.

### Negative, measured on the faithful bed: the published volume is not the cause

`-NoVolume` (commit `ebb9a9b`, md5 `2f7c8f4e`) removes the `call XTIDE_SchedVol` at the end of
`Port_cfg_device`, so no appy-time event is queued, no logical DCB is created and no drive letter
is associated. **It still wedges**, identically — same VMM addresses, same pinned `EDI`, thread
list still non-empty. So the logical DCB, the drive-letter association and the un-cancelled
appy-time event are all exonerated. (The same row in Technique 88's old bisect table said this on
the stride-1 bed and happened to be right for the wrong reason.)

### What survived: the VRP imbalance is exactly 1, in BOTH configurations

The AEP census, decoded from the driver's own debug-port stream (`DBGPORT tag=10`):

| | startup | shutdown | `CREATE_VRP` | `DESTROY_VRP` |
|---|---|---|---|---|
| with volume | `21 4 12 18 17 · 21 4 18 18 19 18 17` | `21 16 19 4 · 21 16 19 4 · 15 14` | **4** | **3** |
| no volume | `21 4 12 18 17 · 21 4 18` | `21 16 19 4 · 21 16 · 14` | **2** | **1** |

Removing the volume removed two creates *and* two destroys and left the same single orphan. And in
the no-volume run the second teardown pass **stops after `AEP_DCB_LOCK`** — IOS pend-unconfigures a
DCB, locks it, and never sends `DESTROY_VRP` or `UNCONFIG_DCB` for it.

One volume record live, one thread that cannot exit, VMM spinning on the thread list. **Start four,
close three — the project owner's own framing, and the only asymmetry left standing.**

### Method note that made the difference: bisect by REMOVING, but measure the census either way

Three fixes were shipped on strong theories in this investigation — handling `AEP_UNINITIALIZE`,
restoring the `AEP_FAILURE` default, the fast-fail clamp — and all three were negative. What moved
it was one hook that dumped raw bytes (the THCB signature) and one build that removed a feature
rather than adding a fix. **When a symptom has resisted three fixes, the next change must remove
something or print something, not fix something.**

### Technique 91a: match hex CASE-INSENSITIVELY, or a zero result will look like a finding

2026-09-04, three times in one evening. A log you did not format yourself mixes cases, and a
mismatched grep returns **nothing** - which is indistinguishable from "this never happened".

```bash
grep -abo "DBGPORT tag=10 val=0000000e" run.log   # 0 hits: the log writes 0000000E
b=$(... | cut -d: -f1)                            # b is empty
tail -c $((tot-b)) run.log > slice.txt            # silently the WRONG slice
```

That slice appeared to show the card's option ROM at `D000`, real-mode segments and our own
driver all executing during the shutdown wedge - a spectacular finding, and pure contamination
from the DOS boot phase. Re-run with the right case: 73 samples after `SYSTEM_SHUTDOWN`, **all**
selector `0028`, nothing else.

**Rules:**
- `grep -i` for any hex or symbol you did not print yourself in this session, or normalise both
  sides (`tr a-f A-F`). Cheaper than being wrong.
- **A shell variable built from a grep must be checked before it is used in arithmetic.**
  `$((tot-b))` with `b` empty does not fail loudly; it produces a slice you will then interpret.
  `[ -n "$b" ] || { echo "MARKER NOT FOUND"; exit 1; }`.
- Same family as technique 19 (a gated hook showing zero) and technique 28 (a patch script that
  "succeeds" by doing nothing): **whenever a zero result is itself the interesting answer, prove
  the query could ever have matched.**

### ⚠ Technique 91 CORRECTED, same evening: the thread list is EMPTY at the wedge

Technique 91 above establishes that the queued nodes are `THCB`s - Thread Control Blocks - and
that stands, it came from a signature dump. **The conclusion drawn from it does not.** Commit
`8fe1609` ("VMM waits for a list our driver is still on") is retracted.

Walking the list at the wedge, from inside the emulator:

```
THREADWALK sentinel C001080C  +0=51494157 +4=C001080C +8=C001080C
THREADWALK end: 0 nodes, last=C001080C (sentinel=C001080C)
```

`[C0010810] == C001080C`, so the `cmp/jne` at `C0008F8A` **passes**. VMM is not waiting for the
System VM thread list to drain; that list is already empty. The 107 writes were ordinary thread
churn during the session, and the last value written was simply the last thread to be linked -
not a leftover, and never ours.

**The lesson is technique 90d's, one level up.** There, a backward `jne` was called a loop without
computing its target. Here, a list-empty test was called a wait without ever reading the list.
**Both times the fix was to dump the actual state instead of reasoning about what the instruction
implied - and both times the dump took one hook and one run.**

### The real loop, and the next target

The non-terminating walk is the OTHER cluster, and it hangs off a thread:

```asm
C0003221:  8B 47 6C     mov  eax,[edi+6Ch]    ; EDI = C159F068, a THCB
C000323C:  8D 50 04     lea  edx,[eax+4]
           8B 42 FC     mov  eax,[edx-4]      ; eax = eax->next
           85 C0        test eax,eax
           85 48 08     test [eax+8],ecx      ; ecx = 80000082
C000324D:  EB F0        jmp  back
```

`EAX` revisits `C0FCF51C`, `C0FCF0F8`, `C0FCEB50`, `C0FCF120`, `C10CD860`, so the walk does not
terminate. **Next measurement: dump the full TCB at `C159F068` and walk the list at `+6Ch`.**

### Still standing after eight boots, and worth not re-deriving

- The orphan is named: VRP `C0FD65C8`, **drive 2 = `C:`**, created after `AEP_ASSOCIATE_DCB` on our
  DCB `C0FD6490` and never destroyed. `A:` (`C0FD2C6C` -> `C0FD2C38`) is created and destroyed on
  the same machine in the same second. Destroys report the VRP **0x34 below** the create's pointer;
  pair on that offset.
- IOS abandons our teardown after `AEP_DCB_LOCK`: `A:` runs `21·16·19·4` on one DCB, ours runs
  `21` on `C0FD6490` then `16` on a *different* object and stops.
- Windows is genuinely 32-bit on this disk: after Windows starts, **5,039,676 XT-IDE accesses, every
  one from selector `0028`, zero from `D000`**. The real-mode INT 13h traffic is all DOS boot.
- **Four DDK contract fixes made tonight are correct and changed the teardown by not one AEP** -
  answering `AEP_PEND_UNCONFIG_DCB` (21) instead of `AEP_FAILURE`; answering `IOR_FLUSH_DRIVE` and
  the other honest no-ops instead of `IORS_INVALID_COMMAND`; quiescing data movement on code 21;
  and declaring `DRP_ESDI_PD` instead of the sample's `DRP_MISC_PD`. Keep them all; none is the bug.
- Our load group was the sample's `'Generic Port Drv'`. Read the DRP straight out of the binaries:
  `ESDI_506` = `ESDI_PD`, `SCSIPORT` = `NT_PD`, `HSFLOP` = `NEC_FLOPPY`, ours was `MISC_PD`. The
  `DRP` struct is `eyecatcher[8], LGN dd, aer dd, ilb dd, name[16]` - scan for a single-bit LGN
  followed by a printable name.

### Two ways a change failed to reach the artefact tonight - check the BUILT thing, always

1. **Unreachable dispatch.** A new `cmp si,AEP_PEND_UNCONFIG_DCB` block was inserted *after* the
   `jne pa_not_unconfig` whose label sat below it, so nothing ever reached it. It assembled, it
   linked, and its "negative result" was not a test. Read the generated `.ASM`/listing for the
   label order, not the source you wrote.
2. **A regex that hit a comment.** `edit()` uses `count=1`, and `PORT.ASM` mentions `DRP_MISC_PD`
   in a comment *before* the `DRP <...>` initialiser - so the patcher reported success having
   rewritten prose. The md5 was unchanged and that is what caught it. **Anchor a patch on the
   syntax it must change, and diff the md5.**

---

## ⭐ Mining Microsoft's own port drivers, 2026-09-04 late — what a clean driver has that we don't

The owner's call, and the right one: *"we have something for a regular IDE disk and other drivers
that can be used as almost a template - all the answers of what we need has to be in them."*
`ESDI_506.PDR` does **exactly our job** and tears down cleanly on this machine seconds before we
fail to. No boots were spent on any of what follows.

Tooling now in the repo, because a scratchpad does not survive:
`tools/vxd_disasm.py` (handles `CD 20` + inline 4-byte service id, which desyncs every naive
disassembler), `tools/vxd_drp.py`, `tools/vxd_aep_audit.py`, `tools/vxd_isp_audit.py`.
Requires `capstone` (5.0.7 present).

### ESDI_506's entire AER — disassembled, not inferred

```asm
0611  mov  word ptr [ebx+2], 0        ; preset AEP_SUCCESS
0617  mov  ax, word ptr [ebx]         ; AEP_func
061A  cmp ax,2  je BOOT_COMPLETE      0624  cmp ax,0   je INITIALIZE
062E  cmp ax,0Fh je UNINITIALIZE      0638  cmp ax,6   je DEVICE_INQUIRY
0642  cmp ax,3  je CONFIG_DCB         064C  cmp ax,5   je IOP_TIMEOUT
0656  mov  word ptr [ebx+2], 0FFFFh   ; AEP_FAILURE to everything else
065C  ret
```

**Six codes. `AEP_FAILURE` for the rest — including `AEP_PEND_UNCONFIG_DCB` (21) and
`AEP_DCB_LOCK` (16).** `HSFLOP` dispatches six too, and also ignores 21. We dispatch fifteen.

**This retires the whole "we answer the teardown wrongly" family.** Windows' own IDE port driver
refuses exactly the codes tonight's fixes were written to answer, and shuts down cleanly. Four
builds went into that theory and every one changed the teardown by not a single AEP - which is
consistent, and now explained.

### The verified ISP service comparison

Counts below are **disassembly-verified**, not scanned - see the trap below.

| ISP service | ours | ESDI_506 | HSFLOP | SCSIPORT |
|---|---|---|---|---|
| `CREATE_DDB` | 1 | 1 | 1 | – |
| `INSERT_CALLDOWN` | 2 | 1 (+1 conditional, same packet reused) | 1 | – |
| **`CREATE_DCB`** | **1** | – | – | – |
| **`ASSOCIATE_DCB`** | **1** | – | – | – |
| **`DESTROY_DCB`** | **1** | – | – | – |
| `DEALLOC_DDB` | 2 | 1 | – | – |
| `CREATE_IOP` / `ALLOC_MEM` / `DEALLOC_MEM` | – | 2 / 1 / 1 | – | 1 / 1 / 3 |
| `GET_DCB` | – | – | 2 | – |
| `DEVICE_REMOVED` / `DEVICE_ARRIVED` | – | – | 1 / 1 | – |

**No Microsoft port driver creates, associates or destroys a DCB.** That is TSD work. We are the
only one doing it - the "be our own TSD" edifice from technique 83, whose premise (a dynamically
registered driver gets no TSD engagement) is already known to be false on the faithful bed, since
DiskTSD assigns `C:` on its own.

**Caveat that stops this being the hang:** in the `-NoVolume` configuration none of that code
executes, and it still wedges. Our *executed* ISP set there is `CREATE_DDB` + one
`INSERT_CALLDOWN` + `DEALLOC_DDB` - near-identical to ESDI_506's.

### The calldown packet, field by field

`ISP_insert_calldown` is `hdr(4), dcb, req, ddb, expan_len(w), flags(dd), lgn(b)`.

| field | ESDI_506 | ours |
|---|---|---|
| dcb / req / ddb / lgn | set, `lgn` from `AEP_lgn` | same |
| **expan_len** | **`24h`** - 36 bytes of per-IOP scratch | **`0`** |
| **flags** | `0`, or `DCB_dmd_phys_sgd` (`800h`) on a capability test | `DCB_dmd_small_memory` (`10h`), unconditional |

Both packets are well-formed, so an earlier worry that the sample left fields as stack garbage was
unfounded. `ISP_i_cd_flags` legitimately takes `DCB_dmd_*` values - `STORAGE.DOC`: *"a layer driver
that satisfies a specific demand stipulated in the DCB dmd flags must turn off the demand bit"*.

**The one difference that is at least coherent with a known contract gap:** per-IOP scratch is what
a driver needs to carry state across a **deferred** completion. ESDI_506 asks for 36 bytes; we ask
for none, because we complete inline. That is self-consistent with our design and different in kind
from how every Microsoft port driver is built - see technique 88's polling contract. **Not a proven
cause. Recorded as a design difference.**

### ⚠ The trap that produced a false lead, and the constraint that kills it

A byte scan for `mov word ptr [reg+disp], imm16` reported `ISP_DISASSOCIATE_DCB` in both working
drivers and none in ours - a beautiful "start four, close three" story that was **completely
wrong**. Disassembling the site:

```asm
0000075F  mov word ptr [edi + 0x68], 0xf     ; a DCB field store. Not an ISP packet.
```

`0Fh` is `ISP_DISASSOCIATE_DCB` *and* an ordinary field value. Worse, the "verify it by checking an
indirect CALL follows" heuristic **also passed it**, because an unrelated call happened to be 44
bytes later.

**`ISP_func` is at offset 0 of the packet, so only a ZERO-displacement store can be one.** With
that constraint the false positive disappears and the table above is stable. `tools/vxd_isp_audit.py`
enforces it.

Generalise: technique 75 already says a raw byte scan is not evidence and each hit must be confirmed
by its idiom. **Add: confirm it against the STRUCTURE's own layout.** A field's offset is a hard
constraint that a plausibility heuristic is not.

---

## ⭐⭐ THE GAP, 2026-09-04 — ESDI_506 returns without completing. We never do.

Read out of `ESDI_506.PDR`'s own request routine (the calldown target named in its
`ISP_insert_calldown` packet, `+8 = 284h`, i.e. object 1 + 284h = file 884h). No boots.

```asm
0884  mov  ebx,[esp+4]              ; the IOP
0888  mov  edi,[ebx+10h]            ; IOP -> DCB
088B  mov  edi,[edi+8]              ; -> the controller object
0891  cli
...                                 ; link this IOP into the controller's queue
08DD  bts  dword ptr [edi+2Eh], 9   ; TEST-AND-SET a busy bit
08E2  jb   98Ah                     ; already owned? -> branch away
08E8  xor  ebx,ebx
08EA  xchg dword ptr [edi+6Ah],ebx  ; atomically take the queue head

098A  sti
098B  ret                           ; <-- RETURNS WITHOUT COMPLETING
```

**A working Win9x port driver serialises the controller with a lock bit, queues the request, and
returns without touching `IOP_callback_ptr`.** The owner of the lock completes it later.

`XTIDE_StartRequest` has exactly one exit. Every path - success, invalid command, quiesced,
bad sector, I/O error - falls through to `xsr_complete`, which calls `IOP_callback_ptr` inline
before returning. **We have no queue, no lock, and no deferred path whatsoever.**

This is the I/O Supervisor Guide's contract, quoted in technique 88: *"Immediately after this
Set_Global_Time_Out call, simply return (WITHOUT doing a JMP to the IOP_callback_ptr routine).
This releases the system from your driver, so the system can run normally for a while."*

### Why this is different from the retracted version of the same idea

Technique 88 reached the polling contract from a binary that could not be rebuilt, and that
diagnosis was correctly retracted - the evidence was void, not the contract. It is back now for a
different reason: **it is what remains** after eliminating, by measurement or by construction:

- both VMM list-waits (`C001080C`, `C0010C98` - walked, both empty at the wedge)
- the per-thread chain at `TCB+6Ch` (walked, terminates at NULL after one node)
- the published volume and the appy-time event (`-NoVolume`, still wedges)
- **the entire own-TSD path** (stripped from the image, verified absent, still wedges)
- every AEP answer (ESDI_506 answers `AEP_FAILURE` to 21 and 16 and shuts down cleanly)
- the load group, the IOR command set, the DDB retention

Our ISP profile now matches ESDI_506's exactly: `CREATE_DDB`, `INSERT_CALLDOWN`, `DEALLOC_DDB`.
The remaining measured differences are all facets of one thing:

| | ESDI_506 | ours |
|---|---|---|
| VMM service calls | **48**, incl. `Set_Global_Time_Out` / `Set_Async_Time_Out` | **0** |
| calldown `expan_len` | `24h` - 36 bytes of per-IOP scratch | `0` |
| completion | queue + lock bit, return without completing | inline, always |

Per-IOP scratch is what a driver needs to carry state across a deferred completion. We request
none because we never defer. All three are the same design decision seen from three angles.

### It is still NOT proven, and here is what would prove it

A driver that holds the system inline works perfectly while the drive answers - which is why
3527/3527 IOPs complete and the desktop is fine. `System_Exit` is precisely where the scheduling
context an inline driver leans on is torn down. That fits, and "fits" has been wrong six times.

**Bisect it, do not build it.** The full restructure is large. The cheap first step is to prove
IOS tolerates a deferred completion at all: on ONE request, arm `Set_Global_Time_Out` and return
without completing, then complete from the timeout handler. If that boots, the model is viable and
the restructure is worth doing. If it hangs the boot, we have learned that for one build.

`HSFLOP.PDR` is the better template than `ESDI_506` for this machine: it is the driver that must
survive a device which may simply not answer, and technique 88 already records its idiom -
`Set_Global_Time_Out` at `2D95h` then `Wait_Semaphore` 43 bytes later at `2DBEh`, with a single
`Signal_Semaphore_No_Switch` elsewhere. Arm a timeout, block on a semaphore, let the timeout
handler signal it. **A port driver MAY block, as long as something asynchronous can free it** -
and that is a far smaller change than a full state machine.

### The DCB declaration gap — ESDI_506's CONFIG_DCB vs ours

Its handler starts at file `3617h` (`AEPHDR` is 12 bytes, so `AEP_d_c_dcb` is `[ebx+0Ch]`):

```asm
361A  mov  ecx,[ebx+0Ch]                   ; the DCB
3620  cmp  byte ptr [ecx+0BAh], 1          ; DCB_unit_on_ctl - master or slave
362C  mov  [edx], ecx                      ; link the DCB into the DDB
362E  or   dword ptr [ecx+20h], 0C0000000h ; DCB_device_flags, top two bits
3635  mov  dword ptr [ecx+50h], 0FFFFFFFFh ; DCB_max_xfer_len   = unlimited
3649  mov  byte ptr [ecx+77h], 11h         ; DCB_max_sg_elements = 17
```

Offsets resolved from `BLOCK/INC/DCB.INC`. **Careful with that file**: `DCB` embeds
`DCB_COMMON DB SIZE DCB_COMMON DUP (?)`, so a naive offset walk is short by `4Fh` for every
field after it - `DCB_COMMON` is `50h` bytes, not one.

| field | ESDI_506 | ours |
|---|---|---|
| `DCB_device_flags` | `or C0000000h` | set (`PHYSICAL`/`WRITEABLE`) |
| `DCB_max_xfer_len` | `FFFFFFFFh` | set |
| **`DCB_max_sg_elements`** | **`11h` (17)** | **never written - stays 0** |

**We declare support for zero scatter/gather elements and are handed scatter/gather lists anyway.**
Technique 85 is the record of that: `IORF_SCATTER_GATHER` requests do arrive, and misreading them is
what wrote a descriptor list into the volume's root directory. The read path never sees them because
VFAT's mount-time reads are single-buffer; its writes are scattered.

Not claimed as the shutdown cause. It is a verified declaration gap of exactly the class worth
hunting, it is one byte to fix, and it is testable in one build.

### ⚠ CORRECTION, and the lead is BETTER for it — read Andrew's IOS Guide, it has the code

**I dismissed `C:\IOSGuide\IOS_Guide.doc` earlier this session as "a dead end for these codes".
That was wrong** - based on four exact tokens (`DCB_LOCK`, `CREATE_VRP`, `DESTROY_VRP`,
`PEND_UNCONFIG`) not appearing. A plain printable-run scan in latin1 extracts **194,000
characters**, including 488 mentions of `DCB`, 21 of `calldown`, 12 of `semaphore` and 8 of
`IOP_callback_ptr`. It contains what we spent the evening reverse-engineering from binaries.

What is actually in it:

1. **The polling procedure, step by step**, naming `ILB_enqueue_iop` / `ILB_dequeue_iop`, and
   quoting **ESDI_506's own source**: `cli` / `push esi` / `push ebx` /
   `call [esdi_ilb].ILB_enqueue_iop`. That matches the `bts`/queue/`ret` shape disassembled out of
   `ESDI_506.PDR` at `08DDh`/`098Ah`, so document and binary agree.
2. **Full assembly source of `IOS_serialize` and `IOS_serialize_callback`** - a working
   serialisation implementation, including the callback-stack insertion
   (`mov [edx.IOP_cb_address], offset32 IOS_serialize_callback` /
   `add [eax.IOP_callback_ptr], size IOP_callBack_entry`).
3. A semaphore worker-thread variant, described as a *"reportedly successful implementation of an
   IOS port driver"*: `CreateSemaphore` / `VWIN32_CreateRing0Thread` / `Wait_Semaphore` / dequeue /
   callback, with the IO handler doing enqueue-signal-return.

**And a correction to the gap itself.** I wrote that we "complete inline, always - no queue, no
lock, no deferred path". False. `PORTREQ.ASM` in our own build already does:

```asm
call XTIDE_WantIop                 ; ours?
call [port_ilb].ILB_enqueue_iop    ; queue it
bts  [edi].DDB_port_flags, DDB_BF_ACTIVE_BIT
jc   Port_rq_ret                   ; already active -> return, no completion
call Port_Start_Request            ; else drain the queue
...
call [Port_ilb].ILB_dequeue_iop
```

**The queue, the lock and the return-without-completing are all present, from the sample.** They
are the same shape ESDI_506 uses. What is missing is only the step *inside the wait*: the Guide
requires `Set_Global_Time_Out` and a return, with the polling handler finishing later. We instead
busy-wait `XT_SPIN` = 400,000 `in al,dx` **while holding `DDB_BF_ACTIVE_BIT`**, so the queue's lock
is held across the whole wait and the system is not released.

That is a far smaller change than the restructure feared in technique 88, and the Guide gives the
shape for it. `Port_iop_timeout` (`AEP_IOP_TIMEOUT`) is also still an empty stub that answers
`AEP_SUCCESS` having done nothing - it is the natural home for the polling handler.

**Method note worth keeping:** a keyword miss is not a document review. Extract the text, measure
how much came out, and grep for the *concepts* (`polling`, `calldown`, `semaphore`) before
concluding a primary source has nothing. This one had the answer for a day and a half.

## 🛑 THE RULE THIS SESSION PAID FOR — read every source you are given

**When someone hands us a source, read it and decide whether it is useful. Each one, at intake.**

@andrew-hoffman posted the *I/O Supervisor Guide* on 2026-09-03. It was downloaded to
`C:\IOSGuide\IOS_Guide.doc` the same day. On 2026-09-04 twelve emulator boots and most of a
session went into reverse-engineering, from binaries, the polling contract and the serialisation
shape **that document states in plain English and supplies as assembly source.** It was on disk
the whole time. I opened it once, grepped four tokens, got no hits, and wrote it off.

- A **keyword miss is not a document review.** Extract the text, print how much came out, and grep
  for the *concepts* before concluding anything.
- **When @andrew-hoffman names a document, read it before writing code.** The IOS punt, the `.INF`
  class, the vendor README, the RAM-granularity bug, the published XT I/O map, and now this - a
  strong enough hit rate that a pointer from him is worth an hour of reading on sight.
  **Corrected at his own request, 2026-09-05:** an earlier version of this line said he "has not
  been wrong yet". He asked for that removed - *"I've been wrong lots of times, and I did waste
  some of your time not realizing about the difference between the 8 bit XT-CF and the full XT IDE
  interface."* The rule was never about him being infallible, and stating it that way makes it
  easier to follow a steer without checking it. **Read the source he names; then verify it against
  the machine like any other source.**
- The project already had this rule for vendor READMEs (technique 75, which cost two patch rounds
  on the LS-120). **It applies to every source, not just READMEs.** Add it at intake, with a note
  saying whether it was read and what it contained - including "nothing relevant", which is a
  result worth recording.

---


---

## Technique 92: read the STRUCT VARIANT before reading a field by offset

2026-09-05. Four runs on the faithful bed chasing the XT-IDE port driver's shutdown wedge. The
location narrowed; the cause did not. What generalises is how the numbers misled.

### Our driver holds nothing - measured twice, and it retires a theory

| | run 3 | run 4 |
|---|---|---|
| Offered / Accepted / Started / Done | `3267` x4 | `3359` x4 |
| our DCB freeze / io_pend / lock | `0 / 0 / 0` | `0 / 0 / 0` |

Balanced both runs. **Nothing of ours is outstanding when the teardown stalls**, which kills the
polling-contract diagnosis *as the cause* (technique 88's "THE GAP") on measurement rather than
argument. The contract violation is real and worth fixing for responsiveness - @zikolas's
`ASYNC-ENGINE.md` records that his synchronous polled engine works, freezes the UI, and tears down
cleanly - but it is not what wedges the shutdown.

### The teardown always stops in the same place

```
21 PEND_UNCONFIG C104D9FC -> 16 DCB_LOCK C104D9FC -> 19 DESTROY_VRP -> 4 UNCONFIG   completes
21 PEND_UNCONFIG C104E1A0 (ours, quiesce matched) -> 16 DCB_LOCK C104D8F4 -> stops
14 SYSTEM_SHUTDOWN -> wedge
```

The one field that IS valid on a logical DCB says why that matters: at the wedge `C104D8F4`'s
`DCB_vrp_ptr` still holds `C104CCEC` - the same VRP announced at `AEP_MOUNT_NOTIFY`. **The volume
was never unmounted.** IFSMGR takes the exclusive lock and does not complete.

### Dead on data, not on opinion

`AEP_d_l_drives` (hdr+16, the bitmap IFSMGR reads back from `AEP_DCB_LOCK`) is `00000000` on the DCB
that tears down cleanly AND on ours. Not the discriminator. Settled in one run, because the driver
already emitted hdr+16 for every AEP.

### The trap: an offset is only valid for the struct VARIANT it belongs to

`DCB_queue_freeze` / `max_sg_elements` / `io_pend_count` / `lock_count` are four consecutive bytes at
`76h`-`79h`. Confirmed three ways - `BLOCK\INC\DCB.INC`, `ESDI_506.PDR`'s own
`mov byte ptr [ecx+77h],11h`, and three live `.IDCB` dumps in the I/O Supervisor Guide. All correct,
and all about the **DCB physical data extension**, which the Guide's dump labels by name.

A **logical** DCB has no physical extension. Reading those offsets off one gave
`freeze=4, sg=193, lock=253` - numbers that look like counters, are not, and are plausible enough to
theorise on. A physical DCB that was not ours read `io_pend=244` in both runs while `freeze` moved
`1 -> 9`: a value identical across runs while its neighbour changes is a tell that you are not
reading what you think.

**The rule: establish which struct VARIANT an object is before reading any field by offset.**
`INC32\AEP.H` gives it free - `AEP_assoc_dcb` +12 is a **physical** DCB, `AEP_lock_dcb` +12 is a
**logical** DCB, `AEP_vrp_create_destroy` +12 is a **VRP**, and `AEP_sys_shutdown` has no field after
the header at all, so `[ebx+12]` there reads off the end. Record which AEP named an object and let
the decoder label it. Technique 91 said identify by signature, not address; this is the same lesson
one level in - identify by *declaration*, not by offset arithmetic.

### Size the capture for the interesting event, not the first one

The first table had 8 slots, filled during startup, and missed `C104D8F4` - the one object the
teardown stops on. A capture that runs out of room before the event is not a measurement.

---

## Technique 93: a diagnostic can throttle the emulator into looking correct

2026-09-05. `hdc_xtide.c` logged one formatted line, with CS:EIP, per **PIO byte**. The Lo-tech
XT-CF's data register is 8-bit, so one 512-byte sector is 512 lines before any status polling:
**11,096,197 lines and 438 MB per boot-and-shutdown**, with 86Box reporting **2-14%** of configured
speed. Gating it behind `XTIDE_TRACE` (default off; the device prints which mode it is in) took the
same bed to a steady **100%**.

- **The title-bar percentage is the ground truth, and it was there the whole time.** This file has
  said so since the "clone runs too fast" write-up. Nobody looked, because the runs "worked".
- **Removing a brake exposes what it hid.** With the trace off the owner immediately saw the BIOS RAM
  count now runs *faster than the real 5160*. That is a genuine `cpu_speed`/waitstate calibration gap
  the logging had been masking. It is not new, it is newly visible - do not put the brake back.
- **Technique 21 again**: that hook's question (does our driver reach the card, at what stride) was
  answered on 2026-09-04. It taxed every boot afterwards.

### Every run gets a fresh image, restored from a master that is never booted

Two runs were abandoned by force-killing 86Box while Windows was running and writing. The next boot
**froze on the Windows splash**, IO.SYS looping at `0070:0465` into the BIOS at `F000:ACxx`, in real
mode - i.e. before our driver loads, so the driver could not be the cause. There was no backup of
that bed's image, and **ScanDisk is not a recovery route: it misbehaves under emulation here.** An
image damaged by a kill is gone.

Technique 23 warned that repeated forced kills poison the next boot. What it did not say:
**the recovery has to exist BEFORE you need it.** `tools/pdr_inboard_run.ps1` now restores
`<image>_master.img` over the working image every run, refuses to start without it, and takes
`-Deploy` so the driver goes in *after* the restore instead of being wiped by it.

Not tidiness: a harness you cannot safely abandon pressures you into keeping a bad run alive - which
is exactly what happened, at 2% speed, for forty minutes.

### Build gotcha, because it costs a full cycle every time

`cmake --build` on `86box_upstream` exits **1 with no diagnostic at all** unless
`/c/msys64/mingw64/bin` is on `PATH`. The same command by hand compiles cleanly, so it looks like a
locked file or a stale object.

```bash
PATH=/c/msys64/mingw64/bin:$PATH cmake --build build -j 8
```

### A documentation-backed "cheap, certain" change still needs measuring

`DCB_max_sg_elements = 17` is what the Guide states, what `ESDI_506` writes, and what three live
`.IDCB` dumps show. Shipped as certain. Measured after, on matched 60 MB windows of protected-mode
traffic - same bed, same image, that byte the only difference:

| | max_sg=0 | max_sg=17 |
|---|---|---|
| `[R] 0300` data bytes | 1,149,041 | 975,008 |
| `[W] 0005/6/7` taskfile commands | 603 | **1,104** |
| bytes per command | 1,906 | **883** |

**1.8x the commands for the same data** - IOS hands us scatter/gather lists and the transport issues
one taskfile command per descriptor instead of one per request. Now behind `-DXT_SG=1`, default off,
until the request path coalesces descriptors.

**Three sources agreeing on what a field MEANS is not evidence about what it DOES to this
transport.** It also shipped in the same binary as an unrelated diagnostic change, so even a clean
run would have attributed nothing. One variable per build, which this file already says.

---

## Technique 94: substitute a KNOWN-GOOD component as a control, before rewriting your own

2026-09-05, and it settled the XT-IDE port driver's shutdown wedge (#21) in one evening after
four sessions of narrowing.

Technique 88 says bisect by **removing** your component. That answers "is it ours?". It does not
answer the question you actually need before committing to a rewrite: **"does the thing I am
about to rewrite it as even work on this machine?"** For that, put someone else's working
implementation of the same job in the same place and watch it.

`T130.MPD` — Adaptec's Trantor T130B SCSI miniport, 10 KB, polling with no IRQ — installed on
the same Windows image the wedge reproduces on, with our `PORT.PDR` removed from `IOSUBSYS` and
its device node removed in Device Manager:

| arm | device held | shutdown |
|---|---|---|
| CD-ROM only | D: | clean |
| CD-ROM **+ FAT16 disk, read and written** | D: + E: | clean |

So the wedge is not a property of this machine's Win95 storage teardown, and the SCSIPORT stack
— DiskTSD, VFAT, IFSMGR, the volume lock, the cache flush — is sound here. That justified the
miniport port on measurement instead of argument. Full costing: `docs/scsi_miniport_costing.md`.

**Two arms, not one.** The CD-ROM arm proves the driver loads and claims; it does **not** exercise
DiskTSD/VFAT/IFSMGR, which is where our wedge lives. A control that does not reach the failing
layer is not a control. Escalate to the device class that matches the bug.

**Verify the control from OUTSIDE the guest.** The owner wrote a file; it was read back out of
`scsi_test.img` on the host, proving the write reached the medium and the shutdown flushed the
cache. Technique 79's discipline, used on someone else's driver.

**And do not read a clean shutdown out of the boot log alone.** Technique 88 already records that
`EndTerminate = KERNEL` is the last line whether the shutdown completed or hung. The evidence is
the safe-to-turn-off screen; the log corroborates. What the log *does* settle unambiguously is
whether every stage that started also closed - count `Terminate` against `EndTerminate` and name
any unpaired stage.

### The prerequisite everyone skips: prove the driver RAN, not that it INSTALLED

The first shutdown of the evening was clean and meant nothing. The driver had been installed but
the machine had not been rebooted, so it had never loaded - the run was the already-known empty
baseline. `BOOTLOG.TXT` said so, and said it in the most misleading way available: it was **stale**,
still naming `port.pdr` from a boot two images ago (technique 23). Delete the log before the run,
then require `Initing <driver>` / `Init Success <driver>` in the new one. Technique 81, one level
down: a green result is only evidence if the thing you changed actually ran.

### A bed whose config names an image that does not exist boots to ROM BASIC

Copying a bed's config and renaming its image left `hdd_01_fn` pointing at the old name. 86Box
**created a blank 2 GB image** at that name - no MBR signature - and the machine fell through to
ROM BASIC. Nothing errored: the file existed, the machine name was right, every harness check
passed. `pdr_inboard_run.ps1` verifies the image file exists but never that the **config points
at it**; that is technique 69's silently-ignored-config in a new place. Assert
`hdd_01_fn` resolves to a real file with an MBR before booting, and delete blank images 86Box
invents - they look like real beds afterwards.

### Structural facts about miniports, checked against binaries

- **A `.MPD` is a PE file, not an LE VxD** - i386, subsystem NATIVE. None of the `.DEF` /
  LE-page / dynamic-load-flag / `W4`-combine problems that cost sessions on the `.PDR` apply.
- **Buildable with the toolchain already in use**: `ML.EXE /coff` + the DDK `LINK.EXE`
  `/SUBSYSTEM:NATIVE` against `BLOCK/LIB/SCSIPORT.LIB`. The DDK ships `SRB.INC` / `SCSIPORT.INC` /
  `MINIPORT.INC` for exactly this. There is no `CL.EXE` in the DDK and none is needed.
- **SCSIPORT owns the polling contract**: `ScsiPortNotification(RequestTimerCall, …)`, and
  `PollingSupportNeeded` is a literal registry value inside `SCSIPORT.PDR`. The contract technique
  88 found us violating is the OS's job in this model.
- **`AdapterSettings` gives a miniport a GUI Settings tab**, passed to `HwFindAdapter` as
  `ArgumentString` - confirmed present on the installed T130B. Base address, stride and transport
  mode become user-settable without hand-edited registry keys.

### ⚠ The DDK ships debug builds WITH SYMBOLS of the whole storage stack - `Windows95_ddk/DEBUG/`

`IOS.VXD` + `IOS.SYM` (named entry points at real offsets), `SCSIPORT.PDR` debug (83 KB vs 23 KB
retail) + `SCSIPORT.SYM`, `DISKTSD`, `DISKVSD`, `CDTSD`, `CDVSD`, `VMM.VXD` + `VMM.SYM`, plus
`WDEB386.EXE` and `DEBUGCMD.TMP` - which provides `.pthcb` (dump a thread control block), `.psem`,
`.pmtx` and `.ps` (ring-0 stack with labels).

Technique 91 spent a session identifying TCBs by dumping bytes for a `"THCB"` signature. `.pthcb`
prints them. Technique 92's struct-variant disaster is what a symbol file exists to prevent.

**It has been on disk since 2026-08-23.** This is the "read every source you are given" rule again,
and this time the source was the DDK we build with daily. **Untested:** the binaries are dated
1996-06-06 (OSR2 era) against an OSR1 build-950 guest, and the `950/951/952/953` per-build
subdirectories are empty. Check version compatibility on the bed, never on the 5160.
