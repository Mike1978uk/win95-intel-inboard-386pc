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

`build.ps1` assembles Microsoft's DDK sample port driver **unmodified** and links it to a `.PDR`.
Nothing here is our code yet; the point is to retire the risk that would kill the project outright.

```
PORT.pdr   6661 bytes   md5 a2e378b9465467c684cae04eaa02103e
```

Reproducible: running `build.ps1` twice gives the same md5. Verified as a real LE VxD — `MZ` then
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

`PORT.PDR` is deployed to `WINDOWS\SYSTEM\IOSUBSYS\` on the CF (md5 verified at the destination,
not the staging copy — Technique 75). It needs a **logged boot** (`F8` → *Logged*) and then:

```bash
grep -i "port.pdr" /d/BOOTLOG.TXT
```

| result | meaning |
|---|---|
| `Initing port.pdr` | **Phase 0 passes.** IOS loaded our binary and called `AEP_INITIALIZE` |
| `Init Success port.pdr` | it also survived init — more than needed, the sample claims no devices |
| nothing at all | IOS never attempted it. Check the file is present and the boot was logged |

`Initing …` is the signal, not `Dynamic load …`. On this machine's last boot only `hsflop.pdr` and
`sd120ppd.mpd` reached that phase, so the list is short and a new entry is unmissable.

**To back it out:** delete `D:\WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR`. Nothing else was changed.

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
