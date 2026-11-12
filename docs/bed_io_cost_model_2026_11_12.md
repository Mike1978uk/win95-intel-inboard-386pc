# Bed I/O cost model, plan, 2026-11-12

Goal: the bed charges port I/O as the card does, so a CPU-vs-port trade ranks the same in both.
Plan only. No code changed. Problem record: `bed_misprices_io_vs_memory` (memory note, 2026-10-21):
register shadowing benched 3% slower in the bed, saves ~15 us a fill on the card.

Tree: beds run `86box_auto/` (AUTOMATION build, `docs/host_tools.md` L18), not `86box_full/`
(stale). Bed `vm_5160_mcp/86box.cfg`: `cpu_family = ibm486bl3`, `cpu_speed = 83500000`.
One cycle = 12 ns.

## 1. What the bed charges now

| Access | Charge | Where |
|---|---|---|
| IN/OUT imm/DX | `CLOCK_CYCLES(12 + io_waitstates)` (10/11 on some forms) | `86box_auto/src/cpu/x86_ops_io.h:7-130` |
| `io_waitstates` | `11 * ratio - 11`, ratio = busspeed/4.77 MHz; 53 at 83.5/3 MHz | `src/device/inboard386.c:278-293` |
| Port nobody claims | extra `cycles -= io_delay`, io_delay = rspeed/3 MHz = 28 | `src/io.c:388` (inb), `:449`, `:535`, `:605`, `:723`, `:810`; value `src/pit.c:1939` |
| `isa_cycles` | forced to 2 for POST pacing; `ISA_CYCLES(x)` used by a few devices | `inboard386.c:377`, `src/cpu/cpu.h:669` |
| Memory read/write | `mem_read_cycles`/`mem_write_cycles` = 8 on the 486BL2 table, rescaled by the Inboard | `src/cpu/cpu_table.c:2120-2135`, `inboard386.c:190-215` |
| Mach8 option ROM (CS=C000) | all of the above zeroed/1 so the ROM self-test is not stretched | `src/cpu/386_dynarec.c:1170-1235` |

Result: ~65 cycles, 0.8-1.1 us, per OUT regardless of width (a `rep outsw` is charged per
iteration through the same opcode, not per byte).

L1: no model. `hascache` is a flag (`src/cpu/cpu.c:654`); `cpu_update_waitstates` only picks
a cycle count when `cpu_cache_int_enabled` (`cpu.c:4765`, prefetch off, no hit/miss). `cachesize`
in `src/mem/mem.c:105` is the page-translation cache, not an L1. No instruction invalidates
anything.

## 2. Gap against the card

Card: `docs/captures/2026-11-12_flushprobe/README.md`, `docs/iowidth_measured_2026_09_20.md`.

| Access | Card | Bed | Gap |
|---|---|---|---|
| OUT byte | 5.695 us | ~1 us | 5-6x |
| OUT word (Mach8 register) | 7.64 us | ~1 us | 7x |
| OUT dword | 12.7 us (3.183/B) | ~1 us | 12x |
| 2nd and later I/O in a burst | bus time only, ~5.6 us (byte IN) | ~1 us | 5x |
| First non-I/O work after a burst | refill, 78 ns per byte of working set touched (1 KB = 84 us, 8 KB = 638 us), once per burst | 0 | all of it |
| ISA memory write | dword 1.83 us/B (7.3 us per dword), posted, no flush | `mem_write_cycles`-scaled, ~0.1 us | ~70x |
| ISA memory read | 3.48 us/B dword (13.9 us per dword), no flush | 14 cycles = 0.17 us | ~80x |
| Cached memory read, hit | ~1 cycle | 8-14 cycles | bed too slow |

The last row is why the bed rewards fewer instructions and under-rewards fewer port writes.

## 3. Proposed model

Two parts, both on the Inboard machine only (`inboard386.c` owns the knobs).

A. Per-access bus cost. Replace the `12 + io_waitstates` formula with a time-based charge:
`io_waitstates` computed per width from card microseconds x CPU MHz: byte 5.7 us = 476 cycles,
word 7.64 us = 638, dword 12.7 us = 1060. Needs the width at the charge site: the byte/word/dword
variants are separate ops in `x86_ops_io.h`, so each gets its own constant (`io_ws_b/w/l`).
String ops charge per element. Claimed and unclaimed ports cost the same on the card
(IN 22h too), so drop `io_delay` for them or fold it in.
Memory on the XT bus (0A0000-0EFFFF, Mach8 ROM, VGA half): charge write dword 7.3 us,
read dword 13.9 us. Needs the address-range hook in the memory map for the ISA window
(`mem_write_cycles` is global; ISA mappings need a per-mapping cost or a wrapper in
`mem.c` read/write for `bus` mappings). Larger change than A-I/O; do I/O first.

