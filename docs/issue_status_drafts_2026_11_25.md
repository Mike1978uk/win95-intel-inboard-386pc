# Issue status drafts, 2026-11-25

Approved by the owner 2026-11-25 with four corrections; posted: #58 https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/58#issuecomment-7162640050, #49 https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/49#issuecomment-7162640472, #48 https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/48#issuecomment-7162640839. Last update on all three issues was 2026-11-18.

## #58 DirectX on the 5160

```
Status 2026-11-25. Native DirectSound: started and working in the emulator and on the card.

SBDS.VXD is a DirectSound driver for the SB Pro (drivers/sbds, docs/sbds_design_2026_11_23.md). It uses the 8237 count for position, so it takes no interrupts, and it resets the DSP and masks DMA when DirectX stops the primary buffer.

dxdiag: all four tests pass on the card (owner, 11-24) and in the emulator bed. DirectX 7 mixes on its own only when VMCPD reports a 486-class FPU. The card has a 387, so it passes; no emulator bed had one, which explains the earlier failure. Every BL3 bed now sets fpu_type = 387 and bed_launch.ps1 refuses one without it.

Card, Mode X ops/s, no sound 14.95; emulated DirectSound idle 12.5; SBDS idle 13.3 (about half the idle cost). Playing 11 kHz: emulation 11.8, SBDS 10.55. Playing is not cheaper yet, because DirectX 7's mixer still does the work. Repeats varied widely and the idle line was not captured, so treat these as a baseline, not a result.

Next: SBDS becomes the mixer (docs/sbds_mixer_plan.md): mono or stereo per moment, never giving up stereo; a feeder writing the DMA ring in dwords; our own timing; a handshake with the display driver. None of it is built.

Also: Doom 95 full screen at 320x200 runs on the VGA half at about 11 fps with -nosound. With sound it hung about 1 run in 3: a Win16 fault in Msgsrv32 (16-bit sound path), intermittent. It still happens with SWAPCOMP out, so SWAPCOMP was not the only cause; it stays retired.

Not tested: Doom 95 with sound on SBDS; any DirectSound title other than dxdiag, M8DDB and DSTEST. Open: a dxdiag Music tab crash on the card only (invalid page fault in USER.EXE).
```

## #49 Mach8 display driver

```
Status 2026-11-25. The driver is on the card with 16 bpp, DirectDraw and Direct3D; the open items from 11-18 have moved.

Since 11-18:
Two 8 bpp pages flip on the card. Twist runs at 6.73 fps (11-17) and 6.84 fps (11-22); flip2d reached 4.33 fps.
320x200x8 is now the driver's own mode on the VGA half (DirectDraw SetDisplayMode). Full frames 13.4 fps on the card; an adaptive delta flip sends only changed rows, 45 fps on the mx_flip test. Doom 95 full screen is playable with -nosound at about 11 fps.
The 16 bpp desktop background is stored on the card (X37), with a palette-matched wallpaper. The fine dither is kept right when drawn over.
Direct3D: tunnel is about 320 ms a frame; corner conversion done once cut draw calls from 77 to 72 ms. One display DC per process for escapes measured level (0.996 of the previous build, pixels equal).
The 11-18 blue screen after a DirectDraw mode change has not been seen since; mode changes such as Doom 95 switching to 320x200 run (bed, 11-25). No fix is recorded against it, so it is not marked fixed.

Not tested: the full 35-test bench against ATIM8 since the VGA-half work (the 1.27x figure is from 10-22). dist/mach8 is still an older build than the card runs.

Next: fixed-point geometry under DirectXT (D3DIM's transform, light and colour are x87 work on the 387), then the sound and display handshake in docs/sbds_mixer_plan.md.
```

## #48 Optimisation timeline

Entries since 2026-10-11. Numbers are quoted from the doc named; none re-measured today. "Unverified" means no figure in a doc I read.

```
Entries added 2026-11-25, 2026-10-12 to 2026-11-24. Sources are in docs/worklist.md and docs/next_session_2026_11_*.md.

2026-10-20 Mach8 driver at 1.045x ATIM8 on the card, pixels identical in all 35 tests (docs/captures/2026-10-21_card/).
2026-10-22 Source colour compare, scan-to-X spans and on-card row copies: 1.27x ATIM8 overall; card bitmaps 10-33x, screen-to-memory 14x. Stretching 0.41-0.46x, still behind.
2026-11-02 to 11-09 16 bpp mode and change detection by hash without imul (#54). Gain: unverified, no figure in the notes read.
2026-11-06 Gradient triangle (M8GTRI) costs 1.14-1.36x a flat one; read-back 0 of 16 centres wrong.
2026-11-09 M8FEED VxD keeps the Mach8 fed after Flip; twist first runs on the card in software. Gain: unverified.
2026-11-10 Direct3D draws 2D over the scene on the card; twist 6.5 fps with text.
2026-11-14 8 bpp full-screen modes from a 16 bpp desktop. 2026-11-15 DirectDraw overlays at 16 bpp, merged in RAM. Gain: unverified.
2026-11-17 Deferred 8 bpp clears, flush timers and 1-bit masks at Flip: flip2d 2.8 -> 3.37 -> 4.33 fps on the card; twist 6.73 fps.
2026-11-18 VPICD programs the 8259 buffered (#10). Card holds: NIC, T130B, Zip, floppy, keyboard, mouse, twist. Speed gain: none claimed.
2026-11-19 Triangles by the top-left rule, card dither from an off-screen map (m8dos). Gain: unverified.
2026-11-20 X33: BKGD_MIX left on after m8dos cost about 8 %; now restored around dither blits only. 16 bpp desktop background stored on the card (X37), 0 pixels differ after refine.
2026-11-21 320x200x8 on the VGA half. Delta flip: mx_flip 13.4 -> 45 fps, flipblt 13.1 -> 42.5. Doom 95 full screen playable with -nosound, about 11 fps.
2026-11-22 SWAPCOMP retired (see 10-02 entry: its 4.5 % paging gain is given up). Twist 6.84 fps; tunnel 327 ms a frame, was about 346.
2026-11-23 One display DC per process: desktop bench 0.996, no gain. Corner conversion once in the D3D path: draw calls 77 -> 72 ms a frame, pixels exact. Measured: emulated DirectSound costs about 22 % of a Mode X test, waveOut about 6 %.
2026-11-24 SBDS.VXD, a DirectSound driver: sound-open idle 12.5 -> 13.3 Mode X ops/s. Playing: no gain yet.

Not measured since 10-11: the XT-CF measured-wait change of 10-11 (a67fefc4) is still listed as unmeasured.
```
