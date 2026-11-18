# Indy 500 and Flight Simulator 3.0: renderers and driver files (desk RE, 2026-11-18)

Static reading only. Nothing was run. Owner's files copied to the session scratchpad
(`...\scratchpad\re\`); the games folder was not modified. Tools: Python 3 + Capstone 5.0.7
(already listed in `docs/host_tools.md`; nothing installed), plus three small scripts written for
this (EXEPACK unpacker, Indy LZSS resource extractor, string dump). Offsets below are in the
**unpacked** load image unless marked as a file offset.

## 1. Files and versions

| Folder | Title / version | Files | Packing |
|---|---|---|---|
| `GAMES\INDY500` | Papyrus "Indianapolis 500: The Simulation", (C) 1989 Papyrus Design Group, EA. No version string | `INDY.EXE` 159593 (md5 `1d2a43fe…`), `INDY.1` 160700, `INDY.2` 245201, `REPLAY`, `SETTING.1-3`, `DEFSET.1-3` | `INDY.EXE` EXEPACK (`RB`, real CS:IP 0000:0700, image 0x300C0, 318 relocs in the EXEPACK table). `INDY.1`/`.2`: resource archives (below) |
| `GAMES\msflt\program` | Microsoft Flight Simulator **3.00** (string "FLIGHT SIMULATOR V3.00"), files dated May 1988 | `FS3.EXE` 83285 (md5 `37901fca…`), 8 drivers `*.GRA`, `README.DOC`, lessons `_L*.*` | `FS3.EXE` EXEPACK (real CS:IP 0000:0000, image 0x15840, 68 relocs). `.GRA` not packed |
| `GAMES\FLTSIM` | Microsoft Flight Simulator **2.1x** booter run by a third-party "FS DOS LOADER VER. 1.02 (c) 1986,1987 L. Dale Rhoton" (`FILE_ID.DIZ` says v2.1, 1984) | `FS.EXE` 6917 (loader), `FS001.DAT` 39936 (the program image), `FS002-034.DAT` scenery | Not packed. CGA only (B800 references), no driver files. **Not analysed further** |

## 2. Indianapolis 500

### 2.1 Video modes and driver selection

| Item | Finding |
|---|---|
| Modes | Switches `/V /E /C[1|2] /T /M` (+ `/A /P /R /S`, not graphics) parsed at 0000:0741; else autodetect order VGA (int 10h 1A00h), EGA (12h/10h), Tandy (F000:FFFE=FF + C000:0=21h), MCGA (1A00h), CGA (CRTC probe). Mode letter is passed to the driver init in AH ('V','E','T','M','C') |
| Driver location | One driver at a time in **segment 0CE4h, offset 000Eh**, called far. Space to DS (1488h): 0x7A32 bytes |
| VGA / EGA | Driver **built into `INDY.EXE`** (image 0xCE4E-0x1487F). Mode 0Dh (320x200x16) for both; VGA also loads the DAC (int 10h 1012h) |
| Tandy / MCGA / CGA | `INDY.EXE` 0000:0869 loads resource **AX=0501h = `INDY.1` entry 5** over the built-in one (0x795B bytes). Modes 8, 4, 6 (MCGA = mode 4 with a DAC palette). Same jump table |
| Dev leftover | If DS:[0273h] bit 0 is set, the built-in driver is written to a file `GFXDRVR.CGA` and the program exits (0000:0712). Nothing found that sets the bit |
| Resource format (`INDY.1`, `INDY.2`) | Header = dword offsets, entry count+1 (INDY.1: 33, INDY.2: 160). Each entry = dword unpacked length + Okumura LZSS (N=4096, F=18, threshold 2, ring pre-filled with spaces, r=0FEEh, flag bit 1 = literal). Loader 0000:0139/0192, decoder 0000:CC07 |
| `INDY.1` entries 1-4 | Code with jump tables, 0xC92/0xE40/0x52E/0xC61 bytes; probably sound drivers (switches A/P/R/S). Not confirmed |

So **the patch target can be a driver file**: replace `INDY.1` entry 5 (store-only LZSS = flag byte FFh + 8 literals, repeated) and start the game with `/M` (passes on any VGA) or `/T`. No change to `INDY.EXE` needed.

### 2.2 Driver interface (segment 0CE4h, far calls, game DS=1488h, registers in, no stack args)

Jump table at 0CE4:000E, 42 entries, 3 bytes apart. Calls counted in `INDY.EXE` (`9A xx xx E4 0C`).

| Entry | Calls | Target (built-in) | Function |
|---|---|---|---|
| 0E | 1 | 008C | Init: AH = mode letter; saves BIOS mode, sets mode, palette, viewport; pages front A000h [DS:14h], back A200h [DS:12h], offscreen A600h |
| 11 | 1 | 052A | Shutdown, restore BIOS mode |
| 14 | 8 | 059A | First call: time vsync, patch itself to 05D5 = "if flip requested: CRTC start = back page, swap [12h]/[14h]; wait vsync" |
| 17 | 3 | 060D | Wait end of vertical retrace |
| 1A | 11 | 0616 | Request flip (DS:[265h]=1) |
| 1D | 8 | 061D | Wait flip done, then restore static part (cockpit) from A600h by latch copy (write mode 1) |
| 20 | 2 | 066D | Clear back page |
| **23** | 38 | 0682 | **Set colour**: AH = colour 0-15 (GC set/reset) |
| **26** | 15 | 0689 | **Move to** BX=x, AX=y (3D window coordinates; row 0 = screen row 24) |
| **29** | 35 | 06B5 | **Line to** BX=x, AX=y (Bresenham, OR into planes) |
| **2C** | 3 | 0A3E | **Span fill, solid**: rows DS:[24h]..DS:[26h]; per row y: x-left = word at DS:[28h]+2y, x-right = word at DS:[2Ah]+2y (arrays at DS:2Ch and DS:124h, 124 rows) |
| 2F / 32 / 35 | 8 / - / 10 | 0AD7 / 0AF5 / 0AFA | Text: set position (AX col, BX text row), advance, put char AL (font in driver at 6220h) |
| 38 / 3B | 2 / 1 | 0C1A / 0C9A | Not determined (palette triplets cycling; table-driven fill from row 24) |
| 3E | 1 | 0DD1 | Clear 3D window (rows 24-147, 0x9B0 words) |
| 41 | 1 | 0E0B | Set pattern / dither colours |
| 44 / 47 | 1 / 1 | 0E6D / 0E70 | Span fill, two-colour pattern (55AAh checker / AL,AH) - same row arrays |
| 4A | 1 | 0F2D | Span fill, pattern from 41 |
| 4D-59, 5C, 62, 65, 68-7A | 1-5 each | various | 2D: latch copies from A600h or between pages ([14h]->[12h]); cockpit, mirrors, gauges. Individual meaning not determined |
| 5F | 27 | 28C5 | Instrument / HUD multi-function (AX = sub-function) |
| 7D / 80 / 83 / 86 | 4 / 6 / - / 1 | 450C / 4513 / 451A / 4581 | Load a picture resource (calls back into the game's loader through the saved game CS) to A600h and show it |
| 89 | 4 | 486A | Screen set-up by index AX (clear, palette) |

Frame: draw into back page in EGA memory (A000h/A200h, 40 bytes a row), static cockpit kept in A600h and latch-copied back after each flip; flip = CRTC start address at vsync. 3D window 320x124 at row 24 (DS:1Eh=140h width, DS:18h=7Ch height, centre DS:20h/1Ah).

### 2.3 Polygon path in the game code (segment 0, near)

| Routine | Role |
|---|---|
| 0000:6320 / 62FA | Vertex fetch by index (BP): transforms the 3D point at [BP+2..6] against the camera (DS:9924h..) on first use, caches a 0Eh-byte record (screen x at +0Ah, y at +0Ch, outcode at +08h) |
| 0000:6BCC | Quad list: per poly colour -> entry 23, 4 vertices, edges -> 70E5, fill -> entry 2C |
| 0000:6D9E | N-gon list (word count/colour, then per poly: vertex count + indices) -> 70E5 per edge -> entry 2C |
| 0000:6E18 / 6E80 / 6EE8 | Same with entries 41+4A (pattern), alternate vertex source 62FA, entries 44/47 |
| 0000:70E5 | Edge walker: dispatch on the two outcodes (tables DS:2214h, 2234h, 2254h); clips to the 3D window, writes x into the left/right arrays, tracks min/max row in DS:[24h]/[26h] |
| 0000:6B30 | Clipped line list (outline) -> entries 26/29 |

Polygons are flat colour (16 colours) or 2-colour patterns. Convex is implied by the one-left/one-right array; not proven.

## 3. Flight Simulator 3.0

### 3.1 Video modes and driver selection

Startup menu, 14 options (key scan E1h-EEh = A-N), table at FS3 image 0xCF43: (variant word, file name).

| Option | File | Variant | Mode (int 10h in the file) | Device res. (DS:9A0h x 9A2h) |
|---|---|---|---|---|
| A, B, D | CGA2.GRA | 0, 1, 2 | 6 | 320x200 (sic, set by the file) |
| C | CGA1.GRA | 0 | 4 | 320x200 |
| E, F | MONO.GRA | 0, 1 | Hercules | 720x348 or 640x350 |
| G, I | EGB1.GRA | 0, 1 | 0Dh | 320x200 |
| H, M | EGA1.GRA | 0, 1 | 10h | 640x350 |
| J | PS21.GRA | 1 | 13h | 320x200x256 (README: "option J") |
| K | INCLR.GRA | 0 | InColor | 720x348 |
| L | TGA1.GRA | 0 | 9 | 320x200 |
| **N** | **CUSTOM.GRA** | 0 | - (not shipped) | - |

**FS3 already has a slot for a third-party driver file, `CUSTOM.GRA`, menu option N.** That is the patch target: write a new `CUSTOM.GRA`; no change to `FS3.EXE`.

### 3.2 GRA format and interface

| Item | Finding |
|---|---|
| Load | `FS3.EXE` 0000:CC1F: read word 0 = file length-2, allocate (len/16+1) paragraphs, read the rest to seg:0002. Code is ORG 0 in its own segment. No checksum seen |
| Header | +0 length word, +2 `JMP init`, +5 `RETF` (MONO/INCLR: a second `JMP`), +6 dword = video segments (e.g. A000h,A000h) |
| Init | Far call. Writes 17 far pointers into FS3's DS at **047Eh-04BEh** (4 bytes each) and DS:[A46h] (edge-list buffer offset in the GRA). Sets device size DS:9A0h/9A2h and virtual->device scale DS:B56h/B58h (x: 8192 virtual units, y: 6144), copies colour tables to DS:2AEh and DS:B6Ch. Calls FS3 services through DS:[BD2h], [BDEh], error exit [BDAh] |
| All 8 shipped GRAs | Same 17 vector slots (checked by parsing each init) |

Vectors (behaviour read from PS21.GRA; call counts in FS3.EXE):

| Slot | Calls | PS21 target | Function |
|---|---|---|---|
| 47E | 3 | 0D03 | **Present**: copy back-buffer rows (DS:216h..21Ah, scaled) RAM -> A000h; draw/erase overlay records list at DS:1001h/1002h (9-byte records) |
| 482 / 48A | 8 / 1 | 140B / 140E | Fill rows BX..SI with colour DS:[2E2h] (window clear, sky/ground) |
| 486 | 1 | 1716 | Tilted horizon fill: two colours DS:[A3Ah]/[A3Ch], slope from CX,DI,SI,BX |
| 48E | 2 | 10E8 | Set video mode and palette |
| 492 / 496 | 2 / 7 | 0DF3 / 0DFB | Draw target = screen / RAM back buffer |
| 49A | 1 | 4532 | Not determined (patches scale constants into its own code) |
| 49E | 5 | 14FA | Text glyph at virtual coordinates (BX, CX) |
| **4A2** | 4 | 1608 | **Line**, device coordinates (SI,BP)-(BX,DI) per the 3D caller at 0000:BF05, colour DS:[9A6h] |
| 4A6 | 2 | 16F2 | Pixel, colour DS:[9A6h] |
| **4AA** | 1 | 1C6F | **Polygon vertex**: DX=x, CX=y, AX=z **in camera space**; the GRA divides (x*[CB0h]/z+[CB2h], y*[CB4h]/z+[CB6h]), builds 16.16 edge records in its buffer, counts in DS:[A48h], bbox DS:402h-40Ch |
| **4AE** | 1 | 1D19 | **Polygon close and fill** (scanline from the edge list, flat colour; colour source probably DS:[2E2h], not confirmed) |
| 4B2 | 1 | 1FF6 | Not determined (draws around the window centre; maybe a marker) |
| 4B6 | 2 | 2161 | Read pixel and compare (CF result) |
| 4BA | 3 | 217E | Shutdown: mode 2 |
| 4BE | 1 | 2192 | `RETF` in PS21 |

3D callers: projection + clip in FS3 0000:BD2D-C7C2; lines at 0000:BF05 (`CALL FAR [4A2h]`), polygon vertices at 0000:C638 (`[4AAh]`), close at 0000:C74A (`[4AEh]`). FS3 is mostly lines with some filled polygons; counts per frame not measured.

Frame: PS21/CGA/TGA allocate a RAM back buffer (PS21: 0FA1h paragraphs = 320x200 bytes) and present with 47E; EGA1/EGB1 allocate none (draw in video memory; page use not read).

Resolution is a GRA parameter: the exe already runs at 640x350 and 720x348. A 640x480 `CUSTOM.GRA` would set 9A0h=280h, 9A2h=1E0h, B56h=1400h, B58h=1400h. **Not determined:** where the projection constants DS:CB0h-CB6h come from (a 1Dh-word window record copied at 0000:1C17 from a far pointer ES:[BX+12h]); whether any window layout in FS3.EXE assumes 350 rows or less.

## 4. Patch plan

| | Indy 500 | FS 3.0 |
|---|---|---|
| Replaceable driver file | Yes: `INDY.1` entry 5, chosen by `/M` or `/T`. Max 0x7A32 bytes (past that the load overwrites DS 1488h) | Yes: `CUSTOM.GRA`, option N. Size limit not found (allocated from DOS) |
| Calls to implement | 42 entries; a large share is 2D latch copies and A600h artwork | 17 vectors; only 47E, 482/48A, 486, 48E, 492/496, 49E, 4A2, 4A6, 4AA/4AE, 4BA needed for flight |
| Primitive shape | Already rasterised to per-row x-left/x-right arrays before the driver sees it (entry 2C). Merge rows with constant slope into `m8_trap`; or hook game 6BCC/6D9E for vertex lists (needs an exe patch) | Vertex lists in camera space (4AA/4AE) and device-space lines (4A2): maps straight to `m8_poly` / `m8_line` |
| Resolution | 320x124 3D window, 16 colours: scale x2, y2.4 | Native 640x480 possible if the exe tolerates it |
| 2D / cockpit | Pictures in A600h, latch-copied every frame: needs card offscreen memory + engine blit (not in `m8dos.h`) | Panel and text through 49E/4A2/4A6 and row fills; RAM buffer present (47E) |

16-bit constraint (both): drivers are real-mode far-called code with the game's DS; DOS/32A is not usable inside them. Needs a 16-bit port of the m8dos queue (as Stunts option B), either inside the driver file (Indy: size-limited) or in a small TSR the driver far-calls through an interrupt vector (keeps the Indy driver small).

Display switching: the 8514 side replaces the VGA picture, so in either game everything shown in flight (cockpit, text) must be drawn through the card.

**Recommendation: FS 3.0 first.** It has an official empty driver slot, a 17-call interface, polygons arrive as vertex lists, and resolution is already a driver parameter.

First concrete step (FS3): a logging `CUSTOM.GRA` = copy of `EGA1.GRA` or `PS21.GRA` with each of the 17 vectors redirected through a stub that counts calls (and vertices per polygon) into a buffer, dumped to a file at 4BA. Run under DOSBox or the bed, one flight of the default demo. That gives per-frame lines/polygons/vertices and confirms the register conventions above before any card code. Cost: one small assembler file, no exe patch.

Indy equivalent, later: same counter in a copy of the `INDY.1` entry 5 driver, repacked store-only, started with `/M`.

## 5. Not determined

- Indy entries 38, 3B, 4D-5C, 62-7A individually; `INDY.1` entries 1-4 purpose; whether polygons are always convex.
- FS3: source of DS:CB0h-CB6h; vector 49A and 4B2; polygon colour source; EGA1 page handling; any exe assumption about rows; per-frame primitive counts (both games).
- Any version number for Indy 500.
- Legal: both are commercial titles the owner says he owns; a private driver file built for his own copy needs no redistribution of game code. A patched driver derived from Microsoft's or Papyrus's binary should not be published.

## Sources

| Source | Gave | Did not contain |
|---|---|---|
| Owner's copies of the game folders (above) | Everything in this file | Documentation of the GRA or Indy driver interfaces (none shipped; FS3 `README.DOC` only names display options for PS/2, EGA, Hercules, Tandy) |
| `tools/dos3d/m8dos.h`, `docs/stunts_m8_patch_plan_2026_11_18.md` | Target API, answer shape, 16-bit option B | - |
