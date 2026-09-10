# Patched files — one page, direct downloads

Everything this project has fixed, in one place, so the files can be dropped into **any** Windows 95
OSR1 install on an Intel Inboard 386/PC — not just the images we publish.

Each file is a **binary patch of Microsoft's or Intel's original**, usually a couple of bytes. Check
the md5 after downloading. Right-click → Save link as.

**Tested status is stated for every file. Nothing here is claimed to work that has not been run.**

---

## ✅ Confirmed on real hardware

### `MSSBLST.VXD` — Sound Blaster Pro audio

**[⬇ MSSBLST_INBOARD.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/sound/MSSBLST_INBOARD.VXD)** · md5 `dcf32b4a7d8dbcc47e659847742417b6` · 17,562 bytes
· [stock original](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/sound/MSSBLST_stock.VXD) `cc7e63aacb1f599fcd5b3fa1eb98169c`

Fixes **distorted digitised audio**. The IBM 5160 has a 4-bit DMA page latch — 20-bit reach, 1 MB.
`MSSBLST.VXD` asks `_PageAllocate` for a buffer anywhere below 16 MB, gets one above 1 MB, and the
page register silently truncates the address, so the card DMAs out of adapter ROM and plays that.

Two bytes: `maxPhys 0xFFF → 0xFF` at both call sites.

#### Two ways to install it — pick one

**A. Copy over the top (proven).** Install the Sound Blaster Pro driver normally, then copy the file
to `C:\WINDOWS\SYSTEM\MSSBLST.VXD`. It is not in a fresh image — it arrives stock from
`WIN95_xx.CAB` when you install the card, so the copy has to happen *after*.

**This is the route confirmed on real hardware.** It stays the default, and it is the one to use if
you want the outcome this project actually measured.

