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

## ⛔ Deployed but not loaded — no effect

### `HSFLOP.PDR` — floppy DMA reach

**[⬇ HSFLOP_XTDMA.PDR](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/floppy/HSFLOP_XTDMA.PDR)** · 18,998 bytes
· [stock original](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/vxd-patches/floppy/HSFLOP_stock.PDR)

Same bug as the sound driver, one byte: `maxPhys 0x1000 → 0xFF`
(`68 00 10 00 00` → `68 FF 00 00 00`, so no instruction boundary moves).

Floppy DMA is **channel 2**. Where sound merely distorted, a floppy read that returns the wrong
bytes fails its CRC and the driver retries forever — motor on, light on.

> ### ⛔ Deployed, but currently INERT — measured 2026-08-25
>
> The patched file **is** on the CF card (md5 verified against the card itself) and the controller
> **is** installed (`PNP0700` present in `SYSTEM.DAT`). But `BOOTLOG.TXT` contains **zero**
> `hsflop` entries — **Windows never loads this driver**, so the patch cannot be having any effect.
>
> The reason is in the same log: `RMM.PDR`, the **Real Mode Mapper**, loads and initialises
> successfully, and `ESDI_506.PDR` never loads either. The whole storage stack — hard disk *and*
> floppy — is running through **real-mode BIOS**, not 32-bit drivers.
>
> So the floppy stall is in the real-mode path (Sergey's ROM + DOS via RMM), not here. The
> 20-bit DMA reach may still be the cause, but it would be biting the real-mode buffer, and
> that is a different fix. Predicted by @andrew-hoffman on
> [#3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3) before it was measured.

`HSFLOP.PDR` lives in `IOSUBSYS` and is loaded dynamically by IOS — it is **not** bundled into
`VMM32.VXD`, so a plain file copy to `C:\WINDOWS\SYSTEM\IOSUBSYS\` is enough.

See [issue #3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3).

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
