# Windows 95 on the Intel Inboard 386/PC

**Windows 95 booting to a full, usable desktop — keyboard, mouse, and 32-bit applications all
working — on a real IBM 5160 fitted with an Intel Inboard 386/PC accelerator card.** As far as we've
been able to establish, this specific combination hasn't been documented as working before.

![status](https://img.shields.io/badge/status-working%20desktop-brightgreen)

📖 **[Read the full writeup](docs/windows95_on_inboard386pc_writeup.md)** — the complete technical
account: every bug found, every fix applied, full credits and sources, and a reproduction guide.

📷 **[Screenshots / photos](screenshots/)** — real-hardware and emulator captures.

💾 **Disk images** — the final working image and the pre-monolith (patches-applied,
pre-Setup-combine) image are available as
[GitHub Release assets](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/tag/win95-desktop-v1)
and on [archive.org](https://archive.org/details/win95-intel-inboard-386pc).

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

## Still open

- SCSI and other attached peripherals not yet detected
- No sound, no network yet
- See the [writeup](docs/windows95_on_inboard386pc_writeup.md#still-open) for full detail and the
  current plan.

## Repository structure

- **`86box_full/`** — the 86Box emulator fork, with the Inboard 386/PC hardware model
  (`src/device/inboard386.c`) and the debug/tracing hooks used throughout this investigation
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

Issues and PRs welcome, especially on the open items above (SCSI/sound/network drivers) — see the
writeup for the current state of each.
