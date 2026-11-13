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

## 10. NMI tools, 2026-10-04

Hypothesis: on the 5150 an NMI (parity or I/O channel check) arrives as Windows 95 switches to
protected mode, and Windows' handler stops the machine. Not yet tested on a 5150.

- `tools/nmichk/NMICHK.COM` (353 B, md5 `cfb2f668`): last line of `AUTOEXEC.BAT`; logs ports `61h`/`62h`
  to `C:\NMICHK.TXT`; `/C` clears the latches; `/M` also masks NMI (port `A0h` = 0) for the session.
  **Sent to Cimon 2026-10-04** for one boot with `/m`, `NODIAGS` unchanged.
- `drivers/nmiwatch/` (`NMIWATCH.VXD`, 5,000 B, md5 `bb5420ab`, 504 B locked): count only. Ports at
  `Sys_Critical_Init` and `Init_Complete`, every NMI counted by source and passed on, latches polled
  each second. PERFLOG `NMIWATCH\*`.
- **Bed, one boot** (`vm_magnaram_nmiwatch`, clone of `vm_magnaram_shadram2`, 09MAY86 ROM):
  desktop reached, SHADRAM 16/16 alongside. `NMICHK`: `61=48 62=20`. `NMIWATCH`: ports `4820h` at
  entry and at `Init_Complete`, 0 NMIs, 0 latched seconds. 86Box models neither check, so this proves
  the tools load and read, not the hypothesis. Raw: `docs/captures/2026-10-04_nmiwatch_bed/`.
- No positive control: there is no safe way to raise an NMI on purpose, so a zero NMI count is weak.
- Open: Windows' PARITY driver is written for an AT, where port `61h` bits 6/7 are the check flags.
  On an XT port `61h` reads `48h` (bit 6 = keyboard clock). If PARITY reads it as an AT, any NMI under
  Windows 95 would look like an I/O channel check. Needs `VMM32.VXD` decompressed (`patcher9x`).

### 10a. Injected NMI in the bed, 2026-10-04

Diagnostic build (`diag-issue10` `6bdb6eb5c`, local): `INBOARD_NMITEST=<n>` sets port `62h` bit 6
and raises one NMI `<n>` instructions after the first protected-mode code at CS `28h` (the VMM);
port `61h` bit 5 clears the latch. Bed `vm_nmitest` (clone of `vm_magnaram_nmiwatch`, no
NMIWATCH, NMICHK or SHADRAM), 09MAY86 ROM, `n` = 2,000,000. Raw: `docs/captures/2026-10-04_nmitest_bed/`.

