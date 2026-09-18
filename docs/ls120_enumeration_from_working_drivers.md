# How the working drivers enumerate, and what we do instead

Read out of two references on 2026-09-18, both of which enumerate this class of
device successfully:

- **`SD120PPD.MPD`** — Imation's own Windows 95 miniport for *this drive*.
  Disassembly `drivers/imation_ls120/SD120PPD_MPD.asm`, 22,503 instructions,
  spanning `0x000400`-`0x01004b` — both executable sections (`.text` 0x400 +
  0xfc00, `PNP` 0x10000 + 0x200), 62.6 KB, verified against the PE section
  table (technique 112 rule 2).
- **`PC2X.C`** — Microsoft's DDK parallel-port SCSI miniport sample, 1,786
  lines of C, `BLOCK/SAMPLES/MINIPORT/PC2X/` in the Win95 DDK.

Addresses below are **rva**. The image base is `0x10000`, so a pushed callback
immediate of `0x148bb` is rva `0x48bb`.

---

## 1. The entry points the vendor registers

`DriverEntry` (rva `0x3793`) builds `HW_INITIALIZATION_DATA`, size `0x4c`:

| offset | field | value | ours |
|---|---|---|---|
| +0x08 | `HwInitialize` | `0x3277` | `LsInitialize` |
| +0x0c | `HwStartIo` | `0x29ac` | `LsStartIo` |
| +0x10 | `HwInterrupt` | `0x2892` | **none** |
| +0x14 | `HwFindAdapter` | `0x3854` | `LsFindAdapter` |
| +0x18 | `HwResetBus` | `0x2914` | |
| +0x20 | `HwAdapterState` | `0x384f` (`mov al,1 / ret 0xc`) | |
| +0x24 | `DeviceExtensionSize` | `0x11` | |
| +0x28 | `SpecificLuExtensionSize` | **`0x7a`** | **0** |
| +0x38 | `MapBuffers` | **0** | TRUE |

It then calls `ScsiPortInitialize` **twice** — `AdapterInterfaceType` 1 (Isa)
then 3 (MicroChannel) — and returns the lower of the two status codes. The
second call is pointless on this machine and is the known double-registration.

## 2. `HwFindAdapter` does NOT touch the hardware

`pedis.py io` finds **2,329 port-I/O sites** in the whole binary and **not one
of them lies between `0x3854` and the end of `HwFindAdapter`.** Detection is
not a probe. What it actually does:

1. Loads the three LPT bases into a local array — `0x378`, `0x3bc`, `0x278`
   (`0x385d`-`0x386b`).
2. Parses the `AdapterSettings` string through a helper at `0x32c0`, called as
   `opt(argstring, name, default)`. The recognised names, read out of `.data`:

   | string | rva | default | stored at |
   |---|---|---|---|
   | `BLK` | `0x20654` | 1 | `0x20494` |
   | `DMA` | `0x20658` | 1 | `0x204c4` (0 → forced to 3) |
   | **`ECP`** | `0x2065c` | **1** | `0x204bc`, and sets `0x204c0` = 1 |
   | `MSN` | `0x20660` | 1 | `0x204a8` |
   | `NATN` | `0x20664` | 1 | `0x204ac` |
   | `NDPC` | `0x2066c` | 1 | `0x204b0` |
   | `port` | `0x20674` | 1 | overrides the base table |
   | `size` | `0x2067c` | 1 | transfer size, `<<11` |
   | `LPT1` | `0x20684` | — | PnP lookup key |

   ⭐ **`ECP` defaults to 1.** The vendor miniport is an ECP driver unless told
   otherwise, which is consistent with the DOS driver selecting ECP Read/Write
   on this machine.
3. Reads PnP/registry through the `PNP` section thunks (`0x10008`, `0x10030`,
   `0x10038`, `0x10048`).
4. Fills `PORT_CONFIGURATION_INFORMATION` and returns `SP_RETURN_FOUND`.

The DDK sample is the one place a probe exists — `PC2xCheckBaseAddress`
wiggles a DMA bit and reads it back — and even that is a two-register
read/write with no waiting.

**We do the opposite:** `LsFindAdapter` reads the status register, and
`LsInitialize` calls `LS_BringUp`.

## 3. `HwInitialize` announces a reset and returns. It does not wait.

Vendor, rva `0x3277` — 20 instructions, **no port I/O**:

```
mov cx,[0x21784] / mov [0x20c18],cx      ; latch config
call 0x2794 / call 0x27a1 / call 0x2799  ; three short setups
push esi / push 3 / call 0x281a          ; ScsiPortNotification(ResetDetected, devext)
xor ecx,ecx / mov al,1                   ; TRUE, unconditionally
ret 4
```

`PC2X.C:1396` is the same shape, and it is the readable version:

