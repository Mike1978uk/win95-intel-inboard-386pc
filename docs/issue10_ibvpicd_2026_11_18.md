# #10: what Intel's IBVPICD.386 changes, and whether our Windows 95 VPICD matches it

Desk read, 2026-11-18. No hardware, no beds. Context: `docs/issue10_old_bios_2026_09_27.md` sections 5, 14-18.

## Files compared

| file | what | md5 |
|---|---|---|
| `XT_project/inboard_files/IBVPICD.386` | Intel, "Win386 VPICD Device (Version 3.02)", 6,219 bytes; same file on the Intel Windows 3.0 disk 10 (SZDD) and in both 3.11 installs | `c28ddb27` |
| VPICD inside `Microsoft Windows 3.0 [Inboard 386]/disk9/WIN386.EXE` (SZDD, expanded) | Microsoft's stock 3.0 VPICD, the base Intel edited | WIN386 `f6a2e5e2` (packed) |
| VPICD inside `95_3.11/Windows_311/WINDOWS/SYSTEM/WIN386.EXE` | stock WfW 3.11 VPICD | - |
| `vxd-patches/VPICD_INBOARD_IRQ2.VXD` | our Windows 95 VPICD (slave neutered + #42 IRQ 2) | `6c304f8c` |

Method: LE objects loaded with fixups applied; capstone linear sweep; stock 3.0 and IBVPICD aligned instruction by
instruction. IBVPICD offsets below are **file** offsets (file = linked + 600h for both objects). Stock VPICD offsets are
linked offsets in its LE image. The other `IB*.386` do not touch ports 20h/21h; only IBVKD (61h/60h) and IBVDMAD (DMA) do I/O.

## What IBVPICD changes against stock 3.0

IBVPICD is stock 3.0 VPICD cut down to one 8259. Nothing else in it is new code.

| # | change | IBVPICD (file) | stock 3.0 (linked) |
|---|---|---|---|
| C1 | **ICW1 `13h`: single mode, no ICW3** | init `1638h`, exit `77Dh` | `11h` cascade + ICW3 `04h` (init `204Ah`, exit `2A0h`) |
| C2 | **ICW4 `09h`: 8086, buffered mode** - the IBM BIOS value | init `1644h`, exit `789h` | `01h`: non-buffered |
| C3 | exit restores the BIOS state: `13h 08h 09h`, OCW3 `4Ah`, saved mask | `77Dh` | `11h 08h 04h 01h` + slave |
| C4 | no slave: all A0h/A1h I/O, the IRQ 8-15 stubs, slave EOI chain removed | - | `89Dh-9B3h`, `D7Dh`, etc. |
| C5 | no IRQ 9 to INT 0Ah redirection, no INT 70h-77h hooks, no IRQ 9 virtualisation (the `IRQ9GLOBAL` string stays, unused) | - | `85h-177h`, init `21B5h-2230h` |
| C6 | IRQ 2 is a plain master IRQ: mask, specific EOI `62h` | stub `C3Ch` | same stub; IR2 masked out of the IRR read at init (`and al,0FBh`, `2032h`) |
| C7 | IRQ limits 15 -> 7; `VPICD_Get_Version` ECX `0Fh` -> `07h`, EBX `1` -> `0` | `E15h`, `E1Ah` | `B79h`, `B7Eh` |

Unchanged, and the same in all three VPICDs: edge-triggered (LTIM 0), master vector base `50h`, special mask mode
(OCW3 `6Bh`), specific EOI `60h+n` after masking, the spurious IRQ 7 check (OCW3 `0Bh`, ISR bit 7), IRR reads through
OCW3 `0Ah`. **IRQ 0 (timer) handling is identical**: IBVPICD's IRQ 0 stub (`C0Ch`) matches stock byte for byte apart from
relocations. No port alias (22h-3Fh) is used by any of them.

WfW 3.11's stock VPICD writes the same `11h ... 04h 01h` as 3.0 (linked: exit `37Fh`, `391h`; init `204Ah`, `205Ch`).

## Our Windows 95 VPICD against that list

| | ours (`VPICD_INBOARD_IRQ2.VXD`, file) | covered? |
|---|---|---|
| C1 | ICW1 `11h` (`7077h`, `149Ah`), ICW3 `04h` -> `00h` (`7083h`, `14A6h`) | functionally, **if** the 8259 reads itself as master (see H1b) |
| C2 | ICW4 `01h` (`7089h`, `14ACh`) | **no** |
| C3 | exit writes `11h 08h 00h 01h`: DOS after Windows runs a non-buffered PIC | **no** |
| C4 | 36 A0h/A1h sites NOPed / `mov al,0` (`patch_vpicd.py`) | yes |
| C5 | IRQ 9 descriptor moved onto master bit 2 (#42); INT 70h-77h reflection not examined | differently; works on the 5160 |
| C6 | stub-table entry 2 -> IRQ 9 stub working on master IR2 | yes, via IRQ 9 |
| C7 | `Get_Version` still ECX `0Fh`, EBX `1` (`2499h`, `249Eh`) | no; no known caller depends on it |

## Why C2 is the 5150 candidate

The 8259's pin 16 is SP/EN. In buffered mode (ICW4 bit 3) it is an **output** that goes low while the 8259 drives the
data bus during INTA, meant to switch a transceiver. In non-buffered mode it becomes an **input** that selects master or
slave. On a 5150 the 8259 sits on the CPU-side AD bus, ahead of buffer U8 (section 14a); on a 5160 it sits behind U15 on
the external bus, and U15 turns towards the CPU on its own interrupt condition. If the 5150 uses EN to keep U8 off the AD
bus during INTA, any VPICD that writes ICW4 `01h` lets U8 and the 8259 fight over the vector byte; the 5160 would not care.
That would explain all four observations: DOS and PICCHK pass (BIOS left ICW4 `09h`, section 16); Harrison's Windows 3.1
needed IBVPICD on his 5150 only (section 5); Windows 95 dies right after the real-mode VxDs, where VPICD's Sys_Critical_Init
reprograms the PIC and the first IRQ follows; the 5160 runs both. **Not verified:** what pin 16 connects to on either
board. That is inference from the 8259A datasheet and the minuszerodegrees diagrams, not from a schematic.

## Hypotheses, ranked, with the cheapest test

1. **H1a, ICW4 non-buffered breaks INTA on a 5150.** Desk first: trace 8259 pin 16 (and CAS0-2, pins 12/13/15) in the
   5150 and 5160 Technical Reference schematics. If the 5150 net reaches U8's enable and the 5160's does not, H1a is strong.
2. **H1b, SP/EN read as "slave".** With pin 16 an input and pulled low, a cascade-mode 8259 is a slave and outputs its
   vector only when CAS0-2 match its ICW3 ID. Same schematic read; same test as H1a below separates them.
3. **H1c, WfW 3.11 contradicts H1a.** Simon's WfW 3.11 reaches 386 enhanced mode on his 5150. If its SYSTEM.INI loads
   `*vpicd` (stock, ICW4 `01h`), H1a and H1b are badly weakened. If it loads `ibvpicd.386`, they are supported.
4. **C3 (exit state).** Only matters for "Restart in MS-DOS mode" on a 5150; not the boot stop.
5. H3 (F0000-F3FFF decode) stays open and independent; H2 is weak (section 18).

**One batched ask for Simon** (owner's decision; one DEBUG session plus one look):

- Look only: the `device=` line for vpicd in his WfW 3.11 `SYSTEM.INI` `[386Enh]`.
- At a plain DOS prompt, `DEBUG`, then type (one line):
  `e 100 E4 21 88 C3 FA B0 13 E6 20 B0 08 E6 21 B0 09 E6 21 B0 FF E6 21 88 D8 E6 21 FB 1E 31 C0 8E D8 8B 36 6C 04 A1 6C 04 29 F0 83 F8 5B 72 F6 1F 89 C2 FA B0 13 E6 20 B0 08 E6 21 B0 09 E6 21 88 D8 E6 21 FB CC`
  - Run A (BIOS values, control): `g=100`. Expect registers back after about 5 s with `DX=005B`.
  - Run B (ICW4 non-buffered, single mode): `e 10e 01`, then `g=100`.
  - Run C (exactly what our Windows 95 VPICD writes, with vector 08h): `e 106 11`, `e 10e 00`, `e 112 01`, then `g=100`.
  - Report for each: registers back after ~5 s, or a hang (power off). Every run restores `13h 08h 09h` and the mask.
- The program reprograms the PIC, waits 91 BIOS ticks on IRQ 0 (keyboard IRQ 1 also live), restores. Source:
  the hex above is the whole program (NASM source was a scratch file, not kept: in, 3 ICW writes + mask, wait, restore, int 3). **Run it on the owner's 5160 and in the bed first**: all
  three should pass there; 86Box's `pic.c` does not model buffered mode, so the bed shows only that the program is sane.

Reading: A pass, B or C hang on the 5150 = H1a/H1b confirmed, and the fix is C1+C2 in our VPICD. All pass = the 8259
mode is not the cause, H1 closed at the INTA level too.

**Candidate fix, not applied** (owner decides; needs a VMM32 mapping script like `patch_vmm32_vpicd_irq2.py`), making ours
write what IBVPICD writes, in `VPICD_INBOARD_IRQ2.VXD`: init `7077h` `11h`->`13h`, `7083h` `00h`->`09h`, `708Ah` `E6 21`->`90 90`;
exit `149Ah` `11h`->`13h`, `14A6h` `00h`->`09h`, `14ADh` `E6 21`->`90 90`. Then bed, then the 5160, then Simon.

## Not determined

- Pin 16 / CAS wiring on either board; whether 16-64KB and 64-256KB 5150 planars differ there.
- Whether Windows 95's VMM or the VMM32 real-mode loader writes ICWs of its own: `VMM32.VXD` here is W4-compressed and
  was not expanded (patcher9x `--vxd-convert` would allow a grep for `B0 11 E6 20`).
- Which VPICD is inside Simon's `VMM32.VXD` (stock or ours); his BOOTLOG shows only "LoadSuccess = VPICD".
- The INT 70h-77h reflection in the Win95 VPICD; IBVPICD's `IRQ9GLOBAL` string is dead text, not checked further.
- What the Inboard itself does on an INTA cycle (it bus-masters through the 8088 socket).
