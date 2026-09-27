# HSFLOP.PDR does not poll during a seek (#41, A17)

The plan said `HSFLOP.PDR` *"polls hard during a seek"*. That was never measured. Disassembly of
`vxd-patches/floppy/HSFLOP_XTDMA.PDR` (the deployed file) says it does not.

## Method

`tools/le_poll_scan.py`: walks every executable LE object, decodes `CD 20` VxD calls as 6 bytes,
and reports any backward jump of at most 0x60 bytes whose body contains an `IN`. Objects decoded
end to end (9, 0 and 1 undecodable bytes: data between functions).

## Result

| | |
|---|---|
| `IN` / `OUT` | 13 / 34 in the whole driver |
| seek | `SEEK` (`0Fh`) at file `0x2FD3` is written and the driver returns; completion arrives on IRQ 6 through VPICD (`Virtualize_IRQ`, `Phys_EOI` imported) |
| delays | `Set_Global_Time_Out` (4 sites), not spin loops |
| `VTD_Get_Real_Time` (4 sites) | timestamps used to work out which sector is under the head after a seek - rotational positioning, not a wait |

Poll loops found - all on the FDC main status register (`3F4h`), all waiting for RQM:

| file | what | bound |
|---|---|---|
| `0x82F` | before each result byte | `2000h` reads |
| `0x85E` | before each command byte | `20000h` reads |
| `0xFAB` | before one result byte | **none** |
| `0x334E` | after a command, only when flag `[edi+79h]` bit 7 is set | 20,000,000 reads |

The 8272 raises RQM within microseconds between bytes, so the first three normally exit after one
or two reads. `0x334E` is the only one that could spin for long; which command sets its flag is not
yet known. `0xFAB` has no timeout: if the controller never raises RQM, that thread never returns.

## What it changes

- A17 is downgraded: no evidence the floppy driver spends the bus while a seek runs. #41's weight
  moves to `T130.MPD` (polled on every SCSI transfer) and `ELNK3.VXD`.
- The owner's "floppy feels slow" is not this driver's polling. DOS mode, where `HSFLOP.PDR` is not
  running, reads B: at 23.2 KB/s against a ~45 KB/s ceiling.
- Sizing `0x334E` would need a count of `3F4h` reads per operation - an 86Box diagnostic build,
  but 86Box's FDC may raise RQM instantly and understate it. Not worth a build unless the floppy
  becomes a priority.
