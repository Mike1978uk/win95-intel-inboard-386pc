# Mach8 VGA half: findings, 2026-11-06

Research and probe design only. Nothing in this note was run on a card or a VM. The 86Box source is not hardware authority.

## Summary

1. No memory path between the halves. Card probe M8XMEM: an engine fill left VGA A000:0-15 unchanged, the engine read its own row back, and a CPU write to A000 did not reach the engine (`docs/captures/2026-10-11_m8probes/README.md:5-8`). The halves have separate chips.
2. The only link is the shared DAC and the output switch (ADVFUNC 4AE8 bit 0, CLOCK_SEL 4AEE bit 0 PASS_THROUGH). Palette writes through either port set reach the same DAC. The switch carries pixels to the monitor only, and costs over 1 s of blank per switch.
3. "(%dK assigned to VGA)" in C-INFO.EXE is never printed on this card. Its gate is a ROM byte (C000:0043h = 34h, not 61h), so that branch is not taken. It is a Mach32 report line (section 2).
4. Open: whether a Mach8 split can be set (MEM_BNDRY 42EE, MEM_CFG 5EEE, MISC_OPTIONS 36EE, EEPROM word 05h); whether 28800 BDh bit 2, AB bits 3 and 6 or AC bit 6 change anything on the 8514 side; whether the VGA half can flip (no source says it cannot). Overlay or mixing of the two halves: UNKNOWN, no source.
5. Probes M8GRAD, M8DACX, M8BNDRY, M8LATCH, M8VFLIP and M8W128 are built and unrun (see Status).

## Levels and paths

Levels: DOCUMENTED = a source says it (cited). INFERRED = a reading of code or sources. UNKNOWN = no source.

Paths are relative to `C:\Users\lycet\RiderProjects\86Box-Inboard` unless absolute. `XT` = `C:\Users\lycet\OneDrive\Desktop\XT_project`. `guide.txt` = `references/ati_mach32/guide.txt`. `ATI.TXT` = `references/vgadoc/ATI.TXT`. Scratch files named as `scratch` were not kept.

## 1. Memory, registers and the pixel path