B. Flush penalty. A flag `io_dirty` set by IN/OUT. At the next instruction that is not
IN/OUT, if set: charge `flush_cycles` once, clear the flag. Burst = consecutive I/O
instructions, so 1000 OUTs pay one refill, as measured (`README`, FLUSHP2 burst).
Size: fixed, `flush_cycles` = 85 us x 83.5 = ~7100 cycles (FLUSHP B, 1 KB), configurable
through the Inboard device config (`flush_us`, default 85, 0 = off). A working-set estimate
(distinct bytes fetched or read since the last flush, x 78 ns) is closer to the card (linear to 8 KB) but needs a per-access hook on the fetch/read path
that costs emulation speed; start fixed, add the estimate only if a bench rank still
disagrees. Hook: set in the ops of `x86_ops_io.h` (and the dynarec `ropIN/ropOUT` equivalents
in `src/cpu/codegen_ops_*`; the dynarec path must also set the flag, check the recompiled
IN/OUT helpers call into `io.c`); clear and charge in the main loop before the next instruction
(`src/cpu/386_dynarec.c` / `386.c` exec loop), or cheaper: charge it in `io.c` `inb/outb/...`
lazily as a "pending" counter added to `cycles` when the next non-I/O memory read or
instruction fetch runs. Preferred: pending counter consumed in the exec loop, one branch.

Risks:
- POST and BIOS delay loops are calibrated against the present IN/OUT cost
  (`inboard386.c` notes, ~L270-420, timing in the PIC IMR self-test, PIT test; `isa_cycles = 2`
  was tuned for ~71 s POST). Raising I/O cost 7x makes POST several times longer. Retune
  or scope: model on only after POST (e.g. when Windows `VMM` runs, or a config switch
  `io_model = real|legacy`). Default `legacy`; beds opt in.
- Mach8 ROM self-test at CS=C000 is zeroed because inflation stretched it to 65-100 s
  (`386_dynarec.c:1175`); the new model must honour that exception.
- Timing-sensitive devices (floppy, PIT, sound) that count cycles around I/O may drift:
  the PIT/IOCHRDY wait is real time on the card too, so a slower I/O is more faithful,
  but the 86Box device tick runs off `cycles`; check the floppy and 3C509B still pass.
- Dynarec: a missed flag set silently makes the penalty vanish in recompiled blocks.
- Upstream: all of this is Inboard-specific; keep it out of generic CPU code paths behind
  one global (default 0), so it can stay in the fork.

## 4. Validation

1. Build `86box_auto` with the model, bed `vm_5160_mcp` (restore `default/` image).
2. Run `tools/flushprobe/FLUSHP.COM` and `FLUSHP2.COM` from DOS; capture raw lines as the card did
   (`C:\F.TXT`, `C:\F2.TXT`).
3. Compare each line to the card README: A 25 us, B 84, C 321, D 638, E 81, I 85, J 124, K 7.5;
   F2 byte/word/dword write 2.15/1.94/1.83 us/B, read 3.72/3.57/3.48, `rep outsb` 5.5 us/B.
   Accept within 10%; F, G, H must show no invalidation.
4. Re-run `iowidth` (`tools/gen_iowidth_probe.py`) on the bed: 5.695 / 3.819 / 3.183.
5. Rank test: M8BENCH register-shadowing build vs plain, bed vs card. The bed should now agree
   (card: shadowing wins; bed 2026-10-21: lost 3%).
6. Regress: POST time, Win95 boot to desktop, `TEST.COM`, 35 Mach8 tests, floppy read, NIC ping.
   Smoke-test an unmodified non-Inboard config boots (G11).

## 5. Open questions

- Does a `rep outsw` pay the 5.5 us/B (byte) or 3.8 us/B (word) rate? Both measured; the
  string op charge per element must match the element width.
- Is the flush really whole-L1 at the module, or only for lines in the 8 KB cache that were
  filled after the last I/O? Test: touch 1 KB, I/O, touch it again versus touch fresh 1 KB.
- Does IN 22h reach the XT bus? Charge is the same either way per README; bus use unconfirmed.
- Working-set estimate: worth its emulation-speed cost, or does a fixed penalty rank right?
- Memory timing for the ISA window: global `mem_*_cycles` or per mapping? Decide before B/A-mem.
- Interaction with the Inboard's own `670h` waitstate register: does it scale the I/O cost?
- Mode 13h at A000 write rate is unmeasured on the card (README says expected same).
