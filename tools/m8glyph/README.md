# M8GLYPH - one-glyph test for the Mach8 glyph cache (#49)

Answers two questions on the real card before any driver work:

1. which `RD_MASK` bit selects which plane for a colour-expand blit
   (expected: plane n is bit (n+1) mod 8 - Richter and Smith, and XFree86);
2. whether a glyph stored in one plane of off-screen memory expands to a
   colour with the background left alone.

The work is done in the Mach8's own memory below line 900, which is separate from
the VGA memory DOS shows. The screen does not change. Results go to `M8GLYPH.BIN`.

## Run it

1. Boot to the DOS prompt, not a DOS box under Windows (Windows owns the card).
2. Copy `M8GLYPH.COM` to `C:\` and run `M8GLYPH` from `C:\`. It takes well under
   a second and prints `M8GLYPH: done, results in M8GLYPH.BIN`.
3. Reboot before starting Windows. The engine is left reset with plain settings;
   the Windows driver sets it up again, but a clean start removes the question.
4. Bring `C:\M8GLYPH.BIN` back (COMrade `file_read`, or the CF in a reader) and run

   ```
   python tools/m8glyph/gen_m8glyph.py decode M8GLYPH.BIN
   ```

## What it touches

Writes: the 8514 engine's reset, pitch, scissors, masks, mixes, colours and
`MEM_CNTL`, and Mach8 memory below line 900. Never written: `ADVFUNC_CNTL`, the
CRT and clock registers, the DAC, and `ROM_ADDR_1` (52EE), which on this card
moves the video BIOS. Every wait is bounded. After a timeout it stops reading
and writes the file.

## Rebuild

`python tools/m8glyph/gen_m8glyph.py build` - emits the bytes directly, no assembler.
Register sequence from XFree86 3.3.6 `XF86_Mach8` (Kevin E. Martin); no code
copied. See `docs/mach8_text_and_driver_plan.md`.
