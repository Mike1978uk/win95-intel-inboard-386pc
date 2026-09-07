# #25 ROOT CAUSE — the right answer is on INT 40h and nothing forwards to it

Measured on the real 5160, 2026-09-07, real-mode DOS. Raw output:
[`FDVEC_2026-09-07.TXT`](FDVEC_2026-09-07.TXT), [`FDCHAIN_2026-09-07.TXT`](FDCHAIN_2026-09-07.TXT).

## The measurement

Same `AH=08h` call, same boot, both vectors, both drives:

| vector | drive | BL (type) | CX | ES:DI | verdict |
|---|---|---|---|---|---|
| **INT 13h** | 0 (A:) | `03` = 720K | `4F09` = 80c/9s | `F000:EFA0` | **WRONG** |
| **INT 13h** | 1 (B:) | `03` = 720K | `4F09` = 80c/9s | `F000:EFA0` | **WRONG** |
| **INT 40h** | 0 (A:) | `04` = 1.44M | `4F12` = 80c/**18**s | `D000:13E3` | **CORRECT** |
| **INT 40h** | 1 (B:) | `02` = 1.2M | `4F0F` = 80c/**15**s | `D000:13C9` | **CORRECT** |

Vectors: `INT 13h = 0575:0122` (INBRDPC.SYS), `INT 1Eh = 0000:0522`,
`INT 40h = D000:10D6` (Sergey's Multi-Floppy BIOS 2.2).

**The machine already knows the right answer.** It is on `INT 40h`, and Windows asks `INT 13h`.

## The static analysis predicted the correct values exactly

From `Sergey_FDD.bin` before any of this was measured:

| ROM branch | predicted | measured on INT 40h |
|---|---|---|
| type 4, `0x9f3`: `lea di,[0x13e3] / mov cx,0x4f12` | `DI=13E3 CX=4F12` | `DI=13E3 CX=4F12` ✅ |
| type 2, `0x9e8`: `lea di,[0x13c9]` | `DI=13C9` | `DI=13C9 CX=4F0F` ✅ |

And `ES=D000` is the ROM's own segment. Sergey's ROM is conclusively exonerated: correct
implementation, correct EEPROM configuration (its POST banner prints
`Drive 0: 1.44 MB, 3.5"` / `Drive 1: 1.2 MB, 5.25"`), correct answers.

## Why it is on INT 40h — the ROM's own install rule

`Sergey_FDD.bin` at `0x00ff`:

```asm
00ff  mov  bx, 0x100          ; default target = INT 40h vector
0102  mov  si, 0x1585         ; default banner  = "installed on INT 40"
0105  cmp  word ptr [0x4c], 0xec59   ; is INT 13h still the STOCK IBM
010b  jne  0x11b                     ; diskette handler at F000:EC59 ?
010d  cmp  word ptr [0x4e], 0xf000
0113  jne  0x11b
0115  mov  bx, 0x4c           ; yes -> take INT 13h
0118  mov  si, 0x155d
011b  mov  word ptr [bx], 0x10d6     ; install
011f  mov  word ptr [bx+2], cs
```

It takes `INT 13h` **only** if `INT 13h` is still exactly `F000:EC59`. By the time the card at
`D000` is scanned, a fixed-disk option ROM has already claimed it (TSROM SCSI BIOS 2.14 is on
the POST screen, and XTIDE is at `D800`), so it correctly falls back to `INT 40h`.

That is the conventional contract: a fixed-disk BIOS relocates the diskette handler to `INT 40h`
and forwards floppy calls there. **On this machine the forwarding does not happen for `AH=08h`.**
`720K / 80 cyl / 9 sect / F000:EFA0` is the stock 1986 IBM answer — the system BIOS's own
diskette parameter table, dumped and confirmed at U18 file offset `0x6FA0`.

## Do NOT fix it by moving the card earlier in the ROM scan order

Tempting, and wrong. Sergey's handler entry at `0x10d6` **never chains hard-disk calls**: `DL>7`
falls through to `0x1169: mov ah,1 / stc` (error), and `AH=08h` with `DL>=0x80` goes to `0xa29`,
also error. It is only safe on a diskette-only vector. Making it win `INT 13h` would break every
hard disk on the machine — including the XT-CF that #21 just got serving `C:` in protected mode.

## The convention is documented — this is a deviation, not a grey area

[AMIBIOS 98 Technical Reference](https://bitsavers.org/pdf/americanMegatrends/MAN-BIOS98-TR_AMBIOS_98_Technical_Reference_19980501.pdf), p.138, *INT 40h Revector for Floppy Functions*:
when the machine has a hard disk the floppy service routine resides at `INT 40h`, and **all**
BIOS floppy functions are revectored there and executed.

So `AH=08h` for `DL < 0x80` is **supposed** to reach `INT 40h`. Sergey's ROM holds up its half
of that contract; whatever owns `INT 13h` does not. The fix below restores documented behaviour
rather than inventing a workaround.

The same document (p.144) gives the floppy `Function 08h` drive-type table — `01h` 360K,
`02h` 1.2M 5.25", `03h` 720K 3.5", `04h` 1.44M 3.5", `05h`/`06h` 2.88M — independently
confirming the decode of Sergey's ROM branches and the values measured on both vectors.
⚠ Its output table names `BH` for the type while its own description text says `BL`. Every
implementation here uses `BL`, and so do our measurements (`BX=0004`, `BH=00`).

Source found independently by the project owner, 2026-09-07; the same document Michal Necasek
had named that morning for `FFF53`.

## Ruled out: there is no XTIDE setting for this

Neither XT-IDE ROM — 2.0.4 as found at `D8000`, nor the configured r638 XT+ image — contains a
single `floppy` or `diskette` string. There is nothing in `XTIDECFG` to switch off; the
interception is unconditional in the code. Clean negative, recorded so it is not re-checked.

## Corroboration from behaviour, not just disassembly

XTIDE intercepts exactly `AH=00h` and `AH=08h` for drives it does not own and chains the rest.
That predicts a split which the machine shows: `AH=02h` reads reach Sergey (**DOS reads 1.44 MB
disks**, which a 1986 XT BIOS physically cannot do), while `AH=08h` returns the stock 720K.
Same vector, same boot, two functions, opposite outcomes.

⚠ Not distinguished: whether XTIDE's own chain-out misses `INT 40h`, or the Trantor SCSI BIOS
(also hooks `INT 13h`; no dump held) is the one answering. **The fix is identical either way.**

## The fix that follows

Hook `INT 13h`; when `AH=08h` **and** `DL < 0x80`, reissue as `INT 40h` and return that result;
chain everything else untouched. Roughly 30-40 bytes, the same shape as `IVT68FIX.COM`, called
from the last line of `AUTOEXEC.BAT` (technique 38 — that timing is load-bearing).

It **forwards to measured-correct data** rather than synthesising geometry, which is what makes
it different from route 2 in the old briefing.

⚠ **A resident `INT 13h` hook makes IOS refuse every miniport** unless it gets an `IOS.INI`
`[SafeList]` line — `docs/ios_safelist_howto.md`. `XTIDEMP.MPD` (#21) is in the blast radius and
is confirmed working on hardware. Verify with `BOOTLOG.TXT` (`Init Success xtidemp.mpd`,
`RMM` never reaching `INITCOMPLETE`) on the first boot after deploying.

⚠ Still unproven: that Windows takes the drive type from `AH=08h`. It fits every symptom, but
the inference has not been measured. And **#18 (wrong data) remains the real blocker** — correct
geometry will not fix a corrupt transfer.
