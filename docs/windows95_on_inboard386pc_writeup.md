# Windows 95 on the Intel Inboard 386/PC: How We Got There

**Status: first confirmed working Windows 95 desktop (keyboard, mouse, and a 32-bit application)
on a real IBM 5160 fitted with an Intel Inboard 386/PC accelerator card, 2026-08-05.**

This document is a technical account of the investigation that got Windows 95 booting to a usable
desktop on the Inboard 386/PC — a 1987 386 accelerator daughtercard for the original IBM PC/XT —
and the fixes involved. As far as we've been able to establish (via FastDoom's own documentation,
a research pass, and direct correspondence with Al Williams, who did hands-on Inboard development
work at the time), this specific combination — Windows 95, real XT + Inboard hardware, working
keyboard and mouse — has not been documented as working before.

## Summary of what works now

- Windows 95 boots to a full GUI desktop, with the Start Menu built and populated.
- Keyboard input works throughout — system dialogs, text-entry fields, everything tested.
- Mouse input works.
- 32-bit Windows applications run (confirmed with the bundled FreeCell).
- Floppy drives A: and B: are detected.

## Still open

- SCSI and other attached peripherals not yet detected — need drivers identified/adapted.
- No sound.
- No network.
- The one fix that hasn't been proven on real hardware in isolation (`IVT68FIX.COM`, see below)
  worked in combination with everything else, but a reboot was needed to get past a black-screen
  stall while Windows was finishing Start Menu/Help setup — the exact cause of that stall, and
  whether it's related to `IVT68FIX.COM`'s timing, is not yet confirmed.
- COMR95 (the Windows 95 half of the COMrade real-hardware bridge, see credits) is built and
  deployed but its correct Win32 command-line syntax hasn't been nailed down yet — it errored on
  first boot. Reaching out to the tool's author is the planned next step.

## Why this is hard

The Intel Inboard 386/PC replaces the IBM PC/XT's 8088 with a real 80386, running its own
`INBRDPC.SYS` device driver (from 1987 — predates Windows 95 by eight years) for RAM mapping and
CPU control. Critically, the XT motherboard underneath has **no real 8042 keyboard controller** —
just the original PC/XT's simpler keyboard interface (a shift register, a strobe line, and status
bits on port 0x61) and **no second (slave) 8259 PIC**, so IRQ8–15 don't exist on this hardware
either. A20 gating, which a real AT does through its keyboard controller, has to go through a
custom Inboard-specific mechanism instead.

Windows 95 — and, underneath it, DOS-era software like HIMEM.SYS — was written assuming genuine AT
hardware: a real 8042 at ports 0x60/0x64, a real second PIC, and the standard AT A20 protocol.
Every one of those assumptions is wrong on this machine. The story below is largely about finding,
one at a time, the places where Windows silently assumed AT hardware that isn't there.

None of this would have been possible starting from zero. The Inboard 386/PC hardware model in this
project's 86Box fork (`86box_full/src/device/inboard386.c`) is a direct **port of SuperFury's
`hardware/inboard.c` from UniPCemu** (2019-2022) — itself built from disassembly of `INBRDPC.SYS`
and primary-source correspondence with Al Williams. That existing, working reference model — memory
waitstate control, A20 gating without a real 8042, BIOS ROM shadow/cache — is the actual foundation
everything in this document is built on top of; without it, this investigation would have started
by reverse-engineering the base hardware itself, not by fixing Windows 95's assumptions about it.

## Chronology of fixes

### 1. Baseline: proving the emulator matches real hardware

Before trusting any emulator-side investigation, we booted a completely unmodified Windows 95 OSR1
install (stock `INBRDPC.SYS`, no patches) on both the emulator and the real 5160, and compared the
resulting `VMM32.VXD` (Windows' combined virtual-device driver blob, built during Setup) and
`SETUPLOG.TXT` byte-for-byte. They matched — same size, same stall point
(`Please wait while Setup updates your configuration files...`). This is what licensed treating the
emulator as a reliable stand-in for real-hardware behavior for the rest of the investigation.

### 2. INBRDPC.SYS: a redundant self-test that never terminates cleanly

Full recursive-descent disassembly of `INBRDPC.SYS`'s `INT 15h AH=87h/88h` handler (its extended
memory block-move API) found a self-test-and-report wrapper that checks a completion flag before
deciding whether to redo a 32KB block-move-and-verify pass. That flag is **never written anywhere
in the file** — it always reads its on-disk default, so the self-test unconditionally re-runs every
time this path is reached during Setup, adding enormous, pointless overhead right when Setup is
already doing the most I/O.

