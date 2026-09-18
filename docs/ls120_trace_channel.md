# Getting a trace out of a Win95 miniport on the 5160

The LS-120 miniport ran for eight days with no way to see inside it. This is the
channel that fixes that, how to prove it works, and what a green result does and
does not mean.

Method and reasoning: `inboard-hw-debug` technique 123.

## Why the obvious channels are not available

| channel | why not |
|---|---|
| COMrade | DOS only on real hardware. Nothing while Windows runs |
| `-DbgPort` | bed only - there is no such port on the 5160. The switch also had no source behind it |
| `BOOTLOG.TXT` | IOS writes it and we cannot add to it. `Init Success` means the driver LOADED, nothing more |
| `IOS.LOG` | only written for failures IOS thinks notable; absent on every boot so far |
| file I/O | not available to a miniport |
| speaker / port `0x61` | bit 7 is the XT keyboard-latch acknowledge. Do not |

## The channel

Windows' VMM identity-maps the first megabyte, so a ring-0 store to linear
`0B9000h` is physical `0B9000h` - text-mode VRAM page 1. Nothing in a VGA
graphics session writes it, DOS rewrites only page 0, and Ctrl+Alt+Del sets
`40:72 = 1234h`, which makes POST skip the memory test. So the buffer survives a
warm reboot into DOS, where COMrade can read it.

Records are 16 ASCII bytes, so a raw `mem_read` dump is already readable:

```
TTTT=XXXXXXXX<CR><LF><SP>
```

Header: `+0` magic `"LS12"`, `+4` a running record count that keeps climbing past
the 200-record ring, so wrap is visible rather than silent. The ring is filled
with `.` on the first record of each boot, so a short run looks short instead of
trailing into the previous boot's records.

Build with `-Trace`. `-TraceBase <addr>` moves it if the poison test rules the
default out.

## Tags

| tag | where | value |
|---|---|---|
| `ENTR` | `DriverEntry` | 0 |
| `FADP` | `LsFindAdapter` entry | 0 |
| `FOUN` | `LsFindAdapter` returning `SP_RETURN_FOUND` | 0 |
| `BRUP` | after `LS_BringUp` in `LsInitialize` | its verdict - which `HwInitialize` deliberately does not act on |
| `SRBF` | `LsStartIo` entry | SRB `Function`; `0` = `EXECUTE_SCSI` |
| `CDB0` | `LsStartIo` entry | CDB byte 0; `12h` = INQUIRY, the one that creates the device |
| `DONE` | completion | `SrbStatus`; `1` = SUCCESS |

The question the trace exists to answer: **does `CDB0=00000012` ever appear, and
what `DONE` follows it.** If INQUIRY never arrives, the fault is above us. If it
arrives and fails, it is the transport.

## ⛔ Prove the channel before believing a record

A channel that silently drops writes is indistinguishable from a driver that
never ran. A VGA in a graphics mode may leave `B8000` unmapped.

**Poison test, one boot:**

1. At a DOS prompt with COMrade up, write a known pattern over `0B9000h`
   (`mem_write`, 256 bytes is plenty).
2. Read it straight back to confirm the write landed.
3. Boot Windows and let it reach the desktop.
4. Ctrl+Alt+Del to warm-reboot to DOS. **Not** a power cycle.
5. Start COMrade and `mem_read 0B9000h`.

If the pattern is still there, the page is retained and the channel is sound. If
it is gone, the trace will be empty for a reason that has nothing to do with the
driver - move `-TraceBase` and retest.

Run this **before** the first traced boot, so a blank ring afterwards means
something.

## Two claims, kept apart

Testing in the 86Box bed proves the records are written, the format is right and
the read-back procedure works. It does **not** prove retention on the real
machine, because the bed's video is a model. A green bed run supports the first
claim only; the poison test on hardware is what supports the second.
