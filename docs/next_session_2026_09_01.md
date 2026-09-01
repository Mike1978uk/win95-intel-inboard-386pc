# Next session — 2026-09-01

## RESOLVED 2026-09-01 - the driver loads

```
[000AC32B] Initing port.pdr
[000AC408] Init Success port.pdr        221 ticks
```

Proven in 86Box, not on the 5160. Test bed `vm_xtide_pdr/` (Compaq Deskpro 386, 86Box's own
`xtide` controller at 0x300, Win95 booting off it), rebuilt from
`vm_win95_at_gap/win95_AT_working.img`. `tools/pdr_loadtest.ps1` does deploy -> logged boot ->
verdict in one command; the baseline image `xtide_base.img` already has the device node
installed and the logged boot armed, so no GUI step is needed again.

221 ticks is ~29 delay units. A failure is code 1-6, i.e. 5-10 units including the fixed
arrival and `reg_result` markers; success is `8 + model length`, ~25. So the probe's IDENTIFY
validated as well. The exact model length is not pinned - that would need one more run.

### The four image-level defects, and the two install-level ones

| # | fault | fix |
|---|---|---|
| 1 | `mod_flags 0x00028000` - not dynamically loadable | `VXD PORT DYNAMIC` |
| 2 | no segment had `PRELOAD` | canonical `SEGMENTS` map |
| 3 | `pagesize 512` (OS/2 layout) | `/ALIGN:4096` |
| 4 | **`PORT_DDB` exported at ordinal 0** | `DESCRIPTION` in `PORT.DEF` |
| 5 | **INF had no `ClassGUID`** | copied from `MSHDC.INF` |
| 6 | **`PORT.INF` was LF-only in this working tree** | rewritten CRLF |

4 was found by dumping the LE structures (`tools/ledump.py`) and comparing against two
independently-built images that load: Microsoft's `HSFLOP.PDR` and zikolas' `CFU1.VXD`. Both
publish `<NAME>_DDB` at ordinal 1.

5 was found by diffing our INF against `MSHDC.INF` in the same install. **It was not the cause.**
The copy on the CF card - `D:\PORTPDR\PORT.INF`, md5 `c4cf035a0eda8eaea34612af093de339` - is CRLF
and has *no* `ClassGUID`, and it installed on the 5160. `ClassGUID` and the install path were
changed in the same step, and the card is the control that separates them: the real fix was the
**path**. Win95's Have Disk rejects a drive root; it wants a directory. `ClassGUID` is kept because
it matches `MSHDC.INF`, not because it fixed anything.

6 was also not the cause, for the same reason - the card's copy was already CRLF. It is still a
genuine defect in this working tree, and it is what made the *emulator* copy fail.

6 matters for the card: the copy deployed to the CF came from this working tree, so
`C:\PORTPDR\PORT.INF` there is probably LF-only too. The committed blob was always fine.

### Two more things learned about the harness

- **`WIN /B` is a Windows 3.x switch.** Win95's `WIN.COM` treats `/B` as a program to run and
  puts up *"Cannot find the file '/B'"*. Use `MSDOS.SYS` `BootMenu=1` + `BootMenuDefault=2`
  (entry 2 is *Logged*) + `BootMenuDelay=1` for a hands-off logged boot.
- **A killed VM leaves the volume dirty**, so the next boot runs real-mode ScanDisk and never
  reaches Windows. `AutoScan=0` plus `tools/fatclean.py` per iteration.

## Phase 2a - sector reads work, 2026-09-01

`XTIDE_ReadSectors` (LBA28, with a real CHS fallback for pre-LBA drives) reads LBA 0 during the
probe and checks for the MBR signature. Safest possible target: read-only, one sector, on a drive
`XTIDE_TryIdentify` has just established is idle.

| run | probe delay code | ticks |
|---|---|---|
| phase 1, IDENTIFY only | `8 + model chars` = 16 units | 221 |
| phase 2a, + LBA 0 read | `24 + partition-type nibble` | **360** |

