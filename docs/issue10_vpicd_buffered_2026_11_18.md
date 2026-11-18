# #10: VPICD in buffered mode - deploy and rollback

Why: `docs/issue10_ibvpicd_2026_11_18.md`. Intel's IBVPICD and the IBM BIOS program the 8259 single, buffered
(ICW1 13h, ICW4 09h); our VPICD writes cascade, non-buffered (11h, ICW3 00h, ICW4 01h). This build matches IBVPICD.

## What changes

`vxd-patches/patch_vpicd_buffered.py`, six bytes in `VPICD_INBOARD_IRQ2.VXD` (md5 `6c304f8c...`):

| | before | after |
|---|---|---|
| init | 11h 50h 00h 01h | 13h 50h 09h (old ICW4 write NOPed) |
| exit | 11h 08h 00h 01h | 13h 08h 09h (same) |

Standalone result `VPICD_INBOARD_BUF.VXD` md5 `993f23fa96d9b28a75d753aff31ea80f`.
`patch_vmm32_vpicd_buffered.py` applies the same bytes to the VPICD inside a VMM32.VXD that already carries the
#42 IRQ 2 patch; route as `docs/vpicd_irq2_patch_2026_09_26.md` (patcher9x W4 -> W3, patch, W3 -> W4).
Bed result `VMM32_buffered.VXD` md5 `ff38c7adc93cd0437f4d9afd6aec6d1a`.

## Bed

`vm_vpicdbuf` (and `_net` with the NIC) reached the desktop on the patched VMM32. Not yet recorded: the guest md5
proving that file loaded, and NIC/T130B traffic on `_net`.

## Card deploy

1. Shut Windows down; image the whole CF (owner, Win32DiskImager).
2. Host: md5 `D:\WINDOWS\SYSTEM\VMM32.VXD`. Build from the card's own file whatever it matches: copy it out,
   patcher9x to W3, `patch_vmm32_vpicd_buffered.py` (expect `Patched: 6`), patcher9x to W4.
3. Rename the card's original to `VMM32.IRQ`, copy the patched file in as `VMM32.VXD`, record both md5s here.
4. Boot. Prove the load: md5 of `C:\WINDOWS\SYSTEM\VMM32.VXD` on the guest equals the patched md5.
5. Check: desktop, NIC (IRQ 9 -> 2), T130B (IRQ 3), keyboard, mouse, floppy, twist once.

## Rollback

From DOS (boot menu SAFE, or F8 command prompt):

    cd \WINDOWS\SYSTEM
    copy VMM32.IRQ VMM32.VXD

Or restore the Win32DiskImager image.

## Results

2026-11-18: CF imaged (`prevpicdpatch_win95_1811.img`, owner). Card VMM32 was `f4933989` (the #42 IRQ 2 build);
rebuilt from it: `Patched: 6`, result `ff38c7ad`, identical to the bed file. Original kept as `VMM32.IRQ`.

Card boot 2026-11-18: desktop, no sound loop. VMM32.VXD fetched back over the V9x agent: `ff38c7ad`. The agent
link itself proves the NIC (IRQ 9 -> 2). Owner: Zip on the T130B (IRQ 3) read, floppy A: listed, keyboard and
mouse work. Twist (`tools/m8cardscripts/twist_card.ps1 -Label vpb1`): 6.73 fps, 149 ms a frame, closed with Alt+F4 -
the same as the non-buffered build (6.73). Holds on the 5160; next is the 5150.
