# Post-install driver fixes — Windows 95 on an Intel Inboard 386/PC (IBM 5160)

**Apply these AFTER you have installed a driver, not to the disk image.**


> **Alternative for the Sound Blaster Pro fix:** it can also be installed in one step, with no copy
> afterwards, via the Have Disk package in
> [`dist/havedisk-sbpro/`](../havedisk-sbpro). That package is **untested**; the copy-over-the-top
> route described here is the one confirmed on real hardware and remains the default.


## Why this exists separately from the image

The pre-monolith image does not contain `MSSBLST.VXD`. Nothing does, until you install the
Sound Blaster Pro driver — at which point Windows extracts a **stock, unpatched** copy out of
`WIN95_xx.CAB` in `C:\WIN95`. So the fix cannot be baked into the image; it has to be applied
after the driver lands.

The same is true of any other driver you install later.

## The bug, briefly

The IBM 5160 has a **4-bit DMA page latch**: DMA reach is 20-bit, 1 MB. Windows 95's drivers assume
24-bit, 16 MB. A driver that allocates its DMA buffer above 1 MB does not crash — the page register
silently drops the high bits and the DMA controller transfers against a completely different
physical address.

For the Sound Blaster Pro that meant `MSSBLST.VXD` allocating at `0x4E0000` and the card DMAing from
`0x0E0000` — adapter ROM space. It played ROM contents. That is the distortion.

```
before:  [dmapage] ch=1 val=4E -> page=0E  *** TRUNCATED, buffer is above 1MB ***
after:   [dmapage] ch=1 val=09 -> page=09  ok
```

Confirmed fixed on real hardware, 2026-08-24.

## Applying the sound fix

With the CF card in a reader (say it mounts as `D:`), from the project repo:

```
./tools/deploy_sound_fix.sh /d
```

It md5-checks the file already on the card and refuses anything it was not derived from, backs the
original up to `D:\PREPATCH\`, verifies the copy landed, and reverts with `--revert`.

Or by hand — copy `MSSBLST_INBOARD.VXD` over `D:\WINDOWS\SYSTEM\MSSBLST.VXD`. Same size, so nothing
else needs to change.

| File | md5 |
|---|---|
| stock | `cc7e63aacb1f599fcd5b3fa1eb98169c` |
| patched | `dcf32b4a7d8dbcc47e659847742417b6` |

## Checking any other driver

`scripts/vxd_dma_audit.py` is read-only and takes seconds:

```
python scripts/vxd_dma_audit.py D:/WINDOWS/SYSTEM/SOMEDRIVER.VXD
```

To check a whole install at once, against a raw image:

```
python scripts/sweep_image_dma.py yourimage.img
```

To patch one it flags:

```
python scripts/patch_vxd_dma_maxphys.py IN.VXD OUT.VXD
```

**Read the verdict, do not patch everything it lists.** `maxPhys` around 16 MB means the driver is
deliberately declaring an ISA DMA constraint — patch that. `maxPhys = 0xFFFFF` means "anywhere", an
ordinary allocation that never touches DMA — leave it alone, because forcing it low spends the only
DMA-capable RAM the machine has.

## Also included

`LPT_INBOARD.VXD` and `QIC117_INBOARD.VXD` — the parallel port and QIC-117 tape drivers have the
same 16 MB assumption. **Built and audited, but not tested**, because neither device is in use on the
machine this was developed on. Correctness rather than a known fix. Treat accordingly.

## Credit

The 4-bit page register was called correctly, from the hardware and before any of it was measured,
by **@andrew-hoffman** on
[issue #5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5).