+139 ticks, about 14 units. Every failure code - 7 not ready, 8 no DRQ, 9 bad geometry, 10 no
`55AA` - is *smaller* than 16, so a failure would have made this run SHORTER. It got longer, and
only the success path exceeds 16. The read happened.

The exact nibble is not pinned: 139 ticks fits `24+6` (partition type `06`, matching a host-side
read of the image) but also `+5` and `+7`, because `DRP_reg_result` is not independently known.
Widen the encoding spacing before relying on the value itself.

## Phase 2b - writes work, and a real bug the round trip could not see

`XTIDE_WriteSectors` plus `XTIDE_WriteData`, sharing one `XTIDE_ProgramTaskfile` with the read
path so the two cannot diverge in how they address the drive. A scratch 8 MB slave disk was added
to the test bed (`scratch.img`, 0:1) so the write self-test never touches the boot volume.

Probe delay, three runs, one straight line:

| run | code | ticks |
|---|---|---|
| phase 1, IDENTIFY | `8 + model` = 16 | 221 |
| phase 2a, + read LBA 0 | `24 + nibble` = 30 | 360 |
| phase 2b, + write/read-back | `40 + nibble` = 46 | 520 |

16 units between 2a and 2b for 160 ticks gives **exactly 10.0 ticks per unit**, which then pins the
other two: nibble 6 (partition type `06`) and model 8 characters (`86B_HD00`, 86Box's own IDENTIFY
string). The encoding is no longer ambiguous.

### The bug, and why the driver's own report did not show it

The first 2b run reported success. The pattern was written to **`xtide_test.img` LBA 100 - the
master, the boot disk** - not to the slave.

`XTIDE_ProgramTaskfile` built Drive/Head from `DRVHD_LBA` (`0E0h`), which has the DEV bit clear.
`XTIDE_TryIdentify` had selected the slave; the taskfile programmer silently re-selected the
master. Both halves of the round trip went to the same wrong drive, so they matched.

**A write/read-back test verifies the transport, not the address.** Check the disk, from the host,
for the exact pattern at the exact LBA on the exact image. After the fix:

```
scratch.img     pattern at LBA 100      <- the slave, as instructed
xtide_test.img  pattern NOT present     <- boot disk untouched
```

That single check also proves the latch write ordering is right: a high/low swap would have stored
`ff00...` instead of `00ff...`, and the read path is separate code that would not have hidden it.

`-Restore` earned its keep - the damaged run cost one file copy.

## Phase 2c - NOT WORKING. Windows protection error the moment a unit is claimed

Read this before touching it again, and start from the bisect table, not from a theory.

`ClaimMask 0` (claim nothing) boots to a desktop and logs `Init Success port.pdr`. Any build that
claims a unit dies with a **Windows protection error**, and `BOOTLOG.TXT` stops at
`Initing port.pdr` with no verdict.

That log position matters: `IOS_Register` runs the whole AEP sequence - `AEP_INITIALIZE`,
`AEP_DEVICE_INQUIRY` per unit, `AEP_CONFIG_DCB` - synchronously inside `SYS_DYNAMIC_DEVICE_INIT`.
Requests come later. So the fault is in inquiry or config, **not** in the request path or the
transport.

### LOCALISED 2026-09-01: the fault is Port_cfg_device's ISP calldown insert

| build | result | binary verified |
|---|---|---|
| `-ClaimMask 0` | boots, `Init Success` | yes |
| `-ClaimMask 2` | protection error | yes |
| `-ClaimMask 2 -NoDcb` (claim, no DCB writes) | protection error | yes |
| `-ClaimMask 2 -NoCalldown` (claim, no calldown insert) | **boots, `Init Success`, 523 ticks** | md5 `10dd4f0a` |
| `-ClaimMask 2 -NoIo` (calldown, stub request handler) | protection error | md5 `493a07ee` |

