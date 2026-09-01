# Next session — 2026-09-02

## Where it got to on 2026-09-01

The 32-bit XT-IDE port driver **services real Windows I/O and publishes a drive letter.**
Both proven in 86Box (`vm_xtide_pdr/`, `tools/pdr_loadtest.ps1 -Restore`). Not yet on the 5160.

```
XTIDE_StartRequest     3
  1  READ   LBA 0        <- partition table
  2  READ   LBA 63       <- our partition's boot sector
  3  READ   LBA 63

got as far as       10  VOLUME PUBLISHED
partition           start 63, 15809 sectors
ISP_CREATE_DCB      0
logical calldown    0
ISP_ASSOCIATE_DCB   0  drive D:
```

Windows read the MBR through our driver, used bytes from it to find the partition at LBA 63,
and we then built a logical DCB and got it a drive letter. Every ISP call returned 0.

## The last blocker, and it was a CONTEXT problem - now solved

`XTIDE_VolCreate` runs inline in `AEP_CONFIG_DCB`, which runs inside `IOS_Register`, which runs
inside `SYS_DYNAMIC_DEVICE_INIT`. `ISP_ASSOCIATE_DCB` notifies IFSMGR and broadcasts
`DBT_DEVICEARRIVAL`; doing that before registration has returned kills the boot — `BOOTLOG.TXT`
stops at `Initing port.pdr`, 11,826 bytes, the same signature as the old protection error.

**DONE - the volume mounts.** `XTIDE_SchedVol` queues `XTIDE_VolUp` via
`_SHELL_CallAtAppyTime` + `CAAFL_RING0`, armed from `AEP_CONFIG_DCB`. Confirmed with a 260 s
window (150 s was simply too short - the queue call had always succeeded):

```
queue call returned  queued (C0FCE384)
got as far as        10  VOLUME PUBLISHED      ISP_ASSOCIATE_DCB 0, drive D:
XTIDE_StartRequest   8
  1 READ LBA 0     partition table
  2-4,6 READ LBA 63  boot sector
  5 READ LBA 126   ROOT DIRECTORY
  7 READ LBA 70    FAT
  8 READ LBA 78    FAT
```

LBA 126, 70 and 78 are the exact addresses `mkfatimg.py` wrote and are not guessable: that is
VFAT mounting and enumerating a real filesystem through this driver. Boot log 18,676 bytes, full
desktop, and the drive is visible in Device Manager on screen.

Still gated behind `build.ps1 -PublishVolume`; the default build boots to a desktop.

**Next step, and it is a copy job.** Use Nick's mechanism - proven in a shipping driver, and his
comment gives the rule we were breaking:

> *"queue the volume work at appy time (**the ISP volume services are appy-time only**; the
> supervisor tick is not). CAAFL_RING0 lets it run before the GUI is fully up during boot."*

So it is not merely "too early", it is the wrong context entirely: **the ISP volume services may
only be called at appy time.** `cfu1-win9x`'s `sched_volup` (CFU1.ASM ~2294):

```asm
        push    0                       ; ulTimeout
        push    1                       ; flags = CAAFL_RING0
        push    0                       ; dwRefData
        push    OFFSET32 CFU1_VolUp     ; the callback that calls vol_create
        VxDCall _SHELL_CallAtAppyTime
        add     esp, 16
```

plus a `vol_busy` flag so only one queue attempt is in flight, and a retry if the queue fails.
Arm it from `AEP_CONFIG_DCB` instead of calling `XTIDE_VolCreate` directly; `XTIDE_VolCreate`
itself needs no changes.

(`win/ASYNC-ENGINE.md` is about his interrupt-driven *transfer* engine, not this - do not confuse
the two.)

`AEP_BOOT_COMPLETE` is **not** an option: measured this session, it never arrives. A dynamically
registered driver gets its own `AEP_CONFIG_DCB` and nothing else — which is the same reason
Windows' own disk TSD never engages our DCB and we had to become our own TSD.

## What is banked and must not be re-derived

| | |
|---|---|
| Image loads, IOS calls our AER | phase 0 |
| IDENTIFY, sector reads, sector writes | phases 1–2b, writes verified from outside the guest |
| Calldown insert accepted by IOS | `ISP_result 0` |
| Real IOS requests serviced | READ LBA 0 / 63, correct data |
| Logical DCB + drive letter | `ISP_ASSOCIATE_DCB 0`, drive D: |

### Four bugs fixed on 2026-09-01, all in code that had never executed

