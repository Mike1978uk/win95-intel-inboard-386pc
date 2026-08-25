# Floppy lockup / issue #3 - state at end of 2026-08-25

Written as a handoff. Token budget resets Thursday evening; this is the record to resume from.

## Settled

**There was no floppy disk controller installed in Windows 95.** Verified on the live CF and on
`win95_cpu_vid_snd.img` (identical `BOOTLOG.TXT` 20516 / `SYSTEM.DAT` 378044):

- `BOOTLOG.TXT` - zero matches for "flop"
- IOSUBSYS loaded `rmm.pdr` (Real Mode Mapper) but no floppy driver
- `SYSTEM.DAT` had no `*PNP0700` node and no `fdc` class key

`A:`/`B:` still appeared in My Computer because the letters come from the BIOS, so the shell offered
drives nothing could service. First read never returned, thread blocked. **Issue #3 was never a
Browse bug** - the Have Disk dialog defaults to `A:\` and dies on that read.

## Done on the real machine (user, 2026-08-25)

Add New Hardware -> conflict reported -> resolved via the conflict troubleshooter, configuration set
**manually** -> reboot. Result:

- Device Manager: *Standard Floppy Disk Controller* under Floppy disk controllers; two
  `GENERIC NEC FLOPPY DISK` under Disk drives.
- Registry now has `HardwareID*PNP0700`, `Classfdc`, `Driverfdc\0000`, `InfPathMSFDC.INF`,
  `PortDriverHSFLOP.pdr`, and a **`ForcedConfig`** on the controller node (the manual assignment).
- Help files now build and open - **may bear on #7**, unproven.
- Reading a disk in **A: (3.5", correctly detected)**: showed a **partial directory listing**, then
  stalled. Same disk reads fine in DOS. **B: not tested** (wrong disk geometry to hand).

The blue screen photographed is **not a BSOD** - it is the Ctrl-Alt-Del "system is either busy or
has become unavailable" screen, i.e. a blocked thread, no fault, no crash dump.

## Ruled out

- **Geometry / sectors-per-track.** Was the leading theory (1.2 MB 5.25" is 15 sectors, 1.44 MB 3.5"
  is 18, same 500 kbps - a 5.25" driven as 3.5" reads 1-15 then fails). **Dead**: the drive that
  stalled is the 3.5", and it is correctly detected as such.
- **`DMABufferIn1MB`.** Already `True`, `DMABufferSize=64`.
- **Corrupt `VDMAD`.** Never deployed - `a4fd183b` sits unused in `C:\patched_files\` and
  `C:\WINDOWS\SYSTEM\VMM32\` holds only `MRCI2.VXD` + `QEMMFIX.VXD`, so `VMM32.VXD` was never
  rebuilt with it. Sound works because `MSSBLST.VXD` is dynamically loaded.

## Deployed, effect UNMEASURED

`HSFLOP.PDR` had `maxPhys = 0x1000` (16 MB, the AT assumption) at file offset `0x3eb4`, on a machine
whose DMA reach is 1 MB. Patched to `0x9F` (two bytes at `0x3eb5`/`0x3eb6`, same 5-byte push
encoding).

- On the CF: `C:\WINDOWS\SYSTEM\IOSUBSYS\HSFLOP.PDR` = `8e695d00` (patched), stock backed up on the
  card at `C:\patched_files\HSFLOP_STOCK.PDR` = `b792b281`.
- Confirmed the FDC install did **not** overwrite it (`MSFDC.INF` is `AddReg` only for the standard
  controller).
- **We do not know whether this patch does anything.** It was deployed pre-emptively, before the
  bug was demonstrated.

## In flight: the measurement that settles it

`vm_fdd/` - a copy of `win95_cpu_vid_snd.img` with two files transplanted **via `fatput.py`**
(same-size, same cluster chain, FAT untouched):

| file | md5 |
|---|---|
| `C:\WINDOWS\SYSTEM.DAT` | `6196eb49` - taken from the CF **after** the controller install |
| `C:\WINDOWS\SYSTEM\IOSUBSYS\HSFLOP.PDR` | `8e695d00` - patched |

So the emulator image boots with the controller already installed - no GUI install needed. `A:` is a
blank 1.44 MB image; any read attempt programs the DMA page register, which is all the test needs.

Run with `tools/dma_fdd_probe.ps1`, which sets `INBOARD_DMA_TRACE=1` and captures:

```
[dmapage] ch=%d val=%02X -> page=%02X *** TRUNCATED, buffer is above 1MB *** (xt_latch=%d)
```

86Box models this correctly for the machine - `dma_force_xt` exists specifically because `dma_at` is
derived from CPU type and would hand a 386-in-an-XT a full 8-bit page register, hiding the bug.

**What the trace answers:** `ch=2` truncated -> DMA is the cause and the patch matters. `ch=2` clean
-> the `maxPhys` patch is doing its job or was never needed, and the stall is elsewhere.

## If DMA comes back clean, next suspects

1. **The `ForcedConfig` may not include a DMA channel.** The user set resources manually after a
   conflict. I could read `I/O 0x3F2-0x3F5` out of the blob but **could not honestly decode IRQ/DMA**
   from raw bytes. **Get Device Manager -> Resources tab** and check a DMA channel is listed at all.
   A config with I/O + IRQ but no DMA gives exactly this symptom: seeks, returns first sectors, then
   any DMA transfer never completes.
2. **IRQ 6 delivery in V86.** A read that starts and never completes with the motor left on fits a
   completion interrupt that never arrives. This machine has form here - the custom `VKD.VXD` had to
   be rebuilt from DDK source because `VKD_Int_09` assumed an AT-style 8042.
3. **64 KB boundary.** An 8-bit DMA channel cannot cross a 64 KB physical boundary. Partial success
   then failure is consistent with a buffer that straddles one.

## Cheap things not yet done

- Boot the machine with **logged boot (F8 -> Logged)** and re-read `BOOTLOG.TXT`. The current one is
  stale - byte-identical to the pre-install image - so it still shows no floppy driver. A fresh log
  would confirm `hsflop.pdr` actually loads.
- Test **B:**.
- Revert to stock `HSFLOP.PDR` (one-line copy, backup is on the card) to find out whether the
  `maxPhys` patch was load-bearing.

## Constraint found while setting the test up

The first probe run reached only `CS:PC=0575:1399` (real mode, still in DOS) at **t+83s**. Two
reasons, both fixable:

1. **A full Windows 95 GUI boot in 86Box on this machine takes minutes, not seconds.** Budget
   15-20 minutes of wall clock for the run, unattended.
2. **This build carries heavy instrumentation that is not the Inboard's.** `stderr` filled with
   4000 `[trace]`, 2000 `[pitxctrl]`, 800 `[iotrace]`, 500 `[pitctrlpc]`, 400 `[gaptrace]`,
   200 `[port62]` lines in 83 seconds. That is a large per-instruction cost and it is what makes
   the guest crawl.

**Do this first next time:** build with the other trace sites off (or find their env gates) so only
`INBOARD_DMA_TRACE` is live. The `[dmapage]` hook itself is cheap - it only fires on a page-register
write. Recall the POST-101 lesson (issue #14): a heavily instrumented build runs few enough guest
instructions per second to change behaviour, so a slow build is not merely inconvenient here, it can
invent symptoms.
