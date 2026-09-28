# `EGACACHE`: what it does, and why the 2026-09-20 A/B could not see it

Static read of the CF's `INBRDPC.SYS` (md5 `d3c458017c296fe01a13bceb19f34106`, 50,837 bytes),
2026-09-28. Reopened by @andrew-hoffman on
[#35](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/35) (2026-09-22), who offered
two explanations for the null result besides "it targets EGA, the Mach8 is a VGA".

## What the code does (offsets are file offsets = driver offsets)

| step | where | what |
|---|---|---|
| gate | `0x9849` | only if the `EGACACHE` flag `[0x2B5]` is `FFFF` |
| detect | `0x98D8` | `C000:0000` must read `AA55`, and the length byte `C000:0002` must be non-zero and at most `0x40` (32 KB). Otherwise message at `0xAB74` ("no EGA BIOS was found") |
| copy | `0x98AA` | `rep movsw` of the whole ROM from `C000:0000` into the driver's own image at `CS:0F00`. Over 32 KB: message at `0xAC6F` |
| redirect | `0x9872` | saves the `INT 10h` vector (`0x0C27`), then writes it back (`0x0C41`) with the same offset in a new segment `(CS*16 + 0F00h) / 16` - the copy. The two calls wrap past 64 KB, so a disassembler prints them as `0x10C27`/`0x10C41` |
| keep | `0xA762` | resident end becomes `0F00h` + ROM length, rounded to a paragraph. **Without `EGACACHE` it is `0EA0h`** |

The file carries a 32 KB zero block at `0x0F00`-`0x8F00` for the copy; it is discarded when the
switch is absent.

## The three explanations

| explanation | verdict |
|---|---|
| It only works on an EGA, the Mach8 is a VGA | **No.** The test is a generic option-ROM header at `C000`; any VGA BIOS passes |
| `INBRDPC.SYS` checks the ROM's signature or size (@andrew-hoffman) | **Yes, both** - `AA55` and length at most 32 KB. The Mach8 BIOS image here (`roms/video/mach8/BIOS.BIN`, `roms/video/ATI_MACH8.bin`) has `AA55` and length `0x40`, exactly 32 KB, so it **passes**. Real card not yet read |
| Another option ROM in `C0000`-`E0000` blocks it (@andrew-hoffman) | **No.** It only looks at `C000` |

## Why the A/B saw nothing

`EGACACHE` does not shadow `C0000`. The ROM stays on the bus at 2.858 us/byte, which is exactly what
was measured - and the width probe never calls the video BIOS. The switch only moves **`INT 10h`
calls** onto a copy in Inboard RAM. The 2026-09-20 test could not have seen it.

## Is it worth having?

- **Who calls `INT 10h`:** DOS text output (the console driver writes every character through it),
  DOS programs that use BIOS video, mode sets. Windows 95's display driver draws directly and does
  not.
- **What it costs:** the copy stays resident - 32 KB of conventional memory, locked for the whole
  Windows session. Against the RAM track that is a loss for Windows.
- **So:** off for Windows, as now. A DOS-only session is the one place it could pay. A fair test is
  a PIT-timed burst of `INT 10h AH=0Eh` output in DOS, with and without.

## Side result for the RAM track

`INBRDPC.SYS` leaves **`0EA0h` = 3,744 bytes** resident without `EGACACHE`. Shrinking its resident
part is not worth doing; `MEM /C` (checklist item 1) should confirm the figure.

## Still to check on the real machine

The real card's length byte: `DEBUG`, `d c000:0 l4`. `55 AA 40` confirms the image above.