1. **`Port_request` destroyed EBX/ESI/EDI**, the three registers its own DDK header promises to
   preserve.
2. **`Port_cfg_device` inserted its calldown into every DCB IOS broadcast**, not just ours.
3. **`EnterProc` emits no stack frame**, so `ArgVar IOP_Ptr` read `[ebp+8]` through the caller's
   EBP — see Technique 82. This was the protection error. Read at `[esp+16]` now.
4. **Geometry belonged in `AEP_CONFIG_DCB`**, not at inquiry (`STORAGE.DOC`).

## Tooling built, and worth keeping

- **`tools/mkfatimg.py`** — builds a partitioned FAT16 volume on a raw image, deliberately clear
  of the marker sector. The scratch disk had no partition table, so Windows correctly issued one
  read and stopped; that looked like a driver failure for two runs.
- **`tools/pdr_reqmarker.py`** + `build.ps1 -ReqMarker` — the driver writes a marker sector to the
  claimed unit; the host reads it back. Counters for every stage from `AEP_CONFIG_DCB` to the
  transport, the first eight requests as (function, LBA), the DCB geometry actually written, and
  the volume-publishing results. **This is the only channel out of a ring-0 driver on an
  unattended harness, and building the whole ladder at once is what finally made runs cheap.**
- `build.ps1` switches: `-Stride`, `-ClaimMask`, `-NoDcb`, `-NoCalldown`, `-NoIo`, `-ReqMarker`,
  `-PublishVolume`.

## Then: the real card

`-Stride 2` for the Lo-tech XT-CF rev 3. **86Box's `xtide` is stride 1, so stride 2 cannot be
tested in the emulator at all** — it goes straight to the 5160. Build without `-ReqMarker` (the
marker writes to the disk, and on real hardware the claimed unit is the CF).

### Will it work on other XT-IDE variants?

Probably, with one change. The two things that vary are already abstracted: the base address comes
from the devnode, and the high-byte latch is autodetected (`XTIDE_TryTransport`). Only the register
**stride** is baked in at build time. Making it autodetect would give one binary for all 8-bit
variants, and the method is already proven — Technique 78's addendum: Status (`+07`) and Alternate
Status (`+0E`) must read alike on any working map, so trying stride 1 and stride 2 and keeping the
one where they agree settles it in a few `in` instructions.

**Owner's note, 2026-09-01, and it is the right constraint:** the geometry is static per disk, so
it would be safe to detect once per install — *except* that an XT-IDE Universal BIOS change can
alter what the card reports, as seen the day before. So re-detect at every boot rather than caching
it in the registry. That is what the driver already does, and it should stay that way.

ATAPI/CD is out of scope: the XT-CF is 8-bit only (D8–D15 unconnected).

## Scope of what is proven, and what C: would take

**Proven:** emulator, stride 1, 8-bit PIO, as a **secondary** volume. Windows mounted it and
assigned D:.

**Not yet exercised: a WRITE through the request path.** All eight requests Windows issued were
READs. The write path is proven only by the driver's own probe-time self-test (phase 2b, verified
host-side against the right LBA on the right image) - VFAT has never written through it. Do that
before trusting it with anything: put a file on D: from a DOS box and check the bytes from the
host.

### The boot drive is a different problem, not a bigger version of this one

Mounting D: means *creating* a volume nobody owns. Owning C: means *taking it away from something
that already has it* - the XT-IDE option ROM's real-mode INT 13h, surfaced by the Real Mode Mapper
(`RMM.PDR`, which Technique 74 already identified as what actually drives this machine's storage).
The DCB for the boot disk already exists when we load, created from the real-mode BIOS drive; we
would have to claim *that* DCB rather than build our own, and IOS would have to retire the mapper
for it. That handoff is the one part of the stack this session has not touched at all.

Consequences to plan around:
- it is the actual goal of issue #3 - "MS-DOS compatibility mode" ends when the boot volume is
  32-bit, not when a second drive is;
- a mistake corrupts the volume the machine boots from, so it stays in the emulator on a **clone**
  of the boot image until it works, and the scratch-slave discipline stays in force;
- `XTIDE_WantIop` currently refuses any DCB we did not claim, and `ClaimMask` is 2 (slave) for
  testing. Both would have to change deliberately, not by accident.

Do not estimate this from how quickly D: came together. The transport, calldown, request path and
volume layer are all now known-good; the handoff is unmeasured, and unmeasured is where every
surprise this session came from.