```c
PC2xWriteControl(deviceExtension, C_IDLE);
PC2X_READ_STATUS(deviceExtension->BaseAddress);
PC2xWriteControl(deviceExtension, C_RESET);
ScsiPortStallExecution(RESET_HOLD_TIME);        /* the HOLD time only */
ScsiPortNotification(ResetDetected, deviceExtension);
return TRUE;
```

**It asserts reset, stalls only the pulse-hold time, tells SCSIPORT a reset
happened, and returns.** It never waits for the device to become ready.
`ResetDetected` is what makes SCSIPORT re-scan, and the scan's INQUIRYs are
where the waiting actually happens — one SRB at a time, through the timer.

**We do the opposite.** `LsInitialize` calls `LS_BringUp`, and
`LS120TR.ASM:193` is `LS_SPIN_RESET equ 0FFFFh ; ~3.0 s`. We spin for up to
three seconds inside `HwInitialize`, at boot, with the parallel port held.

## 4. Everything else runs on a 1 ms self-re-arming timer

Vendor, at `0x2854`, reached whenever the engine says "not finished":

```
push 0x3e8       ; 1000 microseconds
push 0x148bb     ; rva 0x48bb - the timer routine
push devext
push 6           ; RequestTimerCall
call 0x281a      ; ScsiPortNotification  (thunk: jmp [0x21878])
```

The timer routine at `0x48bb` **re-arms itself at the top, before doing any
work** (`0x48db`-`0x48f2`), so it is a free-running heartbeat. The engine step
it drives is `0x2892` — which is also the registered `HwInterrupt`, so the
interrupt path and the polled path are the same code.

`PC2X.C:932` again states it plainly:

```c
PC2xTimer(Context) {
    if (deviceExtension->InterruptPending == FALSE) return;
    restartTimer = PC2xInterrupt(Context) == FALSE;
    if (restartTimer)
        ScsiPortNotification(RequestTimerCall, deviceExtension,
                             PC2xTimer, PC2X_TIMER_VALUE);
}
```

**We have no timer.** `ScsiPortNotification` appears twice in `LS120MP.ASM`,
both in the completion path — `RequestComplete` and `NextRequest`. There is no
`RequestTimerCall` anywhere in the driver. Every wait is a spin inside the call
SCSIPORT made.

## 5. `HwStartIo` answers four SRB functions, not one

Vendor dispatch on `srb->Function`, `0x29f0`-`0x2a27`:

| function | | handler |
|---|---|---|
| `0x00` | EXECUTE_SCSI | `0x2a2c` |
| `0x02` | IO_CONTROL | `0x2b17` |
| `0x10` | ABORT_COMMAND | `0x30e5` |
| `0x12` | RESET_BUS | `0x3119` |
| anything else | | `SrbStatus = 6` (INVALID_REQUEST), complete |

---

## Why this explains "works in the VM, fails on hardware"

The inline design costs nothing in the bed and is ruinous on the bench, and
nothing in between:

| | emulated bridge | real 5160 |
|---|---|---|
| one byte over the link | free | ~39 us |
| BSY after a command | instant before 2026-09-18 | measured still set at 0.5 s |
| settle after SRST | instant before 2026-09-18 | **seconds** |

So every inline wait returns immediately in the bed and holds the machine for
seconds on hardware — with interrupts masked on the nibble path. That is the
Explorer egg-timer, and it is invisible to any bisect, because every commit on
both sides of the range has the same shape.

⚠ **The honest counter-example.** `a925038` was fully synchronous and *did*
enumerate on the real machine (photographed at `L:`, 119.0 Mb, 09-11 12:10).
So inline is not impossible — it is **marginal**, working when every wait
happens to return fast enough. Changing the transport to ECP changes those
timings, which is exactly the kind of thing a marginal design fails on. The
reference shape does not have a margin to lose.

## The change, in order

1. **`LsInitialize`: delete the `LS_BringUp` call.** Pulse SRST, stall the hold
   time only, `ScsiPortNotification(ResetDetected, devext)`, return TRUE.
2. **`LsFindAdapter`: delete the status read.** Options and config only.
3. **Add the engine step and the timer.** One routine that advances the current
   SRB by one step and returns done/not-done; on not-done,
   `ScsiPortNotification(RequestTimerCall, devext, LsTimer, 1000)` and return.
   Register it as `HwInterrupt` too, as both references do.
4. **Convert every `LS_*Wait`/`LS_PfWait` spin into a state the engine polls**,
   with its own deadline counted in ticks, not iterations. Keep the reset
   settle (seconds) and a status poll (milliseconds) as **separate** deadlines
   — technique 122.
5. `SpecificLuExtensionSize` non-zero, `MapBuffers` FALSE, and answer
   `IO_CONTROL` / `ABORT_COMMAND` / `RESET_BUS` instead of failing them.

Steps 1-3 are the ones that decide whether the drive appears. 4 is the bulk of
the work. 5 is cheap and should ride along.
