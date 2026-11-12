# Next session, 2026-11-12

Earlier handoff: `docs/next_session_2026_11_08.md` ("START HERE: 2026-11-10, evening"). Newest wins.

## START HERE

Card: boot to Windows, V9x agent up, `C:\M8D3D.INI` has `[M8TWIST.EXE] lores=0 zsort=1 palette=1`.
Built, not installed: **GX** (`%TEMP%\m8d3gx`) = 219e81e + page-0 clear (`m8dd32.c`, uncommitted
until the card run passes). GW's blue band at twist's start was page 0 still holding the last slow
frame in the desktop colour cube.

## Settled today (2026-11-12)

The I/O cache flush, `docs/captures/2026-11-12_flushprobe/README.md`:
- IN/OUT invalidate the whole L1 (78 ns a byte of working set to refill). ISA memory reads and
  writes do not. **Once per burst**: 1000 OUTs cost the same refill as one.
- Card memory (VGA half): write 1.83 us/B dword, 1.94 word; read ~3.5. PIX_TRANS words 3.82
  plus the refill. Dword OUTs to the Mach8 stay dead (M8DWORD: upper word hits the DAC mask).
- Driver shape: compute a frame into RAM, send it in as few bursts as possible, touch little
  between bursts. FIFO polls interleaved with work are each a refill.
- VGA half beats the engine for whole-frame pixel pushes (video, software 3D, Doom) at low
  resolution; the engine wins for fills, copies, lines and triangles. One half shows at a time.
- Video plan: `docs/video_playback_plan_2026_11_12.md` (issue draft inside, owner posts).

## Today's jobs, in order (tick here and in `docs/worklist.md` as they land)

| # | Job | Model | Where |
|---|---|---|---|
| 1 | Install GX (`install_h.ps1 -Dir $env:TEMP\m8d3gx -Name M8D3GX -Run gx`); owner stops the agent and runs twist: blue band gone? pauses with agent off? | Opus | card |
| 2 | Commit the page-0 fix; push from PowerShell (Bash has no GitHub login) | Haiku | git |
| 3 | ~~Burst census build~~ not needed: Flip already holds the whole frame before sending (`m8d3d_q_hold`), and the send loop touches only the streamed queue. Instead, split GX's twist Flip from its census: writes x 7.64 us (bus), fill pixels x 69 ns (engine), the rest CPU. Tunnel 11-09: ~5,600 writes = ~43 of 81-88 ms | Opus | desk |
| 4 | Feeder: stop cleanly at program exit and on mode change (11-09 hang); then sends shaped as bursts; bed first, then card | Opus | bed, card |
| 5 | Monster Truck Madness setup in bed `vm_5160_mcp`: what it installs (files, registry, DirectX parts), the missing-component error, nothing on the card | Sonnet, background | bed |
| 6 | Bed fidelity plan: what 86Box's Inboard charges for an OUT now against 7.6 us + flush on the card; how to model the flush per burst. Plan doc only | Sonnet, background | desk |
| 7 | Smooth shading: gradient triangle along the 16-step ramps, pattern-register dither between steps | Opus | bed, card |
| 8 | Fable review pack: file list, measured numbers, method docs, open questions, so Fable reads code not docs. **Ask the owner before Fable starts.** Review after 4 lands | Sonnet builds pack; Fable reviews | desk |

Later, in order: DxDiag rerun; MTM on the card; unused silicon (hardware scissor for clipping,
VRAM texture atlas, third page, FIFO wait states, `docs/silicon_for_d3d_2026_11_09.md`);
FastDoom on the VGA-half memory path (#47); video (`video_playback_plan`); DirectSound for the
SB Pro (#58); "DirectXT" (D14). Showcase grid: each demo in each mode, fps, once 7 lands.

## Agent roles (owner, standing)

- **Opus (main):** driver code, diagnosis, reading measurements, card runs. Never delegate diagnosis.
- **Haiku:** every commit and push. Exact paths, message file, `git diff --cached --stat` check,
  never `git add -A`, push from **PowerShell**, report `git log --oneline -3`.
- **Sonnet:** research, write-ups, bed chores with a written brief; background, does not commit.
- **Fable:** hard reviews only, after asking the owner. First: the driver review after job 4.
- One card job at a time. Bed first for anything that fails before the first frame.
- COMrade only with the mouse unplugged; DOS probes via a `.BAT` (keystrokes drop on long lines).
