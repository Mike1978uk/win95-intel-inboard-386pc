# Video playback (AVI) on the 5160: plan, 2026-11-12

Plan only. Nothing here is built or run. Related: #58 (DirectX), worklist X1/X8/D5/D6,
`docs/mach8_frame_rate_plan.md` (P5, B2/B4, CT2).

## Goal

Play a small AVI in Windows 95 on the 5160 (486BL3 83.5 MHz, Mach8, XT-CF, SB Pro) at a
steady frame rate with sound. Find which path (Mach8 engine or VGA half) and which frame
size the 8-bit bus allows, before writing a driver for it.

## Measured inputs

- 8-bit port I/O: byte 5.695, word (OUTSW) 3.819, dword 3.183 us/B. Mach8 PIX_TRANS is port I/O.
- Any port access invalidates the L1; refill is ~83 ns/byte of lost working set
  (`docs/captures/io_cache_flush_2026_09_21.md`). Not a per-port constant: the 90.7 us
  figure is for that test's 1 KB set. SW1 and snoop bit 4 do not change it
  (`docs/next_session_2026_09_28.md:150-165`).
- ISA memory writes (A0000) flushing the L1: NOT measured (#49 says they may not).
- VGA-half write cost: 1.96 us/B is an expected baseline in
  `docs/mach8_vga_half_findings_2026_11_06.md`; UNCONFIRMED until M8VGAW runs (P5/X8).
- No memory path between the VGA half and the 8514 half; only the DAC is shared. The Mach8
  has no YUV conversion and no general stretch blit.
- XT-CF sustained read: no measured figure found. `mach8_frame_rate_plan.md:371` quotes
  2.6 us/B with no source. Treat as UNKNOWN; the planned XT-CF drive tests will settle it.
- Decode cost of any codec on this CPU: not measured.

## Budget per frame (send only, no decode, no disk)

| path | 160x120 (19,200 B) | 240x180 (43,200 B) | 320x200 (64,000 B) |
|---|---|---|---|
| Mach8 PIX_TRANS, 3.819 us/B | 73 ms (13.6 fps) | 165 ms (6 fps) | 244 ms (4 fps) |
| VGA half, 1.96 us/B (unconfirmed) | 38 ms (26 fps) | 85 ms (11.8 fps) | 125 ms (8 fps) |

Each row is the ceiling from the send alone. Decode, XT-CF read and the L1 refill after
each flush come on top. A 12 fps target (83 ms) therefore needs a full-frame send under
~40 ms: only 160x120 on either path, or deltas.

## Paths

1. **Mach8 PIX_TRANS, windowed 1:1.** Works in the GDI driver and Win95 players as they
   are, at 8 bpp, with the palette set from the clip. Cost is the table's first row.
   The window can sit in 800x600; no scaling, so frame size = window size.
2. **VGA half, Mode 13h or Mode X.** Cheapest CPU path on the card if 1.96 us/B holds, but
   it is a separate display (one half per mode, output switch, desktop restore). Win95
   reaches it only through X1 (DirectDraw Mode X) or D6 (VFLATD driver); neither exists.
   320x200 is 125 ms: no faster than 8 fps full screen. No hardware stretch, so a
   160x120 clip shown full screen needs the CPU to double pixels (more writes), unless
   the mode itself is small. Mode X 160x120-class modes need CRTC programming; untried.
3. **Smaller frames.** 160x120 is the only size that reaches 12 fps on a full send. 120x90
   or 128x96 are fallbacks. Lower fps (8-10) at 160x120 is more likely than 12.
4. **Delta codecs, send only what changed.** The driver already sends only changed
   regions (joined same-colour patches, bounding-rectangle upload). A codec that marks
   skipped blocks maps onto that: Video 1 (msvideo1) and Cinepak both skip blocks. At 10%
   changed, 160x120 is ~1,900 B = 7 ms over PIX_TRANS, plus per-run setup (not
   quantified here). Cost moves from the bus to the decoder; decode speed decides.
5. **Text-cell video (CT2).** 80x25 on the VGA character generator, 4,000 B a frame, is
   the strongest option in `mach8_frame_rate_plan.md:369-371`. A different product from
   AVI, but it is the lowest-cost path. Not covered further here.

## The two measurements that decide it

1. **Run M8VGAW** (`tools/m8probe/M8VGAW.EXE`, built, unrun; P5/X8). Gives the real
   VGA-half write us/B. Run twice, prove the machine idle first. If it is near 1.96 us/B,
   path 2 is worth X1/D6; if near 3.8, it is not.
2. **A BUSFLUSH variant with ISA memory writes instead of `in al,0x21`.** Same generator
   (`tools/gen_busload_com.py --flush`, `build_flush`): replace the one port read per KB
   with one or four byte or dword writes to A000:0 (VGA half in mode 13h or 12h). Same four
   alternating passes, capstone check. Result: whether memory writes flush the L1. If they
   do not, VGA-half writes cost only bus time and the 83 ns/B refill disappears from path 2.
   Not built.

Order: M8VGAW first (no code), then the BUSFLUSH variant (one script change, one run).

## Test clips (all untested)

Source `in.mp4`. 8-bit palette output cannot be a plain ffmpeg AVI codec for Cinepak and
msvideo1 on this build; check `ffmpeg -encoders | grep -E "cinepak|msvideo1|msrle"` first.

```
ffmpeg -i in.mp4 -vf scale=160:120,fps=12 -c:v cinepak -q:v 4 -c:a pcm_u8 -ar 22050 -ac 1 cp160.avi
ffmpeg -i in.mp4 -vf scale=240:180,fps=12 -c:v cinepak -q:v 4 -c:a pcm_u8 -ar 22050 -ac 1 cp240.avi
ffmpeg -i in.mp4 -vf scale=160:120,fps=12 -c:v msvideo1 -c:a pcm_u8 -ar 22050 -ac 1 mv160.avi
ffmpeg -i in.mp4 -vf scale=240:180,fps=12 -c:v msvideo1 -c:a pcm_u8 -ar 22050 -ac 1 mv240.avi
ffmpeg -i in.mp4 -vf "scale=160:120,fps=12,split[a][b];[a]palettegen=max_colors=256[p];[b][p]paletteuse" -c:v msrle -c:a pcm_u8 -ar 22050 -ac 1 rle160.avi
```

Cinepak and msvideo1 are 16-bit-class codecs that Win95 decodes to the display depth;
at 8 bpp the decoder dithers on the CPU (extra cost, unmeasured). The msrle line is the
8-bit palette variant. SB Pro: 22 kHz mono 8-bit is within its limits; 44 kHz stereo is
not worth testing first. Start with `cp160.avi` and `mv160.avi`, record fps and CPU idle.

## Players and DirectDraw

- **MPLAYER (Media Player) and ActiveMovie (quartz)**: ActiveMovie draws through
  DirectDraw overlay or primary surface when available, else GDI DIB blits. Today it takes
  the GDI path, so path 1 needs no new code. Which player is installed on the card: not
  checked. Win95 gold ships Media Player with Video for Windows; ActiveMovie comes with
  IE or DirectX.
- **DirectDraw (#58, D5)**: the Mach8 HAL's Blt would give a GPU-side copy only for
  surfaces already in card memory; video frames originate in RAM, so the send still goes
  over PIX_TRANS. DirectDraw changes nothing in the table above. Overlay and
  stretch-blit caps would have to be emulated by the CPU.
- **VGA-half DirectDraw (D5b/D6)** is the only route by which a Win95 player reaches the
  1.96 us/B path.

## DOS-side option

A DOS player (for example a Cinepak or FLC player, or our own) writing Mode 13h directly
needs no driver and tests path 2 at once: the same 160x120 or 320x200 clip, FLC or raw,
timed against M8VGAW. Sound by the SB Pro in auto-init DMA, which FastDoom (#47) already
shows is a problem with sound on (it freezes). Sort that first or play without sound.
Result gives the VGA-half fps ceiling without X1/D6.

## Corrections to the outside suggestion

- A 240x180 8-bit frame is 43,200 B: at 3.819 us/B that is ~165 ms over PIX_TRANS, about
  6 fps from the send alone. It does not fit the bus at 12 fps.
- It said the Mach8 both has and lacks a stretch blit. It has none (no general scaling).
- It said VCACHE second-play keeps the L1 hot. The frame send is port I/O and flushes the
  L1 itself; VCACHE only helps the disk read.
- "~90 KB/s" at 12 fps is a bitrate, not a bus figure; decode and send costs are separate
  and unmeasured for these codecs here.

## Next steps

- [ ] Run M8VGAW twice on the card (P5/X8). Owner-driven.
- [ ] BUSFLUSH variant with memory writes; run on the card.
- [ ] Time XT-CF sustained reads at 64 KB and 1 MB.
- [ ] Encode the clips; play `cp160.avi` in MPLAYER, note fps and idle.
- [ ] Decide X1/D6 on the M8VGAW and flush results; record in worklist.

## Issue draft (owner posts)

```
Title: Video playback (AVI) on the 5160: which path and frame size

Goal: play a small AVI with sound in Windows 95 on the 5160. Related to #58 (DirectX).

Send cost alone: a 160x120 8-bit frame is 19,200 B, ~73 ms over the Mach8 PIX_TRANS at 3.819 us/B (13.6 fps ceiling). A 240x180 frame is ~165 ms (6 fps). The VGA half may be about twice as fast (1.96 us/B expected, not measured).

Two measurements decide the path: M8VGAW (built, not run) for the VGA-half write cost, and a BUSFLUSH variant using ISA memory writes to see whether they flush the L1 like port I/O does.

Not known: XT-CF sustained read rate, decode cost of Cinepak or Video 1 on this CPU, which Win95 player the card has.

Plan: docs/video_playback_plan_2026_11_12.md
```
