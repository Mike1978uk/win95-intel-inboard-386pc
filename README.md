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
- **32-bit protected-mode access to the whole SCSI chain**, confirmed on real hardware 2026-09-06.
  Adaptec's own [`T130.MPD`](drivers/trantor_t130b/) — never shipped in the Windows 95 box, published
  separately — drives the Trantor T130B with no IRQ, polled. The Fujitsu MO, Nakamichi CD changer,
  Iomega Zip 100, Yamaha CD-RW, HP DAT and UMAX scanner all enumerate; `RMM.PDR` stands down and the
  real-mode chain (`MA13B.SYS` and friends) is out of `CONFIG.SYS` entirely
  ([#19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19)). The tape and scanner
  sit as `Unknown` nodes on purpose: Windows 95 has no class driver for either, and both are reached
  over ASPI by their own applications. Both were **exercised from within Windows on the real 5160**
  (2026-09-10), with nothing real-mode or DOS-side driving them
- **32-bit protected-mode disk access on the boot disk**, confirmed on real hardware 2026-09-06.
  This project's own Windows 95 SCSI miniport, [`XTIDEMP.MPD`](FIXES.md), drives the 8-bit
  XT-CF / XT-IDE card directly: `RMM.PDR` stands down, `C:` is served by SCSIPORT and DiskTSD,
  and the machine shuts down cleanly. **This is not Inboard-specific** — it should apply to any
  XT-class Windows 95 machine with an XT-IDE card, and a report either way would be welcome
  ([#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21))
- **32-bit protected-mode access to the parallel-port LS-120**, confirmed on real hardware
  2026-09-20. An Imation SuperDisk behind a Shuttle EPAT bridge at `0x378`, served by the vendor's
  own Windows 95 miniport with this project's INF change ([`dist/ls120_vendor/`](dist/ls120_vendor/)).
  The stock install kills the keyboard on an XT bus — its EPP and chipset probes alias onto the 8259,
  and you then cannot type the switches that would have prevented it, so they go in the INF instead.
  Forced to EPP with `/fe` it runs at **75-99 KiB/s**, and 36,735,152 bytes read back byte-identical
  under `FC /B` — written by the Windows miniport and read by the DOS driver, so a symmetric error in
  one path cannot hide itself.
  ⚠ **Known vendor bug on any XT-class machine, DOS and Windows alike:** both vendor drivers probe
  for a host chipset at `0x22`/`0x23`. An XT decodes the 8259 across `0x20`-`0x3F`, so the probe
  "finds" a chipset and its configuration writes land on the interrupt mask, disabling the keyboard (measured with the miniport; the DOS driver carries the same probe).
  Load `SD120PPD.SYS` with **`/ni`** (skip chipset initialisation); the Windows miniport takes the
  same switch through its INF `AdapterSettings`, as `dist/ls120_vendor/` ships it
  ([analysis](docs/ls120_keyboard_root_cause.md))
- Network working using stock Windows 95 3com 3c509b driver from Windows. Hand configured IP, gateway and subnet and navigated to frogfind.com
- Sound Blaster Pro audio, clean, confirmed on real hardware 2026-08-24 (see below)
- Accelerated video — ATI Mach8 (Graphics Ultra) at 1024x768x256, confirmed on real hardware
  2026-08-24 (see below)

**Video, sound and networking all work at the same time on the real 5160.**

**No real-mode storage drivers are left.** Every disk, floppy, SCSI target and the SuperDisk is
served by a 32-bit protected-mode driver: `XTIDEMP.MPD` for the boot disk, `T130.MPD` for the SCSI
chain, `HSFLOP_XTDMA.PDR` for the floppies, and the vendor miniport for the LS-120. The whole of
`CONFIG.SYS`, read off the machine's own CF card:

```
DEVICE=c:\INBRDPC.SYS NODIAGS NOPAUSE
DEVICE=C:\WINDOWS\SETVER.EXE
DEVICE=C:\WINDOWS\HIMEM.SYS
DOS=HIGH,UMB
device=C:\WINDOWS\COMMAND\display.sys con=(ega,,1)
Country=044,850,C:\WINDOWS\COMMAND\country.sys

REM DEVICEHIGH=C:\SD120PPD\SD120PPD.SYS /port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp /fe
REM DEVICEHIGH=C:\SD120PPD\ASPIHDRM.SYS

LASTDRIVE=M
BUFFERS=30,0
BREAK=ON
Stacks=0,0
FILES=40
```

**Four `DEVICE` lines, and not one of them is storage.** The LS-120's real-mode driver and its
ASPI manager are commented out, kept only as the DOS-side reference — Windows serves that drive
now. `INBRDPC.SYS` stays, and always will: it is the Inboard's own board driver, not a storage
driver, and the machine does not boot without it.

⛔ **`EGACACHE` came off that line on 2026-09-21, and here is why it never mattered.** Intel's
switch *"reserves up to 32K bytes ... for caching the EGA ROM BIOS"* — and this machine's card
is an ATI Mach8, a **VGA**. An A/B on the real 5160 on 2026-09-20 had already measured it as no
change at all (1746 / 1476 / 1340 byte-word-dword against 1746 / 1478 / 1336, inside the
harness's own ±2-tick noise, with `0xC0000` unmoved at 2.858 us/byte). The mechanism explains
the null result rather than leaving it unexplained: the switch does do something, just not for
a card that is not an EGA. **Do not re-propose it.**

Note which way round this is: the line **without** `EGACACHE` is the long-standing
configuration — recorded as `DEVICE=c:\INBRDPC.SYS NODIAGS NOPAUSE` since 2026-07-26 and booted
hundreds of times. `EGACACHE` was added only to measure it. Taking it off restores the proven
baseline; the state that was ever in question was having it **on**.

**Floppy drives work.** The patched [`HSFLOP_XTDMA.PDR`](FIXES.md) loads and initialises on
the real machine (`Init Success`, `INITCOMPLETE`, measured 2026-09-06) — for over a month it was
deployed but never loaded, so nothing measured about it before then meant anything. Read and write
were measured on the real 5160 on 2026-09-09: 121 KB written twice to different sectors, three
binary compares clean, both drives normal.

**Geometry and drive letters are correct** (fixed 2026-09-07,
[#25](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/25)). `INT 13h AH=08h` used to
report 720K for both drives because the Trantor's option ROM at `CA000` was scanned first and claimed
`INT 13h`, so Sergey's Multi-Floppy BIOS — which takes the vector only while it is still the stock
`F000:EC59` — settled for `INT 40h` and the 1986 system BIOS answered from its own 720K table. The
fix was hardware: Trantor's ROM moved to `DA000`. Verified on DOS 6.22 and Windows 95, both drives
reading real media.

**One open caveat.** @andrew-hoffman hit a fatal exception and a corrupted disk after changing media
a few times, in 86Box, on his configuration — analysed as a stale cache page flushed to the wrong
disk, a media-change detection failure rather than the DMA-reach bug the patch fixes. **That path has
not been reproduced here**, and the 2026-09-09 harness above never changed media, so it was never a
test of it. See [#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18).

([#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3), the original report, is
closed — it was the Have Disk browse fault, which turned out to be the missing controller.)

## Patched files

**[FIXES.md](FIXES.md) — every patched file on one page, with direct downloads and md5s**, so they
can be applied to any Windows 95 OSR1 install on this hardware rather than only the images published
here. Tested status is stated for each one.

## Sources, and the software they came with

**[docs/resources_and_sources.md](docs/resources_and_sources.md)** — every datasheet, forum
thread, source tree and driver this project has been given or has found, each with one line on
what it actually gave us, and a ❌ where it turned out not to contain what was hoped. The software
that can be redistributed is held in this repository so a clone is enough; where it cannot be,
the page says where it lives.

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

**The Intel Inboard 386/PC is part of 86Box.** Twelve PRs raised from this project are merged;
none is open.

Not yet upstream:

- The vendor LS-120 DOS driver does not initialise on the EPAT model
  ([#44](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/44)).

The IRQ 9 → 2 work ([#42](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/42)) is
not an emulator change and is not going upstream. It is a Windows 95 fix for XT-class machines, in
`VPICD` and the network driver.

| Merged PR | What it fixed |
|---|---|
| [#7626](https://github.com/86Box/86Box/pull/7626) | The hardware model itself, ported from SuperFury's [UniPCemu](https://superfury.itch.io/unipcemu) `hardware/inboard.c` |
| [#7749](https://github.com/86Box/86Box/pull/7749) | POST 101 (the machine defaulted to an incompatible 1982 ROM); 386DX ran no POST fix-ups at all; double-throttled memory timing |
| [#7760](https://github.com/86Box/86Box/pull/7760) | `rammap()` dereferenced NULL on a page-table walk through unbacked memory — a guest could crash 86Box outright |
| [#7761](https://github.com/86Box/86Box/pull/7761) | The reserved block is 128 KB at a fixed `0x5E0000`–`0x5FFFFF`, not 64 KB derived from RAM size |
| [#7765](https://github.com/86Box/86Box/pull/7765) | `bad extended memory` — the high `0x5F0000` alias must read shadow RAM, not ROM. Now reports **0k** |
| [#7766](https://github.com/86Box/86Box/pull/7766) | POST 1801 on every boot — the machine must not default to a 5161 expansion unit |
| [#7771](https://github.com/86Box/86Box/pull/7771) | The XT 4-bit DMA page latch — truncation was gated on `dma_at`, so an Inboard got an 8-bit page register it does not physically have |
| [#7858](https://github.com/86Box/86Box/pull/7858) | XT-IDE logging was inert on the plain card — only `jride_init()` opened a log handle, so `xtide_log()` wrote to NULL |
| [#8010](https://github.com/86Box/86Box/pull/8010) | A parallel-port LS-120: the Shuttle EPAT bridge, three optional ECP callbacks on `lpt_device_t`, and the SuperDisk drive type enabled |
| [#8012](https://github.com/86Box/86Box/pull/8012) | A parallel-port CD-ROM: `CDROM_BUS_LPT` implemented, the Micro Solutions BackPack modelled from hardware, and a `scsi_cdrom_current_mode()` fix - an unrecognised bus got "no transfer", so the drive enumerated and returned no data for any command |
| [#8076](https://github.com/86Box/86Box/pull/8076) | The 3Com EtherLink III ISA (3C509B), modelled on the card in this machine: jumperless ID-port configuration, its real EEPROM as the template, defaults that work in an XT slot. Merged 2026-09-24; also confirmed by others under NT 3.5 and Linux |
| [#8078](https://github.com/86Box/86Box/pull/8078) | The LPT bridges from #8010/#8012: IDE/SCSI CD-ROMs no longer create a phantom BackPack on LPT1; an LPT CD-ROM is initialised (its first long seek stopped the emulator) and no longer pokes IDE channel 0; both bridges use the port their drive is set to, and a CD-ROM's port is saved; Settings offers ATAPI models for an LPT CD-ROM and keeps it on LPT; the Media menu names the LPT bus; the SuperDisk 120 reports the real drive's MATSHITA identity; logging no longer forced on. Merged 2026-09-25. Tested by the owner on Windows 95 and DOS; an LS-120 on LPT2 and the vendor LS-120 DOS driver were not |

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

- **`drivers/`** — the Windows 95 storage drivers written for this machine, and the
  reverse-engineering behind them. This is where most current work happens:
  - **`xtide_mpd/`** — `XTIDEMP.MPD`, the 32-bit protected-mode XT-IDE driver. Shipped;
    serves `C:` on real hardware ([#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21))
  - **`trantor_t130b/`** — Adaptec's `T130.MPD`, with the `Polling=1` registry entry and the
    notes needed to make it work without an IRQ ([#19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19))
  - **`microsolutions_backpack/`** — the BackPack parallel CD-ROM: the vendor DOS package as
    shipped (`vendor/`), a full disassembly of `BPCDDRV.SYS`, the wire protocol measured off a
    real drive, the 86Box device, and the drive's own identity EEPROM read over the parallel
    port (`capture/`). Enough to reproduce the whole thing
  - **`imation_ls120/`** — the LS-120 reverse-engineering: full disassemblies of the vendor's
    Win95 miniport and DOS driver, `TRANSPORT_SPEC.md` (the parallel-port wire protocol,
    derived and proven on hardware), and `tools/pedis.py`, a PE disassembler that works on any
    period driver
  - **`imation_ls120_mpd/`** — ⛔ **retired, do not install.** This project's own replacement
    miniport, built from that spec. It reached read-only operation on the real 5160 and never
    wrote correctly; the vendor driver above does both, and faster. Kept as a record of the
    transport work, not as a driver — see
    [`what_worked_and_what_didnt.md`](docs/what_worked_and_what_didnt.md)
    ([#22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22))
  - **`xtide_cdrom/`** — notes on reaching an ATAPI CD-ROM through XT-IDE
- **`86box_full/`** — the 86Box emulator fork: the Inboard 386/PC hardware model
  (`src/device/inboard386.c`) plus the debug/tracing hooks used throughout this investigation.
  Carries upstream 86Box's own `.gitattributes` so it stays diffable against master
- **`vxd-patches/`** — the Windows 95 binary patches and the scripts that produce them:
  `VKD.VXD`, `VPICD.VXD`, `VDMAD.VXD`, `KEYBOARD.DRV`, `MSSBLST.VXD`, `HSFLOP.PDR`,
  `INBRDPC.SYS`, each alongside the stock original it derives from
- **`custom_vkd/`** — full assembly source for the custom-built `VKD.VXD` (Microsoft's own DDK
  sample, modified), plus the build script for the genuine period MASM/LINK toolchain
- **`ivt68fix/`** — source + binary for the real-mode INT 68h vector fix deployed on real hardware
- **`vm_win311/`, `mach8_w31_display/`** — the Windows 3.11 side: `IBKBD.DRV`, `IBVKD.386`, the
  INT 15h shim source, and the Mach8 Windows 3.x display driver
- **`vm_xtide_inboard/`** — the reference 86Box config the XT-IDE driver work is tested against
- **`dist/post-install-fixes/`** — what you actually download: the patched files that must be
  applied *after* driver installation, plus the DMA audit scripts
- **[`dist/ls120_vendor/`](dist/ls120_vendor/)** — Have Disk package for the
  parallel-port Imation SuperDisk LS-120: the vendor's Windows 95 driver with its probe
  suppressors applied **at install**, so the keyboard survives it. One INF line differs from
  the vendor original
- **`tools/`** — deployment and capture scripts (`deploy_sound_fix.sh`, `deploy_premonolith.sh`,
  image DMA sweeps, VM setup). Bash — pinned to LF in `.gitattributes`

**Evidence and reference**

- **`hardware/`** — real 5160 reverse-engineering: `INBRDPC.SYS` disassembly, PAL/GAL analysis
- **`test_harness/`** — small real-mode test programs used to isolate a bug without a full
  Windows boot each time
- **`roms/`** — system, video and peripheral ROMs, including `video/ATI_MACH8.bin`, a real dump
  of the ATI Graphics Ultra BIOS not known to be available elsewhere
- **`references/`, `screenshots/`** — third-party specs, and real-hardware and emulator captures
- **`docs/captures/`** — raw per-session capture sets, kept where a later claim depends on them
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

Everything learned the hard way is written back into three skill files, so it is not re-derived.
They are plain Markdown and readable without the tooling.

- **`.claude/skills/inboard-hw-debug/`** — the hardware/timing/boot debugging methodology.
  **127 numbered techniques**, each one written after it resolved *or ruled out* a real bug
  here. Several carry retractions of their own earlier conclusions, which are the most useful
  lines in the file
- **`.claude/skills/win9x-dma-driver-audit/`** — finding Windows 9x drivers that assume 24-bit
  DMA reach on 20-bit hardware
- **`.claude/skills/repo-hygiene/`** — keeping this repository legible to outside contributors

The Windows 95 boot fix inventory that used to live inside the first of these is now
[`docs/win95_boot_fix_inventory.md`](docs/win95_boot_fix_inventory.md) — the canonical list of
everything required to boot Windows 95 on this machine, emulator-side and disk-image-side.

## Credits

Full detail in the [writeup's credits section](docs/windows95_on_inboard386pc_writeup.md#sources-and-prior-art)
and the [contributor ledger](docs/contributor_input_ledger.md).

- **[@andrew-hoffman](https://github.com/andrew-hoffman)** — **the project's most consistent
  outside contributor**, and repeatedly the reason it changed direction. The XT 4-bit DMA
  page-register lead and the sources behind it, which produced the 640 KB figure and an emulator
  fidelity bug now upstream as [#7771](https://github.com/86Box/86Box/pull/7771); the driver-audit
  method; the steer that Trantor T128/T130 might work because the SCSI port drivers came from
  NT 3.51, which is the only reason anyone went looking for `T130.MPD` (#19); his emulated T130B
  boot, which proved the IOS stack accepts a 32-bit miniport and became the control for `XTIDEMP.MPD`
  (#21); the media-change floppy corruption still open as #18; the bus-throughput and
  memory-mapped-storage framing behind #35 and the optimisation track; and this repo's writing and
  line-ending conventions.
- **[Stynx and Harrison Frazier](https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/)**
  (VCFed) — the 4MB Inboard daughterboard (ParrotyError). Windows 95 does not fit without it.
- **SuperFury / [UniPCemu](https://superfury.itch.io/unipcemu)** — this project's entire Inboard
  hardware model is a direct port of UniPCemu's `hardware/inboard.c`. The foundation everything
  else is built on.
- **[Bob Smith](https://github.com/sudleyplace)** (Qualitas) — author of **386MAX**, whose source
  carries first-class Inboard support and is the primary-source evidence for the XT DMA ceiling.
  He states he had no involvement with the Inboard itself: [full detail and quotes](docs/386max_and_the_inboard.md).
- **Al Williams** (Dr. Dobb's Journal, Hackaday) — real 1990s hands-on Inboard development
  experience; his 1990 A20 code matches this project's emulation exactly.
  [Correspondence, 2023](docs/al_williams_inboard_a20_correspondence_2023.md).
- **Michal Necasek** ([OS/2 Museum](https://www.os2museum.com/)) — architectural confirmation,
  historical leads, and a verified `F000:FF53` improvement now upstream.
- **[cimonvg](https://forum.vcfed.org/index.php?members/cimonvg.8268/)** - From vcfed ongoing work pushing the Inboard 386/PC to its limits, and support throughout.
- **[RonnyRoy](https://github.com/ronnyroy111/inboard386)** — reproducing the Inboard as cloned
  hardware, which may be the path past today's 4MB ceiling.
- **Feipoa** — twice now the authority this project has landed on for the CPU upgrade module.
  The [CTCHIP/KTCHIP34 write-up](https://www.vogons.org/viewtopic.php?t=45756) (Vogons) and the
  register-level approach behind it, which closed issue #9; and the
  [IBM 486BL3 module's DIP-switch table](https://www.cpu-world.com/forum/viewtopic.php?t=33652&view=previous&)
  (cpu-world), including the warning that saved us a bad idea — an IBM-based system needs `SW1`
  **ON**, so the cache flushes on every I/O access and that is a constraint rather than a lever.
- **Fenix770** — the VM attachment that root-caused the shadow-RAM alias failure.
- **Wim Osterholt** — [XT, AT and PS/2 I/O port addresses](https://wiki.preterhuman.net/XT,_AT_and_PS/2_I/O_port_addresses)
  (1994), which marks entries `(XT only)` — the exact distinction most bugs here turn on.
  Annotated in [`docs/xt_io_port_reference_annotated.md`](docs/xt_io_port_reference_annotated.md).
- **[FastDoom](https://github.com/viti95/FastDoom)** (viti95) — real-hardware-validated XT
  keyboard ISR reference.
- **Microsoft's Windows 95 DDK** — the genuine period source and toolchain behind the `VKD.VXD` fix.
- **[Kevin Moonlight](https://github.com/yyzkevin)** — original author of
  [COMrade](https://github.com/yyzkevin/COMrade), which is how this project reads and writes the
  real 5160 while it is running; almost every hardware measurement here came back over it. Also
  PicoPCMCIA, and contributions to [ISA-PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM) and
  [PicoGUS](https://github.com/polpo/picogus) — the **CD-ROM emulation** and the **WiFi code**,
  among others. Those projects are read here for what transfers to an 8-bit bus
  ([`resources_and_sources.md`](docs/resources_and_sources.md) §9).
- **Ahmad Byagowi** ([Open-Source-PC110](https://github.com/ahmadexp/Open-Source-PC110)) — ported
  COMrade to Windows 95 as `COMR95.EXE`, used for live real-hardware introspection.
- **[86Box](https://github.com/86Box/86Box)** — the base emulator this project is built on.

## Reproducing this

Windows 95 install media isn't included here (copyrighted, and large) — you'll need your own OSR1
media. The [writeup's reproduction section](docs/windows95_on_inboard386pc_writeup.md#reproducing-this)
lists exactly which files to patch and where to place them on a pre-monolith install.

**The sound fix is not one of them, and cannot be.** A pre-monolith image contains no
`MSSBLST.VXD` at all — it arrives from the `WIN95_xx.CAB` files when you install the Sound Blaster
Pro driver, stock and unpatched. Apply `dist/post-install-fixes/` **after** installing the driver,
not to the image.

## Licence

**[MIT](LICENSE)** for this project's own work — the XT-IDE Windows 95 miniport
(`XTIDEMP.MPD`), the patch scripts, the tools, the INFs and the documentation. Use it, port it,
build on it; keep the copyright notice.

The repository also carries third-party material so results stay reproducible, and **each of those
keeps its own licence**: 86Box is GPL-2.0 (and this project's changes to it are too), Adaptec's
`T130.MPD` is Adaptec's, ROM dumps belong to their owners, and the patched Microsoft VxDs are
Microsoft's — the *patches* are ours, the binaries are not. [`LICENSE`](LICENSE) sets out the split
in full, and each driver directory's README carries the specific provenance and md5s.

## Contributing

Issues and PRs welcome. Most open issues carry a **Status** block at the top, so you can see
where they actually stand without reading the thread.

### The most useful thing anyone could pick up

**[#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18) — build the floppy
media-change reproduction bed.** @andrew-hoffman hit a fatal exception and a corrupted disk after
changing floppies a few times, in 86Box, on his configuration. The DMA-reach bug that `HSFLOP_XTDMA.PDR`
fixes is a different fault, and reads and writes were measured clean here on real hardware — but that
harness wrote twice to one disk and **never changed media**, so it never tested the recorded trigger.

**Needs no Inboard and no hardware at all.** 86Box, the Monster Floppy controller, the patched driver,
and a media change. The job is to make it happen on demand; the diagnosis comes after. If it does not
reproduce, that is worth knowing too, and it goes on the issue either way.

Everything needed is published: the patched driver is in [FIXES.md](FIXES.md) with its md5, and the
analysis so far — a stale cache page flushed to the wrong disk, a media-change detection failure
rather than a DMA-reach one — is on the issue.

### Open issues

| | |
|---|---|
| [#7](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/7) | Setup black-screens right before the Help files (reboot works around it). Undiagnosed and unclaimed |
| [#10](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/10) | Idea: a loadable BIOS-extension shim so 1982-era 5150/5160 ROMs can run Windows |
| [#14](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/14) | POST intermittently halts with 101, at `mem_size` 2688 and 3072. Needs a quiet build, not `86box_full` |
| [#15](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/15) | Windows 3.0 faults after the splash screen in 386 enhanced mode |
| [#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18) | Floppy corruption after a media change. DMA reach is fixed (`maxPhys 0x1000 -> 0xFF`, shipped as `HSFLOP_XTDMA.PDR`) and reads/writes measured clean here, but that harness never changed media, which is this issue's trigger. The reproduction bed - 86Box + Monster Floppy + the patched driver + a media change - is still to be built, and needs no hardware |
| [#20](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/20) | 86Box has no 3C509B. **Merged** in [86Box#8076](https://github.com/86Box/86Box/pull/8076), modelled on this machine's card. Unblocks the `ELNK3.VXD` polling work (#41) and the IRQ 9 → 2 work (#42). Reported on #8076 to freeze Windows for Workgroups 3.1 at boot; to be reproduced on our 3.11 image, which shares the driver base |
| [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23) | `XTIDEMP.MPD` cannot drive the XT-IDE Hi-Speed register map - an A3/A0 swap is a permutation, and the driver computes `base + index * stride`. Blocked on hardware to test against, and on 86Box having no Hi-Speed model |
| [#28](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/28) | Per-component audit: walk every driver and VxD, six questions each |
| [#29](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/29) | Measure DMA: reach, cost per byte, channel inventory, CPU overlap |
| [#31](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/31) | The SCSI chain: does it disconnect, and are the target caches on? Every target has one, and this is the most on-point mechanism in the machine |
| [#33](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/33) | DRAM refresh tuning: a tax every device pays |
| [#34](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/34) | Display mode as a bus lever: 1024x768 vs 800x600 vs 640x480 |
| [#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35) | Shadow RAM inventory, and memory-mapped storage in the emulator |
| [#36](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/36) | `fatcp.py` / `fatls.py` claim FAT12 support but are FAT16 only |
| [#37](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/37) | BackPack parallel CD-ROM. **Merged** in [86Box#8012](https://github.com/86Box/86Box/pull/8012). The regression it introduced (every IDE or SCSI CD-ROM also created a BackPack on LPT1), and the other LPT defects found testing that fix, are fixed in [86Box#8078](https://github.com/86Box/86Box/pull/8078), merged |
| [#38](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/38) | Wire `lpt_epat.c` to upstream's EPP callbacks - the transport the real hardware uses |
| [#40](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/40) | CPU upgrade module: the registers we never explored, and its switches. `CMLR` was one register and took Dhrystone from 2 to 13-15; **`XTOUT` is set where feipoa recommends 0** and has never been tested. The module's switches are undocumented here |
| [#41](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/41) | Pace the polling in every driver that spins: `T130.MPD`, `HSFLOP.PDR`, `ELNK3.VXD`. A poll is **5.55 us** of bus moving nothing against **0.22 us** for a cached delay - and since 2026-09-21 we know it also **flushes the L1**, so each poll removed is worth more than its bus time |
| [#42](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/42) | T130B: an IRQ needs the 3C509B moved off 3 first. The card's jumpers offer **3, 5 or 7 only** - 5 is fixed for the SB Pro, 7 for LPT1 - so the NIC is the one movable claimant, and its only route is IRQ 2 via the card's "IRQ 9" setting (same B4 slot pin on an XT). An IRQ moves no bytes, but **no IRQ is what forecloses #31**: catching a SCSI reselection needs one. The fix is Windows-side - `VPICD` delivering master IRQ 2 to the driver as IRQ 9 - not an emulator change |
| [#43](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/43) | IOCHRDY: what a held bus actually costs. Our own three-window fit already isolates it - per bus cycle **1.978 us** (XT-CF ROM) against **3.805 us** (Mach8 video), same machine, same fixed sync term. And **86Box models no bus stall at all**, so any lever whose whole benefit is holding the bus for less time measures as zero in the bed |
| [#44](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/44) | The vendor LS-120 DOS driver does not initialise on 86Box's EPAT model (pre-existing). The Windows miniport works; next is `CPP(0x40)`/`CPP(0x50)` in `lpt_epat.c`, on an AT bed, clear of the XT chipset-probe bug described under the LS-120 above |

Issues are labelled **`emulator`** or **`real-hardware`** so you can pick by what you have, and
**`upstream`** marks the ones destined for 86Box itself.

**You do not need an Inboard to help.** Most of this was found in emulation, on an 86Box build that
is [in this repo](86box_full/) and now [upstream](https://github.com/86Box/86Box/pull/7626).

### Before installing any stock driver: audit its port writes

**[The XT I/O aliasing gotcha](docs/xt_io_aliasing_gotcha.md)** — an IBM 5160 decodes I/O
incompletely, so its devices answer across far wider ranges than their documented addresses.
Measured on the real machine: ports `0x21`, `0x23`, `0x25`, `0x31` and `0x3F` **all** return the
8259 interrupt mask. AT-era drivers probe for host chipsets at `0x22`/`0x23`, that probe succeeds
against nothing, and the follow-up configuration writes reprogram the interrupt controller.

The Windows 95 LS-120 driver does exactly this and silently masks IRQ 1 — **the keyboard stops
working, with no error anywhere** ([#22](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22)).
The DOS build of the same driver has `/ni` "Skip chipset initialization"; the protected-mode
miniport exposes no equivalent.

```
python dist/post-install-fixes/scripts/xt_port_audit.py YOURDRIVER.MPD
```

Symptom to recognise: **one interrupt-driven device dies and the others do not.** Look at the 8259
mask before the driver stack. Note that 86Box does not model the alias, so this class of bug cannot
reproduce in emulation.

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
