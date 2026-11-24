# SBDS mixes: sound and graphics sharing the bus by design (plan, 2026-11-24)

Owner, 11-24: "with our driver we know when there is space between the graphics and sound subsystems and buffer
accordingly - maybe a feeder like the graphic side could also keep the fifo full on both counts and reduce noise,
cpu and bus effort". Stereo stays available (the SB Pro is here for stereo and CD audio); mono only when nothing
playing needs stereo, with a registry override.

Start: `sbds_design_2026_11_23.md` (SBDS today: primary-only, DirectX 7's kernel mixer fills it).

## What the machine pays today

| | cost | status |
|---|---|---|
| Interrupts | none: position from the 8237 count | done, by design |
| Silence | none: DX7 stops the primary when idle, SBDS resets the DSP and masks DMA | done |
| Format | DirectSound's default primary, 22 kHz 8-bit stereo, 44,100 B/s by DMA | DSOUND decides, not us |
| Mixer timing | DSOUND.VXD's timer, 22-200 ms, blind to the display | DSOUND decides |
| Mixer stores | 486 writes each mixed byte into the DMA buffer; stores are write-through (`project-write-through-stores`), then the 8237 reads it again: two bus crossings per byte | **inferred, measure first** |
| Store width | unknown; memory dword 2.19 us/B against byte 2.86 (`isa_memory_vs_io_2026_09_20.md`) | **measure** |

## The plan: SBDS is the hardware mixer

DirectSound places secondary buffers in "hardware" when the driver offers mixing channels (`DSCAPS_SECONDARY*`,
`dwMaxHwMixing*`). Our 486 code becomes that hardware. DDK guide, "Hardware Mixing":

- each channel: play/stop, looping, stop at end, stop after N bytes, **volume and pan** (else DSOUND will not use
  it), frequency (pitch);
- `DSVOLUMEPAN` hands us 16.16 fixed-point amplitude factors (`dwTotalLeftAmpFactor`, `dwTotalRightAmpFactor`):
  integer mixing, no FPU, so it runs on a machine without one (DX7's own mixer needs VMCPD type >= 3);
- DSOUND **keeps one channel for its own software mixer** and expects the card to add that stream to the hardware
  buffers: SBDS mixes DSOUND's primary stream plus its own channels into the DMA ring;
- `DSDDESC_USESYSTEMMEMORY` + `DONTNEEDSECONDARYLOCK`: DSOUND allocates each buffer and the game writes straight
  into it; no heap of ours.

What that gives us, which today we cannot have:

1. **Format per moment.** We see every buffer, its channels and its pan. Nothing stereo or panned playing: mix mono,
   the DMA carries half the bytes. A panned or stereo buffer starts: switch to stereo at the next half-buffer.
   `ForceStereo=1` in `HKLM\Software\SBDS` keeps stereo always. (Census below decides how common each case is.)
2. **A feeder.** Mix into a cached scratch block, then copy to the DMA ring in whole dwords in one burst: each byte
   crosses the bus once, at the cheapest width. Same idea as the display driver's feeder.
3. **Timing we choose.** Our own `Set_Global_Time_Out`, sized to the 16 KB ring (~370 ms stereo, ~740 ms mono):
   few, large mixes instead of DSOUND's 22 ms wake-ups.
4. **Harmony with the display.** Both drivers are ours. A shared flag (display busy / idle, set by the display
   driver around its engine work) lets the feeder fill the ring in the gaps rather than mid-blit. The ring is the
   slack: it only has to be topped up before the DMA reaches the end.

## Order (measure once, at the end of each stage)

1. **Census + baseline** (card, trace build, one restart): Doom 95 and the other DirectX titles we have - do they
   set the primary format themselves, which buffer formats, do they pan. One M8DDB `modexd/modexdf/modexdi` run after
   the idle settle: today's cost. Doom 95 also tests the open Msgsrv32 sound-loop hang with sound on SBDS.
2. **Measure the store path**: time DSOUND's mixer writing the ring (bus cycles per mixed byte) - confirms or kills
   "two crossings per byte".
3. **Channels, mono, no feeder**: secondaries in SBDS with volume/pan/frequency, mono/stereo per moment. Proven by
   DSTEST (play cursor, end) and dxdiag (all four), then the census titles by ear.
4. **Feeder + own timing.**
5. **Display handshake.**
6. One bench against stage 1's numbers.

## Open questions

- Which DirectSound calls arrive for a hardware secondary, and in what order (Lock/Unlock skipped with the flag;
  SetPosition; the "stop after N bytes" form of Play). The DDK sample is primary-only; the trace build will show it.
- DSOUND's software channel: its format and rate are still DSOUND's choice. Does it run at all when every buffer is
  ours (then the FPU gate stops mattering), or does it keep the primary running?
- 3D buffers (`IDsDriverPropertySet`): refused today (`Buf_QueryInterface`); leave to DSOUND's software mixer.
- Ring-0 mixing happens in our time-out handler: keep it short enough for interrupt latency (serial mouse, NIC).