| Item | What the source says | Helper use | Level | Citation |
|---|---|---|---|---|
| Memory chips | VGA 4 x GM71C4256A = 512 KB; 8514 = 8 ZIP chips = 1 MB. Separate chips, separate memory. | None | DOCUMENTED (card) | `docs/mach8_card_coverage.md:276-278` |
| C-INFO "(%dK assigned to VGA)" | Resolved in section 2. The line is a Mach32 report and is not printed on this card. | None | DOCUMENTED | section 2 |
| MEM_BNDRY 42EE | W. bits 3:0 boundary in 256 KB pages, bit 4 enable: "VGA cannot access page >= / 8514 cannot access page <". Guide text is Mach32 ("memory boundary ... between VGA and accelerator"; "mach32 may re-configure ... dynamically"). | If live on Mach8 it would only partition; no cross access | DOCUMENTED for Mach32; Mach8: UNKNOWN. Card reads 0000. Repo notes say "Mach32 only; the Graphics Ultra's halves have separate memory" (assertion, no probe) | `references/ati_mach32/guide.txt:385-388, 12286-12330`; `docs/mach8_card_coverage.md:249-252`; `M8REGS_5160.BIN` |
| ATIM8 and 42EE | ATIM8 reads/saves 42EE (seg02 22B3), restores it (22DB, 2139), sets bit 4 (2513) then later writes 0. Restore at 2139 is skipped when chip flag [1BF]=1. [1BF]=2 is set after a path that reads 12EE bit 0 (Mach32's 8514_ONLY); [1BF]=1 after a 96EE=1555h/0AAAh test (TEST.COM-style Mach8 test). | Shows Microsoft/ATI treat it as Mach32 state | INFERRED (static) | `references/atim8_disasm/seg02.asm:2716-2718, 2883-2889, 2998-3000, 3015-3017, 3176-3182, 4002-4063`; `docs/atim8_port_census.md:45` ("init"). Tally "42EE bytes 2" is a bed run, not the card (`docs/captures/2026-10-07_w95_usage/atim8_800x600_session_tally.txt:18`) |
| MEM_CFG 5EEE | W. Mach32 aperture (1/4 MB linear or 64K paged). Needs VGA enabled. Guide: aperture unaffected by MEM_BNDRY. Card reads 0000. Card ROM has no 5EEE bytes at all. | Would be the bus-free-ish CPU window; M8APERT found only the VGA text page in A0000-BFFFF | Mach8: DOCUMENTED negative by probe | `guide.txt:12348-12380`; `docs/mach8_driver_method.md:117`; `docs/mach8_static_sweep_2026_10_31.md:50`; ROM byte scan (mine) |
| ATI "Virtual Aperture" | Win3.1 driver serves 4bpp modes on 512 KB boards via SUBSYS_STAT bit 7; MACH8.WRI: "The VGA area is used to provide aperture services by the driver". Our card has 1 MB. | A real VGA-window-to-8514-memory service exists in ATI's driver, but only for 512 KB boards, and still crosses the ISA bus | DOCUMENTED (how it is gated on 1 MB: not decoded) | `docs/mach8_driver_method.md:117`; `XT\ATI\ATIDESK\MACH8.WRI` |
| MISC_OPTIONS 36EE | Mach32: bit 4 DISABLE_VGA, bits 3:2 MEM_SIZE_ALIAS, bit 6 DISABLE_DAC, bits 10-11 block write. Mach8 name is FIFO_OPT, bits 0-1 only, write-only. Card ROM RMWs 36EE with `and 0FCh` (bits 0-1 only) at 7948h-795Bh and 7EDDh-7EF5h. Card reads 0000. | DISABLE_VGA would remove the VGA, not link | DOCUMENTED Mach32; Mach8 bits 2-15 UNKNOWN | `guide.txt:9850-9900`; `docs/mach8_guide_audit.md:29`; ROM disasm (mine, `roms/video/mach8/BIOS.BIN`) |
| CONFIG_STATUS_1 12EE | Mach8 layout: bit 3 EEPROM_ENA, 4 DRAM, 5-6 MEM_INSTALLED, 7 ROM_ENA, 8 ROM_PAGE_ENA, 9-15 ROM location. No VGA-memory field. Card FE21. | None | DOCUMENTED | `references/vgadoc/ATI.TXT:551-562`; `docs/mach8_card_coverage.md:234` |
| CONFIG_STATUS_2 16EE | bit 0 SHARE_CLOCK (Mach8 shares the VGA clock), 1 HIRES_BOOT, 2 EPROM_16, 3 WRITE_PER_BIT, 4 FLASH. Card 0046: SHARE_CLOCK 0, WRITE_PER_BIT 0 -> separate clocks. | Explains no phase lock | DOCUMENTED | `ATI.TXT:598-604`; `docs/mach8_card_coverage.md:235` |
| EEPROM (64 words) | Mach32 map (guide App. C) has word 05 "VGA Boundary: 0 Shared, 1 256K, 2 512K, 4 1MB", aperture location/size at 06h, ZERO_WSTATE bits. The Mach8 map is not in any local doc. Card words 00h-0Ch and 0Fh-10h undecoded (flags, monitor id, memory mode). Word 05h on the card = 020Ah, 06h = 0000, 02h bit 0 set (0919h) enables a ROM routine at 04C0h. | If a Mach8 split field exists it would be in the undecoded words; ROM does not use any such value that I found (ROM never writes 42EE/5EEE) | Mach32 DOCUMENTED; Mach8: UNKNOWN | `guide.txt:15086-15170`; `roms/video/mach8/eeprom_card_5160_2026_10_07.nvr` (my dump of words); `docs/mach8_static_sweep_2026_10_31.md:74-77`; `docs/mach8_card_vs_86box.md:8-30` |
| ADVFUNC_CNTL 4AE8 bit 0 / CLOCK_SEL 4AEE bit 0 PASS_THROUGH | "0 = Enabled - video output is VGA, driven by data from the features connector"; CLOCK_SEL[0] overwrites ADVFUNC[0]. 8-bit-wide connector: "will not pass through Hi Color or TrueColor". | This is the only inter-half pixel link: VGA pixels reach the RAMDAC through the 38800. A pure mux; no mixing documented. | DOCUMENTED (mux); mixing UNKNOWN | `guide.txt:7466-7473, 7495-7510, 9520-9530`; `docs/mach8_card_coverage.md:193` |
| Output switch cost | Black then white, no signal loss, over 1 s per switch; 8514 vsync ~6,400 polls/frame vs VGA ~5,020; not phase locked (switch-to-vsync delay 82-6748 polls) | Closes alternate-frame and raster-split ideas | DOCUMENTED, card, owner-watched (M8FLIP4) | `docs/mach8_levers.md:211-228`; `docs/captures/2026-10-11_m8probes/README.md:12-17` |
| Shared DAC | TEST.COM 797Eh writes A4h to 3C7 and expects 02EC to read A5h; model had to be fixed to match. Mach32 guide: "Writing to the VGA DAC register will also write the (logical) accelerator DAC" when VGA is enabled. | Palette writes by either port set reach the same table: palette effects are not tied to which half draws. Costs one ISA port write per byte on either set | DOCUMENTED (TEST.COM code on real card) | `docs/next_session_2026_10_09.md:105`; `docs/mach8_pr_draft.md:72-73`; `guide.txt:7455-7462` |
| MAX_WAITSTATES 6AEE bit 10 PASSTHROUGH_OVERRIDE | 1 = VGA picture not passed (clock line driven so the DAC can be programmed when the accelerator is in passthrough). Found as the 5160 black screen. | Not a link | DOCUMENTED | `docs/mach8_guide_audit.md:15`; `guide.txt:7475-7482` |
| Feature connector / J3 | 8514/Ultra (no VGA chip) has J3 "VGA Pass Through Connector". Graphics Ultra is the 28800 + 38800 combo; its on-board VGA-to-38800 wiring is not documented locally. | Cannot be used from software beyond the mux | DOCUMENTED (8514/Ultra); combo wiring UNKNOWN | `references/8514a_docs/ardent_ATI_8514_Ultra.html:291-313, 426` |
| Overlay / keying of VGA over 8514 | Only "VGA overscan output enable ... overlay input signal to the RAMDAC" (68800-6, B5 bit 7), Mach32 | None found for Mach8 | Mach8: UNKNOWN, no source | `guide.txt:6100-6125`; `ATI.TXT:276-290` |
| Hardware cursor | Mach32/64 only (0AEE-1EEE, 3AEE/3EEE). 28800 cursor registers (CRTC 0Ah/0Bh/0Eh/0Fh, A3 bit 3, B0 bit 2) are the text-mode cursor. Ferraro: "There is no hardware cursor capability in the ATI chip" (VGA side). | None for 8514 | DOCUMENTED | `ATI.TXT:536-550`; `docs/mach8_card_coverage.md:249`; Ferraro:~116960 |
| 28800 AB bit 3 "select secondary display", bit 6 "restrict CPU access"; AC bit 6 "1024x768x16 planar pass through internal palette"; BA bit 5 grey-scale | One-line descriptions only. AB bits 0/1/5 measured no gain. | AB3/AB6 could be a VGA-side switch; unprobed | DOCUMENTED text, effect UNKNOWN | `ATI.TXT:103-117, 122-125`; `docs/mach8_card_coverage.md:244-246` |
| 28800 B2/BE split read/write bank | BE bit 3 gives separate read and write banks; one-pass A000->A000 copy works; no time gain (bus crossed each way) | Saves code only | DOCUMENTED, card | `docs/mach8_levers.md:189` |
| 28800 latch copy / Mode X / write modes / set-reset | Standard VGA behaviour inside the VGA half; copies inside VGA memory cost one CPU write cycle (plus a read to load the latch). The 28800 has B0 bit 7 "higher bandwidth in VRAM", BF bit 7 "alt memory organisation", ABh bit 7 text latch delay. Cost on this bus unmeasured: M8VGAW built, unrun. | Only helps a VGA-half output (Mode X DirectDraw, text CT2). Nothing in VGA memory is visible to the engine | INFERRED (standard VGA); cost UNKNOWN | `docs/mach8_frame_rate_plan.md:31, 367-376`; `docs/worklist.md:316`; `ATI.TXT:119-149` |
| 28800 text mode (char generator, 8 simultaneous fonts B5 bit 4, line compare, split screen) | Hardware text rendering with no CPU/engine work; B5 bit 4 gives 8 fonts. | CT2 idea, VGA output only | DOCUMENTED regs; use UNKNOWN until timed | `ATI.TXT:279-284`; frame_rate_plan CT2 row (line 367) |
| Line counter A8/A9 | VGA CRTC raster; 11.5 us per A9 poll; counts the VGA's timing, which is not locked to the 8514 | A raster clock for the VGA half only; for the 8514 use DISP_STATUS LINE_SYNC / VSYNC | DOCUMENTED, card; applicability to 8514 INFERRED from no-phase-lock | `docs/mach8_levers.md:190`; `docs/mach8_card_coverage.md:24` |
| Both CRTCs at once | Both run: 8514 vsync polled at 6,400/frame while VGA displayed, VGA 3DA at 5,020 while 8514 was selected; different timings | They tick independently; nothing is shared except DAC and the output mux | DOCUMENTED, card (rates); concurrency of the 8514 CRTC while not selected INFERRED from the vsync counts | `docs/captures/2026-10-11_m8probes/README.md:12-17`; `docs/mach8_levers.md:225` |
| ROM API (64h/68h/6Ch/70h) | 64h accelerator mode load; 68h engine bring-up; 6Ch/70h `stc; retf`. Only TEST.COM and VVESA call it. Writes DISP_CNTL, ADVFUNC, EXT_GE_CONFIG, 26EE/76EE. No VGA-memory or split service. | None for linking | DOCUMENTED (disassembly) | `docs/mach8_levers.md:93-109` |
| VVESA | Writes B2/BE/AE banking, clock bits; calls ROM 68h. Nothing about the 8514 memory. | None | DOCUMENTED | `docs/mach8_levers.md:60-77` |
| Vogons review | Reviewer: VGA programs "at best might make use of the added DRAM on the VGA side"; "dual chipset, one for regular 512K VGA, other one with 1MB vram for windows". | Corroborates separate memories (an opinion, not a datasheet) | secondary source | `references/web_captures/vogons_t28392_p2_mach8_graphics_ultra.txt:77, 523` |

## 2. C-INFO and 8-INFO

Answer: neither tool shows a Mach8 memory split or any link between the halves.

### 2.1 C-INFO flag and the "assigned to VGA" line

- The line is printed as `256 << n`, with n from far fn 1DB:124 = `(index B0h >> 3) & 3` mapped 0->0 (256K), 1->2 (1M), 2->1 (512K). It is printed straight after "With %s of %s memory on board", where the size comes from `in 36EEh & 0Ch` (0=512K,1M,2M,4M) and DRAM/VRAM from `12EEh & 30h`. That whole branch is behind word flag [1512h]. When the flag is 0 (the 12EE bit-4 "Vantage/Ultra" path), it prints only "With 1M/512K/256K Video Memory installed" from the same B0h read. DOCUMENTED (disassembly of unpacked `XT\ATI\ATIMACH8\M8UTL\C-INFO.EXE`; EXEPACK-unpacked copy and listing `cinfo_fn.txt` kept in `references/mach8_static_2026_11_06/cinfo_fn.txt`). Our card: B0h = 12h -> 512K (`docs/captures/2026-10-07_m8txt/M8REGS_5160.BIN`, parsed). `docs/ati_readme_findings_2026_10_23.md:172` listed the string; no doc decoded it before.
- Flag caller, found: main function, image-segment offsets 1DEh-1E7h (`cinfo_fn.txt:1de-1e7`): `lcall 1F1h:00CFh` (image 1FDFh); `mov [1512h], ax`. The routine at 1F1h:00CFh reads the byte at C000:0043h (`lcall 2A2h:28h` with args 43h, C000h) and returns 1 only if it equals 61h, else 0 (scratch `cinfo_unpacked.bin`, file offset 1FDFh-200Fh). DOCUMENTED.
- The routine at 1F1h:0101h (image 2011h) runs only when the flag is already non-zero. It sets [1512h]=1 again and reads 8EEE/7AEE/62EE/66EE/12EE (Mach32 setup).
- ROM byte C000:0043h is the "gate revision": 31h=18800 ... 34h=28800-4 ... 'a' (61h)=68800 Mach32. DOCUMENTED: `references/vgadoc/ATI.TXT:1226-1231`.
- Our card's ROM has 34h at that offset: DOCUMENTED, `roms/video/mach8/BIOS.BIN` byte 43h (bytes 40h-43h are `31 11 34`; the ROM label is "761295520" at 31h).
- So on this card the flag is 0, the "(%dK assigned to VGA)" path is never taken, and the line is a Mach32 (68800) report. DOCUMENTED (disassembly + ROM byte + ATI.TXT). The `[1E0Eh]=='3'` gate that sets before it (image 134Dh) is INFERRED to test the first character of the "31" ID at C000:0040.
- Earlier reading, superseded: the flag-setting routine was first recorded as "caller not located" and the Mach32 branch condition as INFERRED. Both are settled by the caller above.

### 2.2 8-INFO.EXE

`XT\ATI\8514ut\8-INFO.EXE` (EXEPACK; unpacked by `unexe8.py` to scratch `info8_unpacked.bin`, listing `info8_fn.txt`, data base 3C70h). Memory configuration comes only from CONFIG_STATUS_1 12EEh: bit 4 DRAM/VRAM, bits 6:5 "512K" if clear else "1M" (`info8_fn.txt:46b-488`, strings "DRAM","VRAM","512K","1M"), bit 3 EEPROM, bit 1 bus width, passthrough from 16EEh word, ROM address from 52EEh/56EEh. Detection is a 5555h write to 96E8h and a read of 96EEh expecting 1555h (`info8_fn.txt:b5-d7`). No string or register for a VGA share, boundary, aperture or other-half memory. DOCUMENTED (disassembly).

Not contained in either tool: any path from one half to the other; any MEM_BNDRY 42EE or MEM_CFG 5EEE access in 8-INFO; the caller of anything that would set a split on a 38800-1.

## 3. VGA half: flip, start address, 128 KB window, latches

### 3.1 "VGA half cannot flip at all"

The recorded claim is in `docs/directdraw_research_2026_11_01.md:125` and `docs/mach8_unused_silicon_audit.md:18`. No source found states it, and sources say the 28800 has what a flip needs.

- 28800 has CRTC display start address 3D4h index 0Ch/0Dh plus extension: bit 16 = B0h bit 6, bit 17 = A3h bit 4 (28800+). DOCUMENTED: `references/vgadoc/ATI.TXT:84-89, 191-194`.
- B8h bit 2 (write-protect all VGA registers) explicitly exempts the Display Start Address and cursor registers, so ATI kept start address writable under a lock. DOCUMENTED: `ATI.TXT:322-324`.
- ACh bit 0 "Enable Linear Addressing" (28800-6+): `ATI.TXT:146-148`. ATI's VVESA.TXT: keyword PAN "Enable Linear Address: pan or scroll within the entire display memory. Standard VGA will wrap around at the end of 64K in planar mode. Disabled at default" (`XT\ATI\ATIMACH8\V3\VVESA.TXT:56-62`). DOCUMENTED. ATI documents whole-memory panning on this chip family as a switch that is OFF by default.
- Card probe M8LIN set ACh bit 0 and saw "changes nothing visible on this bus". It tested CPU-visible bank/window, not display start (`docs/captures/2026-10-11_m8probes/README.md:9-11`, `tools/m8seq/M8LIN.ASM:1-9`). DOCUMENTED. So ACh bit 0's effect on the display start address is UNKNOWN.
- Whether a start address change on this card takes effect at vertical retrace, and in which units in 256-colour modes: UNKNOWN. Ferraro says the start address covers a 64 KB span in 256-colour modes; ATI.TXT describes an 18-bit extension. These conflict; which governs a 28800-6 in 256-colour modes is UNKNOWN. No probe of 3D4h 0Ch/0Dh exists. M8PAGE (modified, unrun) is the 8514 CRT_OFFSET timing probe.
- Capacity, not capability, is the recorded reason elsewhere (`docs/worklist.md:247-249`: 2 x 307,200 B = 614,400 > 524,288). At 640x400x256 two pages (512,000 B) fit 512 KB; 320x200x256 fits 8 pages.
- Suggested probe (not run): in mode 13h or Mode X on the VGA half, write distinct colours to 2 pages, set 3D4h 0Ch/0Dh (+B0h bit 6, A3h bit 4), and watch; repeat with ACh bit 0 set. Owner watches. Hang risk low; restore on exit. Cost: one DOS program. Built as `M8VFLIP`, unrun.

### 3.2 BDh bit 2 and the 128 KB window

- BDh bit 2 "128K CPU Address. Enables A0000h-BFFFFh as one 128K page if set" (28800-5+): DOCUMENTED, `ATI.TXT:386-390`. Our chip is a 28800-6 (`docs/mach8_card_coverage.md:272`; AAh chip revision `docs/mach8_levers.md:119`). Card BDh = 90h (bit 2 clear), `M8REGS_5160.BIN`.
- The 128 KB window closure in `docs/mach8_frame_rate_plan.md` rests on M8LIN, which did not write BDh. M8LIN tested ACh bit 0 and B6h bit 2 plus GR06 map 00 (which on a plain VGA gives A000-BFFF). Grep of `tools/m8seq/M8LIN.ASM`, `M8BITS.ASM`, `M8VGA.ASM`: no BDh. DOCUMENTED. The closure has an untried register. Even if it works, 128 KB of window is still CPU bus traffic; the closed-list reason (bus crossing) is unaffected.
- The BDh bit 2 reading is in `ATI.TXT` but no doc says it was tried. Cheap to record as a probe (D7).
- BDh bit 2 is 28800-5+ text only. Brady cannot confirm it (see 3.3).

### 3.3 Books and manuals checked

Ferraro (`XT\ATI\Programmers Guide to the EGA VGA and Super VGA Cards 3rd Edition by Richard F Ferraro.txt`, 3.0 MB text capture; chapter 19 = lines 113369-120900):

| Finding | Level | Cite |
|---|---|---|
| Mach-8 "is a separate chip from the VGA controller ... the accelerator or the VGA are separate entities. One either programs to the VGA or the Mach-8. Some higher resolution modes are only supported by the Mach-8 and not the VGA portion" | DOCUMENTED (matches M8XMEM) | Ferraro:113412-113416 |
| 28800 display start address: the host cannot extend control past the 64 KB the CRTC 0Ch/0Dh pair covers; ATI loads extra bits through a macro. 28800 ver 2+: bit 16 in B0h bit 6, bit 17 in A3h bit 4. Mach8/Mach32 add bits 19:18 in index ADh bits 6:5 ("MACH-32 extension") | DOCUMENTED (OCR text; macros read as listings 19.12a-c) | Ferraro:116481-116500, 116599-116830, 116838 |
| 16-colour modes: start address points into a 256 KB page only; 256-colour modes: "64-Kbyte span of the start addresses covers only 64 Kbytes", start cannot exceed scan line 102 at 640 wide, 81 at 800 | DOCUMENTED text; conflicts with the 18-bit extension above; which governs a 28800-6 in 256-colour modes: UNKNOWN | Ferraro:116916-116975 |
| "There is no hardware cursor capability in the ATI chip" (VGA side) | DOCUMENTED | Ferraro:~116960 |
| Dual-bank read/write paging macros (28800 ver 2+, Mach8/Mach32 via B2h/BEh) | DOCUMENTED (already used) | Ferraro:115834-116440 |
| Chapter 19 engine sections 19.7-19.22: scissors, pre-clip, direct line, short stroke, Bresenham, BitBlt, pattern, pixel transfer, compare, mask, status | DOCUMENTED, same content as the Mach32 guide | Ferraro:117270-119900 |

Did not contain: latches, line compare/split screen, page flipping, register BDh, the 128 KB window, any 28800-specific extended register list beyond B0h/B2h/BEh/A3h/ADh/AEh, any statement that the VGA half cannot flip, anything on a Mach8 memory split, nothing on 42EE/5EEE. Chapter 17 (8514/A) was not read.

Brady (`XT\ATI\Advanced_Programmers_Guide_to_SuperVGAs.pdf`, text layer present; `pdftotext -layout` to scratch `brady.txt`, 32,826 lines). Chapter 11 (`brady.txt:14227-15000`) covers the ATI 18800 VGAWONDER only.
- Extended register table B0h-BEh with bit names (`brady.txt:14494-14700`). BDh = EGA switch settings only (D7-D4); D3-D0 "reserved" (`brady.txt:~14690`). So BDh bit 2 is a 28800-5+ addition; Brady cannot confirm it.
- Standard VGA facts (all 28800-compatible by INFERENCE, since the 28800 is a VGA): Start Address 0Ch/0Dh "to pan ... or move between display pages", and basis of split screen (`brady.txt:2461-2470`); Line Compare split screen (`brady.txt:2555-2565`); processor latches, Graphics Controller write mode 1 "use processor latches as write data" (`brady.txt:1864-1890, 2743-2790`); GC Misc register selects the 128 KB window A000-BFFF (`brady.txt:4503-4513`).
- B9h D7 "Lock Line Compare register" (18800): line compare exists in ATI's family (`brady.txt:~14660`).
- Did not contain: any 28800 register (A0h-BFh additions), AB/AC, BDh bit 2, anything on the 8514 side or 38800.

Ardent 8514 Ultra manuals (`references/8514a_docs/ardent/ATI_8514_Ultra_Manual_compact.pdf` 35 pp, `..._v2_1.pdf` 83 pp). No text layer. No Tesseract or Python OCR package installed. Used the OCR built into Windows (Windows.Media.Ocr via `powershell.exe` 5.1, scratch `ocr.ps1`), pages rendered at 130-150 dpi with PyMuPDF. OCR-grade, many misreads. Kept copies: `references/mach8_static_2026_11_06/ocr_compact.txt` and `ocr_v21.txt`. Both are the user/installation guide for the 8514/ULTRA (no VGA chip): jumpers JU3 memory size (512K/1 MB), JU4 compatibility, JU5-7 IRQ, VGA pass-through cable, Turbo AI, AutoCAD ADI driver, diagnostics (power-up test, register integrity test, video RAM test). One line: "If an application uses direct register writes, ensure that it also supports min mode if using a 512K board" (v2_1 OCR line 136). Did not contain: any register list, any VGA/8514 memory relationship (the card has no VGA), any 28800 text.

### 3.4 Latches, split screen, line compare

Standard VGA features (Brady, 3.3), so 28800 support is INFERRED, not shown on the card. ATI-specific: B0h bit 7 "Higher bandwidth in VRAM", BFh bit 7 alt memory organisation, ABh bit 7 text video latch delay (`ATI.TXT:119-149, 196`). Cost on the 8-bit bus: UNKNOWN; M8VGAW built, unrun. Nothing in VGA memory is visible to the engine (M8XMEM, `docs/captures/2026-10-11_m8probes/README.md:5-8`).

## 4. Closed list: why each VGA-related item was closed

Closed list: `docs/mach8_frame_rate_plan.md:209-220`.

| Closed item | Recorded reason | Does it cover the helper ideas in section 1? |
|---|---|---|
| Output switching between halves (C2, C3) | >1 s blank, owner watched; not phase locked (`docs/worklist.md:157-165`, `docs/mach8_levers.md:226-228`) | Covers any idea that alternates or splits the output. Does not cover the DAC being shared (palette effects) or both halves drawing without switching. |
| 28800 zero-wait bits AB 0/1/5 and the unknown 28800 bits | AB bits measured no difference (writes 2.18/1.96/1.86 us/byte, within 0.5%); limit is the Inboard/XT bus cycle (`docs/mach8_card_coverage.md:244-246`). "Unknown bits": no reason given for the unmapped ones in the closed list; `docs/mach8_levers.md:136-138` shows A0 bits 0-3, AF, BC and most B0/B1/B4/B5/BA/BB/BD/BF bits left unmapped, M8BITS (10-16) saw no visible effect or hang for A0, AD4, BC, AF. | The measured reason covers any VGA-side idea whose cost is the CPU's memory write cycle (it says the 28800 is not the limit). It does not cover the unmapped bits as a class. No recorded reason for AB bits 3 and 6, AC bit 6. |
| Split banking for speed | M8SPLIT: 27,114 vs 27,044 ticks per 4 KB; every word still crosses the bus once each way (`docs/mach8_levers.md:189`) | Covers VGA-to-VGA copies done through the CPU. Does not cover latch copies (data never crosses the bus); latch copy was not measured. |
| A 128 KB VGA window | M8LIN: B000 mirrors A000 with GR06 00; BEh bit 2 is the EGA DIP switch select, not a window; ACh bit 0 changes nothing (`docs/worklist.md:140`; `docs/captures/2026-10-11_m8probes/README.md:9-11`). BDh bit 2 "128K CPU address" (ATI.TXT:386-387) is the register the ATI text names for this; M8LIN did not set it. Card BDh = 90h. | Gap: the BDh bit 2 reading is in `ATI.TXT` but no doc says it was tried. Reopened for test (M8W128). |
| VGA-half 512 KB as a cache for the 8514 | The engine cannot read VGA memory, so data parked there crosses the bus twice (`docs/worklist.md:152-155`). | Covers any scheme where the engine consumes VGA data. Does not cover a VGA-half-only output. |
| VGA-half double buffering at 640x480 | 2 x 307,200 B does not fit 512 KB; the half flips at 640x400 and below (`docs/worklist.md:247-249`). The VGA half cannot flip at all per `docs/directdraw_research_2026_11_01.md:125` (no reason recorded there; the 28800 has display start address registers, 3D4 0Ch/0Dh with B0 bit 6 and A3 bit 4). | Capacity reason is solid. "Cannot flip at all" has no source (section 3.1). |
| MACH.DRV's "aperture" is the VGA A000 window, not 8514 memory | M8APERT found only the VGA text page (`docs/mach8_driver_method.md:117`) | Covers the aperture idea on a 1 MB board. |
| Memory aperture / MEM_CFG | Closed as "dead" by M8APERT; MEM_BNDRY listed Mach32-only "by design" (`docs/mach8_card_coverage.md:249-252`) with the note "Not checked: whether the model gates each of these off". No card probe wrote 42EE/5EEE. | A write test on the real card was not done; the closure of MEM_BNDRY is a reading of the Mach32 guide plus the ATIM8 chip-flag skip. Test: M8BNDRY. |
| DMA | Mach8 has no DREQ path (`docs/mach8_frame_rate_plan.md:173-175`) | Not relevant to the halves. |

## 5. Engine source selects

### 5.1 DP_CONFIG (CEEEh, write) fields

DOCUMENTED: `references/ati_mach32/guide.txt:9980-10061` (page header lists both "Mach 32" and "Mach 8" columns; no field is marked per chip except the footnote below).

| Bits | Name | Values |
|---|---|---|
| 0 | READ_WRITE | 0 read from trajectory, 1 write |
| 1 | POLY_FILL_MODE | 1 = polygon-fill blit: "blit source mechanism triggered, VRAM source blit data used as the polygon mask". Each source pixel ORed with inverted RD_MASK; all ones = outline pixel, flag toggles; flag cleared at the start of each row (`guide.txt:9990-9998`) |
| 2 | READ_MODE | 0 host colour data, 1 host monochrome data |
| 4 | DRAW | 1 enables draw |
| 6:5 | MONO_SRC | 0 always "1", 1 mono pattern register, 2 pixel transfer register (host), 3 VRAM blit source |
| 8:7 | BG_COLOR_SRC | 0 BKGD_COLOR, 1 FRGD_COLOR, 2 pixel transfer, 3 VRAM blit source |
| 9 | DATA_WIDTH | 0 = 8 bit, 1 = 16 bit (must be 1 in 16 bpp) |
| 12 | LSB_FIRST | (Mach32: honoured always; Mach8: ignored when DATA_WIDTH=0, footnote `guide.txt:10066`) |
| 15:13 | FG_COLOR_SRC | 0 BKGD_COLOR, 1 FRGD_COLOR, 2 pixel transfer (host), 3 VRAM blit source, 5 colour pattern shift register. 4, 6, 7 not in the guide |

Values 4, 6, 7 are measured on the Mach8 card, not documented: 4 = 4:1 coverage count (11h x population count); 6 = host byte indexes the 32-byte colour pattern as `((byte&7)<<2)|(x&1)` (bit 11 clear); 7 = the same from the VRAM source byte. `docs/captures/2026-10-07_m8txt/README.md:78-109`, `docs/ati_test_com_notes.md:139`, probes `tools/m8seq/M8FG6.ASM`-`M8FG9.ASM`, `M8SRC4*.ASM`. Bit 11 set (CA11h) shifts host data 4 pixels late: unexplained, OPEN.

Colour pattern is 32 bytes on the card (M8FG6.ASM header: "colour pattern holds 80h+n in byte n, n=0..31"); PATT_LENGTH D2EE = length-1 (bits 4:0), PATT_INDEX D6EE next pixel, PATT_DATA_INDEX 82EE then PATT_DATA 8EEE (16 colour words index 0-0Fh, mono words from 10h): `guide.txt:11262-11270, 11679-11760`. The pattern is a linear tiled run, not destination-aligned (`guide.txt:11268-11271`). The 8x8 mono tile and block write are 68800-6 only (`guide.txt:11700, 11745-11780`; already Closed).

Existing values in the repo: `drivers/mach8_display/src/m8.h:129-136`: 3251h host mono, 2211h plain, 2231h mono pattern, 5210h scan read, 6011h VRAM blit, 2213h polygon fill. Also seen in ATIM8: A211h colour pattern, 6211h blit, 2071h mono-from-VRAM (cursor mask; `docs/atim8_disassembly.md:103`), 4210/4215/5211 host data, 0007/0047 read paths (`docs/mach8_levers.md:151`).
Decoded: 2213h = FG src 1 (FRGD_COLOR), MONO_SRC 0, DATA_WIDTH 1, POLY_FILL, DRAW, WRITE. 6011h = FG src 3, 8-bit, DRAW, WRITE.

### 5.2 Answers

- (c) Colour from host data: FG_COLOR_SRC 2 (e.g. 4211h/5211h; documented; used by ATIM8). Costs one bus word per 2 pixels at 8 bpp. DOCUMENTED + card.
- (b) Colour from the 16-entry colour pattern: FG_COLOR_SRC 5 (A211h; documented, used by ATIM8, card-proven M8FG6-9). With a mono mask it is FG 5 + MONO_SRC 1/2/3: A231h, A251h/B251h, A271h. These combinations are allowed by the field table (fields are independent) but no probe or driver uses them: UNKNOWN on the card.
- (a) Colour from VRAM (FG 3) with the mask from somewhere else: field table allows MONO_SRC 1 (pattern: 6231h), 2 (host bits: 7251h with LSB_FIRST, or 6251h), 3 (6271h). With MONO_SRC 3 both colour and mask come from the SAME blit source pixel: guide says "If both COLOR_SRC_BLIT and MONO_SRC_BLIT are selected, then bit 7 of color will be replaced by the mono bit" (`guide.txt:9080-9081`, written for PIX_CNTL, so applying it to DP_CONFIG is INFERRED). There is only one VRAM source stream (extended blit: SRC_X_START B2EE, SRC_X_END BEEE, SRC_Y_DIR C2EE; `docs/mach8_card_coverage.md:216`), so mask and colour cannot be two separate VRAM rectangles: DOCUMENTED structure, no second-source register exists in the guide.
- 8514/A-native route (no DP_CONFIG), DOCUMENTED in the IBM-compatible set: FRGD_MIX bits 6:5 (FSS) 0 BKGD_COLOR, 1 FRGD_COLOR, 2 PC data, 3 display memory; BKGD_MIX bits 6:5 (BSS) same; PIX_CNTL (BEE8 index A) bits 7:6 MIXSEL 0 foreground mix for all pixels, 1 fixed pattern (PATTERN_L/H), 2 PC data expansion, 3 blit source as the mask (`references/8514a_docs/richter_smith_graphics_programming_8514a.txt:12303-12345, 12775-12797, 13051-13054`; XFree86 `references/xfree86_336_mach8/mach8/regmach8.h:260-264`). So FSS=3 (VRAM colour) + MIXSEL=1/2 (pattern/host mask) + BKGD_MIX = 03 "D" (leave destination) is a documented 8514/A combination. Card behaviour with FSS=3 and MIXSEL=1/2: UNKNOWN. XFree86 Mach8 uses DP_CONFIG only as 2211h (`mach8line.c:140`, `mach8seg.c:138`) and the mask-from-VRAM path via PIX_CNTL MIXSEL_EXPBLT with FRGD colour (`ibm8514/fc.c:123`, `blt.c:756`). No XFree86 file combines a VRAM colour source with another mask.
- Richter and Smith worked example for FSS=3 with MIXSEL=1/2: not searched.

### 5.3 Candidate sequences (all INFERRED from the tables above; none run; the first probe for each is the 16x16 test)

Common setup (as `m8d3d_tri`, `drivers/mach8_display/src/m8d3d.c:395-445`): outline drawn into a scratch rectangle with WRT_MASK = one plane P and mix replace; then the fill blit with RD_MASK = P.

A. Polygon filled with a stored gradient or texture, source = scratch that already holds it (one blit source, mask in plane P, colour in the other 7 bits).
 1. VRAM asset (gradient/texture, 7 bits, plane P always 0) blitted engine-side into the scratch rectangle: DP_CONFIG 6211h/6011h, FRGD_MIX/ALU_FG_FN replace, WRT_MASK FFh, extended blit source regs. No bus cost beyond ~10 register writes.
 2. WRT_MASK = P; FRGD_COLOR = P bit; draw edges (LINEDRAW or CMD) into the scratch as today.
 3. WRT_MASK = FFh (or ~P to keep the destination's plane P), RD_MASK = P (so a source pixel is an outline iff plane P is set), DP_CONFIG = **6213h** (FG src 3, DATA_WIDTH, POLY_FILL, DRAW, WRITE), ALU_FG_FN = 07h (replace), SRC_X_START/SRC_X_END/SRC_Y_DIR, CUR_X, DEST_X_START/END, CUR_Y, DEST_Y_END (starts). Expected: interior pixels take the source pixel's colour. Outline pixels carry plane P set (palette pairs or WRT_MASK ~P).
 Unknowns: (i) does FG 3 under POLY_FILL give the source pixel colour or something else; (ii) outline edge rule: type C includes left, excludes right edge (`guide.txt:6937-6939`, Type C column assumed from the table order); (iii) engine time for the extra copy. Test: 16x16 asset with a ramp, a square outline, read back with PIX_TRANS.

B. Polygon filled with a repeating pattern or short ramp, no asset:
 1. 82EEh = 0; 16 words to 8EEEh (32-byte colour pattern); D2EEh = n-1; D6EEh = phase.
 2. Outline into scratch as today (plane P).
 3. WRT_MASK FFh, RD_MASK P, DP_CONFIG = **A213h** (FG src 5 + poly fill), blit as in A step 3 with the source being the scratch.
 Expected (INFERRED): the pattern advances along the row and repeats every n pixels, same on each row (ATIM8 reloads the pattern per scanline for 8x8 brushes: `docs/atim8_disassembly.md:90`; reset-per-row behaviour is UNKNOWN). So it gives a horizontal ramp of up to 32 pixels per repeat; a vertical gradient needs a reload (about 17 writes) per band. Test: A213h on a rectangle first, read back two rows.

C. Polygon with a coarse shade lookup (INFERRED): DP_CONFIG **E213h** (FG src 7: VRAM source byte indexes colour pattern `((byte&7)<<2)|(x&1)`), RD_MASK = 80h so plane 7 is the outline flag and bits 2:0 are an 8-level shade; the pattern's 16 words map shade to any 8 colour pairs (2-pixel dither). Both uses of the same source byte are disjoint bits. Untested combination; relabels a texture's colours with 5-17 writes.

D. Mask-and-colour in one source without poly fill (stored sprite/texture with a polygon-shaped mask baked in): DP_CONFIG **6271h** (FG 3, MONO_SRC 3), RD_MASK = mask plane, BKGD_MIX = 03 (leave) so mask-0 pixels leave the destination. Guide: bit 7 of colour replaced by mono bit (`guide.txt:9080`). Compare with the existing card-proven colour-key blit (92EE/EAEE, BKGD_MIX leave: `docs/captures/2026-10-07_m8txt/README.md:55-63`), which does the same without a mask plane.

E. Mask from host bits, colour from VRAM texture: DP_CONFIG **7251h**, host sends 1 bit/pixel (16 px per word; 3251h with FG 1 is card-proven by M8EXP/M8DPM). Bus cost ~ w*h/16 words, so only for shapes not made of lines.

F. Stipple through a texture (screen-door transparency): DP_CONFIG **6231h** with PATT_DATA_INDEX 10h mono pattern words (2231h is proven), colour from VRAM; BKGD_MIX leave. UNKNOWN.

Choose A or D for textures, B for flat repeating ramps; probe cost is one DOS program each. None need the VGA half.

### 5.4 WRITE_PER_BIT and masked-write facts (behind V17)

- CONFIG_STATUS_2 (16EEh read) bit 3 WRITE_PER_BIT: "For VRAM configuration supporting internal plane masks: 0 = Faster write-masked operation not supported, 1 = supported". DOCUMENTED: `guide.txt:12013-12035` (page header "Mach8"), `ATI.TXT:598-604` ("Write masked VRAM operations supported if set"). Card reads 0046h: bit 3 clear. DOCUMENTED: `docs/mach8_card_coverage.md:235`, `M8REGS_5160.BIN`. Read plainly, the guide says masked writes still work and are not the fast path; it gives no speed figure. Masked-write cost: UNKNOWN (no probe; `docs/mach8_frame_rate_plan.md:203,468`).
- WRT_MASK: planes cleared are protected; masked-out planes "do not participate in any arithmetic ALU operations or destination colour compares" (`guide.txt:9393-9396`); "all unmasked planes are treated as if they were contiguous. During add and subtract operations carries or borrows entering a masked bit are passed through" (`guide.txt:11284-11290`, with the equivalent formulae at 11291-11298; Mach32-chapter text, header names Mach8 for ALU_BG_FN). Card-proven that DEST_CMP_FN masks by WRT_MASK (M8CMP8, `docs/mach8_card_coverage.md:90-95`); WRT_MASK/RD_MASK itself ok/card (TS2, M8BLRD, `docs/mach8_card_coverage.md:71`). RD_MASK is rotated left one bit in IBM monochrome reads but WRT_MASK is not (`docs/mach8_frame_rate_plan.md:203`).
- Polygon fill types: A (PIX_CNTL XXXXX10X) outline detect by RD_MASK, planes where both masks are high are cleared; B uses WRT_MASK as outline and fill planes; C (DP_CONFIG bit 1) reads the outline from the blit source (`guide.txt:8688-8712, 6930-6950`). The driver's use of RD_MASK = one plane with WRT_MASK FFh is type C, card-proven by M8PF (`docs/mach8_poly_fill_design_2026_11_01.md:44`).
- V17 (shade/hue split: low 4 bits shade, high 4 hue, fill with WRT_MASK F0h) is consistent with the guide: a masked fill touches only the enabled planes. The cost question is the 0046h bit above.

### 5.5 8514-side features not in the plan's lever tables

1. Field-confined arithmetic. Because masked planes drop out of the ALU and unmasked ones act as one contiguous number (`guide.txt:11284-11290`), saturating add/subtract and average (mixes 17h, 18h, 1Bh; `guide.txt:8240-8260`) can be applied to only the shade field of a pixel, leaving the hue bits alone, in one engine pass with WRT_MASK set. Plan O9/G10/V17 list the mixes and the plane-mask fill, not this combination. Use: in-engine lighting/fade of an already-drawn textured region with a shade+hue palette layout (V17), no bus bytes. Mach8 applicability to a field narrower than 8 bits: UNKNOWN (M8BLEND/M8MIX covered full-byte mixes).
2. Independent colour and mask sources (section 5.2). The plan lists S4/S5/S7 and the DP_CONFIG working set, not the full field matrix. BG_COLOR_SRC (bits 8:7) is not discussed anywhere in the plan or levers. A pixel can choose between two different sources (e.g. VRAM texture for mono 1 and a register colour for mono 0 with MONO_SRC pattern/host) in one pass. UNKNOWN on the card.
3. VGA-half display start address and linear addressing (3.1): not on the Closed list in that form (only "double buffering at 640x480" is closed, for capacity).
4. Checked and not new: VERT_LINE_CNTR CEEEh is Mach32 only (header lists Mach 32 only, `guide.txt:9697-9702`); EXT_FIFO_STATUS, bounds accumulator, scissor hit test, 92EE source compare, DEST_CMP_FN, hardware pan (CRT_OFFSET) and interrupts are in the plan (S2, S3, S4, O9, O13, G1, P4, P7).

Not found: any Mach8-only register in the guide's register index (`guide.txt:13500-13640`) that the lever tables lack; any 38800-specific document in the local tree beyond the Mach32 guide, ATI.TXT, Ferraro, Richter and Smith, and the XFree86 sources.

## 6. Probes: status and design

Each probe is a DOS program in the `tools/m8seq` style, read-only first. None run. Owner asks first per project rules. Each result is compared with the bed where the bed models the register, but the bed is not authority.

| ID | Name | Status | Notes |
|---|---|---|---|
| D1 | C-INFO caller and 8-INFO strings | DONE (static, section 2) | Result: no Mach8 split, no link. C-INFO line is Mach32-only on this card. |
| D2 | M8BNDRY: MEM_BNDRY live test | Built as `M8BNDRY`, unrun | With 8514 enabled as M8XMEM does (engine fill, read-back by PIX_TRANS), write 42EE = 10h \| 01h (enabled, boundary 1 page), fill a rectangle that straddles 256 KB (rows around y=256 at pitch 1024) and read it back; then 42EE = 00h and repeat; restore 42EE = 00h at the end (ATIM8 ends with 0). Result: fill above boundary blocked only with the register set -> register live on Mach8 (and a split exists); identical both ways -> no effect (the Mach32 guide wording has no meaning on this chip). The test can fail: both outcomes are reachable and the "no effect" arm has a control (the same fill with the register clear). Hang risk: low; worst case partition leaves the engine unable to draw until 42EE is cleared, so the program clears it in an exit handler. Owner need not watch the screen. Also record the read-back (card reads 0000 now). |
| D3 | M8VGAW: VGA memory write cost | Built, unrun (`docs/mach8_frame_rate_plan.md:31`) | No new code. Supplies the 1.96 us/byte baseline that D4 needs. |
| D4 | M8LATCH: VGA latch copy cost | Built as `M8LATCH`, unrun | In a planar mode on the VGA half (mode 0Dh or Mode X), copy 16 KB within VGA memory with write mode 1 (read a byte to load the four latches, write a byte to store four planes), and compare with a plain CPU word copy of the same 16 KB, timed by the PIT as M8SPLIT did. Data check: fill source with a pattern, read destination back afterwards. Result: if latch copy is much cheaper per byte moved -> an in-VGA-memory move helps a VGA-half output (Mode X, text fonts); if not, the CPU address cycle dominates and the idea joins split banking in the closed list. Can fail: the pattern check or the timing ratio. Hang risk: none beyond a wrong mode. Owner needs to watch only if the mode is displayed; run with the screen showing whatever the mode draws and nothing depends on it. |
| D5 | M8DACSH: shared DAC cross-check beyond TEST.COM | Built as `M8DACX` (name mapping INFERRED from the names), unrun | Write 4 palette entries through 3C8/3C9 while the 8514 side is displayed and read them back through 02EC/02ED; then the reverse. Also time a 256-entry load through each port set. Result: same table and same cost -> palette effects can use either set at no difference; a difference in cost or visibility -> pick the cheaper set. Fails visibly: read-back mismatch. Owner watching is optional but helpful for a visible palette change (colour bars on the 8514 side change when written by VGA ports). Hang risk: none known; 02EA-02ED writes are also in ATIM8. |
| D6 | M8PTH: pass-through behaviour | Not built | With ADVFUNC bit 0 set (8514 displayed) draw a bar pattern on the 8514 side; leave the VGA in a graphics mode with a different pattern; toggle only `PASS_THROUGH` mid-test and note whether anything appears except one or the other (a mix, flicker, overscan). Needs the owner at the screen. This is the only probe that can find an undocumented overlay; it can fail by showing only the single-source picture, which is the expected result. Hang risk: the monitor re-sync gap of over a second at each switch (`docs/mach8_levers.md:225`); keep the switch count low, restore ADVFUNC bit 0 and CLOCK_SEL bit 0 at exit. Do not write CLOCK_SEL 0 carelessly (M8FLIP v1 left the screen black). |
| D7 | M8BD2: BDh bit 2 window | Built as `M8W128`, unrun (name mapping INFERRED) | Set bit 2, write a signature at B000:0 and A000:0 and read both back, restore. Result: B000 now independent of A000 -> the 128 KB map exists after all and the "128 KB window" closure has a gap; B000 still mirrors A000 -> closure confirmed with the right register. Can fail both ways. Hang risk: B000 is also the mono text page; restore BDh = 90h at exit. Not an inter-half link; listed because the closure did not record this register. |
| D8 | M8AB3: AB bits 3 and 6, AC bit 6 | Not built | Read-modify-write one bit at a time with the state saved and restored after 1 s; after each, check VGA readback (write/read A000), 3DA toggling, and 02E8 vsync. Result: any change in 8514-side behaviour (SENSE, vsync, engine draw) would be a link; no change -> record in one line. Can fail: the control arm is the unmodified state. Hang risk: highest of the list (AB 6 may block CPU access to VGA memory, locking out the test code in an ISA machine with no recovery except reset; the 5160 hang rule in the project applies: say "it's hung, can you restart"). Owner watching required. Run last; do not combine with D6. |

Other probes named in this note: M8GRAD (V25, engine fill colour sources, built, unrun); M8FLIP / M8PAGE (V10 family, see `docs/mach8_frame_rate_plan.md`). Not proposed: writing 36EE bit 4 (DISABLE_VGA on Mach32). Value is only to remove the VGA, not to link, and the downside is a blank machine.

## 7. Sources

Read in full or by cited lines:
- `docs/mach8_levers.md` (whole), `docs/mach8_frame_rate_plan.md` lines 120-260 and 180-225, 331-346, 438-478, `docs/worklist.md` lines 140-175 and 228-262 (greps elsewhere), `docs/ati_readme_findings_2026_10_23.md:160-211`, `docs/mach8_card_coverage.md` (greps and 240-290), `docs/mach8_card_vs_86box.md:8-50`, `docs/mach8_static_sweep_2026_10_31.md:28-95`, `docs/atim8_port_census.md:30-60`, `docs/mach8_guide_audit.md:10-45`, `docs/mach8_graphics_vision.md:60-90`, `docs/captures/2026-10-11_m8probes/README.md`, `docs/mach8_driver_method.md:117` (grep), `docs/mach8_poly_fill_design_2026_11_01.md:1-60`, `docs/atim8_disassembly.md:84-103`, `docs/ati_test_com_notes.md:139`.
- `references/ati_mach32/guide.txt` (Mach32 guide with Mach8 columns; greps and the passages cited). `references/vgadoc/ATI.TXT` lines 40-445 and 549-606, 1205-1245 (https://pdos.csail.mit.edu/6.828/2018/readings/hardware/vgadoc/ATI.TXT; local copy). `references/8514a_docs/ardent_ATI_8514_Ultra.html`. `references/atim8_disasm/seg02.asm`. `references/web_captures/vogons_t28392_p2_mach8_graphics_ultra.txt`. `references/8514a_docs/richter_smith_graphics_programming_8514a.txt` (cited lines). `references/xfree86_336_mach8/` (regmach8.h, mach8line.c, mach8seg.c, ibm8514 blt.c/fc.c; grep found only `outw(CLOCK_SEL, 0); /* reset pass-through */` at line 378 of the XFree86 Mach8 source, no memory-split code).
- `roms/video/mach8/BIOS.BIN` (byte scan and disassembly of 36EE writes; bytes 30h-60h), `roms/video/mach8/eeprom_card_5160_2026_10_07.nvr` (words dumped), `docs/captures/2026-10-07_m8txt/M8REGS_5160.BIN` (parsed: B0=12h, BD=90h, BE=30h, 12EE=FE21, 16EE=0046, 36EE/42EE/5EEE=0000), `tools/m8seq/M8REGS.ASM`.
- `XT\ATI\ATIMACH8\M8UTL\C-INFO.EXE` and `XT\ATI\8514ut\8-INFO.EXE`: EXEPACK-unpacked and disassembled with capstone (data segment base 4EA0h for C-INFO, confirmed by string references; base 3C70h for 8-INFO). `XT\ATI\ATIDESK\MACH8.WRI` (aperture sentence). `XT\ATI\ATIMACH8\V3\VVESA.TXT:48-80`.
- `XT\ATI\Programmers Guide to the EGA VGA and Super VGA Cards 3rd Edition by Richard F Ferraro.txt` (lines 113369-120900 and greps to 120900); chapter 17 (8514/A) not read.
- `XT\ATI\Advanced_Programmers_Guide_to_SuperVGAs.pdf` (text via pdftotext; ATI chapter, GC/CRTC register sections).
- `references/8514a_docs/ardent/*.pdf` (Windows OCR, section 3.3).
- `tools/m8seq/M8FG6-9, M8LIN, M8EXP, M8DPM .ASM`; `drivers/mach8_display/src/m8.h:129-136`, `m8d3d.c:395-445`.
- Web: one search only (WebSearch result list; pages not opened). https://ardent-tool.com/video/ATI_8514_Ultra.html, https://retronn.de/imports/hwgal/hw_graphics_card_ATI_GRAPHICS_ULTRA.html and https://vgamuseum.info/index.php/home/item/82-ati-38800-1-mach-8-graphics-ultra-8514-ultra-graphics-vantage turned up. The search summary said separate VGA memory and listed 28800 + 38800, 512 KB + 1 MB; it did not give DAC wiring. The ardent page content was read locally instead.

Did not contain what was asked:
- The Mach32 guide has no Mach8-specific text on 42EE, 5EEE, or on how the Graphics Ultra wires the 28800 to the 38800.
- ATI.TXT says nothing on 42EE/5EEE and does not describe a Mach8 memory split.
- The 8514 Ultra manuals in `references/8514a_docs/ardent/` are image scans with no text layer (per `docs/mach8_static_sweep_2026_10_31.md`); OCR output only (section 3.3).
- No source states that the VGA half cannot flip.

Not found or not read:
- A Mach8-specific EEPROM map (Mach32 map only is in the guide; Mach8 words 00h-0Ch and 0Fh-10h undecoded).
- The `XT\ATI\Programmers Guide ...` capture and `Advanced_Programmers_Guide_to_SuperVGAs.pdf` named in `docs/resources_and_sources.md:207-214`: the Brady PDF was read; the capture was not searched this session.
- The ATI Mach8 programmer's guide as a standalone Mach8 document; the local `guide.txt` is the Mach32 guide.
- 86Box `vid_ati_mach8.c` was only grepped for register presence (42EE, 5EEE, 36EE handlers at ~4706-4830, 5966-6065). Not used as evidence of hardware.

## 8. Reference copies

Kept in `references/mach8_static_2026_11_06/` (text, under 200 KB each):
- `ocr_compact.txt`: Windows OCR of `references/8514a_docs/ardent/ATI_8514_Ultra_Manual_compact.pdf`. OCR-grade, many misreads.
- `ocr_v21.txt`: Windows OCR of the v2_1 manual. OCR-grade, many misreads.
- `cinfo_fn.txt`: C-INFO.EXE function listing (section 2.1).

Not kept (scratch only): `cinfo_unpacked.bin`, `info8_unpacked.bin`, `info8_fn.txt`, `brady.txt`, the OCR PNG renders, and the unpacking scripts.

`references/` is not in `.gitignore`: `git check-ignore` returns no match for the new folder. Files are committed as plain text.

## Status

- Built, in `tools/m8seq`, on the card in `C:\M8P` with `GO.BAT`; not run: M8GRAD (V25), M8DACX (D5), M8BNDRY (D2, V29), M8LATCH (D4, V28), M8VFLIP (V28), M8W128 (D7, V28).
- Decoder: `tools/m8seq/m8vga_decode.py`.
- Not built: D6 (M8PTH), D8 (M8AB3).
- Static work done: D1 (section 2). No card or VM run for any item in this note.
- The 86Box source is not hardware authority. `vid_ati_mach8.c` was grepped for register presence only.
- Lever rows in `docs/mach8_frame_rate_plan.md` (V25-V29) and the Closed-list reopen line point here.
