# Windows 95 port driver for the 8-bit XT-IDE card — issue #21

The CF card is this machine's boot disk and it runs in MS-DOS compatibility mode, because
**Windows 95 has no 32-bit driver that will touch an 8-bit XT-IDE controller**. `ESDI_506.PDR`,
Microsoft's own ATA port driver, declines the machine — it never appears in `BOOTLOG.TXT` at all.
Nobody has ever written a replacement. This directory is the attempt.

Plan, emulator-and-skeleton first, hardware last:

| phase | goal | status |
|---|---|---|
| **0** | prove the toolchain, and that IOS loads what it produces | ✅ **PASSED 2026-08-29** |
| 1 | give the devnode an `IOConfig`, then a **selectable** transport: IDENTIFY, then sector reads, master only | next |
| 2 | writes, then master/slave | not started |
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

- ❌ **XT-CF DMA mode is ruled out.** `XTCF.inc` states it is exclusive to XT-CFv3 and transfers on
  DMA channel 3. This board is v2.0, so nothing we write should involve the 8237 - which also keeps
  us clear of the 4-bit page-register trap (Technique 62).
- ❌ **XTIDECFG cannot be read from the host.** It packs its strings; extracting them yields
  nothing usable. It has to be run on the machine.

### System state, same session

PIC1 IMR read `0xAC` at a DOS prompt: IRQ 0 timer, 1 keyboard, 4 COM1 (COMrade itself) and
6 floppy unmasked; 2, 3 (3C509B), 5 (SB Pro) and 7 (LPT) masked. A clean DOS picture, and it
confirms IRQ 1 is healthy now the LS-120 miniport is out.

DMA cannot be mapped this way at all — the XT page registers are write-only and float `0xFF`
(Technique 75). Do not design a probe that reads them back.

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
