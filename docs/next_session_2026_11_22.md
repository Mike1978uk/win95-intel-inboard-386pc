# Next session (written 2026-11-21 evening)

Read this first. Earlier today: `docs/next_session_2026_11_21.md` (X1 committed, `96e99b9`).

## State

- **Card** runs `M8VGH.DRV` + its `M8DD32.DLL` (md5 `94a741cd...`): X1 + palette fix + delta flip +
  diagnostics (VGA trace lines, heartbeat thread). `C:\M8D3D.INI` has `[diag] trace=1`, so every
  VGA-mode program writes `C:\M8TRACE.TXT`. Before any timing run: set `trace=0` or install a clean build.
  Desktop bench of this build: 1.068 vs `card_vgb`, pixels equal.
- **Uncommitted** in `drivers/mach8_display/src` (`dd16.c`, `enable.c`, `m8.h`, `m8dd32.c`, `m8hw.c`) and
  `tools/m8ddbench/M8DDB.C`. Saved whole in `docs/captures/2026-11-21_vga_card/wip_x1_doom_m8vgh.diff`.
  Split it when committing:
  1. **Keep, proven**: palette through 3C8h/3C9h in VGA mode (`m8hw.c` `m8_hw_set_palette`, `enable.c`
     `m8_vga_show` order); adaptive delta flip (`m8dd32.c` `m8dd_vga_send`, record `vga_sent` from
     `dd16.c` `m8dd_vga_alloc`/`m8dd_vga_clear`; Unlock/Blt on A0000 invalidate it).
  2. **Remove before commit** (diagnostics): `m8dd_vga_traced` trace lines, heartbeat thread
     (`m8dd_vga_beat`, `m8dd_vga_n`, `m8dd_vga_last`), `dg_pal`/`dg_pal_busy`/`dg_flags`/`dg_deflags`
     share fields, `m8dd_diag_pal` (dd16.c, m8.h, enable.c SetPalette).
  3. **M8DDB.C**: `modexs` (looped PlaySound) and `modexd` (DirectSound loop + 100 ms shots) - keep, tools.
- Bed `vm_5160_dbg` has the heartbeat build (`%TEMP%\m8vgh`) as `M8PCL.DRV`/`M8DD32.DLL`; trace on.

## Done today

| | Result |
|---|---|
| X1 320x200 on the VGA half | committed `96e99b9`; card 13.4 fps full frames |
| Colours in VGA mode | 86Box keeps two palettes; write 3C8h/3C9h too. Bed Doom correct (`bed_doom95_palette_*.png`) |
| Delta flip (adaptive) | card mx_flip 13.4 -> **45 fps**, flipblt 13.1 -> 42.5; full-change frames as before. A RAM-shadow primary was tried and rejected: RAM stores cost ~1/4 of a bus byte here, so full frames halved (6.6 fps) |
| **Doom 95 full screen on the card** | **playable with `-nosound`**, ~11 fps (heartbeat: 22 flips / 2 s), quits to the desktop |

## Open: Doom 95 + sound hard-hangs the card

Symptom: black screen, sound buffer loops, Ctrl-Alt-Del dead, power cycle needed. Heartbeat (time-critical
thread) stops within ~2 s of gameplay starting, so ring 0 or interrupts off. Level load (~34 s) completes.

| Run | Result |
|---|---|
| bed, same build, sound | plays (bed has SB Pro) |
| card `-nosound` | **plays** |
| card `-nomouse` | hangs (DMOUSE.VXD cleared) |
| card `-nomusic` | hangs (FM/MIDI cleared) |
| card `-nosfxformat` | hangs (primary-buffer SetFormat cleared) |
| card M8DDB `modexs` (PlaySound loop) during 320x200 | fine |
| card M8DDB `modexd` (DirectSound loop + shots) during 320x200 | fine |
| 2026-11-19, old driver, full screen | also wedged (`next_session_2026_11_19.md`) |
| windowed Doom with sound, 8514 path (11-21 am) | fine |

Owner notes: during one hang the Zip (T130B SCSI, IRQ 3) clicked as if accessed.

**Next, in order:**
1. `-640x480` with sound (full screen on the 8514 half, no VGA half): separates "our VGA mode" from
   "full-screen Doom + sound". Script ready: `%TEMP%\m8vga_bed\card8.ps1 -Args2 '-640x480' -Tag fs640`
   (launch, ENTER for New Game, 7 min ping log; trace appended, not deleted).
2. Make `modexd` look more like Doom: per-tic `SetFrequency`/`SetPan`/`SetVolume`, `DuplicateSoundBuffer`,
   11025 Hz lumps, buffers made after DDraw exclusive. If it hangs, fixes can be tried without Doom.
3. Then disassemble what Doom's `I_StartSound`/`I_UpdateSoundParams` call (DOOM95.EXE strings show
   `i_sound.c`, `_lpDSBPrimary`, "Sound set to Normal/High").
4. Owner's question: are the periodic desktop sound loops the same class (IRQ 5 serviced late)? Point
   `modexd`-style DirectSound at desktop activity and measure gaps.

