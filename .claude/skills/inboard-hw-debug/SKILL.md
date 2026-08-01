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
