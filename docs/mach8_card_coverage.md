# Mach8 card coverage - register by register

One row per register: what the guide says, what the model does, whether the card was measured,
and what our own driver, DirectDraw, 3D or demo work could use it for (`docs/mach8_graphics_vision.md`).
Scope is the whole card, not what ATI's drivers touch (owner's rule, `next_session_2026_10_11.md`).

Sources: Mach32 guide `references/ati_mach32/guide.txt` (REG688000-15, page numbers below);
Richter & Smith, *Graphics Programming for the 8514/A* (`references/8514a_docs/`, R&S);
XFree86 3.3.6 `references/xfree86_336_mach8/`. Model: fork `9c9fa70c2`, `src/video/vid_ati_mach8.c`
(`mach_accel_out_fifo` 4471, `mach_accel_in_fifo` 6076, `mach_accel_in_call` 6666) and
`src/video/vid_8514a.c` (`ibm8514_accel_out_fifo` 444, `ibm8514_accel_start` 1192).

Status: **ok** = matches the guide; **ok/card** = and measured on the card; **partial**; **stored** =
kept, no effect; **absent**; **conflict** = model disagrees with the sources.

## Chapter 8 - 8514/A-compatible registers (Mach8 and Mach32)

### CRT

| reg | port | guide | model | use |
|---|---|---|---|---|
| ADVFUNC_CNTL | 4AE8 W | 8-6. Bit 0 VGA/8514 output, bit 2 shadow set 1/2. Mach8 only: a write resets CRT_PITCH, GE_PITCH, CRT_OFFSET, GE_OFFSET | ok. Mach8 path resets pitch to 1024 and offsets | driver mode set |
| DISP_CNTL | 22E8 W | 8-7. Bits 2:1 Y_CONTROL skip, 3 double scan, 4 interlace, 6:5 enable/reset | partial: interlace, double scan used; Y_CONTROL skip and the 5:6 reset state not modelled | the guide's mode-change sequence resets the CRTC first |
| DISP_STATUS | 02E8 R | 8-8. Bit 0 SENSE (RGB > 0.3 V), 1 VSYNC, 2 LINE_SYNC toggles each line | partial: SENSE and VSYNC; LINE_SYNC (bit 2) absent | LINE_SYNC: per-line timing for raster effects (demo) |
| H_TOTAL | 02E8 W | 8-9 | ok | - |
| H_DISP | 06E8 W | 8-8. High byte is an alternate H_TOTAL | ok | - |
| H_SYNC_STRT | 0AE8 W | 8-9 | ok | - |
| H_SYNC_WID | 0EE8 W | 8-9. Bits 4:0 width, 5 polarity | stored | - |
| V_TOTAL / V_DISP / V_SYNC_STRT | 12E8 / 16E8 / 1AE8 W | 8-10..12. 12 bits, SKIP_2 encoding | ok | - |
| V_SYNC_WID | 1EE8 W | 8-12. Bits 4:0 width, 5 polarity | absent: value discarded | - |

CRT writes reach the model only with CLOCK_SEL bit 0 set or the 8514 side off (`mach_accel_out_fifo`
4583-4600). Not in the guide; not checked on the card.

### Engine control and status

