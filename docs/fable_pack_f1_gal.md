# Fable pack F1: ronnyroy111/inboard386 GAL/PAL and netlist read

Source: https://github.com/ronnyroy111/inboard386 , commit
`c51d5df36437b1c5cdf91be750312a7f68a50bc3` (2026-10-29), shallow-cloned to a new temp folder, read only.
The gitignored `references/inboard386_ronnyroy/` is at `3ff366b` (2026-09-08); `logic/` and
`inboard386/` are identical between the two (only `prom/` and `bracket/` differ).
Paths below are repo-relative. `pcb:` = `inboard386/inboard386.kicad_pcb`, `sch:` = `.kicad_sch`.

## 1. Already recorded here

The repo is known: `docs/bus_optimisation_plan.md:523-530,634`, `docs/contributor_input_ledger.md:25,105,349`,
`docs/next_session_2026_09_21b.md:243`, `docs/next_session_2026_10_05.md:55`,
`docs/fable_pack_f1_flush.md:95` (HLDA hit only). Memory: `session_2026_10_05_results.md`,
`shadram_5160_no_pages_2026_10_03.md`. Earlier work read `U71.pld` (memory decode) and the 670h
bit; no earlier note checks the cache pins.

## 2. Files

- `logic/*.pld` (CUPL, 16 files: U70 U71 U74-U77 U84 U86 U87 U91 U99-U102, plus `U71_modified`,
  `U87_modified`), matching `.jed`, `*_NNNNNN.JED` raw dumps and `.txt`. `logic/U98_301317.bin` PROM.
- `inboard386/inboard386.kicad_sch` (121k lines), `.kicad_pcb`, `parts_list.xlsx`, `bom/ibom.html`, photos.
- `prom/` PROM adapter. `logic/README.md`: "Dumps of all PAL chips and PROM U98. Logic equations for all PAL chips."
- Root `README.md`: "the schematic still needs cleanup and further reverse engineering of the programmable
  logic to completely understand."

## 3. Pins on the 386 socket (U73), from the PCB netlist

No pin table exists in a readme; read from pad nets in `pcb:` (U73 footprint starts at line 59577).

| Pin | Pad | Driven by / net | File:line |
|---|---|---|---|
| FLUSH# | E13 | net `unconnected-(U73D-~{FLUSH}-PadE13)`: no other pad | pcb:60496; sch symbol pin 18222 |
| KEN# | B12 | `unconnected-(U73D-~{KEN}-PadB12)`: no other pad | pcb:60145; sch:18168 |
| A20M# | F13 | `unconnected-(U73D-~{A20M}-PadF13)` | pcb:60574; sch:18240 |
| HLDA | M14 | `unconnected-(U73C-HLDA-PadM14)` | pcb:61159; sch:17949 |
| HOLD | D14 | net `HOLD`: only U73 D14 and RP11 pad 4 (SIP pack, value "100", other pad on GND) | pcb:60431, pcb:29704; RP11 value in `pcb` property "Value" "100" |
| SUSP# / SUSPA# | A4 / B4 | unconnected | pcb:59859, 60041 |
| AHOLD, BOFF#, EADS# | none | no such pin or net name in `pcb` or `sch` (grep) | - |
| ADS# | E14 | net `~{ADS}` to U74 p7, U75 p3, U76 p2, 387 socket U85 K7 | pcb:60509, 22612, 41661, 54249, 33583 |
| M/IO# | A12 | net `M/~{IO}` to U102 p3, U77 p8, U84 p1, U97 p14, U85 L6 | pcb:59963, 50388, 56046, 67669, 31775 |
| D/C# | A11 | net `D/~{C}` to U77 p9, U84 p2 | pcb:59950, 56059, 67697 |
| W/R# | B10 | net `W/~{R}` to U99 p9, U71 p10, U100 p3, U91 p5, U76 p8, U84 p3, 387 U85 K4 | pcb:60119 and others |
| READY# | G13 | net `~{READY}` to U74 p8, U76 p3, U85 K8, U59 p8 (output) | pcb:60652, 66432 |

Unconnected means the Inboard leaves the pin to the socket's own pull-up/down. The 386DX has no
FLUSH#/KEN#/A20M# pins; the symbol is a combined 386/486 PGA footprint. Which pin on the BL3
module's own socket arrangement maps to what is not in this repo.

## 4. Equations touching M/IO#, D/C#, W/R#

Only the Inboard-side PALs read these. Verbatim (`logic/`):

