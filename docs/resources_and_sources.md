# Resources and sources

One page for everything this project has been given or has found: the software, the datasheets,
the forum threads, the source trees. Collected here because until now it was scattered across
twenty documents and a hundred issue comments, and a link buried in comment 31 of a closed issue
helps nobody.

Two standing rules, from `CLAUDE.md`:

- **Name plus URL plus one line on what it actually gave us.** The link *is* the credit, and it
  points the next person at something real.
- **Record what a source did not contain.** That saves the next reader a dead end. Those notes are
  marked ❌ below and they are the most useful lines on the page.

People, as distinct from sources, are tracked in
[`contributor_input_ledger.md`](contributor_input_ledger.md) — who gave what, whether it was
verified or disproved, and whether they have been told. This page does not duplicate it.

---

## 1. Software held in this repository

Kept here so a clone is enough. Everything below carries its original licence and provenance in
the directory's own `README.md`.

| What | Where | Provenance | What it is for |
|---|---|---|---|
| **XT-IDE ATAPI CD-ROM driver** (DOS) | [`drivers/xtide_cdrom/`](../drivers/xtide_cdrom/) | `patacd.asm`, sava (t.ebisawa) / lpproj, 2016, ZLIB. **XT-IDE port by [Miran Grča (@OBattler)](https://github.com/OBattler)**, 86Box's developer, posted to the 86Box Discord. Via @andrew-hoffman, issue #21 | The only known working code that drives the XT-IDE 8-bit data latch. Reference for the 32-bit driver (#21). ⚠️ Use `xtidecd(1).sys`; `xtidecd.sys` and the `.asm` are a broken build |
| **Trantor T130B miniport** | [`drivers/trantor_t130b/`](../drivers/trantor_t130b/) | Adaptec `T130.EXE` self-extractor, [archive.org](https://archive.org/details/T130_EXE) | `T130.MPD` — a vendor 32-bit PIO SCSI miniport. `T130-XT.INF` adds `Polling=1` and this machine's I/O base |
| **Imation LS-120 miniport** | [`drivers/imation_ls120/`](../drivers/imation_ls120/) | Imation `SD120PPD.MPD` plus five patch attempts | Issue #22. Parked: six patches, six null results. The real-mode driver works and is what the machine runs |
| **Custom `VKD.VXD`** | [`custom_vkd/`](../custom_vkd/) | Built from the 1995 Win95 DDK source | Fixes `VKD_Int_09`'s AT-only port-0x64 check. First working Win95 keyboard input on real Inboard hardware |
| **`IVT68FIX.COM`** | [`ivt68fix/`](../ivt68fix/) | This project | `INT 68h` / segment-650B fix for OSR1 |
| **Patched VxDs and `.PDR`s** | [`vxd-patches/`](../vxd-patches/), [`dist/post-install-fixes/`](../dist/post-install-fixes/) | This project | The shipped fix set. `FIXES.md` is the index |
| **Sound Blaster Pro Have Disk set** | [`dist/havedisk-sbpro/`](../dist/havedisk-sbpro/) | This project | Issue #5's fix, installable without a working floppy |
| **DMA / port audit tooling** | [`dist/post-install-fixes/scripts/`](../dist/post-install-fixes/scripts/) | This project | `vxd_dma_audit.py`, `sweep_image_dma.py`, `xt_port_audit.py` — find drivers whose DMA buffer exceeds this machine's reach, and probe I/O aliasing |
| **Peripheral and system ROMs** | [`roms/`](../roms/) | Various | Mach8, T130B, Sergey's floppy card, the IBM 5160 BIOS tree, `roms/hdd/xtide/` |

## 2. Software *not* held here, and why

| What | Where | Why not held | Note |
|---|---|---|---|
| **Windows 95 DDK** (`BLOCK/SAMPLES/PORT/`, MASM 6.11) | Microsoft, 1995 | Not redistributable | The sample IOS port driver is the starting skeleton for #21 |
| **[zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x)** | GitHub, MIT | Upstream is maintained; clone it | Nick (@zikolas)'s from-scratch Win9x IOS port driver plus a free toolchain recipe (JWasm + Open Watcom v2). The proof that #21 is buildable |
| **[COMrade](https://github.com/yyzkevin/COMrade)** / COMR95 | GitHub | Upstream | Kevin Moonlight's serial bridge; the [Open-Source-PC110](https://github.com/ahmadexp/Open-Source-PC110/tree/main/Software/COMrade) fork by Ahmad Byagowi is the build used. **This is the measurement path for the real machine** |
| **[386MAX](https://github.com/sudleyplace/386MAX)** | GitHub | Upstream, and large | Bob Smith (Qualitas), open-sourced ([sudleyplace.com](http://www.sudleyplace.com)). Source of the Inboard's real memory-manager behaviour; `ILIM386.SYS` turned out to be 386MAX |
| **[XTIDE Universal BIOS](https://www.xtideuniversalbios.org/)** | Project site | Upstream | The real-mode side of the card, and the 386 build |
| **[FastDoom](https://github.com/viti95/FastDoom)** (viti95) | GitHub | Upstream | Real-hardware-validated XT timing and video code |
| **Adaptec `T128.EXE` / `T338.EXE`** | [T128](https://archive.org/details/T128_EXE), [T338](https://archive.org/details/T338_EXE) | Not needed yet | Siblings of T130. `T128.INF` is where the `Polling=1` precedent came from |
| **Andrew's booting 86Box T130B image** | [Google Drive](https://drive.google.com/file/d/1h0UwPHDcNkupqkuVRaeIFZg6AosWrQp9/view?usp=sharing) | Far too large for git | Win95 booting from emulated T130B SCSI with 32-bit disk access and paging. Needs 86Box build 9812 or newer. Issue #19 |
| **This project's disk images** | [archive.org](https://archive.org/details/win95-intel-inboard-386pc), and GitHub Releases | Size | v1 on archive.org; v2 on Releases |

## 3. Hardware references

- **[IBM PC/XT 5155 & 5160 Technical Reference, March 1986](https://archive.org/details/IBMPCXTIBM51555160TechnicalReference6280089MAR86)**
  — the system board I/O address map. This is what backs the 8259 aliasing across `0x20-0x3F`:
  IBM decoded the PIC on a partial address range, so it answers at sixteen addresses, not two.
  Authoritative, and the reason that claim can go upstream.
- **[`measured_system_map_2026_08_30.md`](measured_system_map_2026_08_30.md)** — what the real
  machine actually answers, read with COMrade at a DOS prompt: PIC mask, the 8259 alias, the
  XT-IDE base, the Mach8 accelerator's `SUBSYS_STAT`, the T130B. Two readings in it are recorded as
  **unexplained** rather than fitted to a story; that is deliberate.
- **Wim Osterholt, [XT, AT and PS/2 I/O port addresses](https://wiki.preterhuman.net/XT,_AT_and_PS/2_I/O_port_addresses)** (1994)
  — the `(XT)` / `(XT only)` markings are what distinguish this machine from an AT, port by port.
  Curated for this project in [`xt_io_port_reference_annotated.md`](xt_io_port_reference_annotated.md).
- **[monotech/NuXT](https://github.com/monotech/NuXT)** and **[spencer-uk/DubaiXTClone](https://github.com/spencer-uk/DubaiXTClone/)**
  — full KiCAD schematics for XT recreations, suggested by @andrew-hoffman. Both leads paid off.
  ❌ Neither is an *IBM* board, so decoding differences must be checked against the IBM tech ref
  above before being treated as original behaviour.
- **[Dallas DS1315 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS1315.pdf)**
  — phantom clock, for the RTC on the XT-IDE card.
- **[skiselev/isa-fdc](https://github.com/skiselev/isa-fdc)** — Sergey Kiselev's floppy controller.
  ❌ Its option ROM at `0xC8000` hangs POST on this machine; deliberately not loaded.
- **Intel Inboard 386/PC**: [US Patent 5,307,459](https://patentimages.storage.googleapis.com/58/f6/19/fb9a77033128bd/US5307459.pdf),
  and Al Williams' 1990 *Dr. Dobb's* article ([archive.org](https://archive.org/details/dr_dobbs_journal_vol_15/page/613/mode/2up)).
  Al's real `a20()` code and correspondence: [`al_williams_inboard_a20_correspondence_2023.md`](al_williams_inboard_a20_correspondence_2023.md).
- **[VCFed: Inboard 386/PC 2 MB expansion clone](https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/)**
  — Stynx and Harrison Frazier's daughterboard. Valid board sizes are **1024 / 3072 / 5120 KB only**.
- **[ronnyroy111/inboard386](https://github.com/ronnyroy111/inboard386)** — RonnyRoy reproducing the
  Inboard as cloned hardware.

## 4. Video — ATI Mach8 / 8514-A

- **[Ardent Tool: ATI 8514 Ultra](https://www.ardent-tool.com/video/ATI_8514_Ultra.html)** and
  **[Mach8 drivers](https://www.ardent-tool.com/video/ATI_mach8_Drivers.html)** — the detailed
  register and driver documentation.
- **[8514/A register reference (PDF)](https://www.ardent-tool.com/video/8514A_Registers.pdf)**.
- **Michal Necasek, [*The 8514/A Graphics Accelerator*](https://www.os2museum.com/wp/the-8514a-graphics-accelerator/)**
  — reframed issue #8. **[PRIMARY]**
- **[DOSDays: ATI Mach8](https://www.dosdays.co.uk/topics/Manufacturers/ati/ati_mach8.php)** — the
  real card's jumper block.
- ❌ The "28800 VGA core + 38800 coprocessor" description circulating in AI-sourced notes is
  **unverified** — check it against Ardent Tool before relying on it.

## 5. DMA, memory and the 8237

- **Michal Necasek (OS/2 Museum)**: [386MAX and EISA DMA](https://www.os2museum.com/wp/386max-and-eisa-dma/),
  [8237A DMA page fun](https://www.os2museum.com/wp/8237a-dma-page-fun/) — why the page register
  on an XT is 4 bits, and what that does to every driver that assumes 24-bit reach. **[PRIMARY]**
- **[MartyPC book: the 8237 DMA controller](https://book.martypc.net/support-chips/dma-8237)** —
  a clear modern write-up of the same part.
- The consequence for this project is written up in
  [`xt_dma_20bit_audit_2026_08_24.md`](xt_dma_20bit_audit_2026_08_24.md) and enforced by the
  `win9x-dma-driver-audit` skill.

## 6. Writing a Win9x driver (issue #21)

- **[zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x)** — MIT. `win/vxd/CFU1.ASM` is a
  complete IOS port driver and TSD; `win/build.sh` is a working JWasm + Open Watcom v2 recipe;
  `win/get-ddk.sh` fetches the DDK pieces. **The MIT notice must travel with anything adapted.**
  Also by Nick: [vsbpcmcia](https://github.com/zikolas/vsbpcmcia), Sound Blaster emulation for
  DMA-less machines via PIO passthrough — directly relevant to this bus.
- **Windows 95 DDK, `BLOCK/SAMPLES/PORT/SAMPLE/`** — `PORT.ASM`, `PORTAER.ASM`, `PORTISR.ASM`,
  `PORTREQ.ASM`. The AEP surface is only five function codes. Its DRP declaration matches
  `CFU1.ASM` line for line, which cross-checks both readings.
  ❌ `PORTISR.ASM` is probably not needed — the XT-IDE card is jumpered without an interrupt, so
  the driver polls and the whole VPICD interaction disappears.
- **[`drivers/xtide_cdrom/`](../drivers/xtide_cdrom/)** — the register-level transport, and
  master/slave via the DEV read-back. See §1.
  ❌ It does **not** independently confirm 86Box's register map: the emulator and this driver's
  XT-IDE port are by the same author (@OBattler).
- **MS KB [Q132061](https://jeffpar.github.io/kbarchive/kb/132/Q132061/)** — behind `fd8xx.mpd`,
  the in-box protected-mode miniport for an 8-bit ISA SCSI card. Proof that Microsoft shipped
  32-bit storage for 8-bit cards.
  ❌ Windows 95 does **not** ship a T128/T130 miniport; that is why `T130.MPD` had to be found.
- **[VCFed, "XTIDE and Windows 95 issues"](https://forum.vcfed.org/index.php?threads/xtide-and-windows-95-issues.52115/)**
  (3 pages, read 2026-09-01) and **[VOGONS t=80163](https://www.vogons.org/viewtopic.php?t=80163&start=20)**.
  Sent by the project owner after a search for prior attempts. Useful for `0040:0075` — post #10
  quotes the BIOS Boot Specification's controller rules, which is the same BDA count this project
  already verified.
  ❌ **Neither contains a 32-bit XT-IDE driver, and the "CMOS 0x12 dummy drive" trick does not
  apply here.** That trick makes Windows' *own* `ESDI_506.PDR` take over a real AT IDE controller
  at `0x1F0` that the system BIOS had been told to hide; the XT-IDE ROM only supplies the
  translated geometry. A 5160 has no CMOS, no `0x1F0` controller, and an 8-bit card on a 2-byte
  register stride at `0x300` that `ESDI_506.PDR` could not drive if it were enabled. The VOGONS
  thread is Windows 3.1 FastDisk (`WDCTRL`/MicroHouse/Ontrack), a different subsystem entirely.
  ⚠ The covering summary was **[AI-SOURCED]** (Google AI Overview text, citation markers intact);
  the CMOS claim is real but its applicability here is not — Technique 58.
- **[BetaArchive: slipstreaming patched files into a Win95 install](https://www.betaarchive.com/forum/viewtopic.php?t=29398)**
  — the harder of the two routes; rewriting an INF to reference patched copies is easier.
- **Microsoft's own IOS port drivers, from the machine itself** — `ESDI_506.PDR`, `SCSIPORT.PDR`
  and `HSFLOP.PDR` in `D:\WINDOWS\SYSTEM\IOSUBSYS`, copied to
  [`roms/xtcf_card/`](../roms/xtcf_card/) as `*_reference.PDR`. The best available documentation
  of the polling contract, because they implement it. All three call `Set_Global_Time_Out`; our
  `PORT.PDR` calls no VMM service at all. `HSFLOP` is the closest model — it is the only one that
  must survive a device that may not answer. Scan them with
  [`tools/pdr_vxd_services.py`](../tools/pdr_vxd_services.py); see technique 88.
  ❌ `ESDI_506.PDR` is the *weaker* reference despite being the obvious one: IDE has IRQ 14, so it
  completes from an interrupt rather than from a timeout handler.
- **Rudolph R. Loew's Win9x patches** —
  [rloewelectronics.com](https://rloewelectronics.com/) (certificate expired),
  [Phil's Computer Lab](https://www.philscomputerlab.com/rudolph-r-loew-patches.html),
  [bundle mirror](https://retrosystemsrevival.blogspot.com/2020/06/rloew-9598me-patches-bundle.html).
  Thirty-odd binary patches including several to `ESDI_506.PDR`, plus an "IO8 Decompresser".
  ❌ **Nothing here documents port-driver structure.** The patches are capacity/RAM limit fixes
  shipped as binaries; the bundle carries no disassembler and no structural notes. The
  decompressor is not needed for this work — the stock `.PDR` files are not compressed.
- **[MSFN, "137GB limit - ESDI_506.PDR and other limits"](https://msfn.org/board/topic/46752-137gb-limit-esdi_506pdr-and-other-limits/)**
  ❌ Read 2026-09-04. Spec-level LBA48 discussion only; the author states he worked from the
  ATA/ATAPI-7 specification, **not** from the binary. No entry points, no request flow, no
  timeout handling. Save the next reader the click.
- Full assessment: [`win9x_port_driver_feasibility.md`](win9x_port_driver_feasibility.md).

## 7. Emulator and upstream

- **[86Box](https://github.com/86Box/86Box)** — the base emulator. Everything this project fixed is
  now merged; the README's upstream section carries the PR list.
- **[86Box discussion #6447](https://github.com/86Box/86Box/discussions/6447)** — an existing
  request for 3C509B emulation, which is issue #20's blocker.
- **[cyberkinetica QEMU patches](https://cyberkinetica.homeunix.net/qemu/)** — a 1,126-line
  `hw/3c509b.c`. The source for [`3C509B_PORT_SPEC.md`](../3C509B_PORT_SPEC.md).
  Card reference: [TheRetroWeb](https://theretroweb.com/expansioncards/s/3com-etherlink-iii-3c509b-tpo),
  ROM dump thread: [VCFed](https://forum.vcfed.org/index.php?threads/dump-rom-for-network-card-3c509b-tpo.1244293/#post-1413897).
- **[SuperFury / UniPCemu](https://superfury.itch.io/unipcemu)** — this project's entire Inboard
  model is a port of SuperFury's work.
  ❌ The port omitted the port-0xA0 remap gating and the `MMU.maxsize` recompute; that omission
  was the `bad extended memory` bug, not a regression.
- **[86Box#7805](https://github.com/86Box/86Box/issues/7805)** — @Fenix770's report that the Machine
  settings dialog snapped RAM to a wrong value. Fixed upstream by OBattler; may also explain #14.
- **[Michal Necasek / OS/2 Museum](https://www.os2museum.com/)** — architectural confirmation
  throughout, including the verified `F000:FF53` improvement. **[PRIMARY]**

## 8. Everything else

- **Fabien Sanglard, [`agent.md`](https://fabiensanglard.net/agent.md/index.html)** — the writing
  rules in `CLAUDE.md` and the `repo-hygiene` skill are adapted from it. Suggested by
  @andrew-hoffman on issue #3.
- **@andrew-hoffman's [`.gitattributes`](https://github.com/andrew-hoffman/WDMHDA/blob/main/.gitattributes)**
  — adopted here, and it caught a live latent bug in the deploy scripts' line endings.
- **[Feipoa, CTCHIP/KTCHIP34](https://www.vogons.org/viewtopic.php?t=45756)** (Vogons) — driving 486
  upgrade-chip registers directly. Closed issue #9.
- **[Vogons thread on Inboard behaviour](https://www.vogons.org/viewtopic.php?p=1392798#p1392798)**.
- **[PCjs: Windows 95 startup and processor checks](https://www.pcjs.org/blog/2015/10/27/)**.
- **[Dynabook support: LS-120 and compatibility mode](https://support.dynabook.com/support/viewContentDetail?contentId=108303)**
  — found by @andrew-hoffman; the first partial escape from MS-DOS compatibility mode.
- **[MSYS2](https://www.msys2.org/)** and **[vcpkg](https://github.com/Microsoft/vcpkg.git)** —
  the 86Box build toolchain. See [`BUILD_SETUP.md`](../BUILD_SETUP.md).
  ⚠️ `PATH=/c/msys64/mingw64/bin` must be set or the build fails silently, with no message.
- **[The project video](https://youtu.be/KxuKTNQyBKE?is=OkK0_sxReKwSKeK6)**.

---

*Adding to this page: put the software in the repository if the licence allows and it is small
enough, give it a directory `README.md` with its provenance, and add a row above. If it cannot be
hosted, say where it lives and why it is not here. If a source turned out not to contain what was
hoped, add it anyway with a ❌ — that is worth as much as a hit.*

## zikolas/cfu1-win9x - Nick (@zikolas), MIT

<https://github.com/zikolas/cfu1-win9x>

A working Windows 9x IOS port driver for the RATOC REX-CFU1 (USB host CF+ PC Card). Load-bearing for
issue #21 twice over:

- **`win/vxd/CFU1.ASM`** - never fails `AEP_INITIALIZE` ("stay resident either way"), and fetches
  resources from CONFIGMG's `CONFIG_START` callback via `_CONFIGMG_Get_Alloc_Log_Conf`, not during
  initialisation. That is what our phase-1 driver got wrong; see `drivers/xtide_pdr/README.md`.
- **`PROBE-NOTES.md` / `README.md`** - documents the IOS AEP sequence and function/result numbers,
  and the finding that `DISKTSD` never configures a dynamically registered port driver's DCB.

What it does **not** contain: anything about 8-bit ATA task-file addressing or the XT-CF register
stride. It is a PC Card/USB device; the value is the IOS-side contract, not the transport.

Not vendored - it is a live MIT upstream. Clone it beside this repo to read it.
