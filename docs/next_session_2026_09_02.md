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

## The one thing left, and it is a timing problem, not a logic one

`XTIDE_VolCreate` runs inline in `AEP_CONFIG_DCB`, which runs inside `IOS_Register`, which runs
inside `SYS_DYNAMIC_DEVICE_INIT`. `ISP_ASSOCIATE_DCB` notifies IFSMGR and broadcasts
`DBT_DEVICEARRIVAL`; doing that before registration has returned kills the boot — `BOOTLOG.TXT`
stops at `Initing port.pdr`, 11,826 bytes, the same signature as the old protection error.

**So it is gated.** `build.ps1 -PublishVolume` turns it on; the default build boots to a desktop.

**Next step: defer it.** Schedule the call so it fires after `IOS_Register` unwinds — a VMM
`Set_Global_Time_Out` / `Schedule_Global_Event` armed from `AEP_CONFIG_DCB`. zikolas/cfu1-win9x
solves exactly this and ships `win/ASYNC-ENGINE.md` describing it; it drives `vol_create` from a
deferred control call for the same reason. Read that before writing anything.

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
