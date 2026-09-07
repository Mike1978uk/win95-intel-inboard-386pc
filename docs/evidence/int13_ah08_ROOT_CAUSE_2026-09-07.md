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
