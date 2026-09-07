# Windows 95 port driver for the 8-bit XT-IDE card — issue #21

The CF card is this machine's boot disk and it runs in MS-DOS compatibility mode, because
**Windows 95 has no 32-bit driver that will touch an 8-bit XT-IDE controller**. `ESDI_506.PDR`,
Microsoft's own ATA port driver, declines the machine — it never appears in `BOOTLOG.TXT` at all.
Nobody has ever written a replacement. This directory is the attempt.

Plan, emulator-and-skeleton first, hardware last:

| phase | goal | status |
|---|---|---|
| **0** | prove the toolchain, and that IOS loads what it produces | ✅ **PASSED 2026-08-29** |
| 1 | `IOConfig` on the devnode, selectable transport, IDENTIFY | **built, awaiting a boot** |
| 2 | sector reads, then writes, then slave | not started |
| 3 | the real card, the real CF | not started |

ATAPI CD-ROM is deliberately out of v1 — it is a different device class needing translation above
the port driver, not just transport. The reference for it is in
[`../xtide_cdrom/`](../xtide_cdrom/) when we get there.

## Phase 0

`build.ps1` assembles Microsoft's DDK sample port driver and links it to a `.PDR`. Nothing here is
our code yet; the point is to retire the risk that would kill the project outright.

```
stock sample          md5 a2e378b9465467c684cae04eaa02103e
+ phase0-no-irq       md5 8868f170898fcc2c73334e34924cb6d8   <- what we deploy
both 6661 bytes
```

**One line is changed from the DDK original**, and only one: the `Port_Set_IRQ_Handler` call is
removed. It registers `DDB_irq_number` with VPICD, which on a devnode declaring no IRQ is zero —
the system timer. Issue #22 is a recent lesson in what a stray IRQ claim costs here. The line goes
rather than being made conditional because the card polls and `PORTISR.ASM` disappears in phase 1
anyway. See `phase0-no-irq.patch`; `build.ps1` applies it and fails loudly if the call site moves.

Reproducible: running `build.ps1` twice gives the same md5, and the md5 differs from the stock
build, so the change demonstrably took (Technique 28). Verified as a real LE VxD — `MZ` then
`LE` at `e_lfanew`, cpu 2 (386), OS 4 (Windows 386), 6 pages, 6 objects (5 code, 1 data, all
32-bit), DDB name `PORT`, description `Generic Port Drv`.

Four files, 36 KB of assembly, and it built clean on the first attempt. The toolchain risk was the
one that could have ended #21 before it started, and it is gone.

### What the sample actually is

Worth knowing before trusting it as a skeleton:

- **It contains no port I/O whatsoever.** Not one `in` or `out` in any of the four files. It is a
  pure IOS-protocol skeleton, which is why it is safe to load on a working machine — it cannot
  reach hardware even by accident. (Audited per Technique 75 before deployment.)
- It registers with `DRP <EyeCatcher, DRP_MISC_PD, Port_Async_Request, port_ilb, ...>` —
  `DRP_MISC_PD` is exactly the load group an XT-IDE port driver wants.
- The whole AEP surface is five function codes: `AEP_INITIALIZE`, `AEP_DEVICE_INQUIRY`,
  `AEP_CONFIG_DCB`, `AEP_IOP_TIMEOUT`, `AEP_BOOT_COMPLETE`. Anything else returns `AEP_FAILURE`.
- `PORTISR.ASM` builds but is expected to be **deleted** in phase 1 — the card is jumpered without
  an interrupt, so the driver polls and the whole VPICD interaction goes away.

### The load test