| reg | port | guide | model | use |
|---|---|---|---|---|
| SUBSYS_CNTL | 42E8 W | 8-18. ACK bits 3:0, ENA bits 11:8, 15:14 engine reset | partial: ACK, reset ok/card (PATT_INDEX cleared, measured). **ENA bits never raise an IRQ on ISA** (`mach8_vga_isa` has no IRQ line; comment at 9905) | **GE_IDLE interrupt instead of polling GE_STAT** - each poll is an I/O read at 5.7 us/byte here |
| SUBSYS_STAT | 42E8 R | 8-20. INT bits 3:0, MONITOR_ID 6:4, MEM_SIZE 7 | partial: VBLANK, INSIDE_SCISSOR (named INT_GE_BSY in code), FIFO empty, MONITOR_ID, MEM_SIZE. **INVALID_IO (bit 2) never set** | INSIDE_SCISSOR + CMD "no draw" = hardware hit test (8-30): pick, collision |
| GE_STAT | 9AE8 R | 8-41. Bits 7:0 FIFO occupied, 8 DATA_READY, 9 GE_BUSY | partial: FIFO bits from a counter reset by every read; busy and data-ready ok/card (TEST.COM) | - |
| MEM_CNTL | BEE8 idx 5 | 8-17. Y skip, PLANE_SELECT (512 KB boards) | conflict: Mach8 model resets pitch to 1024 on this write; the guide says nothing of it. PLANE_SELECT absent (our card is 1 MB) | - |
| FIFO / A14 | - | 8-14. With port bit A14 set, the card adds wait states instead of overrunning the FIFO | absent: A14 is stripped and the FIFO never overruns | **write bursts without reading GE_STAT** - if the card honours A14, a driver skips the FIFO poll |

### Drawing

| reg | port | guide | model | use |
|---|---|---|---|---|
| CMD | 9AE8 W | 8-28..34. Opcodes 0-6, bits 0-12 | ok/card for lines, fills, blits, short strokes, reads (M8CONF 82/82, TEST.COM). Opcode 7 (8514/A: stuck busy) not checked | all |
| CMD bit 4 DRAW=0 | | 8-30. Trajectory runs, nothing drawn; side effects and INSIDE_SCISSOR still happen | unchecked | hit test |
| Poly fill A / B | PIX_CNTL bits 2:1 | 8-34..35 | ok/card (TEST.COM TS1) | **flat-shaded polygons** (3D plan) |
| CUR_X / CUR_Y | 86E8 / 82E8 R/W | 8-37..38. Range -512..1535 | ok/card, 11 bits | - |
| DESTX_DIASTP / DESTY_AXSTP | 8EE8 / 8AE8 W | 8-52..53. 13-bit signed steps | ok/card (13 bits measured, M8SEQ) | - |
| ERR_TERM | 92E8 R/W | 8-39 | ok/card, 13 bits. "-1 when X increases" guide only | - |
| MAJ_AXIS_PCNT | 96E8 W | 8-42 | ok | - |
| MIN_AXIS_PCNT | BEE8 idx 0 | 8-43 | ok | - |
| SHORT_STROKE | 9EE8 W | 8-51. Guide: with host data the first vector of a pair consumes it wrongly | ok/card (M8ROW3) | line art, glyphs |
| SCISSOR_T/L/B/R | BEE8 idx 1-4 | 8-49..50. T/L 11-bit signed, B/R 11-bit unsigned | ok | clipping |
| X wrap mod 1024 | | 8-22 | ok/card (M8WRAP, 10-09 fix) | - |
| Host read outside scissors returns FFh | | 8-23 | guide only | - |

### Data path

| reg | port | guide | model | use |
|---|---|---|---|---|
| FRGD_COLOR / BKGD_COLOR | A6E8 / A2E8 W | 8-27, 8-40 | ok | - |
| FRGD_MIX / BKGD_MIX | BAE8 / B6E8 W | 8-24..25. 16 Boolean + 16 arithmetic mixes | logical ok/card; **arithmetic 10h-1Fh present, never measured on the Mach8** | **average (17h) = 50% translucency; saturating add/sub = light and shadow** (3D, DirectDraw, demo). Check the Mach8 has them at all |
| PIX_CNTL | BEE8 idx A | 8-46. Poly fill, COLOR_CMP_FN 5:3, MONO_SRC 7:6 | MONO_SRC ok/card. Compare ok/card (below) | - |
| COLOR_CMP | B2E8 W | 8-36. TRUE = not written; compare ignores write-masked planes | **ok/card** (fixed `c454a6324`) | **colour-keyed sprites** (DirectDraw colour key); write-protect a colour range |
| PATTERN_L / PATTERN_H | BEE8 idx 8 / 9 | 8-44..45. 4-pixel mono patterns, even / odd nibbles | present (`vid_8514a.c` 1257); not measured | dithered tones |
| WRT_MASK / RD_MASK | AAE8 / AEE8 W | 8-48, 8-54. RD_MASK rotated left one bit in IBM mono reads | ok/card (TS2 fit, M8BLRD) | plane layers (3D plan) |
| PIX_TRANS | E2E8 R/W | 8-47 | ok/card: byte order, data width, mono layout (M8ROW3, M8SEQ, M8MONO) | host images |
| DAC_* | 02EA-02ED | 8-1..3 | ok (TEST.COM DAC test) | palette fades |

