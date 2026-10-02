# Settings review - 2026-10-03

Read from the card's registry and `SYSTEM.INI` with the CF in the host reader (`D:\`).
Nothing below is measured yet; it records the starting point for the #45 A/Bs.

## Registry (`D:\WINDOWS\SYSTEM.DAT`, decoded offline)

| key | value | meaning |
|---|---|---|
| `FileSystem\PathCache` / `NameCache` | 2729 / 64 | the *Network server* template - confirms #45 row 5 |
| `FileSystem\ReadAheadThreshold` | absent | hard-disk read-ahead at its default, Full (#45 row 3) |
| `FileSystem\CDFS\CacheSize` | 32 | CD-ROM supplemental cache (#45 row 4) |
| `FileSystem\CDFS\Prefetch` / `PrefetchTail` | 27 / 2 | read-ahead is on - "No read-ahead" is not selected |

Decoder: value records found by name in the `CREG` file (type, length, data in the 12
bytes before the name). Units of the CDFS values are not confirmed here; the Control Panel
setting is authoritative, so the A/B is done there.

**Owner's point on the CD drives:** each drive buffers in its own RAM (the CD-RW, a much
later drive, likely ~2 MB; the Nakamichi changer, read-only, about 0.5 MB by recollection,
unverified). Windows' read-ahead on top costs bus time and memory for data the drive has
already buffered. Next: CD-ROM tab -> "No read-ahead", then free memory and `T130AB`.

## `SYSTEM.INI`: `DMABufferSize=64` removed

Origin: a Google AI suggestion in the owner's Windows 3.11 build
(`docs/external_clues_and_correspondence_2026_08.md`); 64 was later kept as the largest
size avoiding the 128 KB alignment rule, never because anything needed it.

- Removed; Windows uses its default (16 KB). 48 KB less locked below 1 MB, if the default
  holds - check locked memory with PERFLOG.
- `DMABufferIn1MB=True` **kept**: the 8237 reaches only the first 1 MB, and without it the
  buffer may land anywhere in the 5 MB.
- Users of the buffer: DOS programs doing DMA in a DOS box (SB sound, FastDoom #47, direct
  floppy). Windows' SB driver (`MSSBLST`, patched) and `HSFLOP` have their own.
- Check on the 5160: SB sound in a DOS box, a floppy copy, locked memory. If DOS-box sound
  breaks, try 32.
- Revert: `copy C:\WINDOWS\SYSTEM.BSR C:\WINDOWS\SYSTEM.INI`. The previous `SYSTEM.BSR`
  (pre-SHADRAM) is kept as `SYSTEM.B02`.

## The Google "unturned stones" output [AI-SOURCED]

Mostly a restatement of this repo's open issues; several figures are invented (a "1 ms tick
burning 15%" - row 6 measured no gain; "RAMSHAD.VXD"; #46 described as a memory allocation).
Kept:

- **SWAPCOMP's hash table is 8 KB** (`HASH_LOG 12`, `drivers/swapcomp/src/SWAPCOMP.ASM`).
  With the 4 KB source page and its output that is the whole 16 KB L1. Try `HASH_LOG 10`
  (2 KB): ratio against time, on the 5160 only (the bed does not model the cache).
- **ELNK3 buffers** (22 KB locked): part of the #41 / #28 driver pass, owner's call to keep.

Dropped: 8259 EOI latency (one write per interrupt, unavoidable), #18 change-line (not
ours, technique 104).

## DMA CF cards

Closed: the lo-tech DMA boards are out of production (https://www.lo-tech.co.uk/tag/xt-ide/,
checked by the owner), and DMA 3 is taken by the parallel card.