**First attempt was a null result, and an uninformative one.** `PORT.PDR` was dropped into
`WINDOWS\SYSTEM\IOSUBSYS\` and a logged boot showed no mention of it at all — but that proves
nothing, because IOS binds port drivers to **device nodes**, not by scanning the directory. With no
devnode there was never a reason to load it. A gate artifact, not evidence (Technique 19).

The same boot settled the mechanism, because renaming the LS-120 miniport ran the A/B for free:

| driver | devnode? | initialises? |
|---|---|---|
| `hsflop.pdr` | yes, the FDC | yes |
| `sd120ppd.mpd` | yes — until renamed to `.MP_` | yes → **gone** |
| `scsiport.pdr` | only as that miniport's framework | yes → **gone** |
| `esdi_506.pdr` | none, no IDE controller node | **never** |

`SCSIPORT` was in one boot's `INITCOMPLETE` list and absent from the next, purely because the
miniport that needed it went away. Every port driver that loads here has a device node; every one
that does not, does not.

So the probe needs an INF. `PORT.INF` creates a device node and binds `PORT.PDR` to it.

**Class is `hdc`, not `SCSIAdapter`.** The first draft used SCSIAdapter because that is what
`T130-XT.INF` and the LS-120 INF use and both are proven to bind here — but those are `.MPD`
**miniports**, loaded by `SCSIPORT.PDR`. This is a standalone `.PDR` registering `DRP_MISC_PD`
directly with IOS, architecturally the same as `ESDI_506.PDR`. Under SCSIAdapter the class
installer may hand it to SCSIPORT and fail for reasons that say nothing about our binary — a
second uninformative null. The two registry values are taken verbatim from `MSHDC.INF`'s own
`[ESDI_AddReg]`, which is how Microsoft binds the driver this one replaces.

### Result — PASSED, 2026-08-29

```
[0017A60A] Initing port.pdr
[0017A60C] Init Failure port.pdr
```

**That is the pass.** IOS loaded our binary and called `AEP_INITIALIZE`. The whole chain is proven
before a line of XT-IDE code exists: MASM 6.11c → a `.PDR` IOS accepts → INF → device node →
IOS binding → our code executing on the real 5160.

`Init Failure` was the predicted outcome — the sample claims no devices. Two ticks, no hang.
Verified alongside it: the md5 of `IOSUBSYS\PORT.PDR` matches our build exactly (so the INF's
`CopyFiles` did it, not a hand-drop), `hsflop.pdr` still reaches `Init Success`, and the only
other failures in the log are the four that were already in the baseline boot.

Not distinguished, and not worth a boot to settle: whether `Init Failure` came from
`AEP_INITIALIZE` returning failure, or from IOS dropping a driver that claimed no devices. Both
are expected here.

### What phase 1 needs, exactly

Device Manager shows the node with a **"not configured"** warning. That and the `Init Failure` are
one fact, not two. `Port_Scan_Inp_Params` calls `_CONFIGMG_Get_First_Log_Conf` with
`ALLOC_LOG_CONF` and reads precisely two resource types off the devnode:

```
ResType_IRQ  ->  [edi].DDB_irq_number
ResType_IO   ->  [edi].DDB_base_ioa      <- IOD_Alloc_Base, the card's base address
```

With no allocated resources that call fails, so `DDB_base_ioa` is never set and the driver has no
address to talk to. So phase 1 starts by giving the node an `IOConfig` for the card's real range —
`ConfigPriority=HARDRECONFIG`, never `HARDWIRED`, and **exactly one** I/O descriptor, because the
loop overwrites `DDB_base_ioa` on every `ResType_IO` it sees and the last one wins. No `IRQConfig`:
the card polls.

Leave the probe node installed. Phase 1 reuses it.

### Why the standard IDE node failed here — and the rule it gives phase 1

Installing Microsoft's *Standard IDE/ESDI Hard Disk Controller* was tried on this machine and came
up with IRQ 14, greyed out. `MSHDC.INF` explains it exactly:

```
[esdilc1]
ConfigPriority=HARDWIRED          <- why it was ungreyable
IOConfig=1f0-1f7(3ff::)           <- AT primary IDE, not this card's 0x300
IOConfig=3f6-3f6(3ff::)
IRQConfig=14                      <- AT slave PIC; a 5160 has one 8259, IRQs 0-7
```

Not a Windows fault. `*PNP0600` carries a **HARDWIRED** config describing an AT primary IDE
channel, and `HARDWIRED` is the priority that makes resources non-negotiable — so the node
faithfully claimed hardware this machine does not have and would not let anyone change it.

**Rule for phase 1:** when we declare the card's real range, use `ConfigPriority=HARDRECONFIG` as
`T130-XT.INF` does. Never `HARDWIRED`. A wrong resource you can edit is recoverable; a wrong one
you cannot is a reinstall.

**Hardware ID is deliberately not `*PNP0600`.** That is Microsoft's standard IDE controller ID, and
a node claiming to be one is a node Windows may try to drive the boot disk with. Adding that node
has already been tried here and produced **impossible resources — IRQ 14, ungreyable** (an AT
slave-PIC line this machine does not have). Declaring no `LogConfig` at all is why this probe has
nothing for Windows to assign a wrong value to. If it still greys out at IRQ 14, that is
Technique 65: uncheck *Use automatic settings* to get a `ForcedConfig`.

**Install** (staged on the CF at `C:\PORTPDR`):

1. Add New Hardware → **No, I want to select from a list** (decline autodetection — Technique 65)
2. **Hard disk controllers** → **Have Disk**
3. **Type** `C:\PORTPDR`, do not Browse (issue #3)
4. Pick *"XT-IDE port driver (phase 0 probe - loads nothing)"*
5. Reboot with `F8` → **Logged**

Then:

```bash
grep -i "port" /d/BOOTLOG.TXT
```

| result | meaning |
|---|---|
| `Initing port.pdr` | **Phase 0 passes.** IOS loaded our binary and called `AEP_INITIALIZE` |
| `Init Success port.pdr` | it survived init too — more than needed; the sample claims no devices |
| still nothing | the devnode is not enough either. Next variable is `LogConfig` resources |

`Initing …` is the signal, not `Dynamic load …` — the latter only ever lists `.VXD` files plus
`rmm.pdr`. Only `hsflop.pdr` reaches the `Initing` phase on this machine now, so a new entry is
unmissable.

**To back it out:** remove the device node in Device Manager, then delete
`D:\WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR`. `D:\PORTPDR\` can stay; it is inert.

### The card — MEASURED 2026-08-30

Slot 8, from the owner's `5160 deep dive config.txt`: **Lo-tech XT-CF v2.0**, XTIDE Universal BIOS,
JP1 ROM enable jumped, JP2 open = ROM at **D800h**, JP3 slot 8 enable jumped. No I/O base jumper
exists on this board.

**I/O base is `0x300`**, established two independent ways rather than inferred:

- the XTIDE BIOS boot banner reports `D800h` then **master at 300h**;
- a COMrade read of `0x30E` returns **`0x50`** — `DRDY|DSC`, the textbook idle status of a present,
  ready ATA device, at the alternate-status register.

The option ROM at `D800:0000` agrees: signature `XTIDE204`, *-=XTIDE Universal BIOS (XT)=-*,
v2.0.0 (2013-10-22), and a config block giving base `0x0300` with the **control block at `0x0308`**
— which places alternate status at `0x308+6 = 0x30E`, exactly the port that answered.

`0x320` and `0x340` are ruled out anyway: the 3C509B and the T130B live there.

#### Unexplained, and it must be resolved before writing transport

```
0x301-0x305   0x00
0x306, 0x307  0x09     <- identical at two adjacent registers
0x30E         0x50     <- a live idle drive
```

`0x09` is wrong for both drive/head and status, and two adjacent taskfile registers do not return
the same value. Either XT address aliasing (Technique 75) or this card's taskfile decodes
differently from plain XT-IDE.

**This matters because XTIDE Universal BIOS supports genuinely different transports** — XT-IDE
rev1/rev2 (16-bit through the high-byte latch at `+8`) and XT-CF (8-bit PIO). OBattler's driver in
[`../xtide_cdrom/`](../xtide_cdrom/) implements **the latch**. If this card is XT-CF PIO8, that
reference does not describe our transport and phase 1 writes something different.

The ROM's `wPortCtrl = 0x308` is consistent with *both* designs, so it does not discriminate.
**`XTIDECFG.COM` displays the configured device type outright** — get that before writing code.
This is the single most important open question for phase 1.

### Design decision: the transport is configurable, not hardcoded

**Owner's call, 2026-08-30, and it is the right one.** This driver should work on XT-IDE hardware
generally, not just this card. XTIDE Universal BIOS supports a family of electrically different
adapters - XTIDE rev 1, rev 2 (A0/A3 swapped), Lo-tech XT-CF in several PIO modes, JR-IDE - and
picks between them at configuration time. A Win9x driver that hardcodes one of them is a driver for
one machine.

So the transport is a **parameter**, with two layers:

1. **Autodetect, and self-verify.** `IDENTIFY DEVICE` returns 512 bytes containing ASCII model and
   serial-number strings at known offsets. Read with the wrong transport those strings come out
   visibly wrong - byte-duplicated is exactly what the broken build in
   [`../xtide_cdrom/`](../xtide_cdrom/) produces. So the driver can try a transport and **check its
   own answer** rather than trusting configuration. Cheap, and it runs once at init.
2. **A hand override**, for when autodetect is wrong or a card is unusual. Set from the INF as a
   registry value on the device node, read at `AEP_INITIALIZE`. Precedent for exactly this shape
   exists on this machine: `HKR,,Polling,,1` in `T130-XT.INF`, and the LS-120 miniport's
   `AdapterSettings`.

**This is what unblocks phase 1.** The open question below - what `bDevice = 0x0A` names - stops
being a prerequisite. We implement the transports, let the driver work it out, and keep an override
for when it cannot. Settling `0x0A` then becomes a useful cross-check on the autodetect rather than
a gate on writing any code at all.

### The ROM configuration, decoded by diff - 2026-08-30

The card's option ROM was dumped over COMrade and diffed against a stock 8 KB XTIDE UB build
(`IDE_XT.BIN`). Excluding the build-date string, **only three bytes differ**, which locates the
configuration exactly rather than by guessing at struct offsets:

```
stock  70..87: e0 ec 04 00 00 00 02 80 00 01 00 00 03 08 03 06 00 1d
card   70..87: e0 ec 04 00 00 00 01 80 00 01 00 00 03 08 03 0a 00 1d
                                    ^^ 76                    ^^ 85
