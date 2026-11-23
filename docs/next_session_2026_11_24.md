# Next session (written 2026-11-23, evening)

Earlier: `docs/next_session_2026_11_23.md` (task list T1-T11, all of today's results).

## State

- **Card** boots `M8CW.DRV` (`253b949`, T3b) + its `M8DD32.DLL`, boot 517, idle. `dist/mach8` is still
  `e3544f3`'s build: refreshing it to M8CW needs the owner's go-ahead.
- `C:\WINDOWS\DRWATSON.EXE` installed (from `WIN95_20.CAB`); start it only in sound runs. A BAT's
  "Finished" DOS window can hold focus and swallow the agent's restart keys.
- Never start a card run the owner must exit (Doom 95, `-timedemo`). Doom 95 takes `-320x200` (full
  screen, Mode X) or `-640x480w` (window); without one it uses the launcher's saved setting.
- Run benches one at a time through `v9xctl exec` with `-TimeoutSeconds`, not from one BAT.

## Today, in one line each

| | Result | Where |
|---|---|---|
| Tunnel frame | ~320 ms: our draw calls 72 (after T3b), Flip 87, rest ~155 | `d14a_mode_switch_profile_2026_11_23.md` |
| D3DIM's share | its own geometry on the 387: transform 42 %, point light 24 %, colour 9 % | same, T3c |
| Doom + sound hang | Win16 fault in `Msgsrv32` (16-bit sound callbacks), 1 run in 4, SWAPCOMP out | `captures/2026-11-23_doom_eipsamp/` |
| Windowed Doom in our driver | fb16 fingerprints ~12 %; sending whole tried 3 ways, reverted | same |
| **Sound cost (T4)** | Mode X: waveOut ~6 %, **emulated DirectSound ~22 %** of every test | `captures/2026-11-23_t4_sound/` |

## Next: T5, a DirectSound driver for the SB Pro (#58)

Why: DirectSound titles (Doom 95, most DX games) pay ~22 % of their frame rate for sound, against ~6 % for
plain waveOut. The ~16 % between is DirectSound's emulation: secondary buffers mixed in software into a
buffer that is then handed to the 16-bit wave driver (`waveOut`, MMSYSTEM, the SB Pro driver), with the
callbacks in `Msgsrv32` where today's fault was.

Ceiling, not promise: the SB Pro has no hardware mixing, so DSOUND still mixes secondaries in software with
a driver of ours. What a driver removes is the waveOut path (16-bit thunks, callbacks, the emulation's
buffering). Step 1 measures how the ~16 % splits before anything is built.

1. **Split the 16 %** (cheapest first). EIPSAMP samples the main thread only; DSOUND's mixer is another
   thread. Either add an all-threads mode to EIPSAMP or compare `modexd` with `modexs` by KERNEL32 thread
   times. Result: mixing vs waveOut path, the gain ceiling.
2. **Read before writing.** The DirectX DDK's DirectSound HAL interface (DSDRIVER, `IDsDriver` /
   `IDsDriverBuffer` in a VxD), any sample in the local DDK copies (`reference-local-source-copies`), and
   the SB Pro driver Windows uses now. Name the explored techniques to reuse (owner's rule).
3. **Constraints this machine sets:**
   - 8-bit DMA, channel 1, IRQ 5, port 220h. The XT page register reaches 1 MB only: the DMA buffer must
     sit below 1 MB and not cross 64 KB (`win9x-dma-driver-audit` skill: run it first).
   - SB Pro DSP 3.xx: auto-init 8-bit DMA, 22 kHz stereo or 44 kHz mono at most.
   - Every store crosses the slow bus: the primary buffer DSOUND mixes into is RAM, the DMA buffer is RAM,
     so no extra copy if the DMA buffer *is* the primary.
4. **Then coordinate with the graphics** (owner): mix and flip in one place, so one wake-up a frame serves
   both, not two threads contending.

Measure on the card as T4 did (`M8DDB modexd` vs `modexs` vs `modex`, twice each); the target is `modexd`
close to `modexs`.

Across the stack: DOS titles and DirectXT drive the SB Pro themselves; DirectXT's DirectSound (HX) could
sit on the same mixing code once it exists.
