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
| MEM_CNTL | BEE8 idx 5 | 8-17. Y skip, PLANE_SELECT (512 KB boards). 9-5 and 9-20: on the Mach8 a write resets CRT_PITCH and GE_PITCH to 128 | ok: the model's pitch reset matches 9-5 (was listed as a conflict from chapter 8 alone). PLANE_SELECT absent (our card is 1 MB) | - |
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
| FRGD_MIX / BKGD_MIX | BAE8 / B6E8 W | 8-24..25. 16 Boolean + 16 arithmetic mixes | **ok/card, all 32** (M8MIX, 10-10, `docs/captures/2026-10-10_m8mix/`): card = bed byte for byte, 2 colours x 256 destinations. MIN 10h, D-S 11h, S-D 12h, S+D 13h, MAX 14h, halved 15h-17h (carry shifted in), saturated 18h = 19h D-S, 1Ah S-D, 1Bh S+D, halved 1Ch = 1Dh, 1Eh, 1Fh (FFh on overflow, else (S+D)/2) | **the Mach8 blends on its own: 17h = 50% translucency, 1Bh/18h = saturating light and shadow, MIN/MAX** (3D, DirectDraw, demo) |
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

**Command FIFO, measured 2026-10-10** (M8FIFO, `docs/captures/2026-10-10_m8fifo/`), writes behind a
1024x512 fill:

- **Depth 16.** EXT_FIFO_STATUS (9AEEh) works on the Mach8 and shows all 16 entries; GE_STAT shows the
  first 8 only. A driver can read 9AEEh once and send up to 16 minus its count.
- **A full FIFO drops writes.** Writes 17-24 took no longer than the others and SUBSYS_STAT bit 2
  (INVALID_IO) was set. With A14 (E6E8h) writes 9-17 took ~20 us more each - wait states once the FIFO
  is half full - but the writes past 16 were still dropped and INVALID_IO still set. A14 does not
  make blind bursts safe here.
- **Engine fill rate ~31 Mpixel/s** (8 bpp solid fill: 256x256 2.25 ms, 1024x512 17.0 ms); a command
  costs ~84 us end to end for a 1x1 fill including its seven setup writes. Any fill over ~250 pixels
  outruns the bus that feeds it.
- Model: the engine completes at once, so the FIFO never fills, INVALID_IO is never set and
  EXT_FIFO_STATUS cannot show occupancy - a gap for a driver that relies on the depth.

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
**Soak, 2026-10-10:** 5 minutes at 18 then 5 at 64, 30 checks each: 333 KB conventional and 4,224 KB XMS with 0 word errors at both rates; planar block 0Ch changed at both (DOS). A warm-machine soak is next. **Windows keeps channel 1** (bed, 10-10, `pitlog_ch1_bed.log`: BIOS 18, REFR64 64, then Windows set its tick and wrote channel 1 no more; channel 2 only for POST beeps). Still to check: floppy delays;
and any BIOS delay that counts refresh runs 3.5x longer (technique 134 - WAIT86 covers INT 15h
AH=86h). Cimon's Super PC/Turbo XT BIOS v2.5 also programs 12h (`PCXTBIOS.ASM` line 492), so refresh
is not a 5150 difference for issue #10.

A cross-chip idea, not measured: the Mach8 engine cannot write the VGA side, so in a packed
256-colour bank only the CPU writes A0000-AFFFF. A write-through cache over that window would then
be coherent if flushed on every bank switch. Planar modes read through latches and must stay uncached.

## Chapter 9 - ATI extended registers

Guide lines 9500-13000 read 10-10. Rows are the Mach8's registers; Mach32-only features are listed once at
the end. Model readings are by hand; "card" means measured on the 5160.

### CRT control and engine setup