```

Mapped against `RomVars.inc`'s `IDEVARS`, and corroborated by `bIRQ` landing on `0x00` exactly as a
jumperless-interrupt card requires:

| ROM offset | field | value | note |
|---|---|---|---|
| 76 | `bIdeCnt` | **1** (stock 2) | one controller - user-configured |
| 81-82 | `wBasePort` | **`0x0300`** | identical to stock: the build default, hence no jumper |
| 83-84 | `wControlBlockPort` | **`0x0308`** | identical to stock |
| 85 | **`bDevice`** | **`0x0A`** (stock `0x06`) | **the transport** |
| 86 | `bIRQ` | **`0x00`** | no interrupt - the driver polls |

**Established beyond doubt:** the control block sits at base+8. `RomVars.inc` gives
`DEVICE_XTIDE_DEFAULT_PORTCTRL = 300h + XTIDE_CONTROL_BLOCK_OFFSET`, and standard ATA uses `0x206`
(`1F0` -> `3F6`). Offset 8 means this is an **XTIDE-family device, not standard ATA**.

**Not established: what `0x0A` names.** The `DEVICE_*` equates are expressed relative to
`COUNT_OF_STANDARD_IDE_DEVICES`, and the device list *grew between v2.0.0 and v2.1.0* - the trunk
enum adds `8BIT_ATA`, `JUKO_D16X`, `ADP50L` and three extra XT-CF sub-modes that v2.0.0 lacks. This
card is `XTIDE204` (v2.0.0). Two readings both fit, and they **imply different transports**:

| if the v2.0.0 enum is... | `0x0A` = | transport we would have to write |
|---|---|---|
| trunk order, `COUNT_OF_STANDARD_IDE_DEVICES` = 2 | `DEVICE_8BIT_XTIDE_REV2` | 16-bit via the high-byte latch, **A0/A3 swapped** |
| v2.0.0 menu order x2 | Lo-tech XT-CF | 8-bit PIO, no latch |

Do **not** pick one by reasoning. `XTIDECFG.COM` shows the device type by name in one screen (host
copy under `OneDrive/Desktop/XT_project/HarrisonXT/UTILS/XTIDE/`). That is now a precise question -
*which entry is selected under Device Type* - rather than an open investigation.

**Calibration attempt, and what it gave.** Every XTIDE203 v2.0.0 XT build to hand -
`IDE_XT`, `IDE_XTL`, `IDE_XTP`, `IDE_XTPL` - carries `bDevice = 0x06`, base `0x300`, ctrl `0x308`,
`bIRQ 0x00`. So `0x06` is the XT-build default and this card was deliberately changed to `0x0A`,
consistent with a Lo-tech-supplied ROM configured for their own board. It does **not** pin the
enum: those four builds differ by *CPU target* (8088 / V20 / large), not device type, so they are
one data point rather than four. The v2.0.0-era `RomVars.inc` was not retrievable; trunk is
v2.1.0 and its device list is longer.

Per the decision above, this no longer blocks anything.

Sources: [`RomVars.inc`](https://www.xtideuniversalbios.org/browser/xtideuniversalbios/trunk/XTIDE_Universal_BIOS/Inc/RomVars.inc),
[`XTCF.inc`](https://www.xtideuniversalbios.org/browser/xtideuniversalbios/trunk/XTIDE_Universal_BIOS/Inc/Controllers/XTCF.inc),
[v2.0.0 manual](https://xtideuniversalbios.org/export/504/xtideuniversalbios/wiki/Manual_v2_0_0.wiki).

- ⚠ **"XT-CF DMA mode is ruled out" - RETRACTED 2026-08-31, see the card identification below.**
  The reasoning was sound but the premise was wrong: `XTCF.inc` does say DMA is exclusive to
  XT-CFv3 on channel 3, but this board **is** a rev 3. DMA is therefore available, not excluded.
  We are still not using it - PIO is measured working and the 8237 on this machine is a minefield
  (Technique 62's 4-bit page-register trap) - but that is now a choice, not a constraint.
- ❌ **XTIDECFG cannot be read from the host.** It packs its strings; extracting them yields
  nothing usable. It has to be run on the machine.

### System state, same session

PIC1 IMR read `0xAC` at a DOS prompt: IRQ 0 timer, 1 keyboard, 4 COM1 (COMrade itself) and
6 floppy unmasked; 2, 3 (3C509B), 5 (SB Pro) and 7 (LPT) masked. A clean DOS picture, and it
confirms IRQ 1 is healthy now the LS-120 miniport is out.

DMA cannot be mapped this way at all — the XT page registers are write-only and float `0xFF`
(Technique 75). Do not design a probe that reads them back.

## Phase 1 - IDENTIFY, built 2026-08-30

`src/XTIDETR.ASM` is **our** code (the DDK sample is not redistributable; this is not derived from
it). It builds and links clean into the `.PDR` - `XTIDE_Probe` and friends appear in `PORT.map`
under the locked code and data segments.

```
PORT.pdr   7685 bytes   md5 e3f01f8eb9f4a5e0f19e768a86a02e0b
```

`build.ps1` wires it into the sample's `AEP_INITIALIZE` with two asserted edits - an `extrn` beside
the sample's own, and `call XTIDE_Probe` where the sample calls its own (commented-out) adapter
probe, so the existing `jc Port_i_failure` two lines below already does the right thing. Both edits
fail the build loudly if their anchor moves, rather than silently no-opping (Technique 28).

### What it does

1. Reads `DDB_base_ioa`, which `Port_Scan_Inp_Params` fills from the device node's `IOConfig`.
2. Waits for BSY to clear on **alternate status** (`base+0Eh`) - status proper would acknowledge an
   interrupt, and this driver should not assume a polled card just because ours is.
3. Selects the unit, then **reads Drive/Head back and compares the DEV bit**. That is how
   `ATASelectDevice` in [`../xtide_cdrom/`](../xtide_cdrom/) distinguishes a present slave from an
   absent one, and it is the only reliable way. Unused while phase 1 probes the master; correct
   when phase 2 does not.
4. Issues `IDENTIFY DEVICE` and reads 512 bytes through the selected transport.
5. **Validates the answer**, and only returns success if it is sane.

### The transport is autodetected, and it verifies itself

`XTIDE_ValidateId` checks the 40-byte model string at byte 54 is printable ASCII with real text in
it. Read through the *wrong* transport that comes back mangled - byte-duplicated, in the case of a
latch read that never fetches the high byte. **That exact bug shipped in one of the two binaries in
`../xtide_cdrom/`**, which is where the idea came from: the failure mode is loud enough to detect,
so autodetect is possible.

`XTIDE_Probe` tries `XT_TR_LATCH`, then `XT_TR_PIO8`, and keeps whichever validates. A registry
override is designed in but **not yet read** - the INF carries it commented out rather than
shipping a value the driver silently ignores.

This is what makes the open `bDevice = 0x0A` question a cross-check rather than a blocker: the
driver works out its own transport, and which one it settles on *answers* the question empirically.

### Reading the result

`AEP_INITIALIZE` returns success only if a drive answered and its IDENTIFY data validated. So
`BOOTLOG.TXT` is the instrument, and one line carries the whole result:

| line | meaning |
|---|---|
| `Init Success port.pdr` | **We talked to the CF.** Addressed the card, issued IDENTIFY, got a coherent 512-byte structure with a readable model string |
| `Init Failure port.pdr` | one of: no I/O resource on the node, drive never went ready, or *both* transports produced garbage |

Phase 2 adds a readback channel so we can see *which* transport won and what the model string says.
For now it is one bit, and one bit is the right size for the first attempt.

### ⚠️ Before the first phase-1 boot

This issues `IDENTIFY` to **the live boot disk**, while the real-mode BIOS is still driving it.
`IDENTIFY` does not touch media and the code checks BSY before doing anything, so the drive has to
be idle before we act - but a race with an in-flight real-mode transfer is not impossible.

**Back up the CF image first.** It is cheap, this project has done it before, and it is the
difference between a bad boot and a bad week.

### Install

The phase 0 node has no resources, so it must be **removed and re-added**, not updated in place -
`LogConfig` is applied at install time.

1. Device Manager -> remove the existing *XT-IDE port driver* node
2. Add New Hardware -> **No, I want to select from a list** -> Hard disk controllers -> Have Disk
3. Type `C:\PORTPDR` (do not Browse)
4. Reboot with `F8` -> **Logged**, then `grep -i "port.pdr" /d/BOOTLOG.TXT`

## The toolchain

Genuine MASM 6.11c plus the VC++ 2.0-era `LINK.EXE` — modern `link.exe` dropped the `/VXD` flag
this needs. Both run natively on Windows 11 through WOW64; no DOSBox. The same pair
[`custom_vkd/build.ps1`](../../custom_vkd/build.ps1) uses, which is why phase 0 was cheap.

`$env:INCLUDE` must carry **three** directories — `INC32`, `INC16` (`CMACROS.INC`/`PIF.INC` live
only there) and `BLOCK\INC` (`DDB`, `DCB`, `SCSIPORT`, `IODEBUG`…). The sample's own `MAKEFILE`
assumes `master.mk` sets these; `build.ps1` resolves them by hand.

**The DDK is not in this repo and cannot be** — it is not redistributable. Neither is the build
output, so `build/` is ignored. Point `build.ps1 -DDK` at your own copy.

## Prior art — read before writing any code

- **[zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x)** — Nick (@zikolas), MIT. A
  from-scratch Win9x IOS port driver and TSD, plus a free toolchain recipe. The proof this is
  buildable at all. **The MIT notice travels with anything adapted from it.**
- **[`../xtide_cdrom/`](../xtide_cdrom/)** — the register-level transport, and master/slave via the
  DEV read-back. Contributed by @andrew-hoffman.
- **[`docs/win9x_port_driver_feasibility.md`](../../docs/win9x_port_driver_feasibility.md)** — the
  full assessment, and the XT-IDE register map cross-checked against 86Box's `hdc_xtide.c`.

## Phase 1 result - FAILED then FIXED, 2026-08-31

First run on the real 5160, logged boot:

```
[00109829] Initing port.pdr
[0010982B] Init Failure port.pdr
```

Deployed binary verified as the phase-1 build (`e3f01f8e…`) in both locations before interpreting
anything - Technique 74.

**Two of the three predicted causes were eliminated from the host, with the card in a reader.**

- *No I/O resource*: the node's `ForcedConfig` in `SYSTEM.DAT` decodes to `0300`-`030F`. The
  `LogConfig` applied; removing and re-adding the node was the right call.
- *Drive never went ready*: 2 ticks. `XT_SPIN = 400000` costs roughly 0.6 s on this bus, and the
  same log rates `sd120ppd.mpd` at 895 ticks. No spin loop ran at all.

**The real cause was ours.** `XTIDE_TryIdentify` read the Drive/Head register back through a `DX`
that `XTIDE_WaitNotBusy` had already repointed at alternate status (`base+0Eh`, measured `50h` the
day before). DSC sits in the same bit as DEV, so the test failed instantly on both transports.
Fixed, and the read-back is now slave-only - on the master it could only ever convert a working
probe into an unexplainable failure. Technique 78.

```
phase 1  e3f01f8eb9f4a5e0f19e768a86a02e0b   failed
phase 1a 474b6ba8838ed39818739364f7d6c092   deployed 2026-08-31, both locations
```

No reinstall needed this time: the device node and its resources are already correct, and IOS loads
`.PDR` files from `IOSUBSYS` fresh each boot. Replace the file, boot `F8` -> Logged.

### `tools/XTPROBE.BAS` - the same IDENTIFY, from DOS

Staged at `C:\XTPROBE.BAS`. Run `C:\DOS\QBASIC.EXE /RUN C:\XTPROBE.BAS` from a bare DOS prompt.
Reads only; `IDENTIFY DEVICE` does not touch media.

It removes IOS, CONFIGMG and the device node from the question and answers, in one screen: whether
the card responds at `0x300`, whether `IDENTIFY` completes, **which transport returns a readable
model string**, and what `+06` reads back after selecting the master. That last line is a direct
check on the code path that just broke.

It answers the parked `bDevice = 0x0A` question operationally - which transport works - without
needing the enum resolved at all.

### `xtide202/` is XUB 2.1.0, not 2.0.2 - do not diff the card against it

The owner's `OneDrive/Desktop/XT_project/Windows_311_working_build/xtide202/` holds
`ide_xt.bin`, `ide_xtp.bin`, `ide_386.bin`, `ide_386l.bin`, all stamped `XUB210`. Their header
layout does not match the bytes used for the 2026-08-30 configuration diff, so they are **not**
substitutable into it - the `ROMVARS` offsets moved between releases, which is the same reason the
device enum shifted. `xtidecfg.com` there is packed, as recorded above.

## The register map, MEASURED - 2026-08-31

Over COMrade, on the real card, read-only plus taskfile writes. This supersedes every earlier
offset in this file.

**Read sweep** (`+00` skipped - reading data has side effects):

```
+01=00 +02=00 +03=00 +04=00 +05=00 +06=09 +07=09 +08=5F +09=5F
+0A=01 +0B=01 +0C=E0 +0D=E0 +0E=50 +0F=50
```

Every pair reads alike. `+07` is Status and `+0E` is Alternate Status under the old map, and those
are the same register in any ATA device - they must agree. They did not. That one pair falsified the
map before anything else was tried.

**Write/readback walk**, which residue cannot fake:

| wrote | port | read back | stride 1 predicts | stride 2 predicts |
|---|---|---|---|---|
| `11h` | `302` | `00` | `11h` (sector count) | error register ✓ |
| `22h` | `303` | `00` | `22h` (sector number) | same register ✓ |
| `33h` | `304` | `44h` | `33h` (cyl low) | sector count, last write wins ✓ |
| `44h` | `305` | `44h` | `44h` (cyl high) | same register ✓ |

**A0 is not decoded. Register N is at base + 2N.**

```
300 data        302 err/feat    304 sector count   306 sector number
308 cyl low     30A cyl high    30C drive/head     30E status/command
```

- **IDENTIFY works.** `0ECh` to `0x30E` returns status `58h` = DRDY|DSC|DRQ, four times out of four.
- **First data byte is `4Ah`** from `0x300` - a real IDENTIFY word-0 low byte.
- **There is no `+8` high-byte latch.** Marker test: `5Ah` written to `0x308` read straight back
  during DRQ. That is cylinder low. So `bDevice = 0x0A` is an **8-bit PIO XT-CF-class device**, not
  XTIDE rev 2 - the parked question, answered by measurement rather than by resolving the enum.
- **There is no reachable alternate status**, so the driver polls the status register.

### What could not be measured from the host

The data phase. Driven one I/O per serial round trip, DRQ died after ~19 accesses every time - the
first byte correct, the rest zero. `bus_stim` did the same. That is not the card; it is what happens
when a PIO transfer is left half-finished for milliseconds. **The transfer only exists inside a tight
loop on the machine**, which is exactly what `XTIDE_ReadData` is.

### Driver, rebuilt on the measured map

```
phase 1  e3f01f8eb9f4a5e0f19e768a86a02e0b   wrong map, wrong DEV read - failed
phase 1a 474b6ba8838ed39818739364f7d6c092   DEV read fixed, map still wrong - never deployed
phase 1b 07c11927c5e9562a977e9ab6d3bf520a   measured map, PIO8 default
```

Every port address is now computed once into its own variable by `XTIDE_SetPorts`, which removes the
class of bug that killed phase 1 - a callee can no longer leave a stale port in `EDX` for a caller to
read through.

**The stride is not autodetected, on purpose.** Probing stride 2 on stride-1 hardware writes `0ECh`
to `base+0Eh`, the Device Control register there, and `0ECh` has bit 2 set: that asserts SRST and
leaves it asserted. Hanging someone else's drive with a probe is not an acceptable default. The
transport (PIO8 / latch) *is* autodetected, because a wrong transport only misreads a buffer.

### Incident, 2026-08-31 - 38 root directory entries lost, and how

After several aborted IDENTIFY data phases driven from the host, a file was written to the card over
COMrade while the drive was still unsettled. The root directory came back with 28 of its 66 entries;
`IO.SYS`, `CONFIG.SYS`, `AUTOEXEC.BAT`, `COMRADE.EXE` and nine directories were gone. Subdirectory
trees were intact. Restored from `precfxtide.img` - 66 entries back, verified against the image.

**The rule that came out of it:** a PIO data phase left half-finished is not a safe state to write the
filesystem from. Verify status reads `50h` with no transfer pending before anything touches the disk,
or do not write at all. The ROM backup later in the same session followed that rule and was fine.

Recorded rather than quietly fixed, because the failure mode is not obvious: the drive reports itself
idle and ready (`50h`) while the *host* still believes a transfer is in flight, and DOS keeps serving
a cached BPB that makes the volume look healthy until a directory read fails.

### The control block, and the ROM flash - 2026-08-31

**Alternate status is at `31Ch`, and there is a general formula.** The sweep that found it went
`0x310`-`0x31A` = `FFh` (unimplemented), `0x31C` = `50h` (matching status exactly), `0x31E` = `00h`.

The ROM explains it. `wControlBlockPort = 0308h` is stored **unstrided** - it means "base + 8" in
register indices, which at stride 2 is `base+10h`. Alternate status is control-block register 6, so:

```
alt status = base + (8 + 6) * stride = base + 14 * stride
             stride 2 -> 31Ch   (measured)
             stride 1 -> base+0Eh  (where a classic XTIDE puts it)
