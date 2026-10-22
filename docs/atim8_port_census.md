# ATIM8.DRV port census (static, 2026-10-22)

Source: `references/atim8_disasm/seg*.asm`, all 18 code segments. Nothing run on the card. Script: scratch `census.py`, not in the repo.

## Method and limits

- A port is taken from the last `mov $imm,%dx`; the value from the last `mov $imm,%ax/%al` or `xor ax,ax` before the `out`. The tracker forgets on calls and on writes to ax/dx, and ignores branches, so a value reached by a jump is read as unknown.
- Most coordinate and colour writes load from variables, so their value is unknown. Constant values are reliable for the mode and config registers, which is where the interest is.
- The Mach32 guide (`references/ati_mach32/guide.txt`) is the only bit reference. "Reserved" means the guide lists the bit as reserved or does not describe it.

## DP_CONFIG (CEEE): every constant ATIM8 writes

Bits: 0 write, 1 poly fill, 2 mono host data, 3 reserved, 4 draw, 6:5 mono src (0 one, 1 pattern, 2 PIX_TRANS, 3 VRAM), 8:7 background src, 9 16-bit, 12 LSB first, 15:13 foreground src (0 BKGD, 1 FRGD, 2 PIX_TRANS, 3 VRAM, 5 colour pattern).

| Value | Decode | First site | What draws | Our driver |
|---|---|---|---|---|
| 2211 | fill, FRGD, 16-bit | seg02:2854, seg01:168d | solid fill, opaque rect | yes (M8_DP_8514) |
| 2231 | mono pattern picks FRGD/BKGD | seg08:03b4 | hatch, dashed lines | yes |
| 3251 | mono host data, LSB first | seg01:0e08, 1749 | text, mono bitmap | yes |
| 5210 / 5211 | FG src PIX_TRANS, read / write, LSB first | seg04:04ff / seg01:0871 | screen read, mem to screen | read yes (5210); write no (we use 2211 plus host mix) |
| 6211 | FG src VRAM, 16-bit | seg01:1009, seg15:3755 | screen to screen, brush replicate | we use 6011 (bit 9 clear) |
| 2071 | VRAM mono source, FRGD, 8-bit | seg01:1107 | cursor mask (mix C) | no |
| 4010 / 4011 | FG src PIX_TRANS, 8-bit, read / write | seg06:013a, seg11:088d | 4bpp and 1bpp helpers | no |
| 4210 / 4215 | PIX_TRANS 16-bit read / mono-mode write | seg02:28d9, seg07:006c | GetPixel / SetPixel | no |
| A211 | FG src colour pattern shift reg | seg08:14dd, seg15:43cb | 8x8 colour brush | no |

Reserved bit 3 and poly-fill bit 1 are never set. Background source (bits 8:7) is 0 in every constant: ATIM8 never takes the background from FRGD, PIX_TRANS or VRAM. We do not either.

## Other ports

| Port | Register | Values seen (example) | Notes | Driver |
|---|---|---|---|---|
| D2EE | PATT_LENGTH | 0, 7, 1f, 4107 (seg08:0d64), 411f (seg08:04fe), 6107 (seg08:0cec) | 4107/6107 known no-ops; 411f is the same class. Bit 7 (8x8 tile) never set; coverage doc says Mach32 only | 7, 1f |
| A2EE | LINEDRAW_OPT | 070C (seg08:01e8) | bit 2 LAST_PEL_OFF, bit 3 not described, bit 8 BOUNDS_RESET, 10:9 = 3 patterned-line clip exception | writes 0004 |
| C2EE | SRC_Y_DIR | 0, 1, variable (seg11) | documented | 1 |
| B6EE / BAEE | ALU BG / FG fn | 1, 2, 3, 5, 7, C | all in the 8514 mix table; C is S AND D (cursor, seg01:1138) | 2, 7, 3, 5 |
| B6E8 / BAE8 | mix, 8514 form | 07, 27, 47 | 27 = replace from FRGD, 47 = from host | host form yes |
| BEE8 | MULTIFUNC | A000 only | PIX_CNTL | yes |
| 42E8 | SUBSYS_CNTL | 900F, 500F | reset / run | yes |
| 9AE8 | CMD | 40B3 once (seg02:327c) | only CMD write in the driver | yes |
| 8EE8 / 96E8 | SRC_X / BRES_COUNT | 5555, AAAA (seg02:2f4e) | init memory test pattern | n/a |
| CAEE | SCAN_TO_X | 20, 100, variable | the constants are inits | yes |
| 6AEE | MAX_WAITSTATES | read, `or 0100`, write (seg02:1eb2-1ebc) | bit 8 LINE_OPT_ENA = 1 turns horizontal linedraw optimisation off, for 8514-exact ERR_TERM | no |
| 36EE | FIFO_OPT / MISC_OPTIONS | variable from INI (seg02:1ec4, 1ed2) | BlockWrite and similar | no |
| 42EE | MEM_BNDRY | byte, variable / 0 | init | no |
| 12EE, 16EE, 32EE | config status | 12EE read 2x, write FFFF / variable | CONFIG_STATUS_1 use at seg01:0a83 | no |
| 62EE | EXT_GE_STATUS | 38 reads | idle poll, bit 13 | no |
| 52EE, 56EE | scratch | few reads, one write each | | 56EE only (tap) |
| 26EE, 2AEE, 2EEE, 6EEE, 72EE, 76EE | CRT pitch/offset, GE offset/pitch | 80, 0 (seg02:1efb-1f0f) | init to 1024 pitch | 76EE, 6EEE, 72EE |
| 0AEE, 0EEE, 1AEE/1AEF, 1EEE/1EEF, 3AEE, 3EEE | cursor offset / colour | 1AEF FF or 0F (4bpp), 1EEF 20 (seg01:0b35) | Mach32 hardware cursor; absent on Mach8 per `mach8_card_coverage.md:249`. Gate not traced; probably a chip-ID branch | no |
| 02EA, 02EC | 8514/A DAC mask / write index | 0F (4bpp) | not in the Mach32 guide | no |
| 03C0, 03C6-8 | VGA attribute / DAC | 00, 20 | VGA | n/a |
| 9AEE (read) | EXT_FIFO_STATUS | 312 reads | | yes |

