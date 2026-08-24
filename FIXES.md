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

**Copy to `C:\WINDOWS\SYSTEM\MSSBLST.VXD` AFTER installing the Sound Blaster Pro driver.** It is not
in a fresh image — it arrives stock from `WIN95_xx.CAB` when you install the card.

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
the emulator fails a check real hardware passes ([86Box#7638](https://github.com/86Box/86Box/issues/7638)).
Copy to `C:\INBRDPC.SYS`.

### `IVT68FIX.COM` — INT 68h vector

**[⬇ IVT68FIX.COM](https://github.com/Mike1978uk/win95-intel-inboard-386pc/raw/master/ivt68fix/IVT68FIX.COM)** · 20 bytes

Points `INT 68h` at `F000:FF53` (thanks to Michal Nečasek). **Must be the very last line of
`AUTOEXEC.BAT`** — run earlier it gets clobbered by DOS's own low-memory init.

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

- **ATI Mach 8** display driver — configures but does not work ([#4](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/4), [#8](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/8)). Use `display.drv=vga.drv`.
- **`bad extended memory: 128k`** from a stock `INBRDPC.SYS` without `NODIAGS`. The RAM is fine; the diagnostic is not. Emulator-side, under investigation ([86Box#7638](https://github.com/86Box/86Box/issues/7638)). Workaround: `DEVICE=C:\INBRDPC.SYS NODIAGS`.
- **`ROM BIOS shadow RAM failed`** with a stock driver. Cosmetic — shadowing works; the self-test does not agree.

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
