# Fable pack F1: CPU flush on I/O

Assembled 2026-11-13 from repo files and one web search. No diagnosis, no proposals.


## 0. Added 2026-11-13 night: three sub-packs, and what they settle

- `docs/fable_pack_f1_gal.md` (ronnyroy111/inboard386 PCB + 16 CUPL files): the Inboard drives no
  CPU cache pin. FLUSH#, KEN#, A20M#, HLDA unconnected at the socket; HOLD to GND via RP11; no
  AHOLD/BOFF#/EADS# net; no GAL equation names them.
- `docs/fable_pack_f1_regs.md` (IBM fax #40010 p3-5, van Gilluwe via `resources_and_sources.md`):
  the BL SX2/SX3 flushes the whole cache from MSR 1000h modes: on HOLD/HLDA, or HOLD plus ADS#
  ("to differentiate bus master from refresh"). No source says it flushes on I/O. No BL register
  datasheet is online. The module is an I-O Data PK-A486BL; no pin-level wiring is recorded.
- `docs/fable_pack_f1_inbrdpc.md` (ndisasm of INBRDPC.SYS, UniPCemu inboard.c): INBRDPC.SYS never
  touches CR0.CD/NW, INVD/WBINVD, MSRs or ports 22h/23h; port 670h carries wait states and the ROM
  shadow bit only; nothing runs per I/O or per DMA.

So the flush on I/O comes from the module, not the Inboard or Intel's driver. SW1 (forum tables:
ON = flush on DMA + I/O, OFF = DMA only) is the one known control, and the flush has never been
measured with SW1 OFF. Cheapest test, before any review: one FLUSHP run in DOS with SW1 OFF.

Correctness gate: the Inboard ties HOLD low, so the module's "DMA only" flush (SW1 OFF) may never
see XT-side DMA. The flush on I/O may be what keeps DMA writes into Inboard RAM (floppy read, SB
record, T130B bus master) coherent. SW1 OFF is measured in DOS only, with no floppy or T130B use,
and flipped back before Windows. Keeping it OFF would need uncacheable DMA buffers (LMCR/CMLR) or
an invalidate at DMA completion, then copy-and-compare tests on floppy, SB record and T130B.

## 1. Question

F1 (`docs/fable_driver_review_brief.md:22`): **CPU flush on I/O**: what invalidates the L1 on
every IN/OUT (Inboard glue, BL3 config, INBRDPC.SYS), whether it can be narrowed without breaking
DMA coherency, and driver-side avoidance beyond the known (bursts built in RAM, VGA-half memory
writes).

Added: is the flush required for correctness, can it be narrowed without breaking DMA coherency,
and how can the drivers avoid it.

## 2. Measured facts

Machine: IBM 5160, Intel Inboard 386/PC, IBM 486BL3 module at 2x (CHKCPU 83.5 MHz), Inboard
serves 576 KB of RAM, 5 MB fitted. Floppy uses DMA. DMA ch 3 = parallel card. XT-CF disk is PIO.
ISA DMA page register is 20-bit: DMA reaches only the first 1 MB. (Config: `docs/5160_tuned_config.md`;
DMA reach: `docs/cpu_cache_cmlr_2026_09_12.md:61-65`; channel 3 and PIO: project memory
`xtcf_no_dma_channel3_parallel_2026_09_29`, not in the repo.)

Flush behaviour, `docs/captures/2026-11-12_flushprobe/README.md` (FLUSHP, real card):
- IN 21h, working set 256 B / 1 KB / 4 KB / 8 KB: 25 / 84 / 321 / 638 us per op. 78 ns per byte of
  working set, linear to 8 KB (lines 23-29).
- OUT 21h, 1 KB: 81 us. IN 22h (BL3 internal port), 1 KB: 85 us (lines 29, 33).
- 8 x IN 21h, 1 KB: 124 us; each extra IN adds 5.6 us. Once per burst (line 34).
- ISA memory read B800:1000, 1 KB: 4.1 us, no invalidation. ISA memory write: about 0, posted,
  no invalidation. ROM read F000:FFF0: 0.6 us (lines 30-32).
