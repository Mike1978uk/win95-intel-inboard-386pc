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
- **XT-IDE / XT-CF card variations**, suggested by @andrew-hoffman 2026-09-06:
  [card variations](https://minuszerodegrees.net/xtide/variations/XT-IDE%20and%20XT-CF%20variations.htm),
  [XT-IDE Rev 3 general](https://minuszerodegrees.net/xtide/rev_3/XT-IDE%20Rev%203%20-%20general.htm),
  and the one that matters:
  **[XT-IDE register map](https://minuszerodegrees.net/xtide/XT-IDE%20-%20Register%20map.jpg)**.
  ❌ The two HTML pages carry **no register-level detail at all** — no stride, no bus width, no
  latch. Do not send anyone to them for that; the JPG is the whole answer. It gives two maps:
  *Compatibility* (XT-IDE Rev 1, and Rev 2/3/4 switched into it) and *Hi-Speed* (modified Rev 1,
  and the default on Rev 2/3/4). See [`xtide_register_maps.md`](xtide_register_maps.md) for what
  they mean for our driver — the Hi-Speed map is **not** a stride and we do not implement it.
- **[Bluelavasystems/XT-IDE-CF-MINI](https://github.com/Bluelavasystems/XT-IDE-CF-MINI)** — design
  files for the card @andrew-hoffman has, and therefore the card most likely to produce the first
  independent test result.
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
- **IEEE 1284 and ECP**, for the LS-120's parallel bridge (#22). Found 2026-09-18 while chasing
  why an ECP build fails to enumerate where SPP mounts:
  - **[National Instruments AN062, *IEEE 1284 — Updating the PC Parallel Port*, Heidi Frock,
    October 1995](https://www.ardent-tool.com/comms/an062_Updating_the_parallel_port.pdf)** — the
    only source found that says how ECP mode *ends*. Two lines carry it: `AckReverse*` is driven
    "to follow the level of the `ReverseRequest*` line", and `SelectIn*` — "1284 Active" in ECP
    naming — is driven "high while in ECP mode, and low to terminate ECP mode". So the host must
    wait for the peripheral to follow `ReverseRequest*` back high before dropping 1284 Active.
    That is the handshake `LS_EcpLeave` was missing. Also fixes `AckReverse*` as SPP `PError`,
    status bit 5, and confirms extensibility byte `10h` is ECP-without-RLE, which is what our
    negotiate already sends.
    ❌ **No numbered 1284 events** — "Event 47/49" is Linux `parport` naming and stays out of our
    source. ❌ Its ECP register table and extensibility-byte table are **scrambled by PDF column
    extraction**; do not cite either from this document. It is a text PDF, not a scan — read it
    with `pdftotext -layout`, which is in the MSYS2 tree here.
  - **[Beyond Logic, *Interfacing the Extended Capabilities Parallel Port*](http://wearcam.org/seatsale/programs/www.beyondlogic.org/ecp/ecp.htm)**
    (WearCam mirror) — ECR modes and bits: 011 ECP FIFO, 001 Byte, bit 0 FIFO empty, bit 1 FIFO
    full, bit 2 service. Confirms our `ECR_MODE_ECP` = `74h` and `ECR_MODE_PARK` = `34h`.
    ❌ "Does not explicitly specify how to terminate ECP transfers or return to forward phase" —
    the exact gap AN062 fills. ⚠ It describes bit 2 as "an interrupt request has been initiated",
    weaker than our source comment's "a byte has arrived"; the vendor binary waits on bit 2
    (`SD120PPD.SYS` `48B2h`) and outranks it, so we did not change on this.
  - **[IEEE 1284 ECP Mode, hallikainen.org](https://pic.hallikainen.org/techref/io/parallel/1284/ecpmode.htm)**
    — forward and reverse transfer handshakes signal by signal. ❌ Same gap: "omits return
    procedures". Useful only as corroboration of the signal naming.

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
    - **[*The IHC Damage*](https://www.os2museum.com/wp/the-ihc-damage/)** — Windows 9x stamps
      **"IHC"** (CHICAGO reversed) into the boot sector's **OEM ID field** on any access, for
      Volume Tracker. Named by @andrew-hoffman on #22, 2026-09-13, and it retracted a warning
      we were carrying: 8 changed bytes at offset 3 of the LS-120's boot sector were read as
      ECP write corruption. They are Windows' own signature. The real fault was elsewhere —
      86Box's ECP FIFO dropping 145 bytes of every 512. Read this before calling any 8-byte
      change at offset 3 of a FAT volume corruption. **[PRIMARY]**
      ❌ Does not cover the LS-120, ATAPI or parallel-port bridges — it is about the signature
      only, and its examples are floppies.
- **[AMIBIOS 98 Technical Reference](https://bitsavers.org/pdf/americanMegatrends/MAN-BIOS98-TR_AMBIOS_98_Technical_Reference_19980501.pdf)**
  (bitsavers; also on the trailing-edge mirror) — named by Michal Necasek 2026-09-07 for the
  `FFF53` question, then found independently by the project owner the same day. It has paid off
  **three** times, so it is worth opening for any BIOS-convention question here. **[PRIMARY]**
    - p.22 BIOS entry-point map: `FFF53h IRET Instruction for Dummy Interrupt Handler`, which is
      Michal's point — `IVT68FIX`'s target is an architectural fixture, not a quirk of our two
      1986 ROMs. Same table gives `INT 11h` at `FF84D`, `INT 15h` at `FF859`, `INT 08h` at
      `FFEA5`, and `FFEF3h Initial Interrupt Vector offsets loaded by POST`.
    - p.138 **INT 40h Revector for Floppy Functions**: when a hard disk is present the floppy
      service resides at `INT 40h`, and **all** BIOS floppy functions are revectored there. This
      is the primary source behind #25 — it makes forwarding `AH=08h` to `INT 40h` the
      *documented* behaviour rather than a workaround.
    - p.144 **INT 13h Function 08h (floppy)** drive-type table: `01h` 360K, `02h` 1.2M 5.25",
      `03h` 720K 3.5", `04h` 1.44M 3.5", `05h`/`06h` 2.88M. Confirms our decode of Sergey's ROM.
      ⚠ Its output table names **BH** for the drive type while its own description text says
      **BL**; our measurements and Sergey's ROM both use **BL**. Trust the measurement.

- **[minuszerodegrees ROM archive](https://minuszerodegrees.net/rom/rom.htm)** - IBM and
  third-party ROM images. Confirmed provenance of the Sergey Multi-Floppy BIOS **v2.7** image the
  owner holds (owner, 2026-09-07). Same site as the XT-IDE register map that produced technique 100.
- **Sergey Kiselev, Multi-Floppy BIOS** - **v2.2** is the chip fitted (owner's programmer dump
  `AT28C64B.bin` is byte-identical to `roms/network/Sergey_FDD.bin`). **v2.7** exists and is
  **not** an answer to #25 - identical `INT 13h`/`INT 40h` install rule, verified by
  disassembly.

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

## 8c. The CPU upgrade module (added 2026-09-21)

- **[cpu-world forum, IBM 486BL3 / I-O Data PK-A486BL75 upgrade module](https://www.cpu-world.com/forum/viewtopic.php?t=33652&view=previous&)**
  — supplied by the owner. ⚠ It had been shared in an earlier session and **never recorded**,
  the second such gap found the same day after Trixter. The module: **IBM 486BL3**, 16 KB
  **write-back** cache, Cyrix 87DLC 33 MHz coprocessor.

  **Four DIP switches:**

  | | |
  |---|---|
  | **SW1** | cache flush trigger — ON: **"DMA + I/O read/write"** (IBM PC mode) · OFF: "DMA only" (PC-98 mode) |
  | **SW2** | bus multiplier — ON: **2x** (turbo) · OFF: 1x |
  | **SW3** | coprocessor — ON: disabled · OFF: enabled |
  | **SW4** | unused |

  Also `PK486BL.COM`, a driver offering **1x / 2x / 3x** on the CPU itself, up to 100 MHz.

  ⭐ **The switch table is feipoa's own work in that thread**, as is the compatibility warning
  below — the same person behind CTCHIP/KTCHIP34 (section 8), which closed issue #9. He is the
  authority this project keeps landing on for these upgrade modules.

  ⛔ **SW1 is a CONSTRAINT, not a lever.** feipoa, in the same thread:

  > *"I have found the IODATA unit not very forgiving for IBM-based systems without SW1 set to
  > ON. **Cannot even run DOOM.**"*

  This is an IBM-based system. So SW1 must be **ON**, the cache flushes on **every I/O read and
  write**, and that cannot be turned off for speed. **Do not propose flipping it.**

  ⭐ **What that implies, and it strengthens the plan rather than undermining it.** If every
  port access flushes the L1, then the true cost of an I/O access is the **5.55 us of bus plus a
  cache refill afterwards** — so avoiding a transaction saves more than the bus time alone, and
  *"spend CPU to avoid transactions"* is a better trade than it already looked.

  ⚠ **One measured claim needs narrowing, not retracting.** Technique 109 says a paced-polling
  delay loop *"runs from L1 and costs NO bus cycle"*. The **bus** half stands — the loop lives in
  Inboard-local RAM and never crosses ISA either way, which is the claim that matters for
  occupancy. The **L1** half does not survive a preceding I/O access. Our 0.201 us/iteration
  baseline was measured with no I/O in the loop, so it does not capture this.
  ➡ **Testable with `tools/gen_busload_com.py`**: a pure cached loop against one with an I/O
  access in it. If the mixed loop costs far more than the sum of its parts, flushing is real and
  measurable.

  ⛔ **There is no multiplier lead — corrected by the owner, 2026-09-21, and I had it wrong.**
  An earlier draft of this entry said *"3x is available where we run 2x ... on a 16 MHz Inboard
  that is 48 MHz against a 75 MHz-rated part."* Every number in that sentence was wrong:

  | | |
  |---|---|
  | Inboard crystal | **swapped to 40 MHz** (not the stock 16) |
  | multiplier | **2x** → **80 MHz** |
  | CPU rating | **60 MHz** — so it already runs **33% over** |
  | 3x | **tried, does not work** |
  | 100 MHz | **does not power on** |
  | cooling | a **fan has been fitted** |

  ➡ **The clock is not a lever: it is already past the part's rating and at the limit of what
  powers on.** Do not re-propose a multiplier or crystal change.

  ⭐ **The lesson is the recurring one.** I inferred a stock 16 MHz Inboard from the card's
  nominal spec and a rating from a forum thread about a *different variant*, and wrote both down
  as fact. The owner's machine is not the reference machine in the thread. **Ask what the
  hardware is; do not derive it.**

  ❌ **This thread documents the PC-98 variant.** Whether the owner's module is this exact
  board is **unconfirmed** — the register map we hold (`1000h`/`1001h`/`1002h` via `CTCHIP34
  IBM486`) is consistent with an IBM 486BL3, but consistency is not identification. **Confirm
  the board before trusting a switch table against it.**

## 8b. Bus timing and the demoscene (added to this page 2026-09-21)

⚠ **These were cited only inside `.claude/skills/inboard-hw-debug` (technique 128a) and had
never reached this page** - so nobody reading the repo's own sources list could find them.
Recorded here properly, with what each does **not** contain, which is the more useful half.

- **[Trixter, *Optimizing for the 8088*](https://trixter.oldskool.org/2013/01/10/optimizing-for-the-8088-and-8086-cpu-part-1/)** —
  *"it takes 4 cycles to read a byte, and because the prefetch queue is so tiny, smaller code is
  usually better"*, and string instructions are *"ludicrously powerful"* because they are one byte
  and self-repeating. ⭐ **The method transfers; the content does not.** The Inboard's 386 runs
  cached out of card-local RAM, so code size is free here. What survives is the shape of the
  argument — **rank by bus transactions, not by clock tables** — and that is the principle the
  whole optimisation plan is built on.
  ❌ **Nothing on ISA I/O timing.** Every bus number this project uses was measured here, not
  taken from it: 5.55 us per 8-bit I/O cycle (technique 109), and the byte/word/dword table in
  `docs/iowidth_measured_2026_09_20.md`.
- **[reenigne, *ISA bus sniffer*](https://www.reenigne.org/blog/isa-bus-sniffer/)** and its
  **[update](https://www.reenigne.org/blog/isa-bus-sniffer-update/)** — the useful idea is the
  **instrument**: he built a card to watch every bus cycle because the timing could not be reasoned
  out of datasheets. That is the same conclusion this project reached independently, and our
  equivalent instrument is the emulator plus a PIT-timed probe, which cost nothing.
  ❌ **Neither post gives ISA I/O cycle lengths, T-states or IOCHRDY behaviour.** Do not send the
  next reader there for numbers.
- **[reenigne, *The CGA wait states*](https://www.reenigne.org/blog/the-cga-wait-states/)** —
  *"you can get an extra NOP per word for free because it fits into the wait states"*. The
  transferable form is **dead time inside a transaction is usable time**.
  ❌ Not directly ours: the 386 is hard-stalled during an ISA cycle here. The gap that *is* ours
  is the drive's nWAIT and write latency, when the bus is idle.

⚠ **And a caveat on a source already listed above**: Ardent Tool's *parallel port* pages are
**MCA** and mostly do not apply to an XT bus (technique 128b). The AN062 IEEE 1284 application
note listed in section 3 is a different document and does apply.

## 9. Modern cards on an 8-bit ISA bus (read 2026-09-21)

Suggested by the owner: four projects that put new silicon on an old bus. All of them
**build a card to impersonate a peripheral**, which is the opposite of this project's
problem - we have real cards and the cost is in the host driver. Read for what transfers,
which is less than it looks, but not nothing.

- **[ISA-PicoMEM](https://github.com/FreddyVRetro/ISA-PicoMEM)** (FreddyVRetro) - ⭐ the
  useful one. Moves disk data through a **memory aperture, not I/O ports**: *"PicoMEM Disks
  data transfer done via the emulated Memory"*, 16 KB address granularity, and IOCHRDY
  generation tightened *"from 120ns to 40ns"*. It independently reproduces our #30 result -
  *"single sector read is slower than multiple sector read"*. Relevant to
  [#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35) as a worked
  design for memory-mapped storage, and it **corrects our own record**: technique 128d's
  "nothing can use the cheaper memory path because no card has a data aperture" is true of
  *this machine's* cards, not of ISA cards generally.
  ❌ No figures for an **8-bit XT** bus specifically, and no comparison of aperture vs port
  cost on a 4.77 MHz machine - the thing we would actually need to rank the lever.
- **[PicoGUS](https://github.com/polpo/picogus)** (polpo) - the ISA deadline from the card's
  side: *"the Pico runs at 125MHz, while the ISA bus runs at 8MHz, so to respond within half
  a clock period, the Pico can use at most 6 cycles"*, and the caution that holding IOCHRDY
  low *"effectively halt[s] ISA bus"*. Same currency as our bus-occupancy metric. Raises one
  question worth asking of 86Box: does it model an IOCHRDY stall at all, or is every device
  infinitely fast once addressed?
  ❌ The repository README carries **none** of this - it is in the HN and Hackaday write-ups
  ([HN](https://news.ycombinator.com/item?id=36061326),
  [Hackaday](https://hackaday.com/2023/11/21/picogus-for-all-your-isa-sound-card-needs/)).
  Do not fetch the repo root expecting timing detail; read `sw/` or the wiki.
- **[BlueSCSI v2](https://github.com/BlueSCSI/BlueSCSI-v2)** - on point for
  [#31](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/31). Synchronous
  transfer is a setting of 10 MHz / 5 MHz / **0 = asynchronous**, and their guidance is that
  on old or slow hosts **async is often faster and more reliable than sync** - an Amiga A2091
  benchmarked better in async. Also: fragmented images cost throughput, and *"the SCSI drivers
  can make a huge difference"*. The warning to carry: **the fastest negotiated mode is not the
  fastest real mode.** Settings: [bluescsi.ini](https://bluescsi.com/docs/bluescsi.ini),
  [Performance](https://bluescsi.com/docs/Performance).
  ❌ Nothing on **target-side caching** or disconnect/reconnect policy, which is half of what
  #31 asks. That still has to be measured on our own chain.
- **PicoPCMCIA** (Kevin Moonlight) - ❌ no *bus* learning transferred; PCMCIA is a different
  bus and the problem shape does not match.

⭐ **Two of these are not outside work.** Kevin Moonlight contributed the **CD-ROM emulation**
and the **WiFi code** to **PicoMEM and PicoGUS**, among others, and wrote PicoPCMCIA (owner,
2026-09-21). He also wrote [COMrade](https://github.com/yyzkevin/COMrade) - the tool this
project uses for live introspection on the real 5160, credited in the README since the start.
So the reading above is not a cold survey: it overlaps a contributor to this project.

⚠ **He is not a BlueSCSI contributor** - corrected by the owner the same day, after this page
first said he was. The link originally cited here was his fork, not his work.

⭐ Worth noting for our own CD-ROM work: the PicoMEM CD-ROM emulation is **his**, and we have
just modelled a parallel-port CD-ROM ([86Box#8012](https://github.com/86Box/86Box/pull/8012)).
Different bus, same problem shape.

Two questions the PicoMEM/PicoGUS repositories did **not** answer, both the kind a person
answers in a sentence and a document does not answer at all:

1. Is there an **8-bit XT** aperture-vs-port figure anywhere, or is PicoMEM's memory path
   only ever characterised on faster buses? Ours is the 4.77 MHz case, and it is the one
   that decides whether
   [#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35) is worth anything.
2. How much does holding **IOCHRDY** actually cost a period machine in practice? The PicoGUS
   caution is qualitative, and we have a measured bus-occupancy model to put a number against
   it.

⚠ The **BlueSCSI** question - does it do target-side caching or disconnect/reconnect, and what
did enabling them do on a slow host, which is half of
[#31](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/31) - is **not** his, and
has no contact attached to it. It still has to be measured on our own chain.

⛔ **Nothing has been sent.** Contact is the owner's to make, in his own words.

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
  initialisation. That is what our phase-1 driver got wrong; see `docs/archive/xtide_pdr_retired/source/README.md` (that line is **retired** - superseded by
  the miniport, `drivers/xtide_mpd/`).
- **`PROBE-NOTES.md` / `README.md`** - documents the IOS AEP sequence and function/result numbers,
  and the finding that `DISKTSD` never configures a dynamically registered port driver's DCB.

What it does **not** contain: anything about 8-bit ATA task-file addressing or the XT-CF register
stride. It is a PC Card/USB device; the value is the IOS-side contract, not the transport.

Not vendored - it is a live MIT upstream. Clone it beside this repo to read it.

## Undocumented DOS commands — MULTITRACK and DRIVPARM (added 2026-09-11)

<https://viric.name/oldcomps/files/DOS-undoc.txt> — supplied by the owner. An **undocumented
commands** reference for DOS 2.x-7.x, not an internals reference.

**What it does NOT contain**, recorded so nobody re-reads it hoping: nothing on disk BUFFERS
or HMA placement, `DOS=HIGH`, the Current Directory Structure / `LASTDRIVE`, the List of
Lists, or `FILES`/SFT. It is a command list.

**Two entries land on open work here.**

### `DRIVPARM` — NOT needed. The floppy geometry problem is already solved

> *"Documented in DOS 4.0 through 6.x; undocumented in DOS 3.2, 3.3, PC DOS 7, and PC DOS
> 2000."*

A `CONFIG.SYS` directive that overrides the drive parameters DOS takes from the BIOS.

⚠ **Recorded here only so nobody reaches for it.** When this reference was first read on
2026-09-11 it was written up as a candidate fix for the floppy geometry problem. **That
problem was closed on 2026-09-07** and the note was made by reading issue #25's comment
thread - which ends before the fix - instead of the issue's own CLOSED state or
`docs/next_session_2026_09_08.md`.

**The actual fix was hardware:** Sergey's Multi-Floppy BIOS claims `INT 13h` only when the
vector is still the stock `F000:EC59`. Trantor at `CA000` was scanned first and took it, so
Sergey settled for `INT 40h` and the 1986 system BIOS answered `AH=08h` from its own 720K
table. Moving Trantor's ROM to `DA000` (`SW3 OFF, SW4 ON, SW5 ON`) puts Sergey first.
Verified on DOS 6.22 and Windows 95, both drives reading real media.
Evidence: `docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md`.

### `MULTITRACK` — already ON, but a candidate variable for #18

> *"Default: MULTITRACK=ON. Starting with DOS 4.0 [...] reading and writing of more than one
> track with a single BIOS call has been implemented. But some problems have been observed
> with hard disk drives of some manufacturers. So the MULTITRACK=OFF option limits disk
> access to a single track."*

Not present in this machine's `CONFIG.SYS`, so it is at the default **ON**. Nothing to change
- and worth knowing it is the DOS-side form of the command-merging lever
(`docs/bus_optimisation_plan.md` B1): fewer, larger BIOS calls.

⚠ **But it changes how reads are issued across track boundaries, which makes it a variable
worth holding constant - or deliberately toggling - in the #18 floppy-corruption bed.** That
issue's recorded trigger is a media change flushing a stale cache page to the wrong disk, and
multi-track requests are exactly the shape of access that spans what a cache page covers.

Also in the file, not relevant here: `AVAILDEV` (removed after DOS 3.0), `SWITCHAR`
(gone after DOS 3.0, still reachable via INT 21h AH=37h).
