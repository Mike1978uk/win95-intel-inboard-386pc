# #10: early BIOS revisions with the Inboard - measured 2026-09-27

Question: can a 5150 or an early 5160 run Windows 95 (and 3.11) on an Inboard 386/PC, and if not,
can a loadable BIOS extension fix it for every owner?

## Headline

- **The 1982 XT ROM (08NOV82) boots Windows 95 to the desktop on the Inboard in the emulator.**
  The long-standing claim that 1982 ROMs are incompatible with `INBRDPC.SYS` does not hold.
  Not yet confirmed on a real 1982 XT.
- **The 5150 ROM (27OCT82), with the 5150's own keyboard/switch interface, also boots Windows 95
  to the desktop in the emulator** - once the emulated SW2 reports at most 640 KB (see 4a).
  So as far as the emulator models a 5150, the BIOS is not what crashes Cimon's machine.
- **Five BIOS-service differences, applied to the 1986 ROM, do not stop Windows 95.**

## 1. The "1982 ROM incompatible" claim was an emulator artefact

All nine IBM BIOS revisions carry the same reset vector (`EA 5B E0 00 F0`) and the same three bytes
at `F000:E05B` (`FA B4 D5`). `INBRDPC.SYS` does not look for a signature there: it writes `EA F5 0B`
into its shadow copy through the `0x5F0000` window and checks the write is visible
(`docs/INBOARD_86BOX_PORT_PLAN.md:3232`). That is a test of the Inboard's shadow RAM and works on
any ROM. It failed in August because the emulator mapped the shadow windows wrongly, fixed later
(skill techniques 66/67).

## 2. What the ROMs provide

| BIOS | model byte | INT 15h | runs card ROMs |
|---|---|---|---|
| 5150 24APR81, 19OCT81 | `FF` | cassette only | **no** |
| 5150 16AUG82, 27OCT82 | `FF` | cassette only | yes |
| 5160 16AUG82 | `FF` | none | yes |
| 5160 08NOV82 | `FE` | none (`stc / mov ah,86h / retf 2`) | yes |
| 5160 10JAN86, 09MAY86 | `FB` | `80 81 82 84 85 88 90 91 C0` | yes |

Also on the 1986 ROM only: INT 1Ah returns CF set for functions it lacks (older ROMs `IRET` with
flags untouched), and the diskette BIOS supports functions up to `18h` (older: `05`).
Anyone with a working hard disk has a ROM that runs card ROMs, so the 1981 5150 BIOS is out of
scope (owner, 2026-09-27).

## 3. What Windows 95 asks the BIOS for

Diagnostic build `diag-issue10` (Mike1978uk/86Box, local), bed `vm_3c509b`, full boot to the
desktop on the 09MAY86 ROM. INT 15h calls reaching the ROM: `24 41 86 88 90 91 C0 C2 D8 E8 F9`.
Of these only `C0` (10 calls) and `88` (1 call) behave differently on older ROMs; the rest are
unsupported on the 1986 ROM too. No enhanced-keyboard INT 16h calls (`10h`-`12h`). INT 13h `08` and
`15` and INT 1Ah `02` and `04` were also called.

The ROM model byte is read by eleven code locations, including a VxD (`0028:C0360575`) and a
16-bit Windows module (`01EF:002A`).

## 4. The A/B runs (same bed, same image)

| run | ROM | result |
|---|---|---|
| baseline | 09MAY86 | desktop |
| `INBOARD_OLDBIOS=31`: model `FF`, no `C0`, no `88`, old INT 1Ah, old diskette range | 09MAY86 patched, checksum kept | **desktop** |
| real ROM | 5160 08NOV82 | **desktop** |
| real ROM | 5150 27OCT82, 5150 PPI | POST loop - emulator SW2 bug, 4a |
| real ROM | 5150 27OCT82, 5150 PPI, SW2 capped at 640 KB | **desktop** |

The first `OLDBIOS` attempt halted at `F000:E0AB`: the patches broke the ROM checksum. The build
now rebalances each 8 KB block through unused `CC` padding.

### 4a. The 5150 POST loop was the emulator's SW2