### The compare conflict - fixed 2026-10-10

Both written sources agree: when the compare is TRUE the pixel is **not** written (guide 8-36, 8-46;
R&S p. 246 and p. 9582). R&S also says planes disabled in WRT_MASK are treated as 0 in the compare.

- ATI path (`mach_accel_start`, DEST_CMP_FN): `if (!compare) write` - matches.
- 8514/A path (`ibm8514_accel_start` 1459, PIX_CNTL[5:3]): writes when `dest >= cmp` for mode 2, and
  so on for 3-7 - the opposite. Mode 1 (always TRUE) is never written, which matches. So the path
  contradicts itself. Upstream code (`b33d0adc0` boundary); not ours.
- Neither path masks the compare with WRT_MASK.
- M8CMP (10-07) measured DEST_CMP_FN on the ATI path only. PIX_CNTL with an 8514/A CMD is unmeasured.

TS1/TS2 pass in the bed, so TEST.COM does not exercise a mode 2-7 PIX_CNTL compare; no ATI driver
shape in M8CONF uses one either. Our colour-key path would.

**Measured** with M8CMP8 (`tools/m8seq/`, `docs/captures/2026-10-10_m8cmp8/`): 48 cases, WRT_MASK FFh and
0Fh x three colours x modes 0-7. The card follows the guide with both operands masked by WRT_MASK -
0 of 11,880 pixels off; every model rule missed by 7,000+. Fork `c454a6324` replaces the 17 copies of the
compare with one helper; the bed now equals the card byte for byte, and the other 47 REGR outputs are
unchanged (M8REGS A9h aside, which varies on the card too). Not yet measured: whether DEST_CMP_FN on the
ATI path also masks by WRT_MASK.

## Host-interface tuning across both chips (owner's idea, 2026-10-10)

The CPU work set five BL3 registers together (CTCHIP34: cache, cacheable regions, clock). The card
has the same kind of knobs on both chips, and the bus is where all the cost is: an I/O byte costs
5.695 us here, a memory byte 2.861 us read (technique 128d). Knobs known so far:

| chip | register | bits | POST/driver value | source |
|---|---|---|---|---|
| Mach8 | MAX_WAITSTATES 6AEE | 3:0 max wait states on engine I/O writes; 7:4 ROM_SPEED (POST ROM reads, Fh = wait forever at reset); 9 IOR16_ENA; 8 LINE_OPT_ENA | `009A` from TEST.COM (writes A, ROM 9) | guide 9-10 ("set by the POST ROM, not for applications") |
| Mach8 | FIFO_OPT 36EE (write-only) | 0 W_STATE_ENA: 1 = wait states once the FIFO is half full (power-up), 0 = only when full; 1 HOST_8_ENA | unknown | guide 9-8, Ferraro table 19.11 |
| Mach8 | port bit A14 | waits instead of overrunning the FIFO | - | guide 8-14 |
| 28800 | not yet known | Ferraro 13.7.2 lists fast write, fast decode, 8/16-bit memory, I/O and BIOS as typical Super VGA knobs, per chip in ch. 15-21; XFree86 names none for the 28800 | - | VVESA.COM, Sutty and Blair (18800) |
| Inboard | LMCR 1001h | A0000-BFFFF is not cacheable (`FF/03` = 0-640 KB) | shipped | technique 68 |

Only the card can rank these: 86Box does not model per-card bus timing. First measurement, before
touching any knob - one DOS run, three costs on the card, against an undecoded-port control:

