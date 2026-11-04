# Mach8 lessons applied to the other drivers - review, 2026-11-04 (#57)

Desk review plus two measurement tools; nothing here is measured on the card yet. Builds on
`docs/cross_driver_levers_2026_11_01.md` (read that first). Owner's rule for this review: on this
machine even 1% is worth taking, so items that document ranked "not worth the risk" are back in.

## The lessons, and where each already stands

| Mach8 lesson | Source | Already applied outside the Mach8 driver |
|---|---|---|
| A port access costs ~90.7 us, only 5.7 of it bus: it empties the L1 and the next code refills it | `captures/io_cache_flush_2026_09_21.md` | `MinTimeSlice=54` (+6% idle, +60% during a copy); CD-RW auto insert off (+1.2%); XT-CF paced polls |
| Wait on the measured time, then look | `xtcf_measured_wait.md` | XT-CF `XT_WAIT_FIRST`, `XT_YIELD_COMMIT` |
| Divide by the bytes: at the floor, the only lever is sending fewer bytes | `mach8_driver_method.md` §4c | `SWAPCOMP.VXD` (-4.5% on the whole workload, 2.64:1) |
| Touch less between port accesses: values in registers, compact code | `mach8_driver_method.md` §4b | not yet in the XT-CF driver (item 4) |
| Shadow a register, skip an unchanged write, only where nothing else writes it | `mach8_guide_rules.md` | not yet (item 4b) |
| String I/O over loops, measured | `iowidth_measured_2026_09_20.md` | XT-CF `rep insw/outsw` |
| Measure on the card; the bed misprices I/O and memory | `mach8_driver_method.md` §3 | every item below |

## Is the CPU's 16 KB L1 used well? (owner's question)

- **Switched on, covering all the RAM:** `CPUSET.BAT` (CTCHIP34) sets the Blue Lightning to 2x, L1 on,
  0-640 KB cacheable (LMCR `1001h:0/1` = `03FF`) and 1-16 MB cacheable (CMLR `1001h:4` = `F0`, the
  2026-09-12 fix, `cpu_cache_cmlr_2026_09_12.md`).
- **What limits it is the flush, not the setting.** SW1 is ON and neither SW1 nor snooping stops the
  per-I/O flush, so every port access throws the 16 KB away. The cache earns its keep only between
  port accesses; everything below is about making fewer accesses and touching less between them.
- **Not cached: F0000-FFFFF.** LMCR's bits cover 64 KB blocks; `03FF` stops at 640 KB. The Inboard
  serves the BIOS from its own RAM at F0000 (`romcache_decode_2026_10_03.md`, PHYSPROBE), so BIOS code
  runs from uncached RAM. How much runs there under Windows is not known: if the tick is reflected to
  the BIOS's INT 8 (raised in `captures/2026-10-10_timer/README.md`, unconfirmed), it is 18 times a
  second; DOS boxes call the BIOS too. Making that block cacheable (LMCR bit 15, by the bit pattern of
  `03FF`; check against the CTCHIP documentation) would speed whatever does run there. Small; and the block must
  never be written while cached. Test: set the bit with CTCHIP, IDLECOST before and after, and a DOS
  box `DIR` loop. **A0000-BFFFF (video) must stay uncached.**
- **Our own code's working set:** SWAPCOMP's hash table is 8 KB of the 16 KB (item 3a).

## Findings, ranked by what they could be worth

### 1. Re-price the machine's background I/O with a real working set (no build; card)

Every background port access costs the running program a refill. The two background measurements
so far (tick, CD-RW auto insert) used TIMERRES, whose loop fits in a few cache lines, so they show
the bus and interrupt cost but almost none of the refill. A program with an 8-12 KB working set pays
up to ~1 ms per flush (12 KB at 83 ns/byte). The 1.2% measured for the CD-RW poll may be several
times that for real programs.

`tools/idlecost/IDLECOST.EXE` (new) walks 1, 4, 8 and 12 KB working sets; run it per background state
(network cable out, external chain off, default tick as the yardstick; `tools/idlecost/README.md`).
Sources still unmeasured: 3C509B receive traffic (an interrupt and a PIO read of every broadcast
frame), the V9x agent's serial traffic, VCACHE's lazy writes.