| reg | port | guide | model | use |
|---|---|---|---|---|
| CLOCK_SEL | 4AEE W | 9-4. Bit 0 PASS_THROUGH: 0 = the RAMDAC shows the VGA, 1 = the 8514 buffer. Clock select and divide. Writing it puts the CRTC in ATI mode | ok: bit 0 sets `dev->on`, bit 6 the divide. Carries a `DIAG7` pclog (A7 clean-up) | **one bit switches the shared DAC between the two halves** - section C of `docs/worklist.md` |
| CRT_OFFSET_LO/HI | 2AEE / 2EEE W | 9-5. 20-bit display start, bytes/4 | ok | page flip, hardware scroll |
| CRT_PITCH | 26EE W | 9-5. Units of 8 pixels; Mach8: reset to 128 by ADVFUNC_CNTL or MEM_CNTL | ok | - |
| SHADOW_CTL / SHADOW_SET | 46EE / 5AEE W | 9-6..7. Lock bits; primary / shadow 1 / shadow 2 CRT sets. LOAD_SRC/DST (9:8) 68800-6 only | ok: both used by the CRT code | - |
| FIFO_OPT | 36EE W | 9-9. Bit 0 W_STATE_ENA, bit 1 HOST_8_ENA (8-bit host data) | stored with `&= 0xfff0`, so both bits are dropped; harmless while the model has no bus timing | wait-state knob (worklist B3) |
| MAX_WAITSTATES | 6AEE R/W | 9-10. 3:0 write wait states (reset Ch), 7:4 ROM_SPEED (reset Fh), 8 LINE_OPT, 9 IOR16_ENA, 10 PASSTHROUGH_OVERRIDE | bits 8 and 10 used; wait-state counts stored, no effect (no bus timing in 86Box) | knob (B3) |

### Engine control

