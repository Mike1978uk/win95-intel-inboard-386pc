# Video on the 5160: playback, capture and a video call (owner's idea, 2026-11-05)

Plan and arithmetic; nothing here is measured on the card yet. Owner's report: the Creative Video
Blaster WebCam II (parallel port) driver installs, captures and plays back, but playback is slow.

## The budget every option shares

One 8-bit bus carries the file, the camera, the network card and the pixels to the Mach8.

| path | cost | source |
|---|---|---|
| pixels to the Mach8 | 3.00 us a byte | `M8UPRATE`, `mach8_driver_method.md` §4c |
| XT-CF reads | ~2.6 us a byte (385 KB/s sequential) | `magnaram_and_swap_compression_2026_10_01.md` |
| parallel port, one port read a byte (EPP) | ~5.6 us a byte | technique 109 |
| parallel port, nibble mode | ~11 us a byte | two reads a byte |
| memory copy | ~0.2 us a byte (4 KB in 0.83 ms) | `lz4bench_5160_2026-10-02.txt` |

| frame | 8 bpp bytes | to the Mach8 | ceiling from that alone |
|---|---|---|---|
| 320 x 240 | 76,800 | 230 ms | ~4 fps |
| 160 x 120 | 19,200 | 58 ms | ~17 fps |
| 160 x 120 shown at 320 x 240 by the engine | 19,200 | 58 ms + a few register writes | ~17 fps |

The engine enlargement exists: `stretch_dib` sends the source once and the engine spreads columns
then rows (whole factors, unclipped box; 1.15x ATI, `mach8_driver_method.md`). Video for Windows'
"Zoom 2x" calls `StretchDIBits`, so it may already use it.

## Step 1: what the webcam recorded (no build)

The WebCam II's Windows 95 driver is v1.44 (`wc2p9xup.exe`); Creative documented AVI codec support
only for the later USB model's v1.69 driver
(https://support.creative.com/downloads/searchdownloads.aspx?nLanguageLocale=1033&filename=PD&nPage=9).
The parallel model's CDs are archived at https://archive.org/details/VideoBlasterWebcamIICDs.7z
(not opened here: archive.org is outside the cloud session's network). So the format is unknown;
the file says.

`tools/aviinfo/AVIINFO.EXE <file.avi>` (new, read only) prints the codec (FourCC), size, bit depth,
frame rate, audio format, the data rate the file needs from the disk, what one 8 bpp frame costs on
the way to the Mach8, and the video codecs registered in `SYSTEM.INI`. Run it on the webcam's clip.

Likely, until it says otherwise: an uncompressed or YUV capture. 320 x 240 at 16 bits is 153,600
bytes a frame; reading one from the XT-CF takes ~400 ms, so ~2.5 fps however good the display.

## Step 2: re-encode and play (no code)

Re-encode a clip for this machine on the PC, then play it in Media Player at 1x and at Zoom 2x:

```
ffmpeg -i webcam.avi -vf scale=160:120,fps=15 -c:v cinepak -c:a pcm_u8 -ar 11025 -ac 1 cvid160.avi
ffmpeg -i webcam.avi -vf scale=160:120,fps=15 -c:v msvideo1 -c:a pcm_u8 -ar 11025 -ac 1 msvc160.avi
```

Both codecs ship with Windows 95. Cinepak suits a 486: it was designed for that class of machine
and decodes by codebook lookup, with no transform, so at 160 x 120 the limit should be the bus to the
Mach8, not the decoder. Then `AVIINFO` on each and note smoothness. If Zoom 2x is as smooth
as 1x, the engine path is working; if not, the fix is in the Mach8 driver's `StretchDIBits`.

### Also try: TCPMP (The Core Pocket Media Player)

A Pocket PC media player with integer decoders written for slow CPUs without an FPU and for small
screens, and frame dropping built in. Roytam1's "TCPMP 0.72RC1 Mod 6" restores Windows 95/98/ME
support (VOGONS: https://www.vogons.org/viewtopic.php?p=818712, https://www.vogons.org/viewtopic.php?p=818703;
background https://en.wikipedia.org/wiki/The_Core_Pocket_Media_Player). No source found reports it on
a 486; one user had trouble on a 200 MHz machine. It draws through DirectDraw or GDI, both of which our
Mach8 driver serves. Test it with the same 160 x 120 clips.

A Pocket PC is not slower than this machine: a 2002-era one has a 206-400 MHz ARM and a frame buffer
in its own RAM. The 5160's 83 MHz 486 is a few times slower in the CPU, and its display sits behind a
0.33 MB/s bus, against a Pocket PC's memory-speed frame buffer. TCPMP's frugal decoders still fit;
its assumption of a cheap screen does not, so keep the picture small and let the engine enlarge it.

## Step 3: a player that uses the card (code, if step 2 is not enough)

Describe, don't scan (owner, 11-03): a video decoder already knows which parts of a frame changed.
Microsoft Video 1 codes 4 x 4 blocks as skip, one colour, two colours or eight; mapped to the Mach8:

| Video 1 block | on the Mach8 | bytes over the bus |
|---|---|---|
| skip | nothing | 0 |
| one colour | engine rectangle fill | a few register writes, no pixels |
| two colours | mono expansion with foreground and background | 2 bytes of mask plus set-up |
| eight colours | upload | 16 pixels |

Engine 2x enlargement on top. Before writing it: count block types in real clips (a host script over
an ffmpeg `msvideo1` encode) and price each frame at the measured costs.

## A video call (owner's idea)

The 5160's camera to someone else, and their camera in a small window here.

Pipeline on the 5160, all on the one bus: camera in (parallel), compress, network out (3C509B, PIO);
network in, decode, Mach8 out. At 160 x 120 and only changed blocks sent, a few frames a second each
way fits the budget above; full-frame video does not.

Constraints:
- **The SB Pro is half duplex:** it cannot record and play at once. Audio would be push-to-talk, or
  text chat beside the video.
- **Period software:** CU-SeeMe (Cornell, 1990s) was built for exactly this class of machine: 160 x 120
  greyscale, sending only changed 8 x 8 blocks. Microsoft NetMeeting also supported Windows 95. Neither
  checked here for 486 requirements or a peer that still runs; both need a period peer.
- **Our own, with a modern helper:** a PC on the network does the heavy side. It takes any modern
  camera, reduces it to 160 x 120 in 8 bpp (or 16 grey levels), and sends only the 8 x 8 blocks that
  changed, which the 5160 draws with the engine (one-colour blocks as fills). The 5160 sends its own
  camera's changed blocks the other way; the PC can pass them on to a modern chat. Winsock over the
  3C509B carries it. This is the version most likely to give "a small window of the other camera".

Order: steps 1 and 2 first (no code, they set the numbers); then the step 3 player, whose display
side the call reuses; then the call.
