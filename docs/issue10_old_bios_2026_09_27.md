# #10: early BIOS revisions with the Inboard - measured 2026-09-27

Question: can a 5150 or an early 5160 run Windows 95 (and 3.11) on an Inboard 386/PC, and if not,
can a loadable BIOS extension fix it for every owner?

## Headline

- **The 1982 XT ROM (08NOV82) boots Windows 95 to the desktop on the Inboard in the emulator.**
  The long-standing claim that 1982 ROMs are incompatible with `INBRDPC.SYS` does not hold.
  Not yet confirmed on a real 1982 XT.
- **The 5150 ROM (27OCT82) loops in POST in the emulator.** A real 5150 boots DOS with the Inboard
  and XT-IDE (owner, 2023; Cimon, 2026), so this is an emulator gap. The 5150 is untested past POST.
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
| real ROM | 5150 27OCT82, 5150 PPI | POST reset loop (emulator gap) |

The first `OLDBIOS` attempt halted at `F000:E0AB`: the patches broke the ROM checksum. The build
now rebalances each 8 KB block through unused `CC` padding.

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

## Next

1. Make the 5150 ROM POST in the emulator: log the cause of each restart, and try one boot with a
   plain VGA card to see whether the Mach8 ROM is involved.
2. Then boot Windows 95 and 3.11 on the 5150 ROM.
3. Ask Cimon for `BOOTLOG.TXT`, his 5150's BIOS date, planar RAM and switch settings.