| run | log | result |
|---|---|---|
| 1 | raised at `0028:C0361586`, mask `80h` (delivery not logged) | desktop, PERFLOG ran |
| 2 | raised as run 1; **delivered** to `0028:C0004054`, protected mode | desktop; PERFLOG did not run (startup batch staged with LF line ends - our error) |
| 3 | `n` = 0: raised at the first CS `28h` instruction; delivered to `0008:000007E1` (the loader's handler) | desktop, PERFLOG ran |
| 4 | as run 3 on the **5150 ROM** (27OCT82, 5150 PPI) | desktop, PERFLOG ran; identical log |

Port `62h` was never read and the latch never cleared: Windows 95 took the NMI and carried on
without looking at the cause. In the emulator a single I/O channel check NMI is **not fatal** -
at the switch or after it, on the 1986 XT ROM or the 5150 ROM. The "any NMI is fatal on an XT"
form of the hypothesis does not hold here. Still possible on hardware and not modelled: an NMI
inside the few instructions of the mode switch itself, repeated NMIs, or a check that comes with
corrupted memory rather than alone. Cimon's `NMICHK /m` boot remains the hardware test.

### 10b. Cimon's NMICHK boot, 2026-10-04

`NMICHK /m` on his 5150 (Super PC BIOS, `NODIAGS` unchanged), run just before `IVT68FIX`:
`61=48 62=20 after=20 mode=M`. Nothing latched before Windows, NMI masked for the boot - and Windows 95
still stops after the last real-mode VxD (`EBIOS` LoadFailed), as before. **The NMI lead is closed.**
Files held locally with his others (`NMICHK.TXT`, `BTLG0410.txt`). Left: the 5150 board versus his
exact BIOS build - the `vm_5150` bed separates them once it boots.


## 11. Simon's 5150 results, 2026-11-02

Simon (a second 5150 owner, on #10) reports, in order:

- Phoenix XT BIOS, labelled 1985 and 1986/A, in a socket converter in his 5150 "A" board: stops at the same
  place as the Anonymous BIOS (the Windows 95 stop at the switch to protected mode). A third BIOS family
  with the same result.
- Ruud's Diagnostic ROM v5.7 (minuszerodegrees.net): "a short and long beep, no screen". Asks whether 4.5 is better.
- He is looking for an EPROM to burn the Landmark test ROM.

What the sources say ([Ruud's Diagnostic ROM, 16KB-64KB page](https://minuszerodegrees.net/ruuds_diagnostic_rom/5150/16KB-64KB.htm),
[v4.5 breakdown](https://minuszerodegrees.net/ruuds_diagnostic_rom/misc/Ruuds%20Diagnostic%20ROM%20-%20Breakdown%20-%20Version%204.5.htm)):

- Early in execution the ROM beeps three times, short-long-short, and on a 5150 clicks the relay. He reported two.
- Screen output needs a real MDA or CGA card; an EGA/VGA card driving an MDA/CGA monitor does not count. Without
  one, or 4 KB RAM at A0000/B0000/B8000, "only the initial tests are done". The beeps still happen.
- It needs RAM for variables and stack, taken from the video card or the 4 KB option, not planar RAM.
- v4.5 step 18 looks for MDA, CGA, then EGA video RAM; none found = checkpoint 8A, halt.

Not stated by the sources: any meaning for a short-plus-long beep; any difference in hardware needs between 4.5
and 5.7 (the 5.7 page lists the six video/RAM options; the 4.5 page lists steps only); what video card Simon has;
whether his count was two or he missed one. His one-short-one-long is unexplained.

Meaning for #10: the Phoenix result removes "Anonymous BIOS" as the cause; BIOS-independent points at the board
(planar RAM, switches) or the Inboard/5150 interaction. The Ruud ROM is only useful to him if it runs to the
checkpoint stage; it does not test the protected-mode switch.

Next from Simon: his video card type, and the exact beep count and pattern. 4.5 vs 5.7 is unknown.
## 12. Simon's diagnostics, 2026-11-08

Input from 2026-11-07/08: two IBM 5150A boards (one serial 0116682, an early one), Inboard 386 + 4 MB, SCSI, CGA,
floppy. Photos: `photos\CIMON\ibm5150a-nr1_ERROR.jpg`, `ibm5150anr2-ERROR.jpg`, `test-7nov2026.jpg` (outside the repo).
Identity: the VCF thread below is posted by Cimonvg and the photo folder is `CIMON`; Simon and Cimon are probably one
person (ledger lists them as two rows). Owner to confirm before any reply names him.

### 12a. The ROM on the screen is Supersoft/Landmark v1.2, not Ruud's

The block-letter banner reads "SUPERSOFT ... POST DIAGNOSTICS VERSION 1.2" (partly legible). The test list matches the
Supersoft PC/XT suite in its
[Version 1.2 user manual](https://minuszerodegrees.net/supersoft_landmark/Supersoft%20Landmark%20ROM%20-%20Version%201.2%20-%20Users%20Manual.pdf)
(scanned PDF; I read pp 22-37 as images), not Ruud's list. Ruud's ROM has no per-test screen of this style
([Ruud's tests, v5.7](https://minuszerodegrees.net/ruuds_diagnostic_rom/misc/Ruuds%20Diagnostic%20ROM%20-%20Tests%20-%20Version%205.7.htm);
URL derived from the site's relative links, fetched OK). Site pages:
[Supersoft/Landmark](https://minuszerodegrees.net/supersoft_landmark/Supersoft%20Landmark%20ROM.htm),
[Ruud's ROM](https://minuszerodegrees.net/ruuds_diagnostic_rom/Ruuds%20Diagnostic%20ROM.htm).

### 12b. Beep code: 5 high-low, then 8 short = "Cannot initialize monitor"

Manual p38-39 (PC/XT beep codes by Hi/Lo-Short count): Hi/Lo 5, Short 8 = "Cannot initialize monitor"
(5/9 and 6/1 are the same text). Manual p13: with no usable video the ROM beeps instead; an EGA/VGA card on invalid
switches also beeps. This is a Supersoft code, so the first attempt was the Supersoft ROM. The VGA card gave no
picture; CGA works. Not in Ruud's page: it documents only one pattern, short-long-short at the speaker test.
Whether "8 low" means short beeps is my reading of the table; the code is unique either way.

### 12c. Transcription (both boards read identically; see caveats)

Board 1 = `nr1`, board 2 = `nr2`. Photos are blurred block lettering; names are matched to the manual's list.
"?" = cannot read.

| Test (screen) | Board 1 | Board 2 | Meaning (manual page) | Win95 PM switch? |
|---|---|---|---|---|
| CPU registers and logic | PASSED | PASSED | p17 | no |
| Supersoft ROM checksum | PASSED | PASSED | p17 | no |
| 8253 timer ch 0 / 1 / 2 | PASSED x3 | PASSED x3 | counters run | no |
| 8237A DMA controller | PASSED | PASSED | register test | DMA page regs not tested by Supersoft |
| 8255 parity detected | PASSED | PASSED | p17-18 latch check | PASSED: no parity latch |
| 16K critical memory region | PASSED | PASSED | | no |
| Memory refresh | PASSED | PASSED | p22 | no |
| 8259 interrupt controller | PASSED | PASSED | p23 register read/write | PASSED: alias not a problem here |
| Hot interrupt | PASSED | PASSED | p23 no IRQ with all masked | PASSED |
| **Interrupt level 0** | **1 FAILED** | **1 FAILED** | p24: IRQ0 from timer ch 0 must reach the CPU at the right rate; causes listed: 8253, 8259, 8288, CPU | **Only interrupt-path failure; see 12d** |
| Nonmaskable interrupt | PASSED | PASSED | p25: no NMI from FPU/parity | PASSED: NMI lead (10b) stays closed |
| MDA memory | NOT PRESENT | NOT PRESENT | p25 expected, CGA fitted | no |
| CGA memory | PASSED | PASSED | p25 | no |
| EGA/VGA memory | NOT PRESENT | NOT PRESENT | p25 expected | no |
| 8087 numeric coprocessor | NOT PRESENT | NOT PRESENT | p26 no 8087 on the board | no (Inboard FPU socket is separate) |
| Keyboard controller | PASSED | PASSED | p26 | no |
| Keyboard scan lines | PASSED | PASSED | p27 | no |
| Floppy controller | PASSED | PASSED | p28 | no |
| **Floppy read** | **1 FAILED** | **1 FAILED** | p28: reads track 0 head 0 sector 1 of A:; needs a formatted disk | **no** - expected with no disk |
| System memory to ?0000 | PASSED | PASSED | p29; end address unreadable | no |
| Slow refresh to ?0000 | PASSED | PASSED | p34 | no |
| Top-right panel: system BIOS, BASIC ROM 1-4 | PASSED x5 | PASSED x5 | p35; labels only partly legible | no |
| Summary | passes 00001, errors 00002, no memory errors, no false parity errors | same | 2 errors = the 2 FAILED rows | |
| Switch settings (SW1, SW2, "AT SW") | ? | ? | p36: SW1 pos 1 ON makes POST loop | unreadable at this quality |

Cannot read: switch digits in the three boxes; the end address of the two memory rows; whether the third photo's
screen (blue panel on the rack unit) says the same (it is too small).

Both errors are expected or explained except Interrupt level 0. Floppy read fails with no disk by design. MDA, EGA/VGA
and 8087 "NOT PRESENT" are plain absences. The manual says a failed test does not name the faulty part and clean
runs do not prove a board good. Per the site's
[illogical results page](https://minuszerodegrees.net/supersoft_landmark/Supersoft%20Landmark%20ROM%20-%20illogical.htm)
the ROM runs dependent tests (floppy read, RAM) even after their prerequisite test fails.

### 12d. Interrupt level 0: the one to chase

- Same failure on two boards, with the same Inboard + CPU module logic in both. Two independent faults in the 8253/8259
  are unlikely (estimate). The common part is the Inboard and its CPU module, which the ROM was not written for.
- The ROM measures the CPU being interrupted "at the correct rate". A 386-class CPU running far faster than an 8088
  could make a rate check wrong without any fault (my inference; the manual does not say how the rate is checked).
- It bears on Win95 only if IRQ0 delivery to the CPU is really broken on the 5150. Win95 and WfW both run a timer tick
  on IRQ0 and WfW reaches protected mode on the same board, which argues against a dead IRQ0 (inference).
- Not in any source: whether the early 5150A differs from later boards on this test. The site keeps separate Ruud pages for
  the [16KB-64KB](https://minuszerodegrees.net/ruuds_diagnostic_rom/5150/16KB-64KB.htm) and
  [64KB-256KB](https://minuszerodegrees.net/ruuds_diagnostic_rom/5150/64KB-256KB.htm) 5150 planars, so which he has matters.
- The project's 8259 alias (A0-A4 not decoded on the XT, 8259 appears at 20h-3Fh; `docs/xt_io_aliasing_gotcha.md`)
  is not exercised by these tests; the 8259 test passes.

### 12e. VCF thread summary

[Inboard 386 and compability](https://forum.vcfed.org/index.php?threads/inboard-386-and-compability.66538/), Sep 2018 to Sep 2022,
poster Cimonvg (summary by a fetch tool, not read page by page). Contains: Win32s runs on his 5150 with the Inboard;
Windows 3.1 DOS box gives "Wrong VxD dynamic chain-call"; HIMEM.SYS breaks Norton Utilities 8.0 (keyboard), DrHardware,
DOSBench, an A20 test, and Netroom 3 fixes most of them; INBRDPC.SYS lacks INT 15h function 89h (the AT enter-protected-mode
call), which he asks about in connection with Windows 95; SYSCLOAK reports "BIOS system descriptor table missing";
VIDCLOAK failed with TIGA and Tseng VGA cards and worked on an ATI Mach32 only with /PMBIOS; Win32s wants `*vmcpd` while a
real FPU needs `ibvmcpd.386`; TI486-SXL2 works with the 1982 BIOS, 486BL and Cyrix 5x86 beep and stall on it. Trixter
suggested QEMM 8 and said accelerators never reach native 386 speed; Chuck(G) said baby-AT boards made them obsolete.
Not in the thread: "WfW 3.11", "5150A", the phrase "virtual interrupt by software", NETROOM internals.
Microsoft KB [Q88572](https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/88/572.HTM): Intel says the Inboard
386/PC does not run Windows 3.1 (reason given: designed for 8088/8086 XT machines); Intel's own Windows 3.0 runs in real and
standard modes only.

### 12f. NETROOM (Helix), from sources only

- Wikipedia [Helix Netroom](https://en.wikipedia.org/wiki/Helix_Netroom): loaded network drivers into a virtual machine built on
  an expanded-memory page frame, run as a background process; v1.10 (Jan 1991) added a 286/386 memory manager (386+, and
  286 with the C&T NEAT chipset); v2.1 (Jan 1992) added "Quantum" (BIOS into HMA); v3.0 (1993) added Cloaking; v3.04 (late
  1994) added Windows 95 compatibility. Page does not mention a V86 monitor, hardware interrupt handling, or the 8088.
- Wikipedia [Helix Cloaking](https://en.wikipedia.org/wiki/Helix_Cloaking): relocates drivers to extended memory at ring 0; protected-mode
  services via INT 2Ch (DPMS); a cloaked TSR that hooks an interrupt leaves an 11-byte stub in low memory that calls the
  Cloaking server; it integrates with the existing memory manager rather than switching descriptor tables itself; does not run
  on 286; Helix licensed Award BIOS code for protected-mode system/video BIOSes (real-mode footprint 8 KB, down from 96 KB).
- [WinWorld Netroom](https://winworldpc.com/product/helix-netroom): 3.04 is the last version; no CPU requirement stated.
- Sources do not say Netroom supports the Inboard, nor how it handles hardware interrupts on an XT. A web search found
  none. The "SYSCLOAK ... descriptor table" message in his thread suggests Cloaking wants an AT-style BIOS descriptor table
  (INT 15h 89h), which the 5150 lacks - an observation from his post, not from Helix.
- Could its engine run under Win95? Sources only show 3.04 is Win95-compatible as a DOS-side manager. They do not show it
  fixing anything at the Win95 protected-mode switch. His NETROOM patch floppy API text would be the first real source.

### 12g. Three cheapest next tests, ranked

1. Re-run Supersoft with a formatted 360K/720K disk in A:. Minutes. Clears the floppy row, so the count is honest; confirms
   Interrupt level 0 is the only open item.
2. Re-run with the CPU module at its slowest setting (or CPU module/ Inboard speed jumpers changed), reporting only the
   Interrupt level 0 row. Tests the "rate check on a fast CPU" reading; also ask which CPU module is fitted.
3. Owner runs the same Supersoft ROM (5160 variant) on the working 5160 with the Inboard. If Interrupt level 0 fails
   there too, it is a test artefact and drops off the #10 list. Needs an EPROM burned; costs a trip to the 5160.

Open for Simon: planar type (16-64KB or 64-256KB), BIOS ROM labels, clear photos of SW1/SW2, NETROOM API text.
Ruud's ROM short+long beep (ledger, 11-02) is still unexplained and is not needed if Supersoft runs.

## 13. NETROOM 3 disk (owner's copy), 2026-11-08

Folder `Windows_311_working_build
etrm3` on the owner's desktop. Read as data only; nothing run. Result: **the disk does
not hold the API text, and nothing on it covers an 8088/XT or Win95.** It is the registered-user supplement, not the
patch floppy Simon describes.

### 13a. Files

| File | What it is |
|---|---|
| `README2.TXT` (1993) | Supplemental disk note: STRETCH, CACHECLK, SAVERCLK, WINMEM, "the Cloaking API definition and sample program", the Screen Saver API, `RM386.EXE` + `RMLODHI.VXD`. The text only documents the Windows tools; no API definition is in it |
| `2000.TXT` | Windows troubleshooting after Customize: `WIN /D:CFSVX`, `NOEMMDRIVER`, `EMMEXCLUDE=A000-FFFF`, video/system BIOS cloaking |
| `2010.TXT` | Setup / Customize / cloaked-utility Q&A. Cloaking API paragraph below |
| `2014.TXT` | "Cloaked BIOS as a BIOS upgrade": Award BIOS replaces the system BIOS on 386+ machines |
| `7000.TXT` | Hurricane (Windows 3.x memory utilities) FAQ; unrelated to the monitor |
| `NR.ZIP` (65 members) | Program files: `RM386.EXE`, `SYSCLOAK.EXE`, `VIDCLOAK.EXE`, `RMLODHI.VXD`, `DPMI.EXE`, `README.TXT`, `TECHNOTE.TXT`, `CUSTOMIZ.*`. **Every member is password-encrypted** (zip flag bit 0); not opened, not attacked |
| `SETUP.EXE`, `202W3.EXE`, `203W3.EXE`, `CDFIX.EXE`, `PKUNZIP.EXE` | Installer, WinZip self-extractors (Hurricane patches `PATCHHUR.EXE`, `PATCHW.DLL`), MSCDEX patch, unzip. Strings only; no useful documented switches |

### 13b. What the readable text says about NETROOM

- It is a 386-only manager: `RM386.EXE` plus "cloaked" BIOS (`SYSCLOAK.EXE`, `VIDCLOAK.EXE`), which moves the system and
  video BIOS above 1 MB and runs an Award BIOS over the old one (`2014.TXT`). That needs a 386 and a PC/AT-class BIOS.
- "Another protected mode software is currently running" and the `/D:CFSVX` list (`2000.TXT`) are Windows 3.x enhanced
  mode behaviour; `S` = `SystemROMBreakPoint=FALSE`, `V` = `VirtualHDIRQ=FALSE`. Both are SYSTEM.INI keys Windows 3.x
  reads, not NETROOM features.
- Switches seen: `AUTO`, `NOF`, `NOC`, `X=` (exclude range), `*(size base)` (extended memory), `/PMBIOS`, `/NOTSENG`,
  `/PORT=2D` (SYSCLOAK, Dell only), `/NOAPM`, `/ALTEISA`. None concerns an XT, a PIC base, the timer, or an accelerator.
- Cloaking API (`2010.TXT`): "Helix provides a toolkit" so TSRs and drivers can run in extended memory; Stac, Symantec,
  Logitech, ATI, SMC ship cloaked versions. Described in prose only; no interrupt numbers or function lists.
- Not found: any mention of 8088, XT, PC/XT, Inboard, 8259/PIC, IRQ0, VCPI, GEMMIS, Windows 95 or Chicago. The Windows
  cooperation visible is `RMLODHI.VXD` (loads Windows buffers high, `WINMEM`) and the EMM keys above.

### 13c. The API question

- The "virtual interrupt by software" text Simon mentions is **not here**. Likely it is the Cloaking API definition
  that `README2.TXT` says the supplement carries, or a later patch; the floppy that has it was not in this copy.
- Speculation, unsupported by any file here: a cloaking API that lets a driver run in protected mode implies Helix
  reflects interrupts to real mode itself. That would be NETROOM's own V86 monitor, and Windows 3.x enhanced mode
  would then import it as a VMM-compatible host. Win95's VMM brings its own V86 monitor and VPICD, so a NETROOM API
  could not replace them; at most it shows which interrupt path NETROOM leaves alone that VPICD does not.
- Our own evidence stands: the Inboard aliases ports onto the 8259 (`docs/xt_io_aliasing_gotcha.md`). Nothing from this disk changes the section 12 ranking.

### 13d. What the disk did not contain

The API definition or sample program; any `.DOC` for `RM386`; `TECHNOTE.TXT` and `README.TXT` (inside the encrypted zip);
any XT/5150/Inboard statement; any Win95 statement. If Simon sends the floppy's text file, or the zip password, this
section gets a "13e".

## 14. Simon's data-bus pointer, 2026-11-13

Simon points at the minuszerodegrees data-bus diagrams: [5150](https://www.minuszerodegrees.net/5150/misc/5150_data_bus.jpg),
[5160](https://www.minuszerodegrees.net/5160/misc/5160_data_bus.jpg). Both are labelled "simplified". Read as images.

### 14a. What the diagrams show

- **5150:** the 8088's multiplexed AD0-AD7 feeds buffer U8, which drives the system data bus D0-D7. The **8259 PIC hangs
  directly on AD0-AD7**, on the CPU side of U8. The data bus joins all expansion slots, flow switch U12 (to the planar RAM
  memory bus, MD0-MD7, with parity) and flow switch U13 (to the external bus XD0-XD7: ROMs U28-U33, 8253, 8255, 8237, DMA
  address latch U18, DMA page registers U19, NMI enable U96). U12: 1 write to planar RAM, 2 idle, 3 read from planar RAM.
  U13: 1 internal to external (normal), 2 open circuit during DMA, 3 external to internal on a read of a planar ROM, of any
  I/O port 0-1FF, or of F0000-F3FFF.
- **5160:** same shape with U2, U9 (RAM) and U15 (external). Differences: (1) the **8259 sits on the external bus**, behind
  U15; (2) slots 1-7 are on the system data bus but **slot 8 is on the external bus**; (3) U15 position 3 also results from
  "8259A has received an interrupt request" and from "CARDSELECT signal from expansion slot 8"; (4) the I/O read range is
  0-FF, not 0-1FF; (5) the ROM read condition is "planar ROM" with no F0000-F3FFF range, two BIOS ROM chips (U18, U19)
  instead of six; (6) chip designators differ (U8/U12/U13 vs U2/U9/U15). Planar RAM attachment, the DMA open-circuit
  position and the parity block are the same.
- The 5150 diagram has no slot-8 equivalent and no interrupt condition on U13.

Text confirming points (1), (2), (4): [5160 to 5150 differences](https://www.minuszerodegrees.net/5160/diff/5160_to_5150_motherboard_differences.htm)
(8259 data pins on the CPU's AD bus on the 5150, on the external bus on the 5160; slot 8 on the external bus; pin B8
reserved on the 5150, Card Selected on the 5160; motherboard I/O 000-1FF on the 5150, 000-0FF on the 5160; all four RAM
banks permanently enabled on the 5150). [Slot 8 page](https://minuszerodegrees.net/5160/misc/5160_slot_8.htm): a slot 8 card
must generate CARDSELECT "in order for the data to be gated through chip U15 to the CPU"; timing on slot 8 is stricter;
the 5150 has no slot 8 equivalent.

**Not in the sources:** buffer part types (8286, 74LS245) and their enable pins; INTA cycles and how they reach the slots;
what an Inboard (bus master through the 8088 socket) sees on a read from planar RAM or I/O on either machine; the
5150 B2 slot behaviour. The two index pages I tried (`5150/index.htm`, `5160/index.htm`) return 404; the 5160 technical
reference lists on the site were not read.

### 14b. Hypotheses (none tested)

- **H1, the 8259 is on the CPU-socket side on a 5150.** The Inboard replaces the 8088 through its socket cable, so on a
  5150 the PIC's data pins sit on the same AD lines the Inboard drives and reads, not behind U8. Windows 95's VPICD
  reprograms and polls the PIC far more than WfW 3.11 or DOS. A timing or float difference on 8259 reads/writes or the INTA
  vector read would show only there. Test: on the 5150, a real-mode `.COM` that loops OCW3 reads of IRR/ISR at ports 20h/21h
  against a known value, and a loop of 8259 mask writes read back; run on the 5160 as control. Any mismatch rules H1 in.
  Against it: the Supersoft 8259 and hot-interrupt tests passed (section 12c); only "Interrupt level 0" failed, on both
  boards, which this hypothesis would also fit.
- **H2, I/O 100h-1FFh is motherboard space on a 5150.** A read there turns U13 to position 3 on a 5150 but not on a 5160.
  If Windows 95 or its drivers probe a port in 100-1FF, the 5150 answers from the planar, not the bus. Test: log the ports
  Windows 95 touches in 100-1FF in the bed (`diag-issue10`, I/O trace); an empty log rules it out.
- **H3, F0000-F3FFF read decode on a 5150.** Only the 5150 diagram states this range. If the Inboard shadow RAM serves
  F0000 and the planar also answers, a protected-mode switch that touches that segment could read the wrong source. Test: on
  the 5150, `DEBUG` compare `F000:0 L100` read twice, with and without `INBRDPC.SYS` shadowing; any difference rules it in.
  Weak: DOS and WfW boot with the same shadow.
- **H4, the slot used for the Inboard.** The diagrams say only that slot 8 on a 5160 needs CARDSELECT. Test: ask Simon and
  Cimon which slot the Inboard is in on each machine (look only). Not expected to matter on a 5150, which has no slot 8.
- **Not supported by the diagrams:** planar RAM is attached identically on both (U12 vs U9), so the section "What the
  emulator does not model" is not made more likely by them.

### 14c. Background: omores / ages2001

Simon's video "Windows 3.1 UNLOCKED: USB 3.0, HD Audio, Blu-ray & More" was not watched. The GitHub account behind
Windows 3.x USB, audio, optical and storage drivers is <https://github.com/ages2001/>: [WINUSB](https://github.com/ages2001/WINUSB)
(USB, DOS to ME), [WINAUDIO](https://github.com/ages2001/WINAUDIO) (HD Audio/AC'97), [WINUDF](https://github.com/ages2001/WINUDF)
(UDF, optical), [WINDISK](https://github.com/ages2001/WINDISK) (IDE/AHCI/NVMe/CD). Which repository the video uses is not
confirmed. Needs PCI hardware; no bearing on the 5150 stop. Already recorded for WINUSB and WINDISK in
`docs/resources_and_sources.md` (lines 990-991) and in `docs/contributor_input_ledger.md`; WINAUDIO and WINUDF are not.

## Next

0. `NMICHK /m` boot on the 5150 (sent 2026-10-04); `NMIWATCH` + `NMICHK` boot on the owner's 5160.
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
