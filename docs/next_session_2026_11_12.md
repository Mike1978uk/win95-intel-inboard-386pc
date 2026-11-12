# Next session, 2026-11-12

Earlier handoff: `docs/next_session_2026_11_08.md` ("START HERE: 2026-11-10, evening"). Newest wins.

## START HERE (evening state)

Card: boot 368 on **GY** (`9a82811`), 800x600x16. `C:\M8D3D.INI`: `[feed] on=1` (backup
`C:\M8D3D.BK2`), `[M8TWIST.EXE] lores=0 zsort=1 palette=1`. SYSTEM.INI `[vcache]` Min 128 / Max 256
(was 512/1024; backup `C:\WINDOWS\SYSTEM.B12`; owner: keep). Depth REGs on card:
`C:\V9XREMOTE\REG\M8BPP8.REG` / `M8BPP16.REG` (800x600; reboot after).

Next, in order (owner, 11-12 evening):
1. Read the Filemon bed result (`docs/paging_trace_2026_11_12.md`, Sonnet agent, bed `vm_5160_page`).
2. **Textures on the card** (storage half of D5): claim textures in video memory, store at 8 bpp
   converted once at upload, Lock = temporary RAM buffer uploaded at Unlock, colour summary built at
   upload, fall back to RAM when the card is full. twist uses no textures (census `textured 0`): this
   is for MTM on Direct3D and the tunnel. If Filemon shows RAM surfaces paging, claim those too.
3. ~~Feeder stop points~~ **done, GZ** (card boot 369): the feed ends in the three escapes that
   reach the 16-bit driver (`m8dd_flush`, `m8dd_send`, `m8dd_copied`) and after the 16 bpp branch
   in Blt/Lock/Unlock. twist 5.73 fps (GX 5.17, GY 4.75), frame ~175 ms, ~2,660 of ~2,685 writes a
   frame fed in the background, 3 of 3 clean exits. Ours 21 -> 34 ms a frame: FIFO waits at the stop
   points, next to trim.
4. 8 bpp Direct3D (D1 gate): route (b) = remove one instruction in D3DIM.DLL / D3DIM700.DLL
   (`tools/d3dim_8bpp_patch.py`, dry run on the bed's copies: Patched 1 each, offsets 0x33E9 /
   0x2CE1). **Owner approval pending** before bed then card. Then Monster Truck Madness with
   `useDirect3D=1`. Order agreed: feeder, 8 bpp D3D, textures (at 8 bpp), Fable.
5. Fable review: brief `docs/fable_driver_review_brief.md`; after textures. Ask the owner first.
6. Paging: Filemon hangs the guest (`docs/paging_trace_2026_11_12.md`); 2.02 MB locked at boot
   before twist, ~1,250 discards in twist's first 35 s (code re-read). Cheap next check: locked
   memory after an 8 bpp boot against 16 bpp.

## Results 11-12 afternoon

- GX: blue band gone (owner), twist 5.17 fps. GY (feed stop on owner change and mode change): 3 of 3
  twist exits clean with the feed on; flip 88 -> 39 ms, frame ~210 ms (no gain: item 3 above).
- 2D at 800x600x8, GX against ATI: **1.79x geometric mean, faster in 27 of 35, pixels = ATI in all
  35** (`card_gx81/82`). Only mem_to_mem dips under 1 (0.94; it swings 12 % run to run).
- Negative: a 64-row bitmap memo (skip the hash for many bitmaps) averaged 0.999 over two runs,
  drag_frame -7 %; not committed. First version scanned 2 KB per call and lost up to 33 %: a table
  touched on every call costs refills, not instructions.
- Paging during twist (PERFLOG, `%TEMP%\perflog_vc.csv`): free memory 0, 2.2 MB locked (mostly
  Windows; ours is two 64 KB strips, the feed queue, code), steady ~16 page-ins/s with the cache cap,
  start-up 65 -> 55/s with it. SWAPCOMP serves ~60 % of swap reads compressed. The 16 bpp desktop
  frame buffer is pageable already (`fb16.c:1172`). The owner saw the CF light at each stall.
- MTM trial (`docs/mtm_bed_setup_2026_11_12.md`): runs on our 8 bpp DirectDraw with its own software
  renderer (`useDirect3D=0`); the 24 MB and 256-colour messages are the game's, non-fatal at 8 bpp.
  No installer needed on the card. Demo start unattended: not found yet (`showDemoFlag`).
- Owner: the driver should give each program the depth it asks for (DirectDraw SetDisplayMode).
- Device-bitmap reading: ATI v2.3 kept bitmaps on the card (`docs/mach8_device_bitmaps_design.md`);
  ours already does most of it; GE_OFFSET/GE_PITCH (unused) goes to the glyph cache and textures.

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
