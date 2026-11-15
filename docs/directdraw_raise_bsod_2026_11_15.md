# Fatal exception 0E at 0137:BFF9A07C after DirectDraw programs, 2026-11-15

## What happens

A blue screen, `A fatal exception 0E has occurred at 0137:BFF9A07C`, a second press giving
`0137:BFF76592`, minutes after a DirectDraw program has run. Reproduced:

| where | driver | sequence | result |
|---|---|---|---|
| card | `m8shf.drv` (`33b1104`) | wormhole (8 bpp from 16 bpp), then the next program started | BSOD, twice (boots 401, 402) |
| card | `m8nof.drv` (`32bfaac`, without the shadow free) | the same | BSOD (boot 404) |
| bed | `3aaea3a` + 8 bpp refused from a 16 bpp desktop | wormhole refused cleanly, flip2d at 16 bpp, then mosquito | **the same BSOD, same address** (`captures/2026-11-15_safe_bed/3-mosquito.png`) |
| bed | `3aaea3a` | donuts after several 8 bpp round trips | Explorer "illegal operation", agent gone, CPU in VMM |

So it is **not** the 16 -> 8 -> 16 bpp change, and **not** the 8 bpp shadow free: the bed shows it with
neither. Not yet known which program or call starts it.

## Where the addresses are

KERNEL32.DLL from the bed image (`tools/fatls.py vm_5160_mcp/card.img --get`), base `BFF70000`,
disassembled with MSYS2 objdump:

- The function containing `BFF9A07C` (starts `BFF9A00E`) builds an EXCEPTION_RECORD from its four
  arguments - code, flags (bit 0), parameter count capped at 15, parameter array - and calls
  `BFF76588`. `BFF9A07C` is the return address of that call: this is **RaiseException**.
- `BFF76592` is inside `BFF76588`, the dispatcher.

Both screens are one event: something called RaiseException, and the exception was not handled.
Win95 shows an unhandled ring-3 exception as a fatal-exception screen, not the usual dialog, when it
cannot show the dialog (e.g. the Win16 lock held - true inside DirectDraw HAL callbacks).

## Ruled out today

- **"GDI stays 16 bpp" under an 8 bpp program** (patch kept out of git): Win95 DirectDraw takes the
  primary's depth from GDI. `tools/m8probe/M8DDMODE.EXE` (each DirectDraw call's result) on the bed:
  SetDisplayMode(640,480,8) succeeds, the primary comes back 640x480 **16 bpp**, SetPalette fails
  `88760091`. Not a route on Win95.
- Dr. Watson is not installed on the bed. The bed's 86Box (`86box_auto`) automation has hold, step,
  regs and memory access but no breakpoints.

## Next, cheapest first

1. Which program and which exception code: a debugger-enabled 86Box (MCP `vm_start(debugger=True)`)
   on a copy of the bed, breakpoint at `BFF9A00E`, read code and caller from the stack. One run.
2. If the caller is M8DD32.DLL or the Watcom runtime in it: the HAL runs in every DirectDraw
   process from the shared arena; read its exception and runtime set-up first.
3. Until then: **no DirectDraw programs on the card.** The bed reproduces it.
