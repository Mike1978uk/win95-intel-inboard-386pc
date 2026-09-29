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

## Next

0. Asked Cimon 2026-09-29 (owner): which BIOS the 5150 carries (`05/02/12`), the `BOOTLOG.TXT`,
   and whether the 5160 floppy was formatted elsewhere. Told him not to repeat the probe.
   His ROM is not the whole cause: the owner's former 5150 and others on VCFed failed Windows
   3.11 on stock IBM ROMs.
   @andrew-hoffman suggested GLaBIOS (<https://github.com/640-KB/GLaBIOS>), an open-source
   PC/XT BIOS, to try in the bed; not yet tried.
   Any re-run of the probe needs a free address checked first (`R` in DEBUG shows its own segment).
1. Cimon (message sent by the owner): the DEBUG DMA probe on his 5150 and 5160, `BOOTLOG.TXT`
   from the failing boot, BIOS date, planar RAM and SW1/SW2.
2. The owner can run the same probe on the 5160 as the control.
3. If DMA and CPU disagree on a 5150, model planar RAM in the emulator (DMA-only block below the
   planar limit) and test workarounds there first.
4. Windows 3.11 on the 5150 ROM in the emulator is still untried.