U84 (`logic/U84.pld`, G16V8; pins M_IO=1, D_C=2, W_R=3, A31=4):
```
36: !_CPU_MEMRW = (M_IO & !W_R) #
37:        (M_IO & D_C);
39: !o15 = (!M_IO & D_C & !A31 & !A10 & !A9 & !A8 & !A7 & A5);
41: !o16 = (!M_IO & D_C & W_R & A10 & A9 & !A8 & !A7 & A6 & A5) #         110011xxxxx   660-67F
42:        (!M_IO & !D_C & !W_R & !i13) #
43:        (!M_IO & D_C & A31) #
44:        (M_IO & !D_C & W_R);
46: !o17 = (!M_IO & !D_C & !W_R);
48: !o18 = (!M_IO & D_C & A31 & i14);
50: !o19 = (!M_IO & D_C & W_R & !A31 & !A10 & !A9 & !A8 & A7 & !A6);
```
Line 41 is the 660-67F (670h) I/O write decode (comment is the source's). Where o15-o19 go was not traced.

U77 (`logic/U77.pld`, bus-status encode, M_IO=8, D_C=9):
```
62: !_S0 = (i2 & !i4 & !_S0) #
63:        (!i1 & !D_C & i11) #
64:        (!i1 & D_C & !i11) #
65:        (!i1 & !_S0);
67: !_S2 = (!i1 & M_IO & !D_C & !i11) #
68:        (i2 & !i4 & !_S2) #
69:        (!i1 & !M_IO & i11) #
70:        (!i1 & !M_IO & D_C) #
71:        (!i1 & !_S2);
73: !_S1 = (i2 & !i4 & i11) #
74:        (!i1 & i11);
```
These make 8088-style S0-S2 for the XT bus from the 386 cycle.

U102 (`logic/U102.pld`, M_IO=3, M_IO_L=12; `_CLK88`, `AEN`):
```
44: !rf15.D = (!AEN);
57: !o19 = (M_IO_L & !rf15 & !rf16 & !rf17 & !rf18) #
58:        (M_IO & !rf15) #
59:        (i9 & !rf15 & !rf16 & !rf17 & !rf18);
```
AEN (DMA address enable) gates o19 through rf15.

U100 (`logic/U100.pld`), I/O address decode on A6/A2/A1/A0 and W_R, pins named 8088 ALE:
```
51: !o17 = (!i2 & !W_R & !A1 & !A0 & A2 & A6);
53: !o18 = (!i1 & !i2 & W_R & 8088_ALE & !A1 & !A0 & !A2 & A6);
55: !o19 = (!i2 & W_R & 8088_ALE & !A1 & A0 & !A2 & A6);
```

U71 (`logic/U71.pld`): memory decode. DACK0 (XT refresh DMA) appears at lines 45, 75, 80:
`(DACK0 & !MAENA_BA)` forces RAS enables. `ROMCACHE` at 60, 65, 68, 72. No FLUSH, KEN, HOLD or A20M term.

`grep -niE 'flush|ahold|hlda|boff|eads|a20m|ken'` over `logic/*.pld` returns nothing.

## 5. What the source shows for the I/O flush

- The Inboard does not drive FLUSH#, KEN#, A20M# or HLDA: all four socket pads are unconnected.
- HOLD goes to a 100 ohm SIP-pack pad on GND and to nothing else; no net carries a DMA/refresh hold request
  to the CPU, and no PAL output is named HOLD.
- No PAL equation or net called AHOLD, BOFF# or EADS# exists. The socket has no such pins in this footprint.
- M/IO#, D/C#, W/R#, ADS# reach only Inboard-side PALs, the 387 socket and the 8088 cable logic. No path from
  them to a cache-control pad was found on U73.
- The 0x660-0x67F decode (U84 o16) and 0x670 ROMCACHE bit are I/O decodes toward the Inboard's own registers.
- This is the Inboard board only. The BL3 module's own logic (SW1, "flush on DMA + I/O") is not in this repo.

## 6. Not in the repo

- Any schematic of the IBM 486BL3 module, its PALs, or its mapping of Inboard socket pins to 486 pins.
- Meaning of o15-o19 destinations in U84/U102/U100; full net tracing of the hand-drawn schematic (README says
  it still needs cleanup). I traced pad nets only, not schematic wires.
- Any equation or timing text on cache, snoop or XTOUT. No PDFs, no `.eqn`/WinCUPL project beyond `.pld`.
- 8088 socket cable (U85-adjacent `8088_Cable` symbol, sch:19592) HOLD/RQ-GT mapping: HOLD net has no cable pin
  in the PCB net list, but the cable symbol pin names were not read.
- Confirmation of the SW1 switch behaviour.

Also: https://forum.theretroweb.com/t/intel-inboard-386-pc-accelerator-clone/44 (linked from the README) was not read.