A 5150 BIOS sizes conventional memory from the SW2 switches. 86Box builds that value from total
RAM, which on the Inboard machine includes the card's extended memory (5120 KB here), so POST was
told it had megabytes of conventional memory. Logging showed no reset and one entry to POST;
POST itself was looping. Capping the reported value at 640 KB let it through. Diagnostic only:
the combination exists only in `diag-issue10`.

## What the emulator does not model

It has no planar RAM separate from the Inboard's: CPU and DMA see one flat block. On a real
5150 the planar RAM cannot be disabled, so below the planar limit the CPU may use the Inboard's
copy while DMA uses the planar copy. DOS booting from SCSI or XT-IDE (no DMA) would not notice;
Windows could. Untested. The DEBUG probe below separates the cases; the same probe on a 5160
also answers ledger item E5c (can DMA reach Inboard-served memory):

```
DEBUG
L 1000:0 0 0 1
L 9000:0 0 0 1
C 1000:0 L200 9000:0
```

## 5. Forum context

- **Cimon, VCFed "Windows 3.1 w/ Intel Inboard 386/PC - VxD Issue"**
  (<https://forum.vcfed.org/index.php?threads/windows-3-1-w-intel-inboard-386-pc-vxd-issue.79730/>),
  March 2023: Harrison's Windows 3.1 image would not run 386 enhanced mode on his 5150 and needed
  `IBVPICD.386` to boot; the same files worked on his 5160. The 5150 failure predates Windows 95.
- **modem7, same thread**: the 5150's planar RAM cannot be disabled; its SW1 RAM switches only
  report to software. On a 5160 they disable banks. Recorded as a possible difference; the owner
  thinks it is not the cause, since a 5150 backfilled by the Inboard boots DOS.
- **mtrahms, "Inboard 386/PC 2mb expansion CLONE"**
  (<https://forum.vcfed.org/index.php?threads/inboard-386-pc-2mb-expansion-clone.78562/>): a 5150
  with the Inboard needs Sergey's Multi-Floppy BIOS 2.4; 2.5 and later lock up.
- **Cimon, 2026-09-27**: Windows 95 that runs on his 5160 goes black and crashes on his 5150.

## 6. Cimon's reply, 2026-09-29

Received by the owner (not posted on GitHub). Four photos held locally at
`XT_project/photos/CIMON/`, not in the repo - they are Cimon's.

| machine | BIOS date (bytes after `F000:FFF0`) | DOS 6.22 probe | Win95 DOS probe |
|---|---|---|---|
| 5160, 256 KB planar | `11/08/82` | ran, no differences | "Invalid media type writing drive A" |
| 5150, 64 KB planar | `05/02/12` | `dir a:` works; halts on the first `L` | same halt |

- **A real 5160 with the 08NOV82 ROM runs Windows 95 on the Inboard.** This is the machine Cimon
  already runs it on, so section 1 is now confirmed on hardware, assuming the ROM was not changed.
- **The 5160 control passes:** DMA into `1000:0` and `9000:0` lands where the CPU reads it.
- **`05/02/12` is not an IBM 5150 date.** It reads as a replacement ROM dated 2 May 2012; the
  photos show white-labelled EPROMs on the 5150 board. The emulator runs used 27OCT82, so they
  did not test his ROM. Ask which BIOS it is.
- **The 5150 board is the 16KB-64KB type** (photo), so planar RAM ends at `1000:0`. Both probe
  addresses are above it: the probe as sent cannot test the planar hypothesis on this machine.
- **The halt is not DMA evidence.** `L 1000:0` overwrites whatever is at 64 KB; with DOS and
  drivers loaded that can be DOS itself or DEBUG. The second try's "File not found" is what DEBUG
  prints when the command is typed on the `DEBUG` command line, so it did not run.
- **The planar hypothesis is weaker.** Booting from floppy is itself a DMA read to `0000:7C00`,
  inside the 64 KB planar. It works, so DMA and CPU agree there.
- The Win95-DOS error on the 5160 is unexplained; `L` does not write.
- `BOOTLOG.TXT` from `WIN /B` on the 5150 is very short; Cimon will send it via a portable.

## 7. Cimon's second reply, 2026-09-29 late

Two `BOOTLOG.TXT` files and a board photo, held locally with the others in
`XT_project/photos/CIMON/` (his, not in the repo).

- **The 5150's BIOS is a replacement from a DIY 8088 kit.** He points at Plasma's Super PC/Turbo
  XT BIOS (<https://www.phatcode.net/downloads.php?id=101>, v3.1 of Oct 2017; a drop-in for the
  5150/5160). His `05/02/12` date suggests an earlier release of that line. The page says nothing
  about INT 15h or other AT services.
- **The 5150 log stops at the last real-mode VxD load** (`EBIOS` LoadFailed). On his 5160 the
  next line is `SYSCRITINIT = VMM`, so the 5150 dies at or just after the switch to protected
  mode - or before the log was flushed.
- **The two Windows installs differ.** The 5150 also loads Future Domain SCSI drivers:
  `DCAM950.EXE` and `FDCD.SYS` (both LoadFailed), `mtrr.vxd`, and `V9FCAMD.386`, `FDSCSI.386` and
  `C:\PWSCSI\INT13.386` in place of the standard `int13`. His 5160 loads none of them. That is a
  second variable next to the BIOS; the 5150 failure cannot be pinned on the ROM until it is gone.
- **The photo is labelled "IBM BIOS in PC XT" but looks like a 5150 board:** TMS4116 16 Kbit
  DRAM, several 24-pin ROMs (the 5160 has two ROM sockets), and a BIOS marked `1501476`, which
  from memory is the 5150's 10/27/82 part number. Unverified; the byte read (`11/08/82`) stays
  the record for his 5160.
- The 5160 floppy was formatted elsewhere; `DIR` works on both machines under both DOS versions,
  so the Win95-DOS "Invalid media type" stays unexplained and low priority.

## 8. Cimon's third reply, 2026-09-30

New files, held locally in `XT_project/photos/CIMON/` (his, not in the repo): a 5160 board photo,
a 5150 `BOOTLOG.TXT` without the Future Domain drivers (`5150PC2T.TXT`), and MSD, MEM and Norton SI
reports from the 5150. He confirms the earlier board photo was his other 5150, with IBM ROMs.

- **Removing the Future Domain drivers changed nothing.** The log still ends at `EBIOS` LoadFailed,
  the last real-mode VxD load; his 5160 logs `SYSCRITINIT = VMM` next. The SCSI stack is ruled out.
  What is left is the BIOS or the 5150 board.
- **The 5150's BIOS is Plasma's "Super PC BIOS v2.5 for 8088/V20", dated `05/02/12`** (MSD's
  BIOS version string). Not an IBM ROM.
- **His 5160 carries the IBM `1501512` BIOS (the 08NOV82 part) and `5000027` BASIC** (photo), so
  section 6's hardware confirmation stands: a real 1982 XT ROM runs Windows 95 on the Inboard.
- **HIMEM sees extended memory on the 5150:** MSD reports 4,284 KB XMS free. `INBRDPC.SYS` loads at
  3,744 bytes, as here.
- **Ignore the "18,504 KB extended" in MSD and SI.** 18,504 is `4848h`: both tools read CMOS
  bytes, and an XT has no CMOS, so ports `70h`/`71h` land on another device. Their CMOS disk types
  are the same artefact.
- `MEM-d.TXT` is a different setup (PC DOS, `INBRDPC` 28,416 bytes = `0F00h` + a 24 KB video ROM,
  i.e. `EGACACHE` on; Future Domain `FDBIOS`). Which machine it is was not stated.

Owner replied 2026-09-30 and asked for a dump of the 5150's BIOS (`DEBUG`, `w f000:0` with `BX:CX` = `1:0000`), so the exact ROM can run in the bed on the 5150 machine
type. That separates the BIOS from the board without another boot on his side.

## 9. Cimon's replies to 2026-10-04, and what they rule out

VCFed private conversation "Inboard 386 experiments", page 11 (owner's account; not public).

| fact (Cimon) | consequence |
|---|---|
| WfW 3.11 runs on his 5150 in protected mode (WfW 3.11 is 386 enhanced only) | The switch to protected mode works on a 5150 with the Inboard. Only Windows 95 stops there. |
| The 5150 tests used the hard disk from his 5160 | Same Windows install on both. The logs still differ (`mtrr.vxd`, `VMCPD.VXD` from file only on the 5150); not explained. |
| A second 5150 board, another TMS850 SCSI card, same Super PC BIOS: same stop | Not a faulty board or card. |
| His IBM `1501476` ROM does not pass the first floppy test | Chip probably damaged (board had a RIFA capacitor failure). He has another, not yet found. Every failing boot so far ran the Super PC BIOS. |
| The Super PC BIOS was fitted because the IBM ROM would not run newer CPUs | - |

**Super PC/Turbo XT BIOS v2.5 source read** (<https://www.phatcode.net/downloads.php?id=101>,
`pcxtbios25.zip`, source and binaries; Plasma, May 2012). Nothing found that Windows 95 would trip on:

- INT 15h: `stc / mov ah,86h / retf 2` for every function - byte for byte the 08NOV82 XT stub,
  which runs Win95 on Cimon's 5160 and in the section 4 `OLDBIOS` run.
- Model byte `FF` when built with `IBM_PC` (his MSD reads `FF`).
- Memory size: probes RAM up to `MAX_MEMORY` (640 KB) and never reads SW2.
- 8259 init `13h 08h 09h`, NMI and parity enabled at the end of POST, unexpected-IRQ handler: as IBM.
- The download's binaries are the default XT build (model `FE`, turbo on), not his. His exact ROM
  still needs the dump requested in section 8.

**Our patches on his machine.** His `AUTOEXEC.BAT` (in `MSD5150.TXT`) ends with `c:\ivt68fix.com`.
`F000:FF53` is `CF` (`IRET`) in the Super PC binary and in every IBM 5150 and 5160 ROM in `roms/`,
so `IVT68FIX` works with his BIOS. `WAIT86` is not installed; it only matters for V86 floppy motor
waits (Setup), and his BIOS's INT 15h never waits. `ISPEEDPC.EXE 4` = 0 added wait states, the
default (Intel Appendix D); it changes nothing.

**Intel's troubleshooting notes** (`DOX1.TXT` in the owner's Intel files):

- "A055 201 Parity Check 2: make sure the power supply is rated for 100 watts or greater." The
  stock 5150 supply is 63.5 W. Deprioritised: WfW 3.11 reaches protected mode on the same supply,
  and the stop is at the same point on two boards.
- The same error on IBM PCs when SW1-3 and SW1-4 are ON: "These switches must be OFF for a valid
  memory configuration." The Inboard reads SW1 at power-up (SOS beep if they report over 256 KB).

Parity Check 2 is the I/O channel check (port `62h` bit 6). On the 5160, SHADRAM v2's first write
to never-written card RAM raised it at Windows start-up (`docs/next_session_2026_10_04.md`). SHADRAM
masks NMI (port `A0h` = 0) around that write, so it says nothing about whether an unmasked check
is survivable under Windows.
`NODIAGS` in Cimon's `CONFIG.SYS` means Windows makes the first writes to his extended memory.
Whether the check fires on his 5150, and how Windows 95 reacts, is not known.

## Next

1. Cimon, looking only: SW1-3/4 positions on the 5150; whether a parity message has ever appeared.
2. Cimon, one edit and one boot: remove `NODIAGS` from `INBRDPC.SYS`'s line, so its memory test
   writes all extended memory before Windows starts.
3. If the check is implicated: a small `.COM` before `WIN` that reports port `62h` bits 6/7 and
   clears the latch. A VxD that catches the NMI and continues comes last - it would also hide real
   parity errors, so it must count and report them.
4. His BIOS dump (section 8) in the bed on the 5150 machine type; or his second IBM `1476` ROM, or
   the Super PC BIOS fitted in his 5160, to separate BIOS from board on hardware.
5. The pre-monolith install on the 5150 (Cimon's suggestion) - after 1-4, since it changes the
   whole Windows build.
6. GLaBIOS in the bed (@andrew-hoffman, <https://github.com/640-KB/GLaBIOS>) - still untried.
