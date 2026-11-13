# Fable pack F1 addendum: BL3 registers, pins, module wiring

Assembled 2026-11-13. Companion to `docs/fable_pack_f1_flush.md`. No diagnosis.

## 1. Which document covers the BL3

The part is the IBM Blue Lightning **486DLC3 / "486SX3"** (3x clock, 16 KB write-through L1,
MSR 1000h-1004h, ports 22h/23h). `IBM486.CFG` line 4: `NAME=486DLC3 ; (oder 486SLC2) IBMs Blue
Lightning`.

- Only IBM fax #40010 (May 1995) names it ("Blue Lightning 486SX2 and 486SX3") and compares it
  with the 486DX2. It is a comparison paper, not a register or pin reference.
- The register/pin source is the *Blue Lightning Microprocessor Data Sheet* (IBM, 11/1993;
  7 Feb 1994 per #40017 refs) and the *Blue Lightning 486 DX2 Databook* (1994). **Neither was
  found online.** The other chipdb papers cover the DX2/DX4 (CCR1-3, FLUSH# pin), a different
  core.

## 2. Datasheets fetched (all have a text layer; no OCR needed)

Local copies, not for the repo: `%TEMP%\bl3_ds\*.PDF`; extracted text `%TEMP%\bl3_txt\*.txt`.
Base URL `https://datasheets.chipdb.org/IBM/x86/486/<n>.PDF`.

| Fax # | Pages | Title | BL3 content |
|---|---|---|---|
| 40010 | 6 | Internal Cache Architecture of X86 Processors (Goodnow, 1995-05-23) | **Yes**, SX2/SX3 flush types, KEN, MSR cacheability, snoop. Section 4 |
| 40017 | 5 | Bus Snooping and the IBM 486 DX2/DX4 (Pheasant, 1995-10-12) | No. DX2/DX4 CCR2; no I/O-flush text |
| 40018 | 22 | IBM 486DX2-V addendum (1995-06-06) | No. DX2-V pins/timing |
| 40022 | 24 | IBM 486DX4 addendum (1995-09-12) | No. DX4 pins/timing |
| 40030 | 13 | IBM 486DX4 BIOS Writer's Guide (1995-10-03) | No. CCR1-3 via 22h/23h; same port mechanism |
| 40003, 40004, 40006, 40011, 40024 | 3-10 | DX2 overview, socket, chipsets, voltage, pinout | No. No MSR/ASNP hit |

Not found by search: BL SLC/DLC data sheet, *Blue Lightning 486 DX2 Databook*. Search summary
(unverified) says Vogons post `https://vogonswiki.com/index.php/IBM_CPUs` and
`https://www.vogons.org/viewtopic.php?p=817913` discuss the chipdb list and say a "486SLC
Microprocessor Data Sheet" 1993 is not online. Not opened. Also `https://ardent-tool.com/CPU/8570_DLC2_Upgrade.html`
(IBM 386/486 upgrade options PDF), not opened.

## 3. Register bits

Bit names from `dist/post-install-fixes/ctchip34/IBM486.CFG` (CTCHIP34 config, lines in col 4).
Meanings beyond the name: van Gilluwe, *The Undocumented PC*, pp. 111-115, as tabulated in
`docs/resources_and_sources.md:559-580` (book bit = 32-bit MSR bit). Datasheet column: what 40010
adds. No datasheet gives the bit positions.

### MSR 1000h Processor Operation (CFG bytes 0-2; value we run: `1000h:0`=92, `:1`=8C, `:2`=00)

| Byte:bit | Book bit | Name | Meaning | Ours | Source |
|---|---|---|---|---|---|
| 0:7 | 7 | CE | Internal cache enable | 1 | CFG:27; book; 40010 p4 "CE bit in MSR1000 enable and disable the cache" |
| 0:6 | - | DBCS | Cache double-byte character set | 0 | CFG:28, no meaning found |
| 0:5 | - | PMI | Power management interrupt | 0 | CFG:29, no meaning found |
| 0:4 | 4 | ASNP | ADS# snooping. Book: "flush the L1 in HOLD when a CPU line is driven. Can be used when bit 3 is zero" | 1 | CFG:30; resources_and_sources.md:565 |
| 0:3 | 3 | SNP | Snooping. Book: "invalidate on bus writes while in HOLD". Hangs the 5160 | 0 | CFG:31; resources_and_sources.md:564 |
| 0:2 | - | A20M | A20 mask | 0 | CFG:32 |
| 0:1 | - | CPC | Cache parity checking | 1 | CFG:33 |
| 0:0 | - | CPE | Cache parity error (status) | 0 | CFG:34 |
| 1:7 | 15 | CNPX | NPX operands cacheable (set for Cyrix FPU) | 1 | CFG:17; book |
| 1:5 | - | LPH | Low-power halt | 0 | CFG:19 |
| 1:4 | 12 | XTOUT | Extended OUT: CPU waits for READY after every OUT | 0 | CFG:20; book. Cleared a0f7d44 |
| 1:3 | - | CRLD | Cache reload | 1 | CFG:21, no meaning found |
| 1:2 | 10 | IKEN | Internal KEN: cache enable from 1001h limits, not the KEN pin; settable once per reset | 1 | CFG:22; book |
| 1:1 | - | DCLM | Disable cache lock mode | 0 | CFG:23, no meaning found |
| 2:2 | 18 | LPPLA | Low-power PLA | 0 | CFG:11; book |
| 2:1 | 17 | BUSRD | "Bus Read (ignore Cache)"; book: force external reads (factory test) | 0 | CFG:12; book |
| 2:0 | 16 | CPGE | Cache parity enable error (parity test) | 0 | CFG:13; book |

### MSR 1001h Cache Region (bytes 0-6)

| Byte | Name | Meaning | Ours | Source |
|---|---|---|---|---|
| 0, 1 | LMCR | Low 1 MB cacheable, one bit per 64 KB, lo/hi byte | FF / 03 | CFG:57-66; 40010 p3 "64K regions below 1MB" |
| 2, 3 | LMROR | 1 MB read-only mask: ROM blocks, writes skip cache | 00 / 00 | CFG:46-54; book |
| 4 | CMLR | Upper cacheable limit, 1-16 MB, 64 KB units | F0 | CFG:44; 40010 p3 "upper cacheability limit above 1MB" |
| 5, 6 | ECMLR | Extended limit to 4 GB, lo/hi | 00 / 00 | CFG:35-41 |

### MSR 1002h Clock Control

| Byte:bit | Name | Meaning | Ours | Source |
|---|---|---|---|---|
| 3:5 | EDFS | External dynamic frequency shift | - | CFG:76 |
| 3:4 | DFSRDY | Dynamic frequency shift ready | - | CFG:78 |
| 3:3 | DFSREQ | Dynamic frequency shift request | - | CFG:80 |
| 3:2-0 | - | 000 = 1:1, 011 = 2:1, 100 = 3:1 | 011 | CFG:82-85 |

### MSR 1004h Processor Control (CFG marks "DLC3 only"; book marks "unconfirmed")

| Byte:bit | Name | Meaning (CFG) | Ours |
|---|---|---|---|
| 3:7 | OS2B | 0 = DD1 hardware, 1 = DD0 for OS/2 boot | 0 |
| 3:6 | CR0 | MOV CR0 decode: 0 = DD0/DD1A/B/D, 1 = DD1C | 0 |
| 3:4 | CLP | Cache low power: 1 = disable cache when not in use | 0 |
| 3:2 | NOP | NOP 2T/3T select | 0 |
| 3:1 | NA16 | Bus pipelining for 16-bit | 0 |
| 0:4 | MOVS Split | no description | 0 |
| 0:3 | - | Power-saving cache feature | 0 |
| 0:1 | - | Enable MOV CRx decode (DD1B/DD1C reversed) | 0 |

MSR 1003h, 1005h-1007h: no definition found in any source here.

## 4. What 40010 says about flushing (the only BL3 text found)

Verbatim, 40010 page 3-4:
- Table 1 (p2), flush types: BL SX2/SX3 = "FULL SNOOP, ADS FLUSH, HOLD FLUSH, SOFTWARE". The
  DX2s list "FLUSH PIN". "FLUSH ON CLOCK STOP: YES", "FULL SNOOP ON CLOCK STOP: YES".
- p4: "On the IBM Blue Lightning 486SX2 and 486SX3 a full cache flush is accomplished as either an
  ADS# or HOLD flush. The type of flush is set as bits in the MSR1000 register. If the full snoop
  bits are set the processor will accomplish single line invalidates on a hold. With the bits set
  for a HOLD flush, the assertion of HOLD and HLDA will flush the cache. With ADS# flush set, the
  processor will flush on HOLD, HLDA, and ADS# being active. This allows a differentiation
  between bus master and memory refresh cycles."
- p3: the BL3's "KEN" pin "is shared with a dynamic frequency shifting function".
- p3: "If the PCD bit is changed dynamically, the cache needs to be flushed" (IBM manual quote).
- p6: no cache test registers TR3-TR5 on SX2/SX3; snoop and flush work in both power-down modes.
- p5 (DX2 context): if refresh cycles count as bus-master access, a HOLD-triggered flush recurs
  "every 15 usec".
- p4: SX2/SX3 have two write buffers; write-allocate (write then line fill) on a miss.

Nothing in any fetched document says the cache is invalidated on I/O cycles, on IN/OUT, or by
port 22h/23h. 40030 p1 (DX4) says 22h index writes and 23h data are on-chip; reads of 22h
always go off-chip.

## 5. Pins

| Pin | What the documents say | Source |
|---|---|---|
| FLUSH# | Full-flush input on DX2/DX4 only. BL SX2/SX3 flush by MSR bits (ADS#/HOLD), no FLUSH# listed in Table 1. Vogons anecdote (unread): BL probably does not use FLUSH# | 40010 p2-4; flush.md:106 |
| KEN# | Cacheability input on all listed parts; BL SX2/SX3 share it with dynamic frequency shift. IKEN (1000h:1 bit 2) switches to MSR limits | 40010 p3; CFG:22 |
| AHOLD | DX2/DX4 snoop start: AHOLD/HOLD/BOFF# then EADS#. Not described for SX2/SX3 | 40017 p1; 40018 pins |
| EADS# | Drives snoop address on DX2/DX4. Not described for SX2/SX3 | 40017 p1 |
| HOLD/HLDA | Both start a flush when MSR "HOLD flush" bits set; "ADS# flush" adds flush on ADS# active during hold | 40010 p4 |
| BARB | CCR2 bit 5 on DX4: write back dirty lines on HOLD before HLDA. Write-back only; not a BL3 MSR bit | 40030 p5 |

## 6. Module / interposer facts already recorded

- Hardware: I-O Data PK-A486BL (60 or 75), IBM 486BL3, 16 KB write-back cache per forum (40010
  says write-through), Cyrix 87DLC 33 MHz FPU on the module. PC-98 part.
  `hardware/5160_deep_dive_config.txt:71`; `docs/INBOARD_86BOX_PORT_PLAN.md:101`;
  `docs/resources_and_sources.md:417-452`; memory `real_hardware_peripherals.md:29`,
  `fpu_fitted_on_interposer.md`.
- Inboard crystal changed 32 -> 40 MHz; CHKCPU 83.5 MHz; ISA bus stays 4.77 MHz
  (`real_hardware_peripherals.md:29`).
- DIP switches (ph4nt0m table, cpu-world): SW1 ON = flush on "DMA + I/O read/write", OFF = "DMA
  only"; SW2 ON = 2x turbo; SW3 ON = FPU disabled; SW4 unused. Owner's reads OFF/OFF/ON/ON, acts
  ON/ON/OFF/OFF (`resources_and_sources.md:441-452`; memory
  `cpu_module_switches_transposed_2026_09_27.md:11`).
- Box back panel (Japanese): reset-press count selects mode and flush source, DMA only or DMA +
  I/O port (`resources_and_sources.md:~520`, photo `XT_project\photos\IO_DATA_PK-A486BL_box_2.jpg`,
  owner's OneDrive). Switch photo: `XT_project\photos\blue_lightning_switches.jpeg`.
- `PK486BL.COM` sets 1x/2x/3x (resources_and_sources.md:448).
- feipoa, 2025-09-09, vogons t=66242 p3: SW1 OFF "optimal"; p1: I/O flush "solved a lot of
  the issues, but at speed penalty". IODATA "cannot even run DOOM" with SW1 OFF on IBM systems.
- No pin wiring, schematic, or photo of the interposer traces is in the repo. Repo kicad files
  under `references/inboard386_ronnyroy/` are the Inboard, not the module (HLDA net at
  `inboard386.kicad_sch:17949`, unchecked).

## 7. Other sources in the project

| Path:line | Gives |
|---|---|
| `dist/post-install-fixes/ctchip34/IBM486.CFG` 1-60+ | all bit names above |
| `dist/post-install-fixes/ctchip34/CPUSET.BAT:30-50` | applied sequence, "never CE without ASNP" |
| `docs/cpu_cache_cmlr_2026_09_12.md:7-17,61-88` | register read-back, 22h/23h, REVTO486 dump match |
| `docs/bus_optimisation_plan.md:389-411,1523-1545` | register table, levers C1-C7, C4 SNP/ASNP/BARB |
| `.claude/skills/inboard-hw-debug/SKILL.md:2640-2700` | feipoa BARB vs FLUSH, CTCHIP34 use |
| `docs/captures/io_cache_flush_2026_09_21.md`, `docs/captures/2026-11-12_flushprobe/README.md` | measured flush |
| `docs/contributor_input_ledger.md:119` | feipoa credit |
| memory `cpu_cache_cmlr_fixed_2026_09_12.md:21-47` | REVTO486 MSR dump decode |
| Not in repo | van Gilluwe PDF: owner's `Downloads\Undocumented_register_settings_for_IBM_Blue_Lightning.pdf` |

URLs already recorded: feipoa CTCHIP thread `https://www.vogons.org/viewtopic.php?t=45756`;
revto486 thread `https://www.vogons.org/viewtopic.php?t=66242&start=40`; cpu-world
`https://www.cpu-world.com/forum/viewtopic.php?t=33652&view=previous&`; OS/2 Museum
`http://www.os2museum.com/wp/ibm-blue-lightning-worlds-fastest-386/` (nothing on registers).
Vogons p=1387286 (FLUSH# continuity) unread.

## 8. Does any bit or pin explain flush on I/O?

No source states it. Candidates the sources allow, not tested:
- ASNP (1000h:0 bit 4, ours 1): 40010 p4 describes an ADS#/HOLD-based whole-cache flush.
  A flush "on ADS# while HOLD/HLDA" is the only whole-cache mechanism named for the BL3.
- SW1 module logic: forum says it flushes on "I/O read/write"; the mechanism (pin or MSR
  trigger) is not documented.
- Note: 5160 DMA refresh uses HOLD/HLDA; 40010 p5 warns of a 15 us flush rate in that case.
  The measured flush is per I/O, not per refresh; relationship unknown.

## 9. Gaps

- BL data sheet (11/1993 or 2/1994) and Blue Lightning 486 DX2 Databook: not found.
- Bit meanings for CRLD, DCLM, DBCS, PMI, MOVS Split, LMROR effect: names only.
- Whether "full snoop" = SNP and "ADS flush" = ASNP in 40010 terms is inferred from names.
- Interposer pin wiring (FLUSH#, KEN#, AHOLD, HOLD to the Inboard 386 socket): no source.
- Van Gilluwe pages not re-read here; table is from the repo's earlier transcription.
- PDFs kept in `%TEMP%\bl3_ds\` (40003 40004 40006 40010 40011 40017 40018 40022 40024 40030);
  40010 and 40017 are the ones worth keeping.
