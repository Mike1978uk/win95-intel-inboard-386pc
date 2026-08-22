# Windows 95 on the Intel Inboard 386/PC

**Windows 95 booting to a full, usable desktop — keyboard, mouse, and 32-bit applications all
working — on a real IBM 5160 fitted with an Intel Inboard 386/PC accelerator card.** As far as we've
been able to establish, this specific combination hasn't been documented as working before.

![status](https://img.shields.io/badge/status-working%20desktop-brightgreen)

📼 **See the Video**
https://youtu.be/KxuKTNQyBKE?is=OkK0_sxReKwSKeK6

📖 **[Read the full writeup](docs/windows95_on_inboard386pc_writeup.md)** — the complete technical
account: every bug found, every fix applied, full credits and sources, and a reproduction guide.

📷 **[Screenshots / photos](screenshots/)** — real-hardware and emulator captures.

🖥️ **[Download the emulator + try it now](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/download/win95-desktop-v1/86Box-Inboard-emulator-win64.zip)**
— no real hardware needed. A ready-to-run Windows build of this project's 86Box fork with the
Inboard 386/PC hardware model, ROMs, and a working config already set up. Grab a
[disk image](#try-it-yourself) too and you're running Windows 95 on the Inboard in minutes.

## Try it yourself

Two ready-made disk images are available — the final working image (post-reboot, reaches a full
desktop) and the pre-monolith image (every patch applied, but Setup's own `VMM32.VXD` combine step
hasn't run yet, if you want to watch that happen):

- 💾 **[GitHub Release](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/tag/win95-desktop-v1)** — both disk images, plus a ready-to-run **Windows emulator build** (`86Box-Inboard-emulator-win64.zip`, with the Inboard 386/PC hardware model, ROMs, and a working config included — no compiling, no real hardware needed. See the zip's `README.txt` for exact steps.)
- 💾 **[archive.org](https://archive.org/details/win95-intel-inboard-386pc)** — the two disk images.

**On real hardware**, you need: a real Intel Inboard 386/PC in an IBM PC/XT (or compatible), the
[4MB daughterboard](https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/)
(ParrotyError) — Windows 95 doesn't fit in the stock RAM ceiling — and an **XT-IDE** controller card
(what these images were built and tested against). Write the image to a **2GB CF card** (the images
themselves are sized for a 2GB card — a larger card will work but won't gain you usable space without
repartitioning). **In the emulator**, none of that is needed — just the downloaded zip and a disk
image.

**Worth knowing if you use 86Box for anything else**: the ROM set bundled in the emulator zip
includes `roms/video/ATI_MACH8.bin` — a real hardware dump of the ATI Mach8 (Graphics Ultra) BIOS
that, as far as we know, isn't available anywhere else online. It's directly referenced as the
verified-authentic reference dump in this project's own Mach8 emulation code
(`86box_full/src/cpu/386_dynarec.c`). Useful for anyone emulating a real Mach8 card, Inboard project
or not.

## What this is

The Intel Inboard 386/PC is a 1987 accelerator daughtercard that replaces an IBM PC/XT's 8088 with a
real 80386. Getting Windows 95 running on it is hard: the XT motherboard underneath has no real 8042
keyboard controller and no second interrupt controller, so Windows' AT-hardware assumptions break in
several different, independent places. This repo documents finding and fixing each one, with a
from-source rebuild of Windows' own keyboard VxD (using the genuine 1995 Windows 95 DDK toolchain)
at the center of the fix.

## What works

- Full GUI desktop, Start Menu built and populated
- Keyboard input (system dialogs, text entry, everywhere tested)
- Mouse input
- 32-bit applications (confirmed with the bundled FreeCell)
- Floppy drives A: and B:
- SCSI Devices working by adding relevant entries to config.sys / autoexec.bat devices loading fine in ms dos mode in Windows tested CD drive, MO Drive, Zip 100 drive
- Network working using stock Windows 95 3com 3c509b driver from Windows. Hand configured IP, gateway and subnet and navigated to frogfind.com

## Still open

- No sound
- See the [writeup](docs/windows95_on_inboard386pc_writeup.md#still-open) for full detail and the
  current plan.

## Upstream

**The Intel Inboard 386/PC is now part of 86Box.** The hardware model was merged as
**[86Box/86Box#7626](https://github.com/86Box/86Box/pull/7626)**, and a follow-up fixing three real
defects in it is open as **[86Box/86Box#7749](https://github.com/86Box/86Box/pull/7749)**.

What #7749 fixes, for anyone who tried the merged machine and found it broken:

| | |
|---|---|
| **POST 101** | The machine shared `ibmxt_config`, whose default is a 1982-dated 5160 ROM. `INBRDPC.SYS` — the card's own required driver — genuinely cannot work with that revision; it checks a signature at `F000:E05B` the 1982 ROMs do not carry. Worse, it failed *silently*: the 1986 entries existed only in this repo's tree, so a `bios =` line naming one was not a valid option elsewhere, was ignored without warning, and fell back to the bad default. The machine now has its own BIOS list containing only the two compatible 1986 revisions. |
| **386-class CPUs ran no fixes at all** | `cpu_set()` routes 386DX/386SX to `exec386_2386()`, and every Inboard POST fix-up lived only in `exec386()`. A plain 386DX — the CPU this card was actually sold with — hung in the Mach8 option ROM before the memory count. Now shared between both interpreter loops, gated on the card being present. |
| **Double-throttled memory timing** | `cpu_waitstates` is dead on 486BL but live on 386DX, stacking on top of the Inboard's own bus-speed scaling. |

This also resolves [86Box/86Box#7638](https://github.com/86Box/86Box/issues/7638), where the Inboard
software reported all memory as "BAD" — same 1982-ROM cause.

Full write-up of the submission, including the testing matrix and known limitations, is in
[`docs/PR_description_inboard_post101_fix.md`](docs/PR_description_inboard_post101_fix.md).

The submitted subset is a minimal slice of `86box_full/` (the device model plus the core-file
timing/PIC/DMA fixes it needs) — see [`upstream-submission/`](upstream-submission/) for a standalone
copy of what went up originally. The emulator build linked above is built from `86box_full/`, which
carries the same fixes plus this project's debug tooling.

## Repository structure

- **`86box_full/`** — the 86Box emulator fork, with the Inboard 386/PC hardware model
  (`src/device/inboard386.c`) and the debug/tracing hooks used throughout this investigation
- **`upstream-submission/`** — a standalone copy of the minimal subset submitted to the official
  86Box project ([86Box/86Box#7626](https://github.com/86Box/86Box/pull/7626))
- **`custom_vkd/`** — full assembly source for the custom-built `VKD.VXD` (Microsoft's own DDK
  sample, modified), plus the build script for the real period MASM/LINK toolchain
- **`ivt68fix/`** — source + binary for the real-mode INT68h vector fix deployed on real hardware
- **`vxd-patches/`** — binary-patch scripts (and their outputs) for `VPICD.VXD`, `VDMAD.VXD`,
  `KEYBOARD.DRV`, and `INBRDPC.SYS`
- **`test_harness/`** — small, source-controlled real-mode test programs used to isolate specific
  bugs without a full Windows boot each time
- **`docs/`** — the full writeup, plus historical reference material (Al Williams' 1990s Inboard A20
  correspondence)
- **`.claude/skills/inboard-hw-debug/`** — the debugging methodology this investigation converged on,
  written up as a reusable reference

## Credits

This stands on the work of several people and projects — see the
[full credits section](docs/windows95_on_inboard386pc_writeup.md#sources-and-prior-art) in the
writeup for complete detail:

- **[Stynx and Harrison Frazier](https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/)**
  (VCFed) — designed the 4MB Inboard daughterboard (ParrotyError), without which Windows 95 wouldn't
  run on this hardware at all
- **CimonVg** — ongoing work pushing the Inboard 386/PC to its limits, and inspiration/support
  throughout this investigation
- **[RonnyRoy](https://github.com/ronnyroy111/inboard386)** — ongoing reproduction of the Inboard as
  cloned hardware, which will make sharing this work more widely possible, and may be the path past
  today's 4MB ceiling
- **SuperFury / UniPCemu** — this project's entire Inboard 386/PC hardware model
  (`86box_full/src/device/inboard386.c`) is a direct port of UniPCemu's `hardware/inboard.c` — the
  foundation the rest of this work is built on
- **Al Williams** (Dr. Dobb's Journal, Hackaday) — real 1990s hands-on Inboard development experience
- **Michal Necasek** (OS/2 Museum) — architectural confirmation and historical leads
- **FastDoom** (viti95) — real-hardware-validated XT keyboard ISR reference
- **Microsoft's Windows 95 DDK** — the genuine period source and toolchain that made the real fix possible
- **Kevin Moonlight** — original author of [COMrade](https://github.com/yyzkevin/COMrade)
- **Ahmad Byagowi** ([Open-Source-PC110](https://github.com/ahmadexp/Open-Source-PC110)) — ported COMrade to
  Windows 95 (`COMR95.EXE`), used for live real-hardware introspection
- **[86Box](https://github.com/86Box/86Box)** — the base emulator this project is built on

## Reproducing this

Windows 95 install media isn't included here (copyrighted, and large) — you'll need your own OSR1
media. The [writeup's reproduction section](docs/windows95_on_inboard386pc_writeup.md#reproducing-this)
lists exactly which files to patch and where to place them on a pre-monolith install.

## Contributing

Issues and PRs welcome, especially on the open items above (SCSI/sound/network drivers) — see the writeup for the current state of each.
