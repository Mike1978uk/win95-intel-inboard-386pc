# SBDS stage 3: mixing channels (design, 2026-11-25)

Stage 3 of `sbds_mixer_plan.md`. Inputs: the census (`captures/2026-11-25_sbds_census_bed/`), the DDK guide
(`references/dxddk/ds_ddk_decoded/ds-ddk.txt`), `drivers/sbds/src/SBDS.ASM` (primary-only today).
Built and proven on the bed (`vm_5160_dbg`, 387 fitted); cost is measured on the card only (stages 2 and 6).

## What the census settled

- Programs set the primary format themselves: Doom 95 11,025 Hz 8-bit stereo, M8DDB 22 kHz mono once.
  The output format follows the primary; our mono/stereo choice applies only where it changes nothing audible.
- DSOUND asks `GetCaps` once per buffer Doom creates or duplicates (56 in the run): it will place them with us
  as soon as we offer channels.
- The primary ran ~49 min (Play to Stop) while sound effects were rare: DMA of silence most of that time,
  if the trace clock holds (one timed DSTEST to confirm).
- Doom 95's music is MIDI on the OPL, not DirectSound: it never reaches SBDS. FM register writes are their
  own bus cost (OPL waits after each write); a separate item, not this stage.

## The shape

DSOUND keeps one channel for its software mixer: the primary. So SBDS mixes **the primary + our channels**
into the DMA ring.

| | Today | Stage 3 |
|---|---|---|
| Primary buffer | the DMA ring itself | a 16 KB block of ordinary locked memory; DSOUND writes it as now (no Lock) |
| Secondaries | refused, DSOUND mixes them on the 387 | up to 8 channels, 8-bit mono/stereo, volume, pan, frequency |
| Play cursor of the primary | 8237 count | our mixer's read position (a virtual DMA) |
| Who writes the DMA ring | DSOUND | SBDS, from a `Set_Global_Time_Out` tick |
| Silence | DMA runs until DSOUND stops the primary | DMA stops when no channel plays and the primary block is silent; restarts on Play |

## Driver interface changes

- `GetDriverDesc`: add `DSDDESC_USESYSTEMMEMORY` (DSOUND allocates each secondary, we read it in place) and
  `DSDDESC_DONTNEEDSECONDARYLOCK`. `dwHeapType` stays NOHEAP.
- `GetCaps`: `DSCAPS_SECONDARYMONO + STEREO + 8BIT` (16-bit secondaries left to DSOUND's mixer: SB Pro is 8-bit
  out, so they would only be truncated); `dwMaxHwMixingAll = Streaming = Static = 8`, free = 8 minus in use;
  3D all 0; secondary rates 4,000-44,100.
- `CreateSoundBuffer` (secondary): accept 8-bit PCM with CTRLVOLUME/PAN/FREQUENCY; refuse CTRL3D and 16-bit so
  DSOUND keeps those. Take a channel slot; keep DSOUND's memory pointer and size.
- `DuplicateSoundBuffer`: new slot, same memory, own cursor and controls (Doom plays one effect twice at once).
- Buffer methods for secondaries: `Play` (LOOPING or one-shot), `Stop`, `SetPosition`, `GetPosition`
  (one-shot done: return the buffer length, DSOUND then calls Stop - DDK), `SetFrequency`, `SetVolumePan`.
- The primary keeps its own path (format, refuses controls); its `GetPosition` returns the mixer's read point.

## The mixer (integer, no FPU)

- Per channel: 16.16 position step = channel rate / output rate; `SetVolumePan` builds two 256-entry tables
  (left, right) of signed words = (sample - 80h) x amp factor, from `dwTotalLeft/RightAmpFactor`. Inner loop =
  table load + add, no multiply. Primary contributes (byte - 80h) per channel.
- Sum in a cached word scratch block, clip through a table to unsigned bytes, then copy to the DMA ring in
  dwords (the feeder's copy, stage 4, is this copy plus timing).
- Mono output only when the primary block and every playing channel are mono or centred; otherwise stereo.
  `ForceStereo=1` in `HKLM\Software\SBDS` keeps stereo. Switching happens at a half-buffer with a DSP restart;
  the primary's own format always wins when it is stereo and not silent.
- Tick: `Set_Global_Time_Out` sized to fill half the ring (~370 ms at 22 KB/s). Stage 4 tunes it.

## Probe result (`captures/2026-11-25_sbds_probe_bed/`)

Memory is in the shared arena (risk 1 cleared). Doom 95 holds one buffer per sound and DSOUND places a buffer
with us only while we report a free channel, so 8 channels took Doom's first 8 sounds and the rest went to
DSOUND's mixer. Channels become descriptors: offer 64, mix only the playing ones. Doom changes pitch per play,
so frequency control is needed from the start.

## Risks, in test order

1. ~~Where DSOUND puts a secondary's memory~~ - shared arena, safe from any context.
2. **Paging at tick time.** The tick may not take a page fault. Lock a channel's pages on Play
   (`_LinPageLock`, in the caller's thread), unlock on Stop and at the end of a one-shot (deferred to the next
   call in a thread). Free RAM is 0 under Doom, so lock only what plays.
3. **Latency.** A 370 ms tick is too late for a gunshot. Mix ahead only a short window (~40 ms) on Play and
   on each tick; keep the ring margin small. Stage 4 settles the balance.
4. DSOUND may stop using the primary when all buffers are ours - good; or keep writing silence - then the
   silence test is a scan of the block (cheap: local RAM reads, early exit).

## Proof (bed)

DSTEST (`once`, `loop`, `mono`) cursor and end; dxdiag all four; Doom 95 by ear with the trace build (shows
SetVolumePan, so the pan question is answered). Then one card trip for stage 2 and the bench.