`-NoDcb` exonerates every DCB write. `-NoIo` exonerates the transport and the request path - the
handler touches no hardware and still faults. `-NoCalldown` is the only variant that survives.

**So the bug is inside `Port_cfg_device`'s ~20 lines of ISP calldown insert, or in how IOS reacts
to the packet it builds.** Nothing downstream of it has ever executed.

### Where to look first, next session, before any run

Read `BLOCK/INC/ISP.INC`'s calldown-insert structure and compare field by field against what
`Port_cfg_device` fills. Known suspects, in order:

1. **`ISP_i_cd_dcb`** - the sample gives it the DCB from `AEP_d_c_dcb`. Check whether IOS wants the
   *physical* DCB here.
2. **`Port_Request` vs `Port_request`** - the sample writes `OFFSET32 Port_Request` (capital R) while
   the proc is `Port_request`. It links, so MASM folded the case, but confirm the address is the
   request entry and not something else.
3. **`ISP_i_cd_expan_len`** - written as a word (`mov [edi].ISP_i_cd_expan_len, ax`) from a zeroed
   EAX. Confirm the field is a word.
4. **`DCB_dmd_small_memory`** in `ISP_i_cd_flags` - we are PIO and need no such demand. Try 0.
5. Whether the packet must be zeroed first - it is built on raw stack memory
   (`sub esp, size ISP_calldown_insert`) with only five fields set. Every other field is stack
   garbage. **This is the most likely single cause and the cheapest to fix.**

Item 5 is a one-line fix (zero the packet before filling it) and would be the first thing to try.

### Bisect state - four switches exist, use them

| build | result |
|---|---|
| `-ClaimMask 0` | boots, `Init Success` |
| `-ClaimMask 2` | protection error |
| `-ClaimMask 2 -NoDcb` (claim, write nothing to the DCB) | **still** protection error |
| `-ClaimMask 2 -NoCalldown` (claim, skip the ISP calldown insert) | **NOT YET RUN - do this first** |

`-NoDcb` exonerates every DCB write. The remaining suspects are `Port_cfg_device`'s ISP calldown
insert and whatever `AEP_CONFIG_DCB` does around it. `-NoCalldown` splits those in one run.

### Real bugs found and fixed on the way - keep these regardless

- **`DCB_apparent_*` are in `DCB_BLOCKDEV`, a different STRUC from `DCB`.** Writing them through a
  DCB pointer assembles cleanly and lands on `DCB_bus_type` / `DCB_scsi_*` / `DCB_inquiry_flags`.
  Only `DCB_apparent_blk_shift` is safe, and only because `DCB_COMMON` is the first field of `DCB`.
- **Inquiry identifies, it does not describe.** `NEW95DOC/STORAGE.DOC`: the driver sets
  `DCB_product_id`, `DCB_vendor_id`, `DCB_rev_level` and returns `AEP_SUCCESS`; an absent unit
  returns **`AEP_NO_INQ_DATA`** (1), not the sample's `AEP_FAILURE` (-1).
- **The DDK sample clobbers its own DCB pointer**: `Port_cfg_device` loads ESI from
  `AEP_d_c_dcb` and then executes `mov esi,edi` before handing it to `ISP_insert_calldown`.
  Patched out. Harmless while nothing is claimed, which is why it shipped.
- **A wild pointer of my own**: `push ebx / push edi / push eax` then reading the unit index back
  as `[esp+8]`, which is the saved EBX. Fixed by using the register that still held it.

### The other route, and it is not a consolation prize

