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