- 1000 OUTs then 4 KB re-read: 470 us, same as after one OUT. Refill does not grow with burst
  length. 1000 `rep outsb` = 5.51 ms = 5.5 us/B (lines 78-80).
- 8 KB to/from B800:1000: write 2.15 / 1.94 / 1.83 us/B (byte/word/dword); read 3.72 / 3.57 /
  3.48 us/B (lines 67-72).

Earlier measure, `docs/captures/io_cache_flush_2026_09_21.md:34-50`: one `in al,21h` per 1 KB,
64 passes: 39.5 -> 128.1 ns/B (3.24x); 90.7 us per access, 5.7 us of it bus; 83 ns/B refill.
128.1 ns/B vs 135 ns/B measured for Inboard RAM (E1, 2026-09-11).

I/O cost:
- Byte 5.695, word 3.819, dword 3.183 us per byte (`docs/iowidth_measured_2026_09_20.md:30`,
  word 7.638 us per access).
- Port word write ~7.6 us, OUT word (Mach8 register) 7.64 us (`docs/bed_io_cost_model_2026_11_12.md:37`).
  A 16-byte line touched after a flush ~1.3 us (`docs/atim8_vs_ours_lagging_tests.md:8`).

BL3 registers as run (`dist/post-install-fixes/ctchip34/CPUSET.BAT:30-50`):
- `1000h:0` = 92 (CE, ASNP, CPC on; SNP off). Never set SNP: hangs the 5160.
- `1000h:1` = 8C (was 9C; XTOUT cleared 2026-09-28, `a0f7d44`; FastDoom +1.1%).
- `1001h:0/1` LMCR = FF/03 (low 640 KB cacheable). `1001h:4` CMLR = F0 (1-16 MB cacheable).
  `1001h:2/3` LMROR = 0000. `1001h:5/6` ECMLR = 0. `1002h:3` = 03 (2:1). `1004h:3` = 00.
  (`docs/cpu_cache_cmlr_2026_09_12.md:7-17`, `docs/bus_optimisation_plan.md:389-400`)
- CTCHIP's post-write echo is not a read-back; CPUSHOW.BAT is (`cpu_cache_cmlr...md:81-88`).
- 22h/23h writes land and do not hit the 8259: the BL3 claims them internally
  (`cpu_cache_cmlr...md:70-79`; `bus_optimisation_plan.md:408-411`).
- CMLR change: Dhrystone int 2 -> 13-15; cached working sets ~10x faster, sets over 16 KB ~2.3x
  slower (`cpu_cache_cmlr...md:118-170`).
- CMLR note: do not raise past installed RAM; Inboard BIOS shadow aliases at 0x5E0000/0x5F0000
  (`cpu_cache_cmlr...md:67-68`).

Module switches (forum tables, not measured):
- SW1 ON = flush on "DMA + I/O read/write"; OFF = "DMA only". ph4nt0m table, cpu-world
  <https://www.cpu-world.com/forum/viewtopic.php?t=33652&view=previous&>. feipoa there: IODATA
  unit "not very forgiving for IBM-based systems without SW1 set to ON. Cannot even run DOOM."
- feipoa 2025-09-09 calls SW1 OFF "optimal"; page 3 of <https://www.vogons.org/viewtopic.php?t=66242&start=40>
  (`docs/resources_and_sources.md:535-542`). Page 1: flushing on I/O "solved a lot of the issues,
  but at speed penalty".
- Owner's module reads OFF/OFF/ON/ON, acts ON/ON/OFF/OFF (switches look transposed), so SW1 is
  ON (`resources_and_sources.md:441-452`; project memory `cpu_module_switches_transposed_2026_09_27`).
  The measured flush agrees with SW1 ON. The owner flipped SW1 on 2026-11-08 with the module fitted:
  no measurable difference (project memory `cpu_module_switches_transposed_2026_09_27`). That run
  predates FLUSHP (11-12), so the flush itself was not measured with SW1 OFF. Not a lever to re-propose.
