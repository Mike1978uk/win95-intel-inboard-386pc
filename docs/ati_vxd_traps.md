# ATI.VXD - what the Win95 Mach8 VDD traps (2026-10-12)

`ATI.VXD` (Microsoft-shipped with `ATIM8.DRV`, `msdisp.inf [ATI8]`, `DevLoader *vdd`), static read of the copy
on the card. Each port is `mov edx,imm32` then an `int 20h` VMM call; services named with
`tools/pdr_vxd_services.py`.

| VMM service | calls | ports |
|---|---|---|
| `Install_IO_Handler` (096h) | 11 | 0080, 01CE, 01CF, 02E8, 4AE8, 4AE9, 4AEE, 4AEF (+3 not resolved) |
| `Enable_Global_Trapping` (097h) | 74 | 65: the drawing registers (colours, coordinates, mixes, masks, PIX_TRANS 9AEE/E2E8, the xxEE extended set), plus 01CE/01CF, 4AE8/4AEE, and one read as 62EC (unresolved; perhaps 62EE) |
| `Disable_Global_Trapping` (099h) | 73 | the same set without 4AE8/4AEE |

Reading: the handlers sit on what decides **who owns the screen** - ADVFUNC_CNTL (4AE8, VGA or 8514 to the
monitor), CLOCK_SEL (4AEE, also switches output), the 28800 index pair (01CE/01CF) and 02E8. The global set is
switched on and off as a group: the engine fenced from DOS boxes while Windows owns it, and open to the owner.

For our driver: on Windows 95 a replacement for `ATIM8.DRV` can keep `ATI.VXD` (it already does the DOS-box and
mode-switch work) as long as it switches output through 4AE8/4AEE the way ATIM8 does. Not yet read: the handler
bodies (what each does on a trapped write), and the 3-10 call sites per service whose port is not a constant.
Recovers the worklist's "ATI.VXD port-trapping lead - lost 10-10".
