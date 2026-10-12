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

## Handler install and trapping state (raw bytes, 2026-10-12)

- Every `Install_IO_Handler` is followed at once by `Disable_Global_Trapping` on the same port: 01CE/01CF at
  1B2Dh (`mov esi,handler / mov edx,01CE / int 20h 96h / jc / int 20h 99h`), and a 4-port helper at 1D95h
  (`Install / jc / Disable / inc edx`, four times) for 4AE8-4AEB and 4AEE-4AF1. **Trapping is off after install.**
- 4-port helpers at 5FD3h switch trapping on (`97h`, `inc edx` x4) and off (`99h`) as a group, so 4AE8/4AEE are
  in the disable set too (the first scan missed them: the port is not a constant).
- No local trapping services at all; the switching is driven from callbacks the VDD (device 0Ah: services 0Eh,
  0Fh x30, 10h, 12h, 8015h) makes into ATI.VXD - almost certainly when a DOS box takes the screen. That
  context is NOT yet read.
- Consequence: in normal running Windows' own accesses to 4AE8/4AEE/01CE do not trap - no bus or CPU cost to
  our driver, and per-frame output switching (worklist C2/C3) is not blocked. What ATI.VXD does NOT do is save
  registers it does not know (EEEE DEST_CMP_FN, the BOUNDS registers): the reason for our own VxD later.