```

One expression covers both cards. The driver now polls alternate status rather than the status
register, which is correct ATA practice: reading status acknowledges a pending interrupt and
alternate status does not.

**This retracts an earlier line in this file** that said there was no reachable alternate status
because `base+8` answers as cylinder low. The `base+8` observation was right; the conclusion was
wrong - the control block is a second strided bank, not an offset into the first.

### ROM flashed to r638, and what it taught us

| | before | after |
|---|---|---|
| build | `XTIDE204` v2.0.0ß3+ (2013-10-22), **XT** | `XUB212` r638 (2026-06-09), **XT+** |
| `bDevice` | `0x0A` | `0x0E` - **same device, different enum** |
| L-CHS reported | 986 / 64 / 63, 3,975,552 sectors | 987 / 64 / 63, 3,979,584 sectors |

**`ide_386.bin` cannot be used on this card.** XTIDECFG refuses the XT-CF device type against it:
*"There is no support for this device type in the currently loaded BIOS."* The 386 and AT builds
carry no XT-CF module. **Only the XT-family builds can drive an XT-CF adapter** - which is why the
card shipped with the XT build. That was never a misconfiguration.

`ide_xtl`/`ide_xtpl` (the "large" builds, 10 kiB) do carry it but will not fit an 8 KB EEPROM.
`ide_xtp.bin` (6,629 B, XT+) is the only build that is both supported and small enough, and it is
what the card now runs. Its gain over the plain XT build is `rep insb`, a 186+ instruction the
8088-targeted build cannot use - and on this machine the ROM is unshadowed 8-bit, so loop
instruction fetch is the dominant cost.

**Verify the configured image before flashing, not after.** Only two bytes differed from the stock
download (`wControlBlockPort`), everything else in `ide_xtp.bin` already defaulted correctly for
this card - but the stock file's checksum is *not* valid until XTIDECFG saves it, which is why the
docs insist on saving to disk first. A blind flash of a raw download would have failed.

Heads and sectors were unchanged by the flash, so CHS-to-LBA mapping is identical and the partition
was unaffected. `biosdrvs.com` predicted the new geometry exactly, before the flash - use it as the
pre-flight check.

Backups: `roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin` (the card as found, 8 KB, md5
`86ff8885ee13ee48301bc04f31b18e52`) and `IDE_XTP_configured_2026_08_31.bin` (what was flashed).

### Never fail AEP_INITIALIZE - 2026-08-31, from zikolas/cfu1-win9x

The DOS probe proved the card works completely: PIO8 through `0x300`, full 512-byte read,
`word0 = 044Ah`, geometry `3949/16/63` matching `biosdrvs`, model string **`TRANSCEND`**. So the
register map, the stride, the transport and the data phase are all correct, and the two `Init
Failure` results were our own code.

The answer came from **[zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x)** by Nick
(@zikolas), MIT - a working Win9x IOS port driver. Two things it does that the DDK sample does not:

**1. Resources do not arrive at initialisation time.** It registers a CONFIGMG handler when the
devnode appears and fetches resources when CONFIGMG calls back at `CONFIG_START`:

```asm
VxDCall _CONFIGMG_Register_Device_Driver, <ebx, <OFFSET32 CFU1_ConfigHandler>, ...>
ch_start:
    VxDCall _CONFIGMG_Get_Alloc_Log_Conf, <<OFFSET32 cmconfig>, ebx, CM_GET_ALLOC_LOG_CONF_ALLOC>
    mov ax, word ptr [cmconfig.wIOPortBase]
    mov [iobase], ax
