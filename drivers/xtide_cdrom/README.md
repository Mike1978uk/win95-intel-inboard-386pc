# XT-IDE ATAPI CD-ROM driver (MS-DOS) — reference material

Real-mode MS-DOS CD-ROM driver that talks ATAPI over an XT-IDE card. Kept here as the
reference implementation for the 32-bit Win9x XT-IDE port driver (issue #21): it is the
only known working code that drives this card's 8-bit data latch.

## Provenance

Originally `patacd.asm` — a generic PATA/ATAPI CD-ROM driver for MS-DOS by
**sava (t.ebisawa) / lpproj**, 2016, [ZLIB licence](https://opensource.org/licenses/Zlib)
(full notice at the top of `xtidecd.asm`).

**The XT-IDE port is by [Miran Grča (@OBattler)](https://github.com/OBattler)** — 86Box's
developer since 2016 — posted to the **86Box Discord** and never published anywhere public.
Attribution confirmed by @andrew-hoffman on [issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21),
2026-08-29, who notes the two binaries came from **two separate pinned posts**. That is almost
certainly why they differ (see below) — they are two versions, not a matched pair.

Contributed to this project by **@andrew-hoffman** on
[issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21#issuecomment-3576283472),
2026-08-29, with the caveat "definitely not well tested". Redistributed here under the ZLIB
licence, unaltered.

## Which binary to use

The zip contains two builds. **They are not the same code, and `xtidecd.asm` is not the source
of the working one.**

| file | md5 | XT 16-bit transfer |
|---|---|---|
| `xtidecd.sys` | `a5054f33a239feafa9c9093b989a1851` | **broken** — matches `xtidecd.asm` |
| `xtidecd(1).sys` | `809da6f8afc7fed5adaaf0edc9dd84bf` | **correct** — use this one |

Established by disassembling both, not by running them. `xtidecd(1).sys` implements the latch
protocol exactly as 86Box's `hdc_xtide.c` models it:

```
read   in al,dx (base+0)      ; low byte, latches high
       xor dl,8 -> base+8     ; high byte
write  mov ax,[si] / xchg al,ah
       xor dl,8 -> base+8 / out ; high byte first, latched
       xor dl,8 -> base+0 / out ; low byte, commits the word
```

`xtidecd.asm`'s `ATAReadDataXT` does `add dx,8` but never issues the second `in`, so every word
reads as its low byte duplicated; `ATAWriteDataXT` uses `mov [si+1],al` (a store) where it needs
`mov al,[si+1]` (a load), so it writes stale AL to the card and corrupts the source buffer.
**Fix both before rebuilding from this source**, and mark the result as altered per the licence.

## What it gives issue #21

- **The access order.** Agrees exactly with 86Box's `hdc_xtide.c` and the register map in #21:
  reads low-then-high, writes high-then-low.
  ⚠️ **Not an independent confirmation** — 86Box and this driver's XT-IDE port share an author, so
  the two cannot disagree about a misunderstanding they hold in common. Corroborate against the
  [XTIDE Universal BIOS](https://www.xtideuniversalbios.org/) or the card itself before treating
  the map as settled on real hardware, where card revisions differ anyway.
- **Master/slave, already solved.** `ata_drivenum` is a 2-bit unit number: bit 0 is the DEV bit
  (shifted to bit 4 of the Drive/Head register), bit 1 selects the channel. `ATASelectDevice`
  writes DEV, then *reads the register back and compares the DEV bit* — that read-back is how you
  tell an absent slave from a present one. `DetectATAPICD` scans all four units.
- **Port tables for an 8-bit card:** primary `0x320-0x327` + `0x32E`, secondary `0x340-0x347` +
  `0x34E`. Taskfile at consecutive byte offsets, device control / alternate status at `+0x0E`.
- **`CPU 8086` throughout** — no 286+ instructions, so the transfer loops are directly usable on
  the 5160.
- **ATAPI packet flow**: `0xA0` PACKET command, byte-count in cylinder low/high, interrupt reason
  in sector count — the whole SFF-8020 sequence, polled.

`settings.png` is the contributor's 86Box configuration for the card.

## What it does not give

- No protected-mode anything. This is a real-mode DOS character device (`MSCDEX` client); none of
  the IOS/AEP surface a Win9x port driver needs is here. Only the register-level transport ports.
- No 8-bit-mode `SET FEATURES` negotiation — it assumes the card's latch, not ATA 8-bit PIO.
- Not verified on this project's hardware or in emulation. Nothing above is a test result.