- XTOUT (`1000h:1` bit 4, book bit 12): CPU waits for READY after every OUT. Cleared; feipoa
  recommends 0 (`bus_optimisation_plan.md:1537`; `resources_and_sources.md:578-581`).

## 3. Mechanism sources to read

Repo (about 800 lines):

| Source | Lines | Gives |
|---|---|---|
| `docs/captures/2026-11-12_flushprobe/README.md` | 1-84 | the probe, raw data, what it settles |
| `docs/captures/io_cache_flush_2026_09_21.md` | 1-89 | first measurement, SW1 reasoning, caveats |
| `docs/cpu_cache_cmlr_2026_09_12.md` | 1-97 | register values, DMA-reach argument, 22h/23h |
| `docs/bus_optimisation_plan.md` | 385-410, 1523-1570 | verified register table, levers C1-C7; C4 = SNP/ASNP/BARB-vs-FLUSH, marked correctness |
| `dist/post-install-fixes/ctchip34/IBM486.CFG` | 1-60 | bit names: ASNP, SNP, BUSRD ("Bus Read (ignore Cache)"), XTOUT, IKEN, CRLD, A20M |
| `dist/post-install-fixes/ctchip34/CPUSET.BAT` | 1-56 | the applied sequence and its "never CE without ASNP, the T130B does bus-master DMA" comment |
| `docs/resources_and_sources.md` | 417-452, 535-585 | SW1 tables; van Gilluwe *The Undocumented PC* pp. 111-115 bit map (SNP vs flush snooping, XTOUT, CMLR) |
| `.claude/skills/inboard-hw-debug/SKILL.md` | 2640-2680 | feipoa BARB vs FLUSH: an XT planar never drives FLUSH#; Cyrix default has no DMA invalidation |
| `docs/bed_io_cost_model_2026_11_12.md` | 32-70, 105-116 | how the flush is (to be) modelled in 86Box; open question: whole-L1 or only cached lines |
| `86box_full/src/device/inboard386.c` | 370-420, 855-875 | Inboard cache/ROM-shadow handling (port 0x670 bit 0, NOCACHE, EGACACHE); not an L1 flush model |
| `86box_full/src/cpu/cpu.c` | 3540-3580, 4278-4290 | model's BL3 MSR 1000h-1007h handling; `cpu_read()` returns 0xFF for these, writes dropped (`cpu_cache_cmlr...md:81-88`) |