### 2. T130.MPD: the per-block status spin (vendor binary)

From `t130_mpd_review_2026_09_26.md` (and `cloud/bluescsi/T130MPD.md` on branch
`claude/bluescsi-5160-firmware-o12wij`):

- It sends no IDENTIFY (ATN is never asserted), so the target cannot disconnect: **Device Manager's
  "Disconnect" box has no effect with this driver**, though it is ticked for the changer, Zip, MO and
  CD-RW (`next_session_2026_09_28.md`). Answered on 09-26; recorded here because the box suggests
  otherwise.
- Its 2 ms timer is not a cost with IRQ 3: the driver stops arming it once the IRQ has won five times.
- The cost that remains is in the data phase, run from the interrupt: before each 128-byte block it
  spins on card status (`base+0` bit 2) with up to 10,000 back-to-back reads, then a 1 us stall loop with
  no timeout (`0x17D3`). Each read is a bus cycle and a flush; the bed counted ~2.2 such reads a block,
  and the real card makes the driver wait longer than the emulator does. Pacing that spin the way the
  XT-CF driver paces its polls (a few reads, then a register-only delay) is #41's patch. Measure first:
  a real-card read count per block is the missing number (#43: the bed has no bus hold time).

### 3. SWAPCOMP (ours)

**a. Hash table size.** Compression is the costliest step (4.71 ms a page on the card, 92 cycles a
byte, against 1.17 ms to decompress). Its 8 KB table plus the 4 KB page and the output overfill the
16 KB L1 once code and stack are counted. `tools/swapcomp/hashtest/LZ4B10/11/12.EXE` (new) measure 2,
4 and 8 KB tables on the card, ratio and time, in one 3-minute run. `SWAPCOMP.ASM` now takes
`-DHASH_LOG=<n>` (default unchanged, 12).

**b. All-zero pages.** 1.5% of written pages are all zero (`magnaram_and_swap_compression_2026_10_01.md`).
Today each is compressed to one sector and costs a full XT-CF command each time it is written or read
(~2.9 reads a write): first DRQ ~238 us plus one sector at 1.96 ms. A slot value meaning "zero" (no I/O;
`rep stosd` on read) removes those commands. Design only: `SlotTab` holds sectors 1-6, so e.g. `0FFh`
is free. Needs the checksum build (`SWAPCMPC`) in the bed before the card.

**b2. Which compressor (owner's question, 2026-11-04).** On this bus a sector saved is worth
1.96 ms, about 164,000 CPU cycles, so ratio counts for more than decode speed, but only in whole
sectors, and every page-out pays the compressor (0.35 of a page-out per page-in). Families:

| family | ratio on the swap sample | decode | compress | verdict |
|---|---|---|---|---|
| LZ4 (today) | 2.26:1 (2.64 on live traffic) | 1.17 ms, measured | 4.71 ms, measured; more with the driver's `IMUL` | keep; make the compressor cheaper (hashtest) |
| LZ4 with a longer search, same decoder | 2.36-2.39:1 | same | 9-11 ms, measured | only where it drops a sector ("sector-aware") |
| deflate (entropy coded) | 2.92:1 | ~5-10 ms, estimate | ~8 ms+, estimate | wins only if its decode is under ~3 ms on the BL3: unmeasured |
| bit-oriented LZ (ZX0/LZSA class) | between LZ4 and deflate, estimate | slower than LZ4 | optimal parsers far too slow for page-outs | not before deflate is measured |
| word-based (WKdm) | 1.51:1, measured | fast | fast | no: Windows 95 here pages 16-bit code and bitmaps |
| RAM compression (MagnaRAM) | - | - | - | no: measured slower, it locks the RAM this machine lacks |

`tools/swapcomp/sectors.py` (new) runs every family over the card's `WIN386.SWP` on the host and
reports sectors per page and the modelled ms per page-in, with measured CPU costs where they exist
and labelled estimates elsewhere. It also counts pages that are one repeated dword (store the word,
no I/O; the zero-page design generalised) and how often a longer LZ4 search saves a whole sector.

**b3. Stop compressing a page that will be stored raw.** `LZ4_Compress` (`SWAPCOMP.ASM:454-562`)
has no output limit: it compresses the whole page, then `SC_One` finds more than `SC_MAX_SECTORS`
and writes it raw. On the card ~12% of page writes went raw (258 of 2,109 a run, 2026-10-02), each
after a full compression (~4.7 ms, more on incompressible data, which hashes every position). Abort
once the output passes 3,070 bytes (6 sectors less the 2-byte length).

**c. Fixed cost per page.** A compressed page-in is ~3 sectors (~6 ms of data) plus the miniport's
fixed cost and the card's first-DRQ latency, ~0.3 ms: about 5% of every compressed page-in. So item 4's
per-command savings are worth more on paging than on file reads.

### 4. XT-CF miniport: per-command and per-sector cost (ours; build switches, default off)

Each item is under 1%; together roughly 0.5-1% on paging, less on large file reads.

- **a. Taskfile values in registers before the first OUT.** After each OUT the L1 is empty, so the
  `mov edx,[XTIDE_p*]` / `mov eax,[XTIDE_o*]` pair after it misses twice (`XTIDETR.ASM:1204-1227`,
  six OUTs). Loading the six values and ports first leaves only code fetches between OUTs: ~12 misses,
  ~15 us a command. Grouping the variables into one block does not help: each OUT empties the cache.
- **b. DRV/HEAD only when the unit changes** (`:1205-1209`): saves one OUT, one status read and their
  flushes per command. Legal ATA for this card below 128 GB (`cross_driver_levers_2026_11_01.md`
  candidate 4); the risk is a card that wants it every command. Its own switch.
- **c. READ/WRITE MULTIPLE** (C4h/C5h after SET MULTIPLE C6h): one DRQ wait per block instead of per
  sector, about 7 status reads saved per 4 KB. Needs IDENTIFY word 47 and a fallback (#52 idea 2).
- **d. Status pointer in a register in the per-sector loop**, for the same reason as a.

Not proposed, as before: skipping BSY/DRQ checks, shadowing SecCnt/LBA, DMA (the card has none,
closed 2026-09-29).

### 5. Fewer bytes for program files (#51)

SWAPCOMP reaches only swap: 38% of page-ins are EXE/DLL pages, plus every program load. OSR1 has
DriveSpace built in. An experiment, not a recommendation: a small DriveSpace volume holding a copy of
one large program, launch time against the plain copy. It wins only if DriveSpace's decompression on
the BL3 costs less than the bus time it saves (3.82 us a byte); SWAPCOMP's LZ4 costs 0.29 us a byte.
Risk is to the files on that volume only. #51's first step (metadata share of disk bytes) still applies.

### 6. Memory (resident size)

- SHADRAM: the reserved 128 KB is closed (the card shows nothing there after boot,
  `shadram_5160_2026_10_03.md`); nothing new.
- The RAM audit of resident VxDs on the card is still owed (worklist). SWAPCOMP costs ~22 KB locked
  (8 KB hash, 8 KB slot table, 4.3 KB bounce); a 10-bit table would return 6 KB if 3a favours it.

## Also fixed today

`tools/daynaprobe/DAYNAPRB.EXE` (BlueSCSI branch) was built with MinGW's i686 default and held five
CMOV instructions, which the 486BL3 does not implement; rebuilt with `-march=i386` (`1f28e82`). Both
new tools here are checked for CMOV.

## Test order on the card

| # | test | build | cost |
|---|---|---|---|
| 1 | IDLECOST: base, cable out, chain off, default tick; 3 runs each | none | ~20 min |
| 2 | LZ4B12, LZ4B11, LZ4B10 beside `SWAPPAGE.BIN` | none | ~3 min |
| 3 | LMCR bit 15 (F0000 cacheable) with IDLECOST before and after | CTCHIP line | ~10 min |
| 4 | T130.MPD status reads per 128-byte block on the real card (method to agree: a logic probe on the card's status port, or a patched counting build) | none, then a patch | to agree |
| 5 | XT-CF switches 4a-d, one at a time: 1,000 one-sector and 8-sector reads (PIT), WRTEST, then PAGERUN | DDK build | per switch |
| 6 | SWAPCOMP zero pages, then the hash size from test 2: PAGERUN A/B as on 2026-10-02 | DDK build | ~1 h |

Results go to `docs/captures/<date>_*/` and a row each in this file.
