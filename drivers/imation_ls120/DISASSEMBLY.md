# The vendor drivers, disassembled in full

Produced 2026-09-11 so that nothing about this driver has to be guessed again.

| file | source | lines | coverage |
|---|---|---|---|
| `SD120PPD_MPD.asm` | `SD120PPD.MPD.orig` (Windows miniport, 79,872 bytes) | 22,503 | **complete** |
| `SD120PPD_SYS.asm` | `SD120PPD.SYS.orig` (DOS driver, 56,198 bytes) | 6,238 | from the entry points |

## The MPD dump is complete - here is the proof

```
ImageBase 00010000  sections 6
.text    VA 00000400 vsize   fa12      <- all executable code
PNP      VA 00010000 vsize     4e      <- and this
.data    VA 00010200 vsize   1598
.idata   VA 00011800 vsize    20c
.rsrc    VA 00011c00 vsize    3cc
.reloc   VA 00012000 vsize   1750
```

`.text` + `PNP` are the only sections marked executable (`chars 60000020`). The dump runs
`0x400` to `0x1004a`, which is both of them end to end.

⚠ **RVA vs linked address.** ImageBase is `0x10000`, so an address pushed as a callback is
`0x10000 + RVA`. `push 0x148bb` means **RVA `0x48bb`**. Getting this wrong makes a valid
code pointer look like it lies outside every section.

## Verified facts about the miniport

Each of these is read from the dump, not inferred.

**`HwInitialize` (rva `0x3277`) performs no device I/O.** Twenty instructions: copy a
config word, stash `ConfigInfo`, three small setup calls, one `ScsiPortNotification`,
zero two fields, `mov al,1`, `ret 4`. No reset, no INQUIRY, no sense. It cannot fail.

**`0x281a` is `ScsiPortNotification`.** It is a one-instruction thunk,
`jmp dword ptr [0x21878]`, and `0x21878` is `ImageBase + 0x11878`, the `.idata` slot the
import table lists for `ScsiPortNotification`.

**The driver polls on a self-re-arming timer.** At rva `0x48bb`:

```
cmp  [0x21718], 0        ; is the engine still running
call [0x21790]           ; optional per-tick hook
push [0x216cc]           ; interval
push 0x148bb             ; ITS OWN linked address
push <DeviceExtension>
push 6                   ; RequestTimerCall
call 0x281a              ; ScsiPortNotification
```

A routine that passes its own address as the callback is a self-perpetuating poll. It is
reached from `0x2840`, which calls `0x2892` (`HwInterrupt`) and re-arms only when that
returns FALSE - poll, and try again next tick.

**`HwStartIo` (rva `0x29ac`) dispatches on the SRB Function and returns without
completing.** Handled: `0x00` EXECUTE_SCSI, `0x02` IO_CONTROL, `0x10` ABORT_COMMAND,
`0x12` RESET_BUS. Anything else gets `SrbStatus = 6` (INVALID_REQUEST) and is completed
immediately.

**Imports used (13):** `ScsiPortGetDeviceBase`, `ScsiPortCompleteRequest`,
`ScsiPortGetLogicalUnit`, `ScsiPortInitialize`, `ScsiPortFreeDeviceBase`,
`ScsiPortConvertUlongToPhysicalAddress`, `ScsiPortNotification`,
`ScsiPortStallExecution`, `ScsiPortLogError`, `ScsiPortReadPortBufferUchar`,
`ScsiPortReadPortBufferUlong`, `ScsiPortWritePortBufferUchar`,
`ScsiPortWritePortBufferUlong`.

Note `ScsiPortStallExecution` **is** imported - short in-command delays are legitimate.
It is multi-second waits that are deferred to the timer.

## Where our driver differs

| | vendor | ours |
|---|---|---|
| `HwInitialize` | no device I/O, returns TRUE | reset, INQUIRY, sense drain |
| `HwStartIo` | starts work, returns | runs the whole command inline |
| waiting | 1 ms timer callback, re-armed | blocking spin loops |
| SRB functions | 4 | 1 |
| VMM/port services | 13 imports | 2 |

## DOS driver entry points

Device header: `next FFFFFFFF`, `attributes C000`, **`strategy 0x2074`**,
**`interrupt 0x2082`**, name `SCSIMGR$`. The dump starts at the strategy routine; code
before `0x2074` is header and data.