1. Mach8 register writes: `outsb`/`outsw` to FRGD_COLOR (A6E8h). No `outsd` - A6EAh/A6EBh are not
   registers and their decode is unknown.
2. 28800 memory writes: byte/word/dword to text page 2 (B9000h is the LS-120 trace page; use BA000h).
   Technique 128d measured memory reads only.
3. Mach8 ROM reads at C000h, which ROM_SPEED governs. Shadowed? Check first - EGACACHE is off.

**Measured on the 5160, 2026-10-10** (`tools/gen_m8bus_probe.py`, `docs/captures/2026-10-10_m8bus/`),
512 bytes each, us per byte:

| | byte | word | dword |
|---|---|---|---|
| out, undecoded control 0278h | 5.696 | 3.814 | - |
| out, Mach8 FRGD_COLOR A6E8h | 5.690 | 3.827 | - |
| in, undecoded control 0278h | 5.782 | 3.807 | - |
| in, Mach8 GE_STAT 9AE8h | 5.782 | 3.824 | - |
| write, 28800 memory BA000h | **2.324** | **2.092** | **1.981** |
| read, Mach8 ROM C0000h | 2.865 | 2.416 | 2.187 |

- **The Mach8 adds nothing at idle**: its ports cost what an empty port costs, to 0.3%. Its
  wait-state and FIFO knobs can only matter when the FIFO fills - next test is a burst behind a
  long fill.
- **VGA memory writes are the cheapest path on the card**: sync ~0.46 us, ~1.86 us per bus cycle;
  half the read sync (0.88). Host pixels cost 3.8 us/byte through PIX_TRANS and 2.0 through the VGA
  window - but only the VGA side can use the latter.
- The ROM at C0000h is not shadowed and costs what the XT-CF's ROM does (technique 128d), so
  ROM_SPEED = 9 shows no penalty on reads.

If (1) equals the control, the Mach8 adds no wait states at idle and its knobs matter only when
the FIFO fills - then measure a burst. Any knob change is followed by M8CONF and TEST.COM, as the
CPU changes were (SNP `8A` hung the 5160).

**DRAM refresh, measured 2026-10-10** (`tools/gen_refresh_probe.py`, `docs/captures/2026-10-10_refresh/`).
Refresh is a DMA cycle on channel 0 paced by PIT channel 1; Speeder, FastV20 and GLaBIOS lengthen it.
Interleaved arms, divisor written back to 18 at the end, us per byte:

| divisor | out 0278h byte | write BA000h dword | read C0000h byte |
|---|---|---|---|
| 18 (IBM) | 5.696 / 5.693 | 1.987 / 1.974 | 2.865 / 2.868 |
| 64 | 5.533 / 5.533 | 1.866 / 1.860 | 2.707 / 2.711 |
| gain | **2.9%** | **6.0%** | **5.5%** |

Repeats agree to 0.2%. Software only - two `OUT`s, no ROM. Before shipping it: this rate refreshes
**all** the memory, not only the planar's 64 KB - the Inboard has no refresh of its own and enables
every bank's RAS on DACK0, the XT's refresh DMA cycle (RonnyRoy's reproduction, `U71.pld`). Its
TMM41256s want 256 rows per 4 ms (15.6 us); divisor 18 gives 15.08 us, 64 gives 53.6 us. So it needs
a soak of planar, conventional and XMS memory (`tools/soak/RSOAK2`); INBRDPC.SYS sets no refresh;
and any BIOS delay that counts refresh runs 3.5x longer (technique 134 - WAIT86 covers INT 15h
AH=86h). Cimon's Super PC/Turbo XT BIOS v2.5 also programs 12h (`PCXTBIOS.ASM` line 492), so refresh
is not a 5150 difference for issue #10.

A cross-chip idea, not measured: the Mach8 engine cannot write the VGA side, so in a packed
256-colour bank only the CPU writes A0000-AFFFF. A write-through cache over that window would then
be coherent if flushed on every bank switch. Planar modes read through latches and must stay uncached.

## Chapter 9 - ATI extended registers

Not started.