Searched and not present in the repo: no BL3 or 486SLC datasheet under `references/`; no
PAL/GAL equations (`hardware/pal_gal_reverse_engineering/` is empty); `hardware/inboard386_reverse_engineering/`
holds only `DOLLAR386.ASM` and `MINBRDPC.ASM` (no hits for FLUSH#, KEN#, AHOLD, HLDA). The only
HLDA hit is a KiCad net in `references/inboard386_ronnyroy/inboard386/inboard386.kicad_sch:17949`
(and `.kicad_pcb:61159`, "unconnected-(U73C-HLDA-PadM14)"); not checked against FLUSH#/AHOLD routing.
Nothing in the repo disassembles INBRDPC.SYS for CPU/cache config; `docs/INBOARD_86BOX_PORT_PLAN.md`
and `docs/egacache_static_read_2026_09_28.md` mention its cache options (EGACACHE, NOCACHE) only.

Web (one search, 2026-11-13; contents not read):
- IBM x86 486 datasheets, <https://datasheets.chipdb.org/IBM/x86/486/40010.PDF> and
  <https://datasheets.chipdb.org/IBM/x86/486/40017.PDF>. Image-only PDFs; fetch returned no text,
  titles and pages unknown. The search summary says one result concerns the Blue Lightning SX2/SX3
  and a microprocessor-specific register; unconfirmed. Also listed, untried: 40022.PDF, 40018.PDF,
  40030.PDF in the same directory.
- Vogons post, <https://www.vogons.org/viewtopic.php?p=1387286>: reported by the search as saying
  the Blue Lightning probably does not use FLUSH# (continuity check, anecdotal). Not read.
- Cyrix Cx486SLCe data sheet, <https://www.dosdays.co.uk/media/cyrix/Cyrix_Cx486SLCe_Data_Sheet_1992.pdf>.
  Different family (CCR0/FLUSH#/BARB), not the BL3; page numbers not found.
- OS/2 Museum, <http://www.os2museum.com/wp/ibm-blue-lightning-worlds-fastest-386/>: nothing on
  ISA I/O timing (`resources_and_sources.md:593`).

No BL3 datasheet with a page range was found.

## 4. Driver-side context

Known, so the reviewer adds rather than repeats:
- Build commands in RAM, send in tight bursts, keep code and data touched between bursts small;
  each burst costs one refill (flushprobe README:49-50).
- Memory writes to the card (VGA half, A000/B800) post and do not flush; Mach8 engine is I/O-only,
  so sends keep paying one refill a burst (README:51-53). Dword memory write 1.83 us/B vs word OUT
  3.819 and dword OUT 3.183 (README:74-76).
- Fewer commands, fewer polls, paced polling each remove flushes: `XT_POLL_BACKOFF`, request
  merging A1 (`io_cache_flush_2026_09_21.md:56-66`).
- Joined same-colour patches cut writes (~18.6k -> ~2k); fewer lines and fills is the engine lever
  (`docs/silicon_for_d3d_2026_11_09.md:7`, items 1, 9 FIFO_OPT/MAX_WAITSTATES, ~28% of a PIX_TRANS
  cycle is IOCHRDY).
- CPU stages data in VGA memory while the engine draws; VGA half has no memory path to the engine
  (`docs/mach8_levers.md:224-228`; `silicon_for_d3d...md:75`).
- Register-only delay loops: bus cost nil, but the driver's working set is lost at the preceding
  poll (`io_cache_flush_2026_09_21.md:68-73`).
- Mach8 AB zero-wait bits: no gain, XT bus cycle is the limit (`mach8_levers.md:136`).

## 5. What is NOT known

- No BL3 datasheet in the repo; web copies found are unread image PDFs.
- No scope or logic-analyser capture of FLUSH#, HOLD/HLDA, AHOLD or KEN# on the module or Inboard.
- Whether the flush comes from SW1 hardware, an IBM-glue pin, or a BL3 internal mode is inferred,
  not shown. SW1 OFF (11-08) changed nothing measurable, which argues against SW1 as the only source.
- Whether IN 22h reaching the BL3 without an XT bus cycle is true is unconfirmed (flushprobe README:44).
- Whether the flush is whole-L1 at the module or only valid lines is open (`bed_io_cost_model...md:109`).
- Whether DMA (floppy, ch 3) alone triggers a flush, and whether SNP/ASNP/BUSRD affect I/O flush:
  untested. SNP hangs the 5160; ASNP on.
- Meaning of `1000h:2`, `1004h:0` (MOVS Split), LMROR effect: bit names only (`IBM486.CFG`,
  `bus_optimisation_plan.md:1537-1541`); book map marks `1004h` "unconfirmed".
- No INBRDPC.SYS disassembly for CPU/cache configuration; no PAL/GAL equations.
- Real-hardware effect of CMLR above 16 KB working sets: the 2.3x slowdown cause (line fill) is
  untested.
- Mixed writes: whether a posted memory write followed by I/O flushes the cache earlier is unmeasured.

## 6. Constraints

- Never propose disabling INBRDPC.SYS; it is required.
- Never propose raising the CPU clock; 3x fails, the clock is not a lever.
- No dword OUTs to the Mach8.
- Real hardware outranks the 86Box model; the model drops BL3 MSR writes.
- Never set SNP (`1000h:0` bit 3): hangs the 5160.
- Do not raise CMLR beyond installed RAM (BIOS shadow aliases near 5.9 MB).
- DMA correctness must hold for floppy DMA; the T130B bus-masters (CPUSET.BAT comment).
