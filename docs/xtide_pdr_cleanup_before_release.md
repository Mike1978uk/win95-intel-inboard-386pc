# XT-IDE port driver — what must be cleaned up before this ships

Written 2026-09-01, the night Windows 95 first read and wrote the real XT-CF through this
driver. The milestone is real; the driver is **not releasable**. This is the list, ordered by
what would hurt someone else's machine first.

Nothing here is speculative — each item is something observed or deliberately left undone.

## Blocking — would damage or annoy a stranger's system

1. **Shutdown hangs.** Observed on the 5160 on the claiming run: Windows would not complete a
   shutdown and the machine had to be powered off. We handle no teardown at all — no volume
   unmount, nothing on `AEP_UNCONFIG_DCB` or system shutdown. A boot volume that cannot be
   cleanly unmounted is worse than one that cannot be taken over, so this gates everything
   after it.

2. **Two volumes over one partition.** We publish our own logical volume from the boot disk
   while the Real Mode Mapper still owns C:, so the same filesystem is mounted twice with two
   independent caches. It survived a full boot on hardware and on a clone, but it is a
   corruption risk by construction and must not ship. The fix is the C: takeover - claim the
   DCB that already exists rather than creating a second one - not a warning in a README.

3. **`-NoWriteTest` is not the default.** The probe's write self-test writes a sector to LBA
   100 of whichever unit answered. On a single-disk machine that is the boot volume, and its
   only guard is a DEV-bit read-back. It must be off by default and opt-in for a scratch
   slave, not the other way round.

4. **The request marker must be impossible in a shipped build.** `-ReqMarker` writes a sector
   to the claimed unit. It is already opt-in and verified absent from hardware builds by
   listing, but the data block is still linked in unconditionally. Make the whole thing
   conditional, so there is no path and no payload.

## Correctness — known-wrong, not yet harmful

5. **The device node reserves `0300-030F`, sixteen ports.** Stride 2 puts alternate status at
   `0x31C`, outside the declared range. Win9x resource lists are bookkeeping rather than
   enforcement so it works, but `PORT.INF` should declare `0300-031F`.

6. **`DriverDesc` still says "phase 1 - IDENTIFY only".** It services 3000+ requests a boot.
   The string is what a user sees in Device Manager.

7. **`IORF_LOGICAL_START_SECTOR` handling is written but has never executed.** Every request
   measured, on both beds and on hardware, arrived absolute (`IOR_flags 0x401`). The bias code
   is there because a volume taken over from RMM may not be absolute - but it is unexercised,
   and unexercised code is where every expensive bug this project has had came from.

8. **Stride autodetect has only ever chosen 1.** 86Box's XT-IDE is stride 1, so the branch that
   picks 2 has never run. Hardware builds pin `-Stride 2` and should keep doing so until
   autodetect gets a run of its own.

## Environmental — true of the test machine, not of the driver

9. **The real-mode SCSI/ASPI chain is REM'd out of `CONFIG.SYS` on the 5160.** That is what
   let IOS load the driver at all - `MODISK2.SYS` was flagged `Unsafe`/`Monolithic` and pinned
   six units in real mode. It is a workaround. The devices are meant to come back through the
   32-bit T130 miniport (issue #19), and until they do, this driver's success on that machine
   is conditional on hardware being switched off.

10. **One real-mode unit remains**, `SD120PPD`/`ASPIHDRM` - the parallel-port LS-120. Worth
    removing next to see whether IOS goes fully clean, and it is a one-line REM.

## Not defects, but state the reader needs

- **ATAPI/CD is permanently out of scope.** The Lo-tech XT-CF has D8-D15 unconnected.
- **Everything is 8-bit PIO.** A 16-bit XT-IDE variant needs a data-path change, not a switch.
- **The DDK sample this is built on ships three bugs of its own** - a request routine that
  destroys the registers its own header promises to preserve, a calldown spliced into every
  DCB the OS broadcasts, and an `EnterProc` that emits no stack frame outside a DEBUG build.
  All three are fixed here; anyone starting from the same sample will meet them.

## The boot-order question, and the exact test that settles it

Owner's read, 2026-09-02, and it is worth proving rather than assuming: **the position of a
real-mode driver in `CONFIG.SYS` may matter as much as its presence.** IOS scans what exists when
it runs, so which units are claimed - and by whom - depends on load order.

Right now we cannot tell the two apart. We REM'd the whole SCSI/ASPI chain, so "MODISK2 absent"
and "MODISK2 loaded late" have never been separated.

**The single-variable test:** restore `MODISK2.SYS` exactly as it was, but move it to the **end**
of `CONFIG.SYS`, after every other `DEVICEHIGH` line. Nothing else changes.

| result | meaning |
|---|---|
| IOS still flags it `Unsafe`/`Monolithic` | it is **presence**. Order is a red herring and the chain has to go, or move to the 32-bit T130 miniport |
| flag gone, `Init Success` holds | it is **order**, and that is a far better answer - the devices can stay, and we ship a supported ordering |

The second outcome is the one worth hoping for, because it turns a caveat into an instruction.
Either way it belongs in whatever ships: **anyone running this on a machine with a real-mode ASPI
or MO/Zip stack will hit the same wall**, and `IOS.LOG` is how they will recognise it - look for
`Unsafe driver` and how many units are `going through real mode drivers`.

That diagnosis generalises even where `MODISK2` specifically does not, so it is the caveat to lead
with, not a footnote.