**Fix:** flip that one flag word (file offset `0x6BA`, `0x003C` → `0xFFFF`) so the self-test reports
"already done" immediately. This alone got Setup past a stall that had blocked every previous
attempt at this project, on both OSR1 and the earlier OSR2 track.

### 3. INT 68h: an uninitialized interrupt vector and a wild jump into real code

Deeper in Setup, execution reliably reached `0EAF:3067`, which executes `INT 68h` — a private,
undocumented multiplex-style call, not a standard BIOS/DOS/IRQ vector. Its IVT slot (`0x68 * 4 =
0x1A0`) is **never initialized by anything in the boot** before this point. The CPU faithfully does
exactly what real x86 hardware does with an uninitialized vector: it jumps to `0000:0000` and starts
executing raw Interrupt Vector Table bytes as instructions. This "accidental sled" walks forward
through the table until it happens to land on a valid 5-byte `CALL FAR` opcode — whose operand bytes
are themselves the *legitimate* INT 0Eh (floppy IRQ) vector, decoding to `650B:4500`. So execution
lands inside real, legitimate code (very likely the floppy driver, relocated by IO.SYS) but with a
completely bogus calling context — no real interrupt frame, garbage registers — which is why it then
jumps erratically inside that segment.

**Fix (emulator):** pre-initialize the INT 68h vector to point at a one-byte `IRET` stub, timed to
fire the moment CS first becomes `0x0EAF` (not at boot start — POST and DOS's own low-memory init
clobber an earlier write before `INT 68h` ever runs).

**Fix (real hardware):** this had no file-based equivalent — it only existed as a live memory write
inside the emulator's CPU core. We translated it to a tiny (26-byte) hand-assembled real-mode
`.COM` program, `IVT68FIX.COM`, that pokes the identical bytes: an `IRET` at `0000:03C0` (borrowed
scratch space, INT 0F0h's otherwise-unused vector slot) and the INT 68h vector at `0000:01A0`
repointed at it. It's called from the very last line of `AUTOEXEC.BAT`, as late as possible before
Windows auto-launches, mirroring the only timing that worked in the emulator.

### 4. VPICD.VXD and VDMAD.VXD

Both stock OSR1 VxDs contain code assuming standard AT interrupt-controller/DMA topology in a few
specific places. Scan-based patches (adapted from an earlier OSR2-era investigation) found and
neutralized these sites. Full detail and patch scripts are in `vxd-patches/`.

### 5. The keyboard investigation

This was the largest single piece of work, and the reason Windows never responded to a single
keystroke for most of the project's history.

**First finding — VKD.VXD's receive-wait.** Windows' 386-Enhanced-mode keyboard VxD, `VKD.VXD`,
polls port 0x64's status byte for the AT "data available" bit before ever trusting port 0x60. Since
this hardware has no real 8042, that bit never sets. Byte-matched the exact `IN AL,64h / TEST AL,1 /
LOOPE` idiom in the file (offset `0x3e11`) and patched it to report "ready" immediately.

**Second finding — KEYBOARD.DRV does its own, independent check.** The user's own memory of
Windows 3.11 needing *both* `IBKBD.DRV` and `IBVKD.386` together (not just one) turned out to be the
key clue: Windows 95's `KEYBOARD.DRV` (the GUI-level driver referenced in `SYSTEM.INI`'s
`keyboard.drv=` line, separate from `VKD.VXD`'s `keyboard=*vkd` line) does its *own* raw port-0x64
check, completely independent of whatever VKD.VXD already determined. Found and patched the same
way (file offset `0xf14`).

**Both patches confirmed active at runtime** via a live memory read at the exact instruction —
important, because the combined `VMM32.VXD` file on disk does *not* preserve patched bytes in a
way that's findable by simple file search (Setup's combine step transforms the code), so only a
live read can confirm a patch survived.

**Still not enough.** With both confirmed active, a real dialog box still didn't respond to a
keyboard event. CPU-starvation was checked and ruled out (execution during the freeze was genuinely
varied, not a spin loop). The actual answer turned out to be a different function entirely.

