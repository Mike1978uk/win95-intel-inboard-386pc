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

- 💾 **[Latest release — `win95-sound-fixed-v2`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/tag/win95-sound-fixed-v2)** — the current pre-monolith image (58 MB compressed) plus the post-install fixes. **Use this one.**
- 🔧 **[Emulator build](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/download/win95-desktop-v1/86Box-Inboard-emulator-win64.zip)** — ready-to-run Windows build of this project's 86Box fork, with the Inboard hardware model, ROMs and a working config. No compiling, no real hardware needed. (Still hosted on the v1 release; see its `README.txt`.)
- 📦 **[win95-desktop-v1](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases/tag/win95-desktop-v1)** — kept as an archive. ⚠️ **Its disk images carry the corrupted `VDMAD.VXD` that causes the Sound Blaster Pro BSOD.** If you downloaded images from there, take them from v2 instead.
- 💾 **[archive.org](https://archive.org/details/win95-intel-inboard-386pc)** — mirrors the older v1 images.

⚠️ **The sound fix is not in any image and cannot be** — a pre-monolith image contains no
`MSSBLST.VXD` at all; it arrives stock from the CABs when you install the Sound Blaster Pro driver.
Apply `inboard-post-install-fixes.zip` (or [FIXES.md](FIXES.md)) *after* installing your drivers.

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
- Keyboard input (system dialogs, text entry, everywhere tested). Set Windows to the **US** layout
  for a working `\` - an 83-key XT keyboard has no key for the UK one
- Mouse input
- 32-bit applications (confirmed with the bundled FreeCell)
- SCSI Devices working by adding relevant entries to config.sys / autoexec.bat devices loading fine in ms dos mode in Windows tested CD drive, MO Drive, Zip 100 drive
- Network working using stock Windows 95 3com 3c509b driver from Windows. Hand configured IP, gateway and subnet and navigated to frogfind.com
- Sound Blaster Pro audio, clean, confirmed on real hardware 2026-08-24 (see below)
- Accelerated video — ATI Mach8 (Graphics Ultra) at 1024x768x256, confirmed on real hardware
  2026-08-24 (see below)

**Video, sound and networking all work at the same time on the real 5160.**

**Floppy drives do not work yet.** A controller is now installed and correctly resourced, but
reads still stall part-way — see [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3).
An earlier version of this page claimed A: and B: worked; that was wrong.

## Patched files

**[FIXES.md](FIXES.md) — every patched file on one page, with direct downloads and md5s**, so they
can be applied to any Windows 95 OSR1 install on this hardware rather than only the images published
here. Tested status is stated for each one.

## What worked, and what didn't

**[docs/what_worked_and_what_didnt.md](docs/what_worked_and_what_didnt.md)** — the flat inventory:
every fix that shipped, and every dead end, one line each. Read that before re-walking anything.
The [writeup](docs/windows95_on_inboard386pc_writeup.md) has the full narrative.

## Sound: a machine-class bug in Microsoft's own drivers

Worth calling out separately, because it is not specific to this project and will bite anything
XT-class. The IBM 5160 keeps a **4-bit DMA page latch**, so DMA reach is **20-bit (1 MB)**. Every
ISA-era Windows driver assumes **24-bit (16 MB)**. A driver that puts its DMA buffer above 1 MB does
not crash - the page register silently drops the high bits and the 8237 transfers from a completely
different physical address:

```
[dmapage] ch=1 val=4E -> page=0E *** TRUNCATED, buffer is above 1MB ***
```

`MSSBLST.VXD` asked `_PageAllocate` for a buffer anywhere below 16 MB (`maxPhys = 0xFFF`), got
`0x4E0000`, and the card played whatever was at `0x0E0000` - adapter ROM space. That is why the
audio was distorted rather than silent or fatal.

The fix is `maxPhys` `0xFFF` -> `0xFF`, two bytes: `vxd-patches/sound/MSSBLST_INBOARD.VXD`, deployed
with `tools/deploy_sound_fix.sh`. `tools/sweep_image_dma.py` audits a whole install for the same
mistake. **@andrew-hoffman** called the 4-bit page register from the hardware before any of this was
measured.

## Video: the driver was never the problem

The ATI Mach8 spent months looking like a missing-driver problem. It was not. **Windows 95 ships
its own Mach8 driver** — `ATIM8.DRV` + `ATI.VXD`, `MSDISP.INF` section `[ATI8]`, listed as *ATI
Graphics Ultra (mach8)*. ATI never wrote a Windows 95 driver for this card, which is what made it
easy to conclude none existed; Microsoft's was in the box the whole time.

**Selecting it is not enough.** The mach8 is not PnP-enumerable on this bus, so Windows'
automatic configuration has nothing to work from and leaves the device node with no resources —
the driver then loads against a device it cannot reach and gets nowhere.

The recipe, on real hardware:

1. Display adapter → driver → **ATI Graphics Ultra (mach8)** (Windows 95's own, from the CABs).
   Do **not** install the Windows 3.1x driver (`MACHW3.DRV`) — it was tried first and failed.
2. Device Manager → the adapter → **Resources** → untick *Use automatic settings* → pick a
   configuration → reboot. Windows accepts it on the way back up.

You can tell which devices have had step 2 by reading `SYSTEM.DAT`: a manually configured node
carries a **`ForcedConfig`**, a detection-configured one carries a **`BootConfig`** plus a
`DetFunc`. On this machine the working Mach8 and COM1 have the former; the phantom PS/2 mouse
Windows invented ([#6](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/6)) has the latter. That test generalises to any non-PnP card on
this hardware — and it tells the two situations apart: the Mach8 was a real device with an
unconfigured node, so forcing its configuration fixed it, whereas the mouse node was always fiction
and there was nothing to configure.

## Upstream

**The Intel Inboard 386/PC is part of 86Box.** Seven PRs are merged:

| PR | What it fixed |
|---|---|
| [#7626](https://github.com/86Box/86Box/pull/7626) | The hardware model itself, ported from SuperFury's [UniPCemu](https://superfury.itch.io/unipcemu) `hardware/inboard.c` |
| [#7749](https://github.com/86Box/86Box/pull/7749) | POST 101 (the machine defaulted to an incompatible 1982 ROM); 386DX ran no POST fix-ups at all; double-throttled memory timing |
| [#7760](https://github.com/86Box/86Box/pull/7760) | `rammap()` dereferenced NULL on a page-table walk through unbacked memory — a guest could crash 86Box outright |
| [#7761](https://github.com/86Box/86Box/pull/7761) | The reserved block is 128 KB at a fixed `0x5E0000`–`0x5FFFFF`, not 64 KB derived from RAM size |
| [#7765](https://github.com/86Box/86Box/pull/7765) | `bad extended memory` — the high `0x5F0000` alias must read shadow RAM, not ROM. Now reports **0k** |
| [#7766](https://github.com/86Box/86Box/pull/7766) | POST 1801 on every boot — the machine must not default to a 5161 expansion unit |
| [#7771](https://github.com/86Box/86Box/pull/7771) | The XT 4-bit DMA page latch — truncation was gated on `dma_at`, so an Inboard got an 8-bit page register it does not physically have |

Between them these close [86Box/86Box#7638](https://github.com/86Box/86Box/issues/7638) (all memory
reported "BAD", 640K available) and this repo's issues #11, #12, #13 and #16.

One further upstream bug was reported from here and fixed by 86Box directly, with no PR from us:
[#7805](https://github.com/86Box/86Box/issues/7805) — the Machine settings dialog snapped RAM to a
bitmask from zero rather than from the machine's minimum, so opening the dialog and clicking OK
turned 5120 KB into 4096 and 3072 into 2048. Found by @andrew-hoffman, diagnosed at source level
here, fixed by OBattler in `9ee5197`.

### Which build to test on

**Test on upstream 86Box master, not on [`86box_full/`](86box_full/).** Every Inboard change is now
merged, so master carries the whole model — the device, the machine entry, the 1986-only BIOS list
and the DMA page latch — plus upstream's own fixes as they land. `86box_full/` is a vendored
snapshot that also carries the investigation's tracing hooks; those hooks cost roughly 3.45× in
guest instructions per second, which is why [#14](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/14)
needs a quiet build. Keep it for reproducing the traces, not for measuring behaviour.

The POST 101 story is worth knowing if you tried the merged machine early and found it broken: the
machine shared `ibmxt_config`, whose default is a 1982-dated 5160 ROM. `INBRDPC.SYS` — the card's
own required driver — cannot work with that revision; it checks a signature at `F000:E05B` the 1982
ROMs do not carry. It failed *silently*: the 1986 entries existed only in this repo's tree, so a
`bios =` line naming one was not a valid option elsewhere and was ignored without warning. The
machine now has its own BIOS list containing only the two compatible 1986 revisions.

### Worth knowing — the XT 4-bit DMA page latch ([#7771](https://github.com/86Box/86Box/pull/7771))

Merged 2026-08-25. Described here because it is **not Inboard-specific** — it is correct for any
PC/XT-class machine, and before it no emulator could reproduce the driver bug class described
[below](#testing-a-driver-for-the-20-bit-dma-bug).

86Box already truncated the page register for a genuine XT, in `dma_page_write()`:

```c
dma[addr].page = dma_at ? val : val & 0xf;      /* before #7771 */
```

But `dma_at` is assigned `is286`. An Inboard is an XT board with a 386 on it, so `dma_at` came
out true and the machine was handed a full 8-bit page register it does not physically have.
`dma_force_xt` — which had reached upstream earlier — was not consulted here at all. The fix gates
on that instead:

```c
static int dma_page_is_xt(void) { return dma_force_xt || !dma_at; }
...
dma[addr].page = dma_page_is_xt() ? (val & 0x0f) : val;
```

plus the matching `dma_m` mask in `dma_reset()`. Any machine that does not set `dma_force_xt`
behaves exactly as before. **Flagged by @andrew-hoffman** on
[issue #3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3); +14 −2, one file.

The guest-side patches are Windows files, not emulator code, so they stay hosted here — see
[FIXES.md](FIXES.md).

Full write-up of the original submission, with the testing matrix and known limitations, is in
[`docs/PR_description_inboard_post101_fix.md`](docs/PR_description_inboard_post101_fix.md).
[`upstream-submission/`](upstream-submission/) holds a standalone copy of what went up first.

## Repository structure

**The work**

- **`86box_full/`** — the 86Box emulator fork: the Inboard 386/PC hardware model
  (`src/device/inboard386.c`) plus the debug/tracing hooks used throughout this investigation.
  Carries upstream 86Box's own `.gitattributes` so it stays diffable against master
- **`vxd-patches/`** — the Windows 95 binary patches and the scripts that produce them:
  `VKD.VXD`, `VPICD.VXD`, `VDMAD.VXD`, `KEYBOARD.DRV`, `MSSBLST.VXD`, `HSFLOP.PDR`,
  `INBRDPC.SYS`, each alongside the stock original it derives from
- **`custom_vkd/`** — full assembly source for the custom-built `VKD.VXD` (Microsoft's own DDK
  sample, modified), plus the build script for the genuine period MASM/LINK toolchain
- **`ivt68fix/`** — source + binary for the real-mode INT 68h vector fix deployed on real hardware
- **`dist/post-install-fixes/`** — what you actually download: the patched files that must be
  applied *after* driver installation, plus the DMA audit scripts
- **`tools/`** — deployment and capture scripts (`deploy_sound_fix.sh`, `deploy_premonolith.sh`,
  image DMA sweeps, VM setup). Bash — pinned to LF in `.gitattributes`

**Evidence and reference**

- **`hardware/`** — real 5160 reverse-engineering: `INBRDPC.SYS` disassembly, PAL/GAL analysis
- **`test_harness/`** — small real-mode test programs used to isolate a bug without a full
  Windows boot each time
- **`roms/`** — system, video and peripheral ROMs, including `video/ATI_MACH8.bin`, a real dump
  of the ATI Graphics Ultra BIOS not known to be available elsewhere
- **`analysis/`, `references/`, `screenshots/`** — traces, third-party specs, and real-hardware
  and emulator captures
- **`upstream-submission/`** — standalone copy of the minimal subset first submitted to 86Box
  ([#7626](https://github.com/86Box/86Box/pull/7626))

**Documentation**

- **[`FIXES.md`](FIXES.md)** — every patched file, with downloads, md5s and tested status
- **[`docs/what_worked_and_what_didnt.md`](docs/what_worked_and_what_didnt.md)** — flat inventory,
  dead ends included
- **[`docs/windows95_on_inboard386pc_writeup.md`](docs/windows95_on_inboard386pc_writeup.md)** — the
  full narrative
- **[`docs/contributor_input_ledger.md`](docs/contributor_input_ledger.md)** — who contributed what,
  whether it was verified or disproved, and whether they have been told
- **`docs/archive/`** — superseded session notes, kept for provenance. Treat as unverified

**Methodology, as reusable skills**

- **`.claude/skills/inboard-hw-debug/`** — the hardware/timing/boot debugging methodology
- **`.claude/skills/win9x-dma-driver-audit/`** — finding Windows 9x drivers that assume 24-bit
  DMA reach on 20-bit hardware
- **`.claude/skills/repo-hygiene/`** — keeping this repository legible to outside contributors

## Credits

Full detail in the [writeup's credits section](docs/windows95_on_inboard386pc_writeup.md#sources-and-prior-art)
and the [contributor ledger](docs/contributor_input_ledger.md).

- **[Stynx and Harrison Frazier](https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/)**
  (VCFed) — the 4MB Inboard daughterboard (ParrotyError). Windows 95 does not fit without it.
- **SuperFury / [UniPCemu](https://superfury.itch.io/unipcemu)** — this project's entire Inboard
  hardware model is a direct port of UniPCemu's `hardware/inboard.c`. The foundation everything
  else is built on.
- **[@andrew-hoffman](https://github.com/andrew-hoffman)** — the XT 4-bit DMA page-register lead
  and the sources behind it, which produced the 640 KB figure, an emulator DMA fidelity bug now
  upstream as [#7771](https://github.com/86Box/86Box/pull/7771), the driver-audit method, and this
  repo's writing and line-ending conventions.
- **[Bob Smith](https://github.com/sudleyplace)** (Qualitas) — author of **386MAX**, whose source
  carries first-class Inboard support and is the primary-source evidence for the XT DMA ceiling.
  He states he had no involvement with the Inboard itself: [full detail and quotes](docs/386max_and_the_inboard.md).
- **Al Williams** (Dr. Dobb's Journal, Hackaday) — real 1990s hands-on Inboard development
  experience; his 1990 A20 code matches this project's emulation exactly.
  [Correspondence, 2023](docs/al_williams_inboard_a20_correspondence_2023.md).
- **Michal Necasek** ([OS/2 Museum](https://www.os2museum.com/)) — architectural confirmation,
  historical leads, and a verified `F000:FF53` improvement now upstream.
- **CimonVg** — ongoing work pushing the Inboard 386/PC to its limits, and support throughout.
- **[RonnyRoy](https://github.com/ronnyroy111/inboard386)** — reproducing the Inboard as cloned
  hardware, which may be the path past today's 4MB ceiling.
- **Feipoa** (Vogons) — the [CTCHIP/KTCHIP34 write-up](https://www.vogons.org/viewtopic.php?t=45756)
  and the register-level approach behind it, which closed issue #9.
- **Fenix770** — the VM attachment that root-caused the shadow-RAM alias failure.
- **Wim Osterholt** — [XT, AT and PS/2 I/O port addresses](https://wiki.preterhuman.net/XT,_AT_and_PS/2_I/O_port_addresses)
  (1994), which marks entries `(XT only)` — the exact distinction most bugs here turn on.
  Annotated in [`docs/xt_io_port_reference_annotated.md`](docs/xt_io_port_reference_annotated.md).
- **[FastDoom](https://github.com/viti95/FastDoom)** (viti95) — real-hardware-validated XT
  keyboard ISR reference.
- **Microsoft's Windows 95 DDK** — the genuine period source and toolchain behind the `VKD.VXD` fix.
- **Kevin Moonlight** — original author of [COMrade](https://github.com/yyzkevin/COMrade); and
  **Ahmad Byagowi** ([Open-Source-PC110](https://github.com/ahmadexp/Open-Source-PC110)) — ported it
  to Windows 95 as `COMR95.EXE`, used for live real-hardware introspection.
- **[86Box](https://github.com/86Box/86Box)** — the base emulator this project is built on.

## Reproducing this

Windows 95 install media isn't included here (copyrighted, and large) — you'll need your own OSR1
media. The [writeup's reproduction section](docs/windows95_on_inboard386pc_writeup.md#reproducing-this)
lists exactly which files to patch and where to place them on a pre-monolith install.

**The sound fix is not one of them, and cannot be.** A pre-monolith image contains no
`MSSBLST.VXD` at all — it arrives from the `WIN95_xx.CAB` files when you install the Sound Blaster
Pro driver, stock and unpatched. Apply `dist/post-install-fixes/` **after** installing the driver,
not to the image.

## Contributing

Issues and PRs welcome. Most open issues carry a **Status** block at the top, so you can see
where they actually stand without reading the thread.

### The most useful thing anyone could pick up

**[#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8) — the ATI Mach 8
self-test.** Reproduced on stock upstream 86Box, so it is an 86Box defect, and it is currently
**blocked on information rather than effort**: the accelerator-side memory banking is undocumented
in every source we have checked. If you know the Mach8/8514-A register set, that issue needs one
answer, not a week of work.

### Open issues

| | |
|---|---|
| [#7](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/7) | Setup black-screens right before the Help files (reboot works around it). Undiagnosed and unclaimed |
| [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8) | ATI Mach 8 — the option ROM's self-test reports `RAM Addressing` in 86Box where the real card reports `Ok`. Reproduced on a stock upstream build, so it is an 86Box defect |
| [#10](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/10) | Idea: a loadable BIOS-extension shim so 1982-era 5150/5160 ROMs can run Windows |
| [#14](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/14) | POST intermittently halts with 101, only at `mem_size` 3072. Needs a quiet build, not `86box_full` |
| [#15](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/15) | Windows 3.0 faults after the splash screen in 386 enhanced mode |
| [#17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17) | Drives run in MS-DOS compatibility mode. Real-mode units are down from six to one after removing a parallel-port ASPI driver; the survivor is the XT-IDE boot disk, which stays real-mode permanently |
| [#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18) | Floppy reads return garbage once a 32-bit driver loads — `HSFLOP.PDR`'s DMA buffer lands above 1 MB. Patched, not yet measured |
| [#19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19) | Trantor T130B — a Windows 95 32-bit miniport (`T130.MPD`) exists but is untested here |
| [#20](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/20) | 86Box has no 3C509B device, so emulated networking cannot match the real machine's card. Low priority, emulation fidelity only |

Issues are labelled **`emulator`** or **`real-hardware`** so you can pick by what you have, and
**`upstream`** marks the ones destined for 86Box itself.

**You do not need an Inboard to help.** Most of this was found in emulation, on an 86Box build that
is [in this repo](86box_full/) and now [upstream](https://github.com/86Box/86Box/pull/7626).

Before opening a PR here, please read [`CLAUDE.md`](CLAUDE.md) — short commit subjects, short
bodies, reasoning in `docs/` rather than in the history.

### Testing a driver for the 20-bit DMA bug

If you run Windows 9x on any XT-class machine and a device produces **corrupt data rather than no
data** — distorted audio, garbled tape or scanner transfers — it is worth checking for the bug
described [above](#sound-a-machine-class-bug-in-microsofts-own-drivers). It is not specific to this
project or to the Inboard: it is Microsoft's own drivers assuming a 24-bit DMA reach on hardware
that only has 20 bits.

The audit is read-only and takes seconds:

```
python dist/post-install-fixes/scripts/vxd_dma_audit.py YOURDRIVER.VXD
```

Open an issue with the output — and the driver, if licensing allows — and it can be checked and
patched. `.claude/skills/win9x-dma-driver-audit/` is a self-contained writeup of the whole method,
including which drivers must **not** be patched, if you would rather do it yourself.

### What is most useful

Real-hardware results, positive or negative, on a machine that is not this one. A fix that works
here and nowhere else is not finished, and several conclusions in this repo have been overturned by
somebody measuring rather than arguing.
