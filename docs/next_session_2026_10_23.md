# Next session - from 2026-10-22/23

Supersedes `docs/next_session_2026_10_22.md` (read it for the day's detail). Card log and one row per run:
`docs/captures/2026-10-22_card/`.

## State

- Card: `M8NW.DRV` selected = master (all of the below). `CONFIG.SYS` `menudefault=LEAN,5` for driver work
  (backup `C:\CONFIG.F22`; FULL stalls at `Initing sd120ppd.mpd`, #46). Set back to FULL when it ends.
- `tools\m8_card_cycle.ps1` restarts through Start > Shut Down > Restart (the agent's own reboot ended in a
  fatal exception 0D twice), waits at most 4 minutes, logs each restart to `%TEMP%\m8_card_restarts.log`
  (3 of 3 ok so far), waits for `M8IDLE`, benches once. ATI's reference runs are LEAN too: like for like.
- Bench spread between boots is 2-12% per test: judge changes under ~2% by the bus operations they remove
  and by no test getting worse, not by the headline number.

## Where we are: 1.274x ATI overall, pixels = ATI in all 35

| Behind ATI | now | lever |
|---|---|---|
| stretch_dib / stretchblt | 0.46 / 0.41 | trace ATIM8 `seg14`; the write is the cost |
| dib_8bpp, mono_bmp | 0.60-0.67 | measure writes and CPU per call first |
| floodfill, polygon, ellipse | 0.66-0.76 | per-call fixed cost |
| setpixel, getpixel, rectangle, fill_small, blit_small, lines_hv | 0.8-0.9 | **per-call path**: setpixel's GDI cost is 0.39 ms (no-draw build); our draw adds ~0.20 ms, ATI's ~0.09 ms. Engine work is the same (51 us a pixel, M8OPTS) |
| text_varied | 0.83 | seen-once string cache done; next the replay cache's miss recording (CPU), then a glyph cache (XFree86 `mach8fcach.c`) for longer strings |
| cursor_move | 0.85-0.97 | keyed copy done; boot-to-boot spread ~12% |

## Next, in order

1. **Per-call path for small operations** (benefits most of the table): the fill path rewrites DP_CONFIG,
   PIX_CNTL and FRGD_MIX every call (never cached, because a write to one changes what the others hold) and
   waits for an empty FIFO. Cache the *tuple* written by one routine, invalidated by any other write to the
   group; skip the FIFO-empty wait when nothing in the group is rewritten. Measure with the no-draw method.
2. Carry-overs to check after every win (owner): does it apply elsewhere on the card? Pending: the replay
   cache's seen-once rule; trailing idle waits in other routines that never send host data.
3. Stretch: ATIM8 `seg14`. 4. DIB and mono uploads. 5. Then DirectDraw, more colours, 3D
   (`docs/mach8_unused_silicon_audit.md`, `docs/atim8_port_census.md`).

## Done 10-22 evening (all card, pixels = ATI)

- ScanLR and GetPixel by SCAN_TO_X reads (ATIM8 seg07); floodfill 2x, getpixel 1.37x. Self-test matched GetPixel.
- Repeated rows of a fallback write-back become engine copies (stretch 1.5x).
- Cursor: masks off-screen, then one keyed copy using the unused source compare (92EEh 28h = source equal to
  key; reset to 0 after, as an engine reset does not clear it). 488 -> 879-1014 moves/s. `M8CURRD` reads the
  cursor past the driver.
- Strings cached on their second sighting: text_varied 1.26x.
- No idle wait after polylines and plane blits: text 1.10-1.20x, lines 1.21x (the 10-21 test had bundled
  this with host transfers, which hid it).
- Negative: LINEDRAW_OPT bit 3 / 070Ch; 4215h pixel sets; free-slot FIFO waits; paired cursor copy; -ox.
- Port census of every ATIM8 constant: `docs/atim8_port_census.md`.

## People

- Andrew (#50, 10-22): CH375/CH376 ISA USB, polling cost, WDM out. Reply drafted by Claude in the 10-22 session,
  owner to post. Leads for him: https://github.com/FreddyVRetro/CH375-USB-DRIVER (CH375 "modified for speed
  XT/286"), https://github.com/ronnyroy111/inboard386. BlueSCSI plan: SCSI-side experiment, multi-LUN behind
  one ID first (one HDD image works, two LUNs on one ID do not both appear yet), then network, USB.