**B. [Have Disk package](https://github.com/Mike1978uk/win95-intel-inboard-386pc/tree/master/dist/havedisk-sbpro) (untested).**
`dist/havedisk-sbpro/` installs the card and the patched driver in one step, with no copy afterwards
and nothing to forget. Add New Hardware → **No** to autodetect → *Sound, video and game controllers*
→ Have Disk → **type** the path.

The INF is not hand-written: its sections are lifted verbatim from OSR1's own `WAVE.INF` by
following the references out of `[PNPB002_Device]` transitively, so the install logic is Microsoft's
byte for byte and only the file source changes. It is nonetheless **untested** — built and
internally consistent, never installed. Suggested by @andrew-hoffman on issue #5.

> **Do not use Browse in the Have Disk dialog.** It defaults to `A:\`, this machine has no floppy
> controller installed, and that read never returns. Type the path. See issue #3.

### `VDMAD.VXD` — Sound Blaster Pro BSOD

**[⬇ VDMAD_INBOARD_FIXED.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/VDMAD_INBOARD_FIXED.VXD)** · md5 `09af55b032aa7139d92d34b4246e7a05` · 41,844 bytes

Fixes `A fatal exception 0E … in VXD VDMAD(01) + 00001660`. Neuters the phantom second DMA
controller an XT does not have. ⚠️ **The repo also contains three copies of an earlier build
(`a4fd183b…`) that CAUSES that BSOD** — see [`vxd-patches/README.md`](vxd-patches/README.md).

**Bundled into `VMM32.VXD` by Setup — a file copy after that is silently ignored.** Use a
pre-monolith image; see *Applying to a bundled VxD* below.

### `VPICD.VXD` — phantom slave PIC

**[⬇ VPICD_INBOARD.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/osr1/VPICD_INBOARD.VXD)** · 46,543 bytes

Neuters the phantom slave 8259 at `0xA0`/`0xA1` (36 sites). An XT has one PIC; VPICD assumes two.
Bundled — pre-monolith route.

### `VKD.VXD` — keyboard input

**[⬇ VKD_CUSTOM_INT09FIX_v2.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/custom_vkd/build/VKD_CUSTOM_INT09FIX_v2.VXD)** · 18,698 bytes

Built from the 1995 DDK source. `VKD_Int_09`'s AT-only port-`0x64` check discards every keystroke on
a machine with no 8042. First known working Windows 95 keyboard input on real Inboard hardware.
Bundled — pre-monolith route.

### `KEYBOARD.DRV` — port 64h

**[⬇ KEYBOARD_INBOARD.DRV](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/osr1/KEYBOARD_INBOARD.DRV)**

Same class of fix one layer up. Plain file copy to `C:\WINDOWS\SYSTEM\`.

### `INBRDPC.SYS` — self-test skip

**[⬇ INBRDPC_selftest_skip.SYS](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/osr1/INBRDPC_selftest_skip.SYS)** · md5 `d3c458017c296fe01a13bceb19f34106`
· [stock original](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/osr1/INBRDPC_stock.SYS) `c25b951a0a6093dcfa5138d89159cbf6`

Skips Intel's driver self-test. **This is load-bearing, not a convenience** — with the stock driver
the emulator fails a check real hardware passes. **Root-caused 2026-08-24** ([#12](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/12)): 86Box
places the card's high BIOS-shadow alias at `0xF0000 + mem_size*1024`, but the driver targets a fixed
`0x5F0000`, so the two agree only at `mem_size = 5120`.
Copy to `C:\INBRDPC.SYS`.

### `IVT68FIX.COM` — INT 68h vector

**[⬇ IVT68FIX.COM](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/ivt68fix/IVT68FIX.COM)** · 20 bytes

Points `INT 68h` at `F000:FF53` (thanks to Michal Nečasek). **Must be the very last line of
`AUTOEXEC.BAT`** — run earlier it gets clobbered by DOS's own low-memory init.

---

### `XTIDEMP.MPD` — 32-bit disk access for the XT-CF / XT-IDE

**[⬇ XTIDEMP.MPD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/dist/xtide_mpd/XTIDEMP.MPD)** · 10,752 bytes · md5 `561fb45b598ef5985e5a803016321f76`
· **[⬇ XTIDEMP.INF](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/dist/xtide_mpd/XTIDEMP.INF)**

Not a patch — a driver. A Windows 95 SCSI miniport that presents an 8-bit XT-CF / XT-IDE card as
a SCSI disk, so Windows drives the boot disk in protected mode instead of falling back to
real-mode BIOS. Source in [`drivers/xtide_mpd/`](drivers/xtide_mpd/).

> ### ✅ Confirmed on the real 5160 — 2026-09-06
>
> Read back from the card itself, not reported from the screen:
>
> ```
> [001612B7] Initing xtidemp.mpd
> [001612CE] Init Success xtidemp.mpd
> [0016136F] INITCOMPLETESUCCESS = DiskTSD
> [00161372] INITCOMPLETESUCCESS = SCSIPORT
>            rmm.pdr  Dynamic load success ... never reaches INITCOMPLETE
> shutdown   7 stages started, 7 closed, none unpaired
> WINDOWS\IOS.LOG   does not exist
> ```
>
> `RMM.PDR` loading and then standing down is the boot-disk takeover — the Real Mode Mapper
> found nothing left to claim. `C:` is served guest → IFSMGR → VFAT → DiskTSD → SCSIPORT →
> `XTIDEMP.MPD` → XT-CF. Windows reached the desktop, `C:` was navigable, and the machine shut
> down to "It's now safe to turn off your computer" with no errors in Device Manager.
>
> `BOOTLOG.TXT` itself is a write that reached the medium through this driver and was read back
> host-side afterwards. The binary above is byte-identical to the one that ran.
>
> **The I/O base comes from you, not from a probe.** Device Manager → the controller →
> Settings → `PORT=0x300`. The card's base is set in the XTIDE Universal BIOS, so whoever
> installs this already knows it; the driver drives that port and never reads the device node's
> assigned resources. On the tested machine the node held a *forced* `0300-031F` and the driver
> was indifferent to it.
>
> **Install order matters.** Remove any older XT-IDE `PORT.PDR` node, then **reboot**, then Add
> New Hardware. Installing without that reboot had CONFIGMG skip `0300` — which is free, and
> measured free — and assign `0340` instead, then report a conflict when `0300` was set by hand.
> Nothing owns `0300` on this machine, so that conflict appears to have been a claim not yet
> released by the node just removed. Recommended sequence, not a proven mechanism.
>
> **Still unmeasured:** sustained write load (this was a boot, a look at `C:`, and a shutdown),
> and responsiveness under a heavy teardown flush — `XtStartIo` still completes every transfer
> inline.

This replaces the IOS port driver **retired to**
[`docs/archive/xtide_pdr_retired/`](docs/archive/xtide_pdr_retired/), which reached the same disk and then
wedged Windows at shutdown for four sessions. The miniport deletes that layer rather than
debugging it: SCSIPORT owns the polling contract, the DCB lifecycle and scatter/gather, and every
bug in that investigation lived in one of the three. Reasoning and the control that justified it:
[`docs/scsi_miniport_costing.md`](docs/scsi_miniport_costing.md).

**Install:** Add New Hardware → decline autodetect → SCSI controllers → Have Disk. `inbrdpc.sys`
must be in `[SafeList]` in `WINDOWS\IOS.INI` first, or IOS declines every miniport
([#17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17)).

### Does this work on cards other than mine?

Partly, and less than this section claimed before 2026-09-07. There are **three** XT-IDE register
maps, not two, and this driver expresses two of them
([`docs/xtide_register_maps.md`](docs/xtide_register_maps.md)):

- ✅ **Lo-tech XT-CF**, A0 undecoded, register N at `base + 2N`. This is the card here, measured,
  and the only configuration ever confirmed — one card, one base, one machine.
- ⚠️ **Compatibility map** — XT-IDE Rev 1, or a Rev 2/3/4 switched into it with XUB device type
  `XTIDE rev1`. This is the driver's stride 1. **Confirmed in emulation 2026-09-07**
  ([#24](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/24)) on a faithful
  register-map model, with a host-verified write and a clean 7/7 teardown — but **never run on a
  physical card**, which is the outstanding ask. Previously this said the path had never
  executed — in emulation or on hardware. Stock 86Box's `xtide` device does model this map, so it
  is testable in the VM without a card.
- ❌ **Hi-Speed map** — the *default* on XT-IDE Rev 2/3/4. **Not supported.** It comes from swapping
  the A3 and A0 address lines, which permutes the registers instead of scaling them, and
  `base + index * stride` cannot express a permutation. Expected to be declined by the read-only
  stride probe rather than misdriven, since that probe compares Status against Alternate Status and
  writes nothing — but that has never been observed on such a card.

If you have an XT-IDE or XT-CF card on a Windows 95 machine, this is the test the project cannot
run itself — please report the result on
[#21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21), working or not. It does
not need an Inboard; the only Inboard-specific prerequisite is the `IOS.INI` `[SafeList]` line,
which does not apply to a machine without `INBRDPC.SYS`.

See [issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).

---
## ⚠️ Loads on real hardware, not yet proven correct

### `HSFLOP.PDR` — floppy DMA reach

**[⬇ HSFLOP_XTDMA.PDR](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/floppy/HSFLOP_XTDMA.PDR)** · 18,998 bytes
· [stock original](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/floppy/HSFLOP_stock.PDR)

Same bug as the sound driver, one byte: `maxPhys 0x1000 → 0xFF`
(`68 00 10 00 00` → `68 FF 00 00 00`, so no instruction boundary moves).

Floppy DMA is **channel 2**. Where sound merely distorted, a floppy read that returns the wrong
bytes fails its CRC and the driver retries forever — motor on, light on.

> ### ✅ It loads now — measured on the real 5160, 2026-09-06
>
> ```
> [000C260D] Initing hsflop.pdr
> [000C2614] Init Success hsflop.pdr
> [000C2740] INITCOMPLETESUCCESS = HSFLOP
> ```
>
> md5 `8e695d00c20c6e43084d91d8ed111c52` on the card — the patched build, not stock. So for the
> first time a result here means something.
>
> **Supersedes the 2026-08-25 status**, which said this file was deployed but INERT: `BOOTLOG.TXT`
> then contained zero `hsflop` entries, because the whole storage stack was running through the
> real-mode mapper. That was true when written and is no longer. Clearing the IOS punt (#17) and
> installing the controller is what changed it.
>
> ### ⚠️ Correct on a fixed disk; the media-change path is still open
>
> Read/write was measured on the real 5160 on 2026-09-09: 121 KB written twice to different sectors,
> three binary compares clean, both drives normal. **The earlier "keep floppies read-only" caution is
> withdrawn.**
>
> That harness never changed media, and a media change is the open fault. @andrew-hoffman has run
> this driver in 86Box — the floppy works and is faster than real mode, with A: and B: leaving
> compatibility mode — but after changing disks a few times he hit `Fatal Exception 0E` and a
> corrupted image, which analysed as **a stale cache page flushed to the wrong disk**: a
> media-change detection failure, not the DMA-reach bug this patch fixes.
>
> Tracked as [#18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18), reopened 2026-09-10.

`HSFLOP.PDR` lives in `IOSUBSYS` and is loaded dynamically by IOS — it is **not** bundled into
`VMM32.VXD`, so a plain file copy to `C:\WINDOWS\SYSTEM\IOSUBSYS\` is enough.

See [issue #18](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/18) (#3 is closed —
it was split).

---

## ⚠️ Built and audited, NOT tested

Neither device is present on the development machine, so these are correctness rather than proven
fixes. Same `maxPhys` change as the sound driver.

| File | md5 | Device |
|---|---|---|
| **[⬇ LPT_INBOARD.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/dma/LPT_INBOARD.VXD)** | — | parallel port ECP DMA |
| **[⬇ QIC117_INBOARD.VXD](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/dma/QIC117_INBOARD.VXD)** | — | QIC-117 floppy tape |

---

## Not fixed

- **`bad extended memory: 128k`** from a stock `INBRDPC.SYS` without `NODIAGS`. The RAM is fine; the
  preliminary check is not — `bad` reads exactly 128k at every RAM size. Workaround:
  `DEVICE=C:\INBRDPC.SYS NODIAGS`. Tracked at [#11](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/11); upstream 86Box#7638 is closed `NOT_PLANNED`.
- **`ROM BIOS shadow RAM failed`** with a stock driver. Does not stop the machine being used, and does
  not cost you memory. **Root-caused, not yet fixed** — the emulator's shadow alias is at the wrong
  address for every RAM size except 5120 ([#12](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/12)). Correcting the address alone is not
  enough: it exposes a NULL dereference that crashes 86Box ([#13](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/13)). **Do not switch to
  `mem_size = 5120` to dodge this** — that is the one path that crashes.

---

## Not a patched file, but needed

- **`IOS.INI` `[SafeList]` — the manual edit every 32-bit storage driver here depends on.**
  Windows 95's I/O Supervisor refuses to hand a disk to a 32-bit driver when something it does not
  recognise has hooked `INT 13h`. On this machine that is `INBRDPC.SYS`, which is **required, not
  optional**. Add `inbrdpc.sys` on its own line under `[SafeList]` in `C:\WINDOWS\IOS.INI` and
  reboot. **No installer does this and no patch here applies it for you.** Pass condition is that
  `WINDOWS\IOS.LOG` does not exist — it is written only when IOS has a complaint. Full write-up,
  including what that `INT 13h` hook actually does (it adjusts the Inboard's wait states around the
  call and is not in the data path) and the class-wide ASPI variant:
  [`docs/ios_safelist_howto.md`](docs/ios_safelist_howto.md). Found by @andrew-hoffman
  ([#17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17)).
- **SCSI chain — working as of 2026-09-06** ([#19](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/19)).
  Nothing to patch: use Adaptec's own **`T130.MPD`**, the 32-bit protected-mode Trantor miniport
  Microsoft never put in the Windows 95 box. It was published separately and is held here at
  [`drivers/trantor_t130b/`](drivers/trantor_t130b/) unmodified, md5
  `9cc532791b9e911bfba89afbc920c4c7`, with its archive.org provenance. Install via **Have Disk**
  with [`T130-XT.INF`](drivers/trantor_t130b/T130-XT.INF), which differs from Adaptec's stock INF in
  three ways this machine needs: **no `IRQConfig`** (the card is jumpered without an interrupt),
  `Polling=1`, and `DMAConfig` dropped — DMA channel 0 is DRAM refresh on a 5160, and while the
  driver is PIO-only and never programs the 8237, Adaptec themselves comment that line out in the
  sibling `T128.INF`. `DontLoadIfConflict` comes from the same sibling. Raised by
  @andrew-hoffman.
- **ATI Mach 8 — working as of 2026-08-24** ([#4](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/4)). Nothing to patch: use Windows 95's
  **own** driver, *ATI Graphics Ultra (mach8)* (`ATIM8.DRV` + `ATI.VXD`, `MSDISP.INF` section `[ATI8]`),
  then **set the adapter's configuration manually** in Device Manager (Resources → untick *Use
  automatic settings*). The driver alone does nothing; the manual configuration is the step that
  matters. Do **not** install the Windows 3.1x driver (`MACHW3.DRV`) — it was tried first and failed.
  `display.drv=vga.drv` is **no longer needed**.
- **Keyboard `\`** — set Windows to the **US** layout. On an 83-key XT keyboard the UK backslash sits on
  a scancode the hardware cannot send ([#2](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/2)).
- **Mouse** — driven by `msmouse.vxd` from `SYSTEM.INI`, not by a Device Manager node. An errored or
  absent mouse entry is expected ([#6](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/6)).

---

## Applying them

### Plain file copy (takes immediately)

`MSSBLST.VXD`, `LPT.VXD`, `QIC117.VXD`, `KEYBOARD.DRV`, `INBRDPC.SYS`, `IVT68FIX.COM`.

```
./tools/deploy_sound_fix.sh /d          # sound, with md5 guard + backup + --revert
```

### Applying to a bundled VxD

`VDMAD`, `VKD` and `VPICD` are combined into `VMM32.VXD` by Setup. **After that combine, a
replacement is silently ignored** — `BOOTLOG.TXT` keeps loading the bundled copy, and `VMM32.VXD` is
`W4` compressed so it cannot be patched in place. This cost the project eighteen days.

Drop them into `WINDOWS\SYSTEM\VMM32\` on a **pre-monolith** install (Setup has not yet combined),
then let Setup run:

```
./vxd-patches/deploy_premonolith.sh /d     # applies the whole set, backs up to \PREPATCH
```

`BOOTLOG.TXT` tells you which path a driver took:

```
Loading Vxd = VDMAD                     <- bundled; a file copy will NOT take
Dynamic load device  mssblst.vxd        <- from disk; a file copy WILL take
```

### Prebuilt images

[Releases](https://github.com/Mike1978uk/win95-intel-inboard-386pc/releases) — the latest carries
everything above except the sound fix, which cannot live in an image (see its entry).

---

## Checking your own drivers

The DMA bug is **Microsoft's, not ours**, and hits anything XT-class. If a device produces corrupt
data rather than no data, audit it — read-only, seconds:

```
python dist/post-install-fixes/scripts/vxd_dma_audit.py YOURDRIVER.VXD
```

Method and judgment rules (including which drivers must **not** be patched):
[`.claude/skills/win9x-dma-driver-audit/`](.claude/skills/win9x-dma-driver-audit/).