| reg | port | guide | model | use |
|---|---|---|---|---|
| DP_CONFIG | CEEE W | 9-26..27. Read/write, POLY_FILL_MODE blit, READ_MODE, DRAW, MONO_SRC (3 = **VRAM blit source**), BG/FG source, DATA_WIDTH, LSB_FIRST | ok/card for MONO_SRC 3 (4 M8CONF shapes, from ATI's Win 3.11 driver) and FG sources 0/1/2/3/5; **poly-fill blit ok/card** (M8PF, 10-10: edges 10/20, 5/30, 10/20/30 fill left-inclusive, right-exclusive, flag toggling; card = bed) | **colour-expand from card memory**: glyphs and masks kept on the card as 1 bpp, expanded with no host data |
| EXT_FIFO_STATUS | 9AEE R | 9-28. One bit per FIFO entry, 16 | engine finishes at once: occupancy never shows (FIFO probe, 10-10) | driver sends up to 16 minus count |
| EXT_GE_CONFIG | 7AEE W | 9-29..31. 8-bit-slot layout puts the EEPROM lines in bits 0-2 and 7; 16-bit layout adds pixel width, DAC 8-bit | EEPROM path works in the bed (ATIM8 reads it) | - |
| GE_PITCH / GE_OFFSET | 76EE / 6EEE+72EE W | 9-20..21. Drawing pitch and 20-bit drawing base; Mach8 pitch resets as CRT_PITCH | ok | **draw into any off-screen page**: render page N+1 while N shows |
| LINEDRAW_OPT | A2EE R/W | 9-37..38. POLY_MODE, LAST_PEL_OFF, DIR_TYPE, OCTANT/DEGREE, 8 BOUNDS_RESET, 10:9 CLIP_MODE | implemented, incl. bounds reset and clip modes with an overrun count | hardware pre-clip (9-36 sample code) |

### Drawing operations (to 9-49)

| reg | port | guide | model | use |
|---|---|---|---|---|
| BOUNDS_L/T/R/B | 72EE / 76EE / 7AEE / 7EEE R | 9-48. Box around the **points written through LINEDRAW** only - not blits or fills | ok/card (TS2 sub-tests 17-23, clamped outside -512..1535) | bounding box of vector and polygon drawing without CPU work |
| CLIP_MODE pre-clip | A2EE 10:9 | 9-33..36. Trivial reject / accept / exception; CLIP_OVERRUN in EXT_GE_STATUS | implemented | the CPU clips only the rare exception line |
| Extended blit source | B2EE, BEEE, C2EE | 9-42..44. Source shape and direction independent of the destination; 32-byte source FIFO; SRC_X_START = SRC_X_END aborts | **ok/card** (M8TILE, 10-10, `docs/captures/2026-10-10_m8tile/`, card = bed): the source is one stream - at SRC_X_END it moves to SRC_X_START on the next source row, independent of the destination's rows | **unpack a linearly stored sprite into any rectangle in one blit** (sprites packed with no pitch waste in spare memory). Not a horizontal tiler: a narrow source does not repeat along a row |
| ALU_FG_FN / ALU_BG_FN | BAEE / B6EE W | 9-47. Mix codes as 8-24 | ok | arithmetic mixes - see chapter 8 row |
| BRES_COUNT | 96EE R/W | 9-49. Starts a raw Bresenham line; reads back MAJ_AXIS_PCNT | ok | - |

### Drawing operations (9-50 on)

| reg | port | guide | model | use |
|---|---|---|---|---|
| DEST_CMP_FN | EEEE W | 9-50. Codes 0-7; TRUE = pixel kept; unsigned; **only WRT_MASK-enabled planes compare** | ok/card on the ATI path. **Gap on 8514/A commands:** the card applies DEST_CMP_FN to a CMD fill (M8CMP 10-06, M8CMPA 10-10); the model ignores it there and writes every pixel (bed M8CMP 255/256 written vs card 32 or 223). Codes 40h-78h (the Graphics Ultra nibble test) ignore WRT_MASK on the card (M8CMPA); colour codes 08h-38h on CMDs unmeasured | colour key |
| DEST_X_START / X_END / Y_END | A6EE / AAEE / AEEE W | 9-44..46. Writing Y_END starts the blit; left inclusive, right exclusive; direction from the ends | ok/card (M8CONF) | all blits |
| EXT_SCISSOR_L/T/R/B | DAEE / DEEE / E2EE / E6EE W | 9-53..54. 12-bit signed, -2048..2047 | ok | - |
| EXT_SHORT_STROKE | C6EE W | 9-55. Two SSVs per 16-bit write; packed mono; colour patterns | implemented; not card-measured | glyph strokes |
| LINEDRAW / LINEDRAW_INDEX | FEEE / 9AEE W | 9-56..57. Index 0-3 draw, 4-5 move; `rep outsw` a polyline; index 4/5 grows the bounds without drawing | ok/card (TS2) | polylines at one 16-bit write per coordinate |
| PATT_DATA / PATT_DATA_INDEX / PATT_INDEX / PATT_LENGTH | 8EEE / 82EE / D6EE / D2EE | 9-58..60. 16 colour + mono pattern registers, linear patterns; 8x8 mono tiling is 68800-6 (Mach32) only | ok/card (M8ROW3, M8FG6-9) | dithers, line styles |
| SCAN_TO_X | CAEE W | 9-61. Span from CUR_X to the written X; fill-flag toggles for host polygon scan conversion; fast horizontal lines | ok/card: plain spans (18 M8CONF shapes) and the **fill flag** (M8PF: after CUR_X, successive SCAN_TO_X writes alternate draw and move). Model fixed in fork `a52d2f2ce`: M8PF bed = card; M8CONF 81/82, test 58 being the known inherited-colour case (`next_session_2026_10_10.md` 1e), unchanged with the flag disabled | **flat-shaded spans at about one write each** (3D plan); with the flag, a scanline's edge list is one write per edge |
| SRC_X_START / SRC_X_END / SRC_Y_DIR | B2EE / BEEE / C2EE W | 9-62..63 | implemented | see extended blit source above |
| R_SRC_X / R_SRC_Y | DAEE / DEEE R | 9-60..61. Source pointer read-back; indeterminate after a blit (32-byte source FIFO) | implemented | - |
| EXT_GE_STATUS | 62EE R | 9-68. CLIP_OVERRUN 3:0, CLIP_INSIDE, CLIP_FLAGS, GE_ACTIVE, EE_DATA_IN | implemented | pre-clip loop (9-36) |
| CONFIG_STATUS_1 | 12EE R | 9-64. Clock mode, **BUS_16**, EEPROM, DRAM/VRAM, memory installed, ROM location | **card 0xFE21 (10-10 COMrade, and `M8REGS_5160.BIN` 10-07), model was 0x0021**: low byte matched (clock chip, 8-bit bus, 1 MB VRAM). **Fixed in fork `a7b84dbe4`: bed now FE21** (`M8REGS_bed_cfg1.BIN`) | - |
| CONFIG_STATUS_2 | 16EE R | 9-66. **SHARE_CLOCK** (Mach8 shares the VGA's clock), HIRES_BOOT, **WRITE_PER_BIT** (fast write-masked ops) | **ok/card: 0046** (`M8REGS_5160.BIN`, 10-07, cold and warm; model 0046): **SHARE_CLOCK 0, WRITE_PER_BIT 0**. The 10-10 single-byte timeouts were the COMrade link | **the halves run off separate clocks**: same frequency possible, no phase lock - constrains worklist C3. No fast write-masked ops: plane layers cost full writes |

**VGA half, ATI extended registers A0h-BFh (1CEh/1CFh)**, `M8REGS` 10-07/10-08, card cold vs bed `ea1f70b5d`:
card `4d 1f 00 02 54 76 20 00 00 3d 06 80 03 10 00 00 12 00 00 00 00 08 00 6c 40 33 01 21 00 90 30 88`,
bed  `0d 00 00 02 00 00 20 00 01 17 06 00 00 00 00 00 12 00 00 00 00 08 00 88 40 33 01 21 00 90 30 88`.
Differ at A0, A1, A4, A5, A8, A9 (varies on the card too), AB, AC, AD, B7. Meanings from vgadoc `ATI.TXT`
(<https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vgadoc/ATI.TXT>, local `references/vgadoc/`):
**AB = 80 on the card: bit 0 video-memory zero-wait-state write, bit 1 BIOS zero-wait-state read, bit 5
(28800-6) zero wait state - all off**; bit 7 text-mode video data latch delay. AC 03: bit 0 linear addressing.
A4/A5: ROM pages. A8/A9: vertical line counter (why A9 varies). B7 6C: bit 0 clear = 8-bit ISA, bit 2 DRAM,
bit 3 EEPROM data, bit 5 I/O decode. A0 bits 0-3, A1 bits 3-4 (monitor detect), AD bit 4: undocumented here.
**Lead (worklist B2): AB bits 0 and 5 could cut the 2.0 us/byte VGA write cost** - measure with the M8BUS
write test, then soak for stability on the XT bus. ATI.TXT says nothing on how many waits each saves.

Mach32 only, absent from the Mach8 by design: hardware cursor (0AEE-1EEE, 3AEE/3EEE), overscan (62EE W,
66EE, 02EE-06EF), VERT_LINE_CNTR (CEEE R), MEM_BNDRY (42EE; the Graphics Ultra's halves have separate
memory), MEM_CFG aperture (5EEE), MISC_OPTIONS (36EE R/W), 8x8 mono patterns. Not checked: whether the
model gates each of these off for `mach8_vga_isa`.

**Card probes this chapter adds to worklist A2:** 16EE (timed out 10-10); the polygon-fill blit; SCAN_TO_X's fill flag; DEST_CMP_FN with WRT_MASK 0Fh on the ATI path. Colour-expand from VRAM and plain SCAN_TO_X are already = card through M8CONF.