```

The DDK sample instead queries CONFIGMG *during* `AEP_INITIALIZE` via
`_CONFIGMG_Get_First_Log_Conf(ALLOC_LOG_CONF)`. If nothing is allocated yet at that instant it
returns nothing and `DDB_base_ioa` stays zero.

**2. It never fails `AEP_INITIALIZE`** - its own comment is the whole lesson:
`mov word ptr [ebx+2], 0 ; AEP_SUCCESS (stay resident either way)`.

**Failing initialisation makes IOS drop the driver permanently**, so the CONFIGMG callback that
would have supplied the resources never arrives. Device Manager's **code 10 is IOS reporting that we
hung up on it**, not an independent fault. Claiming a device belongs to `AEP_DEVICE_INQUIRY`, where
"nothing here" is `AEP_NO_MORE_DEVICES` (2) - still not a failure.

His documented sequence: `AEP_INITIALIZE` -> `AEP_DEVICE_INQUIRY` -> `AEP_CONFIG_DCB`;
funcs INIT=0 BOOT_COMPLETE=2 CONFIG_DCB=3 UNCONFIG_DCB=4 DEVICE_INQUIRY=6;
results SUCCESS=0 FAIL=-1 NO_MORE=2.

**And a phase-3 gift.** He found that `DISKTSD` never configures a *dynamically* registered port
driver's DCB - the TSD layering pass only runs during boot-time configuration - so he had to write
his own TSD. We load at boot from `IOSUBSYS`, so that pass runs for us and the drive letter comes
free. His hardest problem is one we do not have.

### The boot log as a readback channel

`XTIDE_Probe` now always returns success and encodes *why* it ended as a deliberate delay, because
`BOOTLOG.TXT` timestamps every driver's init and that is the only channel out of IOS initialisation:

| delay | `XTIDE_FailCode` | meaning |
|---|---|---|
| none | 0 | probe succeeded, model string validated |
| ~1 s | 1 | no I/O resource reached the DDB - the CONFIGMG handler is then the fix |
| ~2 s | 2 | BSY never cleared |
| ~3 s | 3 | drive rejected IDENTIFY, ERR set |
| ~4 s | 4 | DRQ never asserted |
| ~5 s | 5 | data read, model string failed validation |
| ~6 s | 6 | slave did not answer with its DEV bit |

Roughly 33 ms per tick on this machine, so the buckets are far apart. Calibrate from the observed
delta rather than trusting the constant.

**The validator was also wrong.** `printable >= 4` anywhere in the 40-byte field let obvious garbage
through - a latch read of cylinder-low scored 10 and was accepted. It now requires an unbroken *run*
of printable non-blank characters, which noise rarely produces.

```
phase 1d  e64320eeec74bea54c75367af291512a   deployed 2026-08-31, both locations
```

### The real cause of every "Init Failure": we never declared DRP_FC_DYNALOAD

`Init Failure port.pdr` was never our probe. It comes from the VxD's own
`PORT_Device_Init`, which reports carry-set - "Device not initialized" - if `IOS_Register` hands
back anything other than `DRP_REMAIN_RESIDENT` or `DRP_MINIMIZE`:

```asm
	VxDCall	IOS_Register
	cmp	Drv_Reg_Pkt.DRP_reg_result, DRP_REMAIN_RESIDENT
	je	Port_Init_Done
	cmp	Drv_Reg_Pkt.DRP_reg_result, DRP_MINIMIZE
	je	Port_Init_Done
	stc