Everything failing here is IOS plumbing that a `.MPD` miniport does not have to write - `SCSIPORT.PDR`
owns the DCB, the queue and the calldown insert. Two miniports already survive into Windows on this
machine (`T130.MPD`, and the LS-120's). The DDK ships `BLOCK/SAMPLES/MINIPORT`, unopened so far.
Cost is SCSI CDB translation; the proven transport would not change at all.

### Next

1. Run `-ClaimMask 2 -NoCalldown`. One run, splits the remaining space.
2. Read `BLOCK/SAMPLES/MINIPORT` and `NEW95DOC/STORAGE.DOC` on `AEP_CONFIG_DCB` **before** the
   next fix, not after it., so Windows actually routes I/O through it. This means
   **claiming the device**, which is normal (`ESDI_506.PDR` takes the boot disk off the real-mode
   mapper exactly this way) but wants a scratch slave disk in the test bed first, and writes
   implemented, not a read-only volume.
   `tools/pdr_loadtest.ps1 -Restore` rolls the image back from `xtide_base.img` before a run.
2. Add an XT-CF rev 3 model to the 86Box fork (`reg = (port & 0x1F) >> 1`, alt status at
   `base+1Ch`, no latch) so the owner's exact card is reproducible, and so autodetect can be
   tested against two genuinely different XT-IDE variants. Upstreamable on its own.
3. Only then the real machine.

### Process note

The handover below listed *"Compare against something known to work, early"* as failure #5 of
2026-08-31. This session repeated it: two speculative causes were proposed for the Have Disk
rejection (line endings, then the install path) before anyone diffed against `MSHDC.INF`,
which was sitting in the same install the whole time. The line-ending fault was real but was
not the cause.

---

## The one rule for this session

**Do not test driver loading on the real 5160.** Whether a `.PDR` loads is a Windows VxD-loader
question and is entirely machine-independent. Reproduce it in 86Box, iterate there in seconds, and
only go to the real machine once `Init Success port.pdr` appears in an emulated boot log.

Eleven boots were spent on the real machine on a question the emulator answers for free. That is
the single biggest process failure of 2026-08-31.

---

## State

`PORT.PDR` `0681f388eab8173b483074aa00caa11d` (16,544 bytes) is deployed on the CF in both
`WINDOWS\SYSTEM\IOSUBSYS\` and `C:\PORTPDR\`. **It does not load.** Nothing is broken by this — a
port driver that fails to load has no effect on the running system.

`BOOTLOG.TXT` shows, every time:

```
[…] Initing port.pdr
[…] Init Failure port.pdr        1-4 ticks, and NO instrumented delay ever fires
```

An arrival marker on the **first instruction** of `PORT_Device_Init` never fires, so the VxD's
control procedure is never entered. The image is refused by the loader before any control message.

## What is PROVEN, and does not depend on the driver

All measured on the real hardware; none of it needs revisiting.

- **Register map:** stride 2, register N at `base + 2N`. Established by write/readback
  (`11h,22h`->`302,303` both read `00`; `33h,44h`->`304,305` both read `44h`), not by residue.
- **Control block** is a second strided bank: `alt status = base + 14 * stride` = `31Ch` here,
  `base+0Eh` on a classic XTIDE. Sweep showed `310`-`31A` = `FFh`, `31C` = `50h`.
- **8-bit PIO through `0x300`, no high-byte latch.** Marker test: `5Ah` written to `base+8` read
  straight back mid-DRQ, so `base+8` is cylinder low.
- **Full IDENTIFY works**: `word0 = 044Ah`, `3949/16/63`, LBA28 3,980,592 sectors, model
  **`TRANSCEND`**, block mode max 1 (no READ MULTIPLE), PIO mode 4 / 120 ns.
- **Card:** Lo-tech XT-CF rev 3 (sold by TexElec). Option ROMs: Mach8 `C0000` 32K, Sergey
  Multi-Floppy `D0000` 8K, XT-CF `D8000` 8K.
- **ROM flashed** XT v2.0.0b3+ -> XT+ r638, verified; geometry unchanged (`987/64/63`, heads and
  sectors identical so CHS-to-LBA mapping is untouched). Backup in `roms/xtcf_card/`.
- **Real-mode baseline: ~240 KB/s**, and only ~25% of that time is the data transfer. That is the
  number the 32-bit driver has to beat.

## What is BLOCKING

The DDK block-port sample cannot produce a loadable VxD as documented. It ships **no `.DEF`** and
`MASTER.MK` has no `.DEF` convention. Three image faults found and fixed by diffing against
`WINDOWS\SYSTEM\IOSUBSYS\HSFLOP.PDR`, which loads on the same machine:

| # | fault | fix | verified |
|---|---|---|---|
| 1 | `mod_flags 0x00028000` — not marked dynamically loadable | `VXD PORT DYNAMIC` in `PORT.DEF` | flags now `0x00038000`, matches |
| 2 | no segment had `PRELOAD` — nothing resident to hold the DDB/control proc | canonical `SEGMENTS` map | obj flags now `0x2045/0x2015/0x2023`, matches |
| 3 | `pagesize 512` (OS/2 layout) | `/ALIGN:4096` | now `4096`, `numpreload 1`, matches |

**All three were real and none was sufficient.** Every comparable LE header field now matches
`HSFLOP.PDR` and the image still will not load, so the remaining difference is below the header —
object page table, fixups, or something the loader checks that we have not compared.

## Driver-side fixes already in, waiting for the driver to run

These are correct and will matter the moment loading works. Do not re-derive them.

- `EDX` clobber: DEV was read through a register left pointing at alternate status (technique 78).
- Wrong register stride (was 1, is 2).
- Validator accepted garbage — `printable >= 4` anywhere; now requires an unbroken run.
- `Port_device_inquiry` tested an **uninitialised EAX** (its `sniff_for_drive` call is commented
  out) and answered `AEP_FAILURE` on a coin flip. Phase 1 now answers `AEP_NO_MORE_DEVICES`.
- **Never fail `AEP_INITIALIZE`** — IOS drops the driver permanently, so the CONFIGMG callback that
  supplies resources never arrives. From zikolas/cfu1-win9x.
- Probe outcome is delay-coded into the boot log; success also reports the model string's length
  (9 = `TRANSCEND`), so `~315 ticks` is the full-success signature.

## Startup list

1. **Set up an 86Box Win95 VM for driver-load testing.** Any machine type — this is not an Inboard
   question. Candidate images already in the repo: `vm_golden/new_golden_premonolith.img`,
   `vm_test_canonical/premonolith_canonical.img`. Mount host-side, drop `PORT.PDR` into
   `WINDOWS\SYSTEM\IOSUBSYS\`, boot logged, read `BOOTLOG.TXT`. Target: `Init Success port.pdr`.
2. **Diff below the header.** Object page table, fixup records and the resident name table against
   `HSFLOP_reference.PDR` (in the repo root). The header matches; the fault is deeper.
3. **Consider abandoning the DDK sample's build entirely.** zikolas/cfu1-win9x builds a *working*
   Win9x VxD with JWasm + Open Watcom v2 and a known-good `.DEF`/link recipe. Cloned at
   `../cfu1-win9x`. Borrowing his build harness may be faster than fixing MASM/LINK output.
4. Only then: real hardware, `F8` -> Logged.

## Owed

- **@andrew-hoffman** — the T130B outcome on the real 5160. Unchanged, still outstanding.
- **Nick (@zikolas)** — credited in `drivers/xtide_pdr/README.md` and `docs/resources_and_sources.md`
  for the `AEP_INITIALIZE` and CONFIGMG findings. Owner knows him personally and will tell him.

## Process failures to not repeat

1. **Test machine-independent questions in the emulator.** Eleven boots, all avoidable.
2. **Diff the whole header, not the fields you suspect.** Faults 1 and 2 cost a boot each because
   only guessed-at fields were compared. Fault 3 took one command once everything was diffed.
3. **Instrument on the near side of the call you doubt.** A delay placed *after* `IOS_Register`
   could not distinguish "never called" from "never returned" — three boots lost to that.
4. **`Initing` is not `Init Success`.** Phase 0 was recorded as passing on the wrong log line, so a
   driver that never loaded was believed working for four sessions.
5. **Compare against something known to work, early.** `HSFLOP.PDR` was in the same directory,
   loading successfully, in every boot log read.