No `out` with a known port to: 22E8, 4AE8, 46E8, 5AEE, 7AEE, 92EE, EAEE, EEEE, 9EE8, C6EE (the last five agree with `atim8_disassembly.md` section 9; EEEE's use in `mach8_levers.md:19` is not seen here).

## Findings that change what we believed

1. `docs/mach8_levers.md` lists "DP_CONFIG mono expansion from display memory" as used by no ATI driver. ATIM8 uses 2071 (seg01:1107) for the cursor mask. The glyph-cache use is still not in ATIM8.
2. ATIM8 never enables the hardware 8x8 tile (D2EE bit 7); it loops per scanline. That fits "Mach32 only", so no lever there.
3. The 8-bit-width host forms (4010/4011/4215) are ATIM8's only drawing use of DATA_WIDTH=0.

## Worth a card probe

Ranked by expected gain on a bench gap. Each is a hypothesis from the guide text; none is verified.

1. **LINEDRAW_OPT 070C instead of 0004** (bit 8 BOUNDS_RESET, 10:9 clip mode 3). Used in ATIM8 polylines (seg08:01e8), which draw with LINEDRAW and CAEE. Hardware clip exception could reject or accept a segment with no CPU clip. Helps: lines. Uncertain: modelled only (`card_coverage.md:215`); bit 3 is undescribed; reading the overrun count back (62EE) costs a port read.
2. **SetPixel through PIX_TRANS, DP_CONFIG 4215** (seg07:0066-00a7). Writes: CEEE, BAEE, B6EE=1, CUR_Y, CUR_X, CAEE = CUR_X+1, then one word to E2E8 with the colour in the high byte. No FRGD_COLOR write, no start-register write. Helps: ellipse, polygon and floodfill only if their cost is per-pixel plotting. Uncertain: saves one or two writes; wins only when the colour changes per pixel.
3. **2071: mono from VRAM, 8-bit width, mix C** (ATIM8 cursor). Same family as the planned glyph-cache path. Helps: text_varied, mono_bmp once glyphs sit in off-screen memory. Uncertain: ATIM8 uses it for one 32x32 mask, so it says nothing about speed.
4. **A211, colour pattern shift register** (FG src 5; seg08:14dd, seg15:43cb). ATIM8 feeds 8 pattern rows, then copies with 6211. Helps: rectangle, fill_small with dithered brushes; polygon with patterns. Uncertain: behaviour is already measured (M8FG6-9); the open question is cost per row.
5. **6211 against our 6011** for screen to screen. Helps: blit_small. Uncertain: bit 9 changes the CPU data width, not the VRAM copy; probably equal.
6. **6AEE bit 8 and the 36EE value.** ATIM8 sets the first; the second comes from INI. Helps: lines, for ERR_TERM exactness rather than speed. Low value.

## Not worth probing

- 411f, 6107, 4107: same class as the known no-ops.
- Hardware cursor writes (1AEE, 1EEE, 3AEE): Mach32 branch.
- Background source bits 8:7: unused by ATIM8 and by us, a cheap probe, but no bench gap names a use. Mono bitmap over existing pixels is the only fit and mix 3 on BKGD already does it.
- stretch, dib_8bpp, mono_bmp, text_varied: ATIM8's methods there are host-side (generated stub, FIFO budget). The census finds no register they use that we do not.
