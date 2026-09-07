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

---

## The live chain walk, 2026-09-07 (COMrade, real-mode DOS, 35 ms RTT)

Vectors read live, confirming the DEBUG probe exactly:

| vector | value | owner |
|---|---|---|
| `INT 13h` | `0575:0122` | a resident |
| `INT 1Eh` | `0000:0522` | relocated DPT in low RAM |
| `INT 40h` | `D000:10D6` | **Sergey's Multi-Floppy BIOS** |
| `INT 11h` | `F000:F84D` | stock BIOS - matches AMI's `FF84D` entry-point map |
| `INT 12h` | `F000:F841` | stock BIOS |
| `INT 41h` | `0000:0000` | hard-disk parameter table pointer is **null** |

### Hop 1 - `0575:0122`

```asm
0122  sti
0123  test dl, 80h          ; hard disk?
0126  jz   0132             ; NO (floppy) -> chain immediately
0128  cmp  ah, 02  / je 0137    ; read
012D  cmp  ah, 03  / je 0137    ; write
0132  jmp  far cs:[006C]    ; -> 0206:0B78
0137  ...handle...
```

### Hop 2 - `0206:0B78`, INBRDPC's wait-state wrapper

```asm
0B78  cli / pushf / push ax
      mov ax, cs:[02B1]     ; hard-disk waitstate value
      cmp dl, 80h / jae +4
      mov ax, cs:[02AF]     ; ...else the floppy one
      mov dx, 0670h / out dx, al
      call far cs:[004B]    ; -> 0070:03EE   (the ORIGINAL handler)
      mov dx, 0670h / out dx, al      ; restore
      retf 2
```

### Hop 3 - `0070:03EE` (IO.SYS resident area; `INT 16h` also lives at `0070`)

```asm
03EE  call +0x95
03F1  jmp  far cs:[0148]    ; -> FFFF:2585 - the HMA (CONFIG.SYS has DOS=HIGH,UMB)
```

`C800:0000` reads all `FF` - **no option ROM there**, so the Trantor SCSI ROM is elsewhere.
Its segment is still unmapped; `D000` is Sergey and `D800` is XTIDE.

### What this establishes

**`INBRDPC.SYS` is completely transparent for floppy calls.** Both of its hooks chain every
one of them: the first bails out to the chain the moment `DL` says floppy, and the second
*wraps* rather than answers - it sets wait states, calls the original handler, restores, and
returns, with **no `AH` filtering at all**. The one resident everybody would suspect is clean,
and that is now measured rather than assumed.

The chain then leaves DOS into the HMA, so whatever answers `AH=08h` sits **below DOS in the
ROM layer** - consistent with the stock BIOS answering and `INT 40h` never being consulted.

⚠ Still not distinguished: which ROM. The fix does not depend on it.

## Ruled out: Sergey's ROM v2.7 does NOT fix this

The owner holds a **v2.7** image (`Multi-Floppy BIOS, Version 2.7, Copyright 2010-2025`,
`GR2764@DIP28.BIN`, md5 `b727f971cca52cbe7cbabb80ece4c42e`) alongside the **v2.2** actually
fitted. Swapping it in is the obvious thing to try. It would not help - the install rule is
byte-for-byte identical:

```asm
  v2.2 @ 0x105 / v2.7 @ 0x0EB
  cmp word ptr [0x4c], 0xec59   ; still only takes INT 13h if it is the
  jne  -> INT 40h               ; stock IBM diskette handler
  cmp word ptr [0x4e], 0xf000
  jne  -> INT 40h
```

v2.7 adds a configurable **IPL type** (Floppy BIOS / System BIOS) and an IPL retry prompt.
Neither touches `AH=08h`. Clean negative, recorded so the ROM swap is not spent on a boot.

**The fitted chip is confirmed as our analysis target**: the owner's programmer dump
`AT28C64B.bin` is md5 `df93d1d546c9c3dc7722754545b3997b` - **byte-identical to
`roms/network/Sergey_FDD.bin`**. The static analysis above was of the real silicon.

