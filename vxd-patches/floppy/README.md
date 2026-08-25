# Floppy on the Inboard: two bugs that chain

## 1. There is no floppy controller installed in Windows 95

Confirmed from `win95_cpu_vid_snd.img` (the live image), not from a screenshot:

- `BOOTLOG.TXT` - **zero** matches for "flop". `HSFLOP.PDR` never loads.
- IOSUBSYS loads: `apix atapchng cdfs cdtsd cdvsd disktsd diskvsd drvspacx necatapi
  rmm.pdr scsi1hlp torisan3 voltrack` - no floppy driver among them.
- `SYSTEM.DAT` - **no `*PNP0700` node and no `fdc` class key**. Only the class description
  string `"Floppy disk controllers"`, which every install has.

`A:` and `B:` still appear in My Computer because the drive letters come from the BIOS. The
shell offers drives nothing can service, the first read never returns, and the calling thread
blocks - motor on, light on, hourglass, eject irrelevant.

This is also the whole of the "Browse button faults" bug (issue #3): the Have Disk dialog
defaults to `A:\`, issues that read, and dies there.

DOS reads both drives fine because DOS uses INT 13h directly and never needed a Windows driver.

## 2. When you DO install one, `HSFLOP.PDR` has an XT DMA bug waiting

`HSFLOP.PDR` allocates its floppy DMA buffer with `maxPhys = 0x1000` - the standard AT
assumption, "anywhere below 16 MB":

```
0x3eb4  68 00 10 00 00   push 0x1000    maxPhys == 16 MB
...
0x3ec9  cd 20            _PageAllocate
```

This machine's DMA reach is **1 MB** - the PC/XT page latch is 4 bits, and an Inboard 386 in a
5160 keeps the XT's latch. With 5 MB installed the buffer can land anywhere from 1-5 MB, the
page register drops the high bits, and the controller transfers against a different physical
address. Same bug, same shape, as the Sound Blaster Pro one (`MSSBLST.VXD`).

**So installing the controller alone may just move the failure**, from "hangs because nothing
drives it" to "hangs because the data never arrives". Do both.

## Files

| file | |
|---|---|
| `HSFLOP_stock.PDR` | as extracted from the live image, 18998 bytes |
| `HSFLOP_XTDMA.PDR` | `maxPhys` `0x1000` -> `0x9F`, 2 bytes changed at `0x3eb5`/`0x3eb6`, same size |

`0x9F` is 640 KB, the strict XT ceiling this project already uses (386MAX `@DMA_PA_XT`), rather
than `0xFF` (1 MB) - above 640 KB is adapter ROM and video, not allocatable RAM.

## Deployment

`HSFLOP.PDR` lives in `IOSUBSYS` and is loaded **dynamically by IOS at runtime**, not bundled
into `VMM32.VXD`. So unlike the VDMAD/VKD work this is a **plain file copy** - no pre-monolith
staging needed. Verify against `BOOTLOG.TXT` after installing the controller: you want to see it
load before you trust the patch.

## Order of operations

1. Add New Hardware -> **do not** let it search -> select from list -> **Floppy disk
   controllers** -> *Standard Floppy Disk Controller* (IRQ 6, DMA 2, I/O `0x3F0-0x3F7`).
   The auto-detect path avoids the Browse button, which is the thing that is broken.
2. Reboot, confirm `hsflop.pdr` now appears in `BOOTLOG.TXT`.
3. Copy `HSFLOP_XTDMA.PDR` over `C:\WINDOWS\SYSTEM\IOSUBSYS\HSFLOP.PDR`.
4. Reboot and test `A:` in Explorer.

Worth doing in emulation first - it costs nothing and is repeatable.

## Note on the audit tooling

`vxd_dma_audit.py` originally reported this driver as clean ("maxPhys is a register - not
statically decidable", "0 allocation(s) exceed the XT's 20-bit DMA reach"). That was a false
negative caused by the walk-back stopping at the first non-`push`; this compiler interleaves
`xor`/`shl`/`mov` between the pushes. Fixed in commit `935fe99`.