**The real root cause, found by reading source, not more binary patching.** Rather than keep
guessing at patch offsets, we built a genuine, from-source `VKD.VXD` using Microsoft's own 1995
Windows 95 DDK sample (`KEYB\SAMPLES\VKD\`), compiled with the real period toolchain — MASM 6.11c
(`ML.EXE`) and the Visual C++ 2.0-era `LINK.EXE` with its `/VXD` flag, both of which run natively on
a modern Windows 11 machine via WOW64, no DOS emulation required. Reading `VKDPHYS.ASM`'s
`VKD_Int_09` — the actual IRQ1 keystroke interrupt handler — directly in source form immediately
showed the real bug: it polls port 0x64 for the data-available bit and **silently discards the
interrupt** if that bit isn't set, before ever reading port 0x60. This is a different function from
the two already-patched checks, and more fundamental: it's Windows' own stock interrupt handler, the
first thing that runs on every single keystroke.

**Fix:** removed the poll and the discard-on-not-ready branch. The routine now unconditionally reads
port 0x60 once IRQ1 fires, trusting the interrupt itself as the "data is ready" signal — exactly
what real-mode BIOS INT9 and FastDoom's real-hardware-validated Inboard/XT keyboard code already do
successfully on this hardware (see credits).

**Confirmed live**, first on the emulator, then on real hardware: the "Windows Mouse Support" system
dialog dismissed on a real keypress, and the user typed their own name into the Windows logon
dialog and watched it appear correctly.

**A second, subtler bug found the next day.** The real XT keyboard interface (not an emulator
quirk — confirmed via FastDoom's `I_KeyboardISR_XT`, which real-hardware Inboard users rely on)
latches one scan code at a time and won't present the next one until software strobes port 0x61 bit
7 high then low to acknowledge it. AT-targeted code has no reason to do this (a real 8042 doesn't
need it), and stock `VKD_Int_09` doesn't either. Our own emulator's keyboard-controller model
(`kbc_xt.c`) has a bounded self-heal for this that made the first fix look complete during emulator
testing — but that self-heal only exists in the emulator's C source, and has **no real-hardware
equivalent**. Without it, real silicon would very likely accept exactly one keystroke and then never
deliver another.

**Fix:** added the same three-instruction acknowledgment FastDoom performs — read the current port
0x61 value, OR in bit 7 and write it back, then write the original value back — directly into our
custom `VKD_Int_09`, immediately after reading the scan code. This makes the fix self-contained and
correct on real hardware without depending on any emulator forgiveness.

### 6. The display driver: a Setup bug, not a hardware bug

After the keyboard fix, boot progressed unattended through Plug-and-Play detection, Control Panel
setup, and Start Menu building, then went to a black screen. The video memory checksum was frozen
solid while the CPU kept executing varied code — not a crash, something waiting. Inspecting
`SYSTEM.INI` directly found the cause: the real ATI Mach8 driver files had already been copied to
`WINDOWS\SYSTEM\` by the PnP step, but `SYSTEM.INI`'s `[boot]` section still pointed
`display.drv=pnpdrvr.drv` (Windows 95's temporary placeholder) — the step that should have
finalized this to the real driver never completed. The working, already-proven `vga.drv` (which
Setup itself had used successfully the whole way through) was sitting on disk unused.

**Fix:** point `display.drv` at `vga.drv` in the `[boot]` section. Note this needs to be the
`[boot]` section's line specifically — `[boot.description]` has a copy of the same key that's purely
cosmetic (shown in Control Panel) and has no effect on what actually loads.

### 7. Deploying to real hardware, and a genuine surprise

All of the above translate cleanly to files placed on the CF card, with one exception (item 3,
`IVT68FIX.COM`, discussed above). Deployed the full set — patched `INBRDPC.SYS`, patched
`VPICD.VXD`/`VDMAD.VXD` and the custom-built `VKD.VXD` into the *pre-monolith* `WINDOWS\SYSTEM\
VMM32\` staging folder (so Setup's own combine step bakes them into `VMM32.VXD` itself), patched
`KEYBOARD.DRV` directly into `WINDOWS\SYSTEM\` (it isn't part of the combine), the real
`display.drv` fix, and `IVT68FIX.COM` wired into `AUTOEXEC.BAT`.

First real-hardware boot reached the identical point the emulator had — Start Menu built, about to
show Help files — then went black, same symptom as the pre-display-driver-fix emulator stall. A
simple **reboot** got past it cleanly, straight to a working desktop with keyboard, mouse, and
FreeCell all confirmed. Whether that black screen was the same class of bug as the earlier
display-driver issue, something about `IVT68FIX.COM`'s timing, or something else that a cold restart
happens to clear, is not yet root-caused — worth investigating further given how easy it is to
reproduce.

## Sources and prior art

This investigation stands on several people's and projects' work; all of it materially shaped the
outcome.

- **Al Williams** (Dr. Dobb's Journal, Hackaday) — did real, hands-on Inboard 386/PC development
  work in 1990. Direct email correspondence and his own hardware-tested `a20()` routine confirmed
  this project's independently-derived Inboard A20 port constants (`KB_PORT=0x64`, `INBA20=0x60`,
  `INBA20ON=0xDF`, `INBA20OFF=0xDD`) exactly, and his documented errata fallback path helped confirm
  which real-mode components on this OSR1 image were and weren't Inboard-aware.
- **Michal Necasek** (OS/2 Museum) — a 2023 correspondence, independent of this project's own later
  investigation, that directly confirmed the single-PIC/no-hardware-IRQ8 architecture of this
  machine and correctly framed the open question as "does Windows require IRQ8+, or just tolerate
  its absence" — also surfaced the existence of Intel's own original `IBVPICD.386` (a custom
  Inboard-aware VPICD replacement Intel shipped for Windows 3.0/3.11) as a reference worth further
  study.
- **SuperFury / UniPCemu** — this project's entire Inboard 386/PC hardware model
  (`86box_full/src/device/inboard386.c` — memory waitstate control, A20 gating without a real 8042,
  BIOS ROM shadow/cache) is a **direct port of UniPCemu's `hardware/inboard.c`** (2019-2022), itself
  built from disassembly of `INBRDPC.SYS` and correspondence with Al Williams. This is the
  foundational piece the whole rest of this investigation stands on, not a secondary reference — see
  the introduction above. UniPCemu's documented behavior around this port also independently
  cross-validated several later findings (A20 via port 0x60 hijack, port 0xA0 as a dual-purpose
  Inboard-control/slave-PIC address depending on machine class, and a named single-PIC/VPICD failure
  mode on Windows 95).
- **FastDoom** (viti95, `github.com/viti95/FastDoom`) — a real-hardware-validated DOS Doom source
  port with confirmed-working Inboard/XT keyboard support. Its `I_KeyboardISR_XT` routine — read
  port 0x60, then explicitly strobe port 0x61 bit 7 to acknowledge the XT keyboard latch, then EOI
  — is the exact reference pattern used for the second `VKD_Int_09` fix, and confirmed real-mode
  BIOS INT9 wasn't doing anything exotic that couldn't be ported to protected mode.
- **Microsoft's Windows 95 DDK** (1995) — the genuine period source (`KEYB\SAMPLES\VKD\`) and
  toolchain (`MASM611C\ML.EXE`, `MSVC20\LINK.EXE`) made it possible to fix the actual root cause in
  readable assembly rather than continue guessing at binary patch offsets — the single highest-
  leverage change in approach across the whole investigation.
- **Intel's own Windows 3.11-for-Inboard drivers** (`IBKBD.DRV`, `IBVKD.386`) — while not used
  directly in the final Windows 95 fix, the fact that Intel themselves shipped *two* separate
  drivers for this exact same class of problem on Windows 3.11 was the direct clue that led to
  finding `KEYBOARD.DRV`'s independent, second port-0x64 check.
- **Kevin Moonlight** (`github.com/yyzkevin/COMrade`) — original author of COMrade, the serial-bridge
  tool that runs on the real target machine and lets a host query live hardware state directly. Used
  throughout for real-vs-emulator comparison (e.g. confirming port 0x64 reads back `0x00` on the real
  5160, not just in emulation).
- **Ahmad Byagowi's Open-Source-PC110 project** (`github.com/ahmadexp/Open-Source-PC110`) — forked COMrade and
  added `COMR95.EXE`, the Windows 95 Win32 port, which is what's deployed for live introspection on
  the real machine going forward.
- **86Box** — the base open-source PC emulator this project's Inboard 386/PC hardware model and all
  debug/tracing tooling is built on.

## Methodology notes, for anyone reproducing this

- **Read source before guessing at binary patches.** Every binary patch in this project was a real,
  necessary fix, but the single biggest breakthrough came from reading Microsoft's own DDK source
  directly instead of continuing to guess at patch offsets in a disassembler.
- **Differential AT-vs-XT testing** is the fastest way to tell "is this bug real and hardware-
  specific, or a red herring" — clone the *same, unpatched* disk image onto a known-good AT profile
  (a genuine 8042/dual-PIC machine) and confirm the same UI element behaves normally there. Clone
  from an unpatched base only — cloning an already-XT-patched image defeats the comparison, since
  those patches assume no real 8042.
- **A combined `VMM32.VXD` cannot be trusted for static verification.** Setup's combine step
  transforms the code in ways that make a patched byte pattern unfindable by simple file search,
  even when the patch is genuinely active. Only a live memory read at a known runtime address can
  confirm a patch survived.
- **Capped trace hooks silently saturate.** More than one "zero activity" finding this session
  turned out to be a trace hook that had already hit its hit-count cap tens of minutes earlier — an
  absence-based conclusion from a capped log needs the actual hit count checked first.
- **Emulator self-healing workarounds don't automatically translate to real hardware.** The
  `kbc_xt.c` `blocked`-flag timeout is the clearest example: it made the emulator behave correctly
  even with an incomplete fix, which would have been a nasty surprise on real silicon if not caught
  and fixed at the actual root (the missing port-0x61 acknowledgment) before deployment.

## Reproducing this

The individual patches, patch scripts, and the custom `VKD.VXD` source are all in this repository
(`vxd-patches/`, `custom_vkd/`) if you want to apply them to your own legally-obtained OSR1 media —
that's the most reproducible path, and makes clear exactly what changed and why (this document).

For convenience, two ready-made disk images are also available — the final working image, and the
pre-monolith image with every patch applied but Setup's own `VMM32.VXD` combine step not yet run —
as [GitHub Release assets](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/tag/win95-desktop-v1)
and on [archive.org](https://archive.org/details/win95-intel-inboard-386pc).

**Files needed, and where they go**, all applied to a *pre-monolith* Windows 95 OSR1 install (i.e.
before Setup's own `VMM32.VXD` combine step has run):

| # | Component | Destination on the target install | Source in this repo |
|---|---|---|---|
| 1 | `INBRDPC.SYS` self-test-skip | `C:\INBRDPC.SYS` (replaces the driver loaded from `CONFIG.SYS`) | `vxd-patches/osr1/INBRDPC_selftest_skip.SYS`, built by `vxd-patches/patch_inbrdpc_selftest_skip.py` |
| 2 | `VPICD.VXD` patch | `C:\WINDOWS\SYSTEM\VMM32\VPICD.VXD` (pre-combine staging) | `vxd-patches/osr1/VPICD_INBOARD.VXD`, built by `vxd-patches/patch_vpicd.py` |
| 3 | `VDMAD.VXD` patch | `C:\WINDOWS\SYSTEM\VMM32\VDMAD.VXD` (pre-combine staging) | `vxd-patches/osr1/VDMAD_INBOARD.VXD`, built by `vxd-patches/patch_vdmad.py` |
| 4 | Custom `VKD.VXD` — built from Win95 DDK source, not a binary patch. Both keyboard fixes (port-64h discard removed, port-61h XT ack added) | `C:\WINDOWS\SYSTEM\VMM32\VKD.VXD` (pre-combine staging; kept under the stock filename so `SYSTEM.INI`'s `keyboard=*vkd` picks it up with no `.INI` edit) | `custom_vkd/src/*.ASM`, built by `custom_vkd/build.ps1` (real MASM 6.11c + VC++2.0 `LINK.EXE`); known-good output archived at `custom_vkd/build/VKD_CUSTOM_INT09FIX_v2.VXD` |
| 5 | `KEYBOARD.DRV` patch | `C:\WINDOWS\SYSTEM\KEYBOARD.DRV` (not part of the combine — a direct file replace, works even on an already-installed system) | `vxd-patches/osr1/KEYBOARD_INBOARD.DRV`, built by `vxd-patches/patch_keyboarddrv_kbdready.py` |
| 6 | Real display driver fix | `C:\WINDOWS\SYSTEM.INI`, `[boot]` section: `display.drv=vga.drv` (not the `[boot.description]` copy, which is cosmetic only) | edited directly, no script |
| 7 | INT 68h vector fix, real-hardware translation of the emulator's `[patchint68]` hook | `C:\IVT68FIX.COM`, called from the very last line of `C:\AUTOEXEC.BAT` | `ivt68fix/IVT68FIX.ASM`, assembled with NASM (`nasm -f bin IVT68FIX.ASM -o IVT68FIX.COM`); byte-verified to match the binary actually tested on real hardware |
| 8 | COMR95 for live introspection while testing | `C:\WINDOWS\COMR95.EXE`, auto-launched via `C:\WINDOWS\WIN.INI`'s `[windows] run=` line | prebuilt binary from Ahmad Byagowi's [Open-Source-PC110](https://github.com/ahmadexp/Open-Source-PC110) fork of Kevin Moonlight's [COMrade](https://github.com/yyzkevin/COMrade) — not built in this repo |

## Acknowledgments

Thanks to everyone credited above whose published work, source code, and direct correspondence made
this possible — this was very much a "standing on the shoulders" effort, not a from-scratch one.