```

The DDK sample ships `PORTFeature EQU 00H` in `PORTINFO.INC` - no feature flags - while
dispatching `SYS_DYNAMIC_DEVICE_INIT`, i.e. it *is* dynamically loaded. `DRP_FC_DYNALOAD` is
`10000H`. Registering as a dynamic port driver without declaring it gets the registration refused,
and **IOS never dispatches the AER at all**, so `AEP_INITIALIZE` never arrives and no probe runs.

zikolas/cfu1-win9x sets the same flag for the same reason: *"Added a DRP (feature DRP_FC_DYNALOAD
since we load late via CONFIGMG)"*.

**This retracts the phase 0 result.** Phase 0 was recorded as passing because `Initing port.pdr`
appeared in the boot log. That line only proves IOS *attempted* the load - it is printed before
registration is accepted. Every driver from phase 0 to phase 1f failed at the same place, and every
probe fix in between was correct work on code that was never called.

**The lesson, and it is the same one as technique 78:** `Initing` is not `Init Success`. Pick the
log line that proves the thing you actually want, not the one that appears when it is attempted.
The delay-coded readback is what exposed this - a zero delay could only mean the probe was never
reached, which pointed above the probe rather than inside it.

```
phase 1g  aafd092c53f660621a99c08bdb0b6ec4   DRP_FC_DYNALOAD declared
```

### The image was never dynamically loadable - LE module flags

`PORT_Device_Init` never ran. Proved by instrumenting `PORT.ASM` so that **every** path through it
delayed, including success, before any AEP - and the boot log still showed a zero delay. So
`IOS_Register` was never called either, and `DRP_reg_result` was never the question.

The difference is in the LE header, and a working driver on the same machine shows it:

| | module flags | objects | pages |
|---|---|---|---|
| ours | `0x00028000` | 6 | 8 |
| `HSFLOP.PDR` (loads fine) | **`0x00038000`** | 4 | 5 |

One bit: `0x00010000`, the **dynamic-load marker** in the LE module-type field. IOS loads IOSUBSYS
drivers dynamically, so the loader refuses an image that is not marked dynamically loadable -
before dispatching `SYS_DYNAMIC_DEVICE_INIT`. No control message, no registration, no probe, just
`Init Failure` with none of our code having executed.

That bit comes from a module definition file, `VXD PORT DYNAMIC`. The DDK sample ships **no `.DEF`**
and its `MAKEFILE` links with `/VXD /NOD` only; `MASTER.MK` has no `.DEF` convention either. So
building the sample exactly as documented produces an image IOS will not load. `PORT.DEF` is now in
this directory and `build.ps1` links with `/DEF:PORT.DEF`.

**Verified on the host before booting:** the rebuilt image reports `0x00038000`, byte-identical to
`HSFLOP.PDR`.

```
phase 1h  84c3fad89ace1089fbeaa1bb06fc04fd   7696 bytes, mod_flags 0x00038000
```

**Method note.** Six builds were spent above the probe because each failure was diagnosed by
reasoning about which layer *should* fail rather than by comparing against something known to work.
`HSFLOP.PDR` was sitting in the same directory, loading successfully, in every boot log we read.
When a driver will not load, diff its image against one that does - it is a host-side check that
costs no machine time.

### Nothing was PRELOAD - the second half of the missing .DEF

The arrival marker settled it: a delay placed on the *first* instruction of `PORT_Device_Init`,
before `IOS_Register` is touched, still produced zero. The routine is never entered, so no control
message reaches the driver at all.

Comparing LE object tables against `HSFLOP.PDR` - which loads on this same machine, two lines above
ours in every boot log - showed why:

| | obj 1 | obj 2 | obj 3 | obj 4 |
|---|---|---|---|---|
| ours | `0x2005` | `0x2005` | `0x2005` | `0x2003` |
| HSFLOP | **`0x2045` PRELOAD** | `0x2015` DISC | `0x2005` | `0x2023` SHARED |

**Not one of our segments was PRELOAD.** A VxD's locked code and data carry the DDB and the control
procedure, and the loader has to call them the instant the image is mapped. With nothing resident
there is nothing to dispatch to.

`PORT.DEF` now carries the canonical VxD `SEGMENTS` map as well as `VXD PORT DYNAMIC`. Rebuilt, the
image reports `0x00038000`, four objects, and the same attribute set as `HSFLOP.PDR` - PRELOAD
locked, DISCARDABLE init, SHARED data. It also shrank 7696 -> 7328 bytes, because the init segment
is finally discardable.

```
phase 1i  5e5bb975d5d10cb90a158f295bbb136a   7328 bytes, PRELOAD locked segment
```

**Both halves of this were the same root cause:** the DDK sample ships no `.DEF`, and `MASTER.MK`
has no `.DEF` convention, so building the sample exactly as documented yields an image that is
neither marked dynamically loadable nor has a preloaded locked segment. Every failure from phase 0
to phase 1h was that one omission.

**Instrument on the near side of the call you doubt.** Three boots could not distinguish "the
message never arrived" from "IOS_Register never returned", purely because the delay sat *after* the
call. Moving it to the routine's first instruction answered it in one.

### LE page size: 512 vs 4096

A full LE header diff against `HSFLOP.PDR` - rather than the handful of fields picked by hand -
found the last structural difference:

```
field       OURS    HSFLOP
pagesize    512     4096      <-- the Windows VxD loader works in 4 KB pages
numpreload  4       1
```

`LINK /VXD` alone produced 512-byte LE pages, the OS/2-style layout. `/ALIGN:4096` fixes it, and
`numpreload` falls into line as a consequence. The file grows to 16544 bytes because it is now
padded to whole 4 KB pages - which is what every working `.PDR` on the machine looks like.

Full link line: `/VXD /NOD /ALIGN:4096 /DEF:PORT.DEF`.

```
phase 1j  0681f388eab8173b483074aa00caa11d   16544 bytes, 4 KB pages
```

**Diff the whole header, not the fields you suspect.** Three of these image-level faults - the
dynamic flag, PRELOAD, and the page size - were each found by comparing against a driver that
loads. The first two took a boot each to disprove because only some fields were compared; the last
took one command once the whole header was diffed.