Scripts: `%TEMP%\m8vga_bed\` (`card7/8.ps1` Doom runs, `beddoom.ps1` bed Doom, `cycle.ps1` bed 320x200).
Doom 95 on the card is `C:\DOOM95\DOOM95.EXE`; the launcher's Advanced -> Screen Resolution is fixed
(320x200, 320x240, 640x400, 640x480; window/full screen). Quitting by agent works:
`input -Sequence "key ESCAPE; delay 1500; key UP; delay 800; key ENTER; delay 1500; key Y"`.

## Results 2026-11-22: the Doom + sound hang is SWAPCOMP

| Run | Result |
|---|---|
| card `-640x480` + sound (8514 half, no VGA half) | black throughout (title never shown: separate palette bug), stutters, then sound loop + hang. **VGA half not needed** |
| card 320x200 + sound + PERFLOG (buffered rows) | hang; rows lost in VCACHE at the power cycle. PERFLOG now takes `flushrows` and commits each write |
| card 320x200 + sound, **SWAPCOMP out** (`;DEVICE=c:\swapcomp.vxd`, PERFLOG `NS`) | **plays ~9 min**, full screen, jittery; free RAM 0, swap 5.5 -> 11 MB, ~15,000 page-ins, CPU 100 % |

Owner: disk active just before and after the loop, so the machine kept running and paging; the yesterday
`-nosfxformat` trace shows the heartbeat alive 16 s after Doom stopped drawing. Reading: heavy paging + sound
with SWAPCOMP in the page path hangs; without it, the same thrash survives. 6 hangs with, 1 pass without - repeat once.
Probably #55 as well (boot and restart: paging with sound playing; #55 opened a month after SWAPCOMP went on).

- Card now: SWAPCOMP commented out in `C:\WINDOWS\SYSTEM.INI` (backup `SYSTEM.BSR`, md5 `cd805526`; copy in
  `docs/captures/2026-11-22_doom_sound/SYSTEM_card_before.INI`). SHADRAM still loaded. `C:\PERFLOG.EXE` is the
  flushrows build; the old one is in the captures folder. `C:\DOOMLOG.BAT` = PERFLOG DS + Doom.
- Watch: if #55 sound-loop hangs stop with SWAPCOMP out, it is the same bug.
- `dist/swapcomp/README.md` carries a warning; worklist item added. Fix only if worth it (~4.5 % measured).
- Doom speed next: F5 detail, view size (`-`), `-nomusic`, background programs closed; no PERFLOG for timing.
- Not caused by: the VGA half, `-nomouse`, `-nomusic`, `-nosfxformat`. DirectSound on the SB Pro is emulated
  (no DS driver) and the 1 MB DMA limit cannot hang playback (DMA only reads); neither needs patching.

## Issue #10 - the 5150 runs Windows 95 (Simon, 2026-11-21)

Simon's recipe on his 5150: the 08-24 release image's Windows directory, our buffered `VMM32.VXD`
(`vxd-patches/VMM32_buffered.VXD`, md5 `ff38c7ad...`, ICW4 09h + #42 IRQ 2) in `C:\WINDOWS\SYSTEM`, and
**delete `C:\WIN386.SWP`** (the image's swap file; Windows makes a new one). That cleared the
"Insufficient memory" stop. Cause of the swap-file effect not verified. His draft VCFED post is with the
owner; he asks whether the buffered VMM32 is public (it is in the repo, not in a release or FIXES.md).

To do (owner asked for all of it; not started beyond the survey below):
1. **Buffered builds of every shipped variant.**
   - `VPICD_INBOARD.VXD` (non-IRQ 2, `65fa4757`): same 8259 sequences at 0x7076/0x1499 with ICW3 04h
     (IRQ 2 build has 00h). `patch_vpicd_buffered.py` is pinned to the IRQ 2 md5; extend it to accept
     `65fa4757` too (the ICW3 byte is overwritten with 09h either way). Output e.g. `VPICD_INBOARD_BUF_NOIRQ2.VXD`.
   - The release image's `VMM32.VXD` (08-24, non-IRQ 2): extract from
     `vm_simon5150/win95_sound_network_release.img`, patcher9x W4->W3, buffered patch, W3->W4.
     `patch_vmm32_vpicd_buffered.py` expects the IRQ 2 VPICD inside; same extension.
   - Already buffered: `VPICD_INBOARD_BUF.VXD` (`993f23fa`), `VMM32_buffered.VXD` (`ff38c7ad`, the card's since 11-18).
   - `VMM32_combined.VXD`, `osr1/vmm32_combined_kbdfix.bin`: history, not offered for download; leave.
2. **FIXES.md**: a 5150 section (buffered VMM32 + delete WIN386.SWP), links to the buffered files with md5s;
   say buffered is right on the 5160 too (card since 11-18: NIC, T130B, Zip, floppy, keyboard, mouse, twist 6.73 fps).
3. **README**: 5150 supported; #10 row.
4. **Issue #10**: status block + closing comment, drafted for the owner (owner approves wording; thank Simon).
   `docs/contributor_input_ledger.md` entry. Consider a release with the buffered VMM32 in the image (owner
   verifies releases, G10).
5. Reply to Simon about his VCFED draft if the owner wants one (owner sends all contact).

Standing rule (owner, 11-21): cheaper agents for prescribed work - Haiku commits/pushes (PowerShell, exact
paths), Sonnet docs/research (no commits); git and driver review once work is proven.