⚠ **The Trantor ROM at `CA000` is still uncaptured.** It was read live over COMrade (6144 bytes,
`55 AA 0C`, entry `CA00:0083`, *"IBM Compatible SCSI BIOS / TSROM: SCSI BIOS, Version 2.14 /
Copyright (C) 1989-92, Trantor Systems, Ltd."*) but the transfer out was truncated to 5433 bytes
and the partial dump was discarded rather than analysed. Re-read it in chunks if the mechanism
question is ever worth closing.

## Option ROMs captured off the machine, 2026-09-07

Written to files by the guest itself (DEBUG script via COMrade) and collected from the CF in a
reader - **not transferred over the serial link**, which had already truncated one attempt.
Owner's suggestion, and the right one.

| file | segment | bytes | CRC-32 | identity |
|---|---|---|---|---|
| `TRANTOR.BIN` | `CA000` | 6144 | `f64b78ef` | Trantor TSROM SCSI BIOS 2.14 |
| `XTIDEROM.BIN` | `D8000` | 8192 | `3ee14993` | **XTIDE r638 (XT+)** - matches `IDE_XTP_configured_2026_08_31.bin` |
| `SERGEY.BIN` | `D0000` | 8192 | `5fa1d3c7` | Multi-Floppy BIOS 2.2 - matches `roms/network/Sergey_FDD.bin` |

**CORRECTION.** An earlier version of this section, and two statements in the session that
produced it, called the Trantor dump the first capture of that ROM and said no copy existed.
**That was wrong** - `roms/scsi/trantor_t130b_bios_v2.14.bin` was already in the tree. Nothing
new was captured. What the exercise actually produced is **verification**, which is worth
having but is a different claim, and the check that would have caught it was one `ls` of
`roms/scsi/`.

**All three captures are byte-identical to images the repo already held**, so none were added:

| live chip | archived image | result |
|---|---|---|
| `CA000` Trantor, 6144 B | `roms/scsi/trantor_t130b_bios_v2.14.bin` | identical over the declared 6144 B; the archive is the same ROM padded with `0xFF` to a full 8K EPROM |
| `D8000` XT-IDE | `roms/xtcf_card/IDE_XTP_configured_2026_08_31.bin` | identical, CRC `3ee14993` |
| `D0000` Sergey | `roms/network/Sergey_FDD.bin` | identical, CRC `5fa1d3c7` |

So every ROM this project reasons about is now confirmed against the silicon actually fitted,
and the CRCs computed on the DOS box match those computed on the host after collection - the
card-reader path is clean end to end.

Two facts this settles:

- **The fitted XT-IDE image is r638, not the 2.0.4 "as found".** That had been treated as
  ambiguous. The `AH=08h` interception is byte-identical in both, so the diagnosis is unaffected -
  but any future claim about the card's BIOS must name r638.
- **Sergey's ROM is confirmed three ways** - repo copy, the owner's programmer dump
  (`AT28C64B.bin`), and the live chip - all the same CRC. The static analysis was of exactly what
  is running.

Option ROM map, measured: `C8000` empty (`FF`), `CA000` Trantor, `D0000` Sergey, `D8000` XT-IDE.
Scan order therefore Trantor -> Sergey -> XT-IDE, which is why Sergey found `INT 13h` already
taken and correctly fell back to `INT 40h`.

### Two COMrade mechanics worth remembering

- **Do not issue COMrade calls in parallel.** It is one serial link; four concurrent `file_write`
  calls produced three successes and one 8 s timeout.
- **A `.BAT` or DEBUG script written over the bridge must be CRLF.** An LF-only batch prints
  `OFF` (from `@ECHO OFF`) and does nothing - the exact symptom the repo-hygiene skill records.
  Write them base64-encoded so the bytes are exact.
- Overwriting one particular existing file timed out repeatedly while writing new names
  succeeded; if a `file_write` hangs, write to a fresh filename rather than retrying.

## ⭐ DOS 6.22 CONTROL, 2026-09-07 - the bug predates Windows 95 entirely

**Same machine, same ROMs, same option-ROM scan order. Only the CF and the OS differ.** That is
what makes this a clean control rather than a comparison.

| call | DOS 6.22 | Windows 95 | verdict |
|---|---|---|---|
| `INT 13h` A: | `BL=03 CX=4F09 ES:DI=F000:EFA0` | identical | **wrong** |
| `INT 13h` B: | `BL=03 CX=4F09 ES:DI=F000:EFA0` | identical | **wrong** |
| `INT 40h` A: | `BL=04 CX=4F12 ES:DI=D000:13E3` | identical | correct |
| `INT 40h` B: | `BL=02 CX=4F0F ES:DI=D000:13C9` | identical | correct |

`VER` confirms genuine **MS-DOS 6.22**, not DOS 7.

### What it settles

**Windows 95 is not the outlier.** The wrong `AH=08h` answer is present under plain DOS 6.22, so
it comes from the ROM-level `INT 13h` chain and not from anything Windows installs. An earlier
draft of this session conceded that Win95's `IO.SYS` was the likely culprit, on the strength of
`0070` appearing in the chain walk. **That concession was wrong** and this measurement retires it.

### And it reconciles the owner's recollection, which was also correct

The owner recalled the floppies working fully under DOS 6.22 and Windows 3.11. They did - and
`AH=08h` was wrong the whole time. **Nothing before Windows 95 ever asked.**

DOS and Win 3.11 drive floppies through the real-mode data path, which works: reads and writes
chain onward to `INT 40h` and reach Sergey's ROM. That is why 1.44 MB disks have always read
correctly. `AH=08h` is a *query*, and Windows 95's hardware detection is the first thing on this
machine to ask it and believe the answer.

So the defect has always been present and simply had no consumer. That is also why it is not a
known issue in the wider community, and why Sergey shipping this BIOS is entirely reasonable.

### Incidental: the INT 40h front-end differs, the answer does not

| vector | DOS 6.22 | Windows 95 |
|---|---|---|
| `INT 13h` | `0247:0B78` (INBRDPC's wrapper, same `0B78` offset, lower load address) | `0575:0122` then `0206:0B78` |
| `INT 1Eh` | `0000:0522` | `0000:0522` |
| `INT 40h` | `0392:0846` - a RAM driver front-ends it | `D000:10D6` - Sergey's ROM directly |

Under 6.22 something at `0392` hooks `INT 40h`, but `ES=D000` in its reply proves it still chains
through to Sergey. Windows 95 also adds a hop at `0575` in front of INBRDPC that 6.22 lacks.
Neither difference changes the answer.

### Consequences

- `FD08FIX`'s design stands, and is **XT-general** - it has no Inboard dependency at all.
- **The XTIDE / Trantor question is live again.** One of those ROMs answers `AH=08h` rather than
  forwarding to `INT 40h` as AMI's reference documents. Identifying which is worth doing properly
  before anyone raises it with the XTIDE maintainers.

## ⭐ PREFERRED FIX: move Sergey's ROM to C8000 - two switches, no software

### The mechanism, now fully pinned

Neither fixed-disk ROM originates the wrong answer:

- **Trantor TSROM 2.14** has **no `AH=08h` handler at all** - no `cmp ah,08`, no `int 40h`, no
  `EFA0` immediate. Its `cmp ah` sites are SCSI/ASPI function codes.
- **XTIDE** intercepts `AH=08h` for foreign drives but its floppy path *chains* and returns
  whatever comes back. Had it answered from its own copy of the table (ROM `0x166d`), `ES` would
  read `D800`; it reads `F000`.

So the **1986 system BIOS answers**, from its own diskette parameter table at `F000:EFA0`,
reporting the best it knows: 720K. (The earlier "no `cmp ah,08` in U18" proved nothing - the XT
BIOS dispatches `INT 13h` through a jump table, not a compare chain.)

It is an **ordering conflict**:

```
system BIOS      INT 13h = F000:EC59
Trantor  CA000   saves INT 13h (= system BIOS), takes INT 13h
Sergey   D0000   INT 13h is no longer EC59, so it settles for INT 40h
XTIDE    D8000   saves INT 13h (= Trantor), takes INT 13h

floppy AH=08h:   XTIDE -> saved(Trantor) -> saved(system BIOS) -> 720K
                 Sergey sits on INT 40h and nothing routes there
```

Sergey hooks `INT 40h` expecting the fixed-disk BIOS to revector floppies there, as AMI's
reference documents. Trantor and XTIDE instead chain to their *saved `INT 13h`* - also common,
and correct only if nothing hooks `INT 40h` afterwards. Sergey does, and loses.

### The fix: scan Sergey FIRST

```
Sergey   C8000   INT 13h is still EC59 -> its own rule fires, it takes INT 13h
Trantor  CA000   saves INT 13h (= Sergey), takes INT 13h
XTIDE    D8000   saves INT 13h (= Trantor)

floppy AH=08h:   XTIDE -> Trantor -> Sergey -> CORRECT
hard disk:       handled by XTIDE/Trantor, never reaches Sergey
```

**This retires an earlier warning in this document.** A previous section said do NOT move the card
earlier, because Sergey's handler errors `DL>=0x80`. That reasoning was wrong: with Sergey scanned
first, the fixed-disk ROMs install **in front of it** and service hard disks themselves, so Sergey
never sees `DL>=0x80`.

### It is two switches, and the address is available

Silkscreen, `SW2.3-SW2.7 ROM Address Selection`, `1 = ON, 0 = OFF`:

| | SW2.3 | SW2.4 | SW2.5 | SW2.6 | SW2.7 |
|---|---|---|---|---|---|
| `0xD0000` (current) | ON | **OFF** | **ON** | ON | ON |
| `0xC8000` (target) | ON | **ON** | **OFF** | ON | ON |

`SW2.1 Enable ROM` stays ON. Measured option-ROM map:

| range | occupant |
|---|---|
| `C0000-C7FFF` | Mach8 video BIOS - header `55 AA 40` = 32 KB |
| **`C8000-C9FFF`** | **free, reads all `FF`** |
| `CA000-CB7FF` | Trantor, 6 KB |
| `D0000-D1FFF` | Sergey, 8 KB, current |
| `D8000-D9FFF` | XTIDE, 8 KB |

`C8000` is the **only** slot below Trantor - `C2000`/`C4000`/`C6000` all fall inside the Mach8
ROM - and Sergey's 8 KB ends exactly where Trantor begins.

### Why this beats FD08FIX

Fixes it in the ROM chain for **every OS**, with no resident code, no `[SafeList]` entry, no
`AUTOEXEC.BAT` line, and nothing for IOS to object to. `FD08FIX` becomes the fallback for anyone
whose floppy ROM address is not selectable.

⚠ **One recorded caution.** This project's own 86Box config carries a note that Sergey's ROM at
`0xC8000` **hangs POST**, which is why it is not loaded there in emulation. Whether that is an
emulator artefact or a real constraint is unknown - on this hardware `C8000` measures free. It is
a switch change and trivially reversible, so it is worth trying, but expect the possibility and
know the way back is two switches.

⚠ Unverified: that nothing calls `INT 13h` between the `C8000` and `CA000` ROM scans, which would
reach Sergey before Trantor is in front of it. Unlikely in a contiguous scan, stated for honesty.
