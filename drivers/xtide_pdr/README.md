# Windows 95 port driver for the 8-bit XT-IDE card — issue #21

The CF card is this machine's boot disk and it runs in MS-DOS compatibility mode, because
**Windows 95 has no 32-bit driver that will touch an 8-bit XT-IDE controller**. `ESDI_506.PDR`,
Microsoft's own ATA port driver, declines the machine — it never appears in `BOOTLOG.TXT` at all.
Nobody has ever written a replacement. This directory is the attempt.

Plan, emulator-and-skeleton first, hardware last:

| phase | goal | status |
|---|---|---|
| **0** | prove the toolchain, and that IOS loads what it produces | **build done**, load test pending |
| 1 | swap in XT-IDE transport: IDENTIFY, then sector reads, master only | not started |
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

So the probe needs an INF. `PORT.INF` creates a device node and binds `PORT.PDR` to it, copying the
binding shape from `T130-XT.INF` — the pattern already proven to work on this machine.

**Install** (staged on the CF at `C:\PORTPDR`):

1. Add New Hardware → **No, I want to select from a list** (decline autodetection — Technique 65)
2. SCSI controllers → **Have Disk**
3. **Type** `C:\PORTPDR`, do not Browse (issue #3)
4. Reboot with `F8` → **Logged**

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

### The card

From the owner's `5160 deep dive config.txt`, slot 8: **Lo-tech XT-CF v2.0**, XTIDE Universal BIOS,
JP1 ROM enable jumped, JP2 open = ROM at **D800h**, JP3 slot 8 enable jumped.

**No I/O base jumper is listed**, so the base is fixed on this board. Lo-tech's fixed base and
86Box's default are both `0x300`. That is inference from absence plus the emulator default, not a
measurement — confirm it with COMrade (`io_in`, from real-mode DOS) before phase 1 relies on it.

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
