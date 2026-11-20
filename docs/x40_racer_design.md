# X40: our own racer - design

Worklist X40 (owner, 2026-11-20): "something playable in DirectDraw and Direct3D - we did all this work
to add the support". A short time-trial racer in the flat-shaded style of the early-1990s consoles
(Super FX era) and Amiga 3D games. **Our own design throughout: no names, tracks, cars, graphics or
music taken from any existing game.** Working title: *XT Rally*.

## The game

- One closed circuit, three laps against the clock. Best lap kept; a ghost car replays it.
- Track: straights, bends, one hill, a crest, one banked turn, trackside markers, a few buildings,
  trees as two crossed flat shapes, a start gantry. Night or dusk sky as the M8SHOW sky band.
- Driving: accelerate, brake, steer. Off the tarmac the car slows and rumbles. No damage, no
  collisions with scenery (the car is held to the track's surface). An AI car is a later extra.
- Screens: title, race with HUD (lap, time, best, speed), results. Esc quits at any time.
- Keys: the 5160's 83-key keyboard, so the keypad arrows (8/2/4/6) and Space; letters as a fallback.

## How it looks and what it costs

- 640x480, 8 bpp, two pages flipped (as M8SHOW). Palette: a few ramps (tarmac, grass, sky, car,
  buildings) so shading is an index offset, plus the HUD colours.
- **Budget: at most 250 polygons a frame, target 25 fps, floor 20 fps on the card.** M8SHOW gives
  the yardstick: 30 fps with floor, solids, shadows, spot and glass, and the floor fill (~13.5 ms)
  is the biggest cost - so the ground here is one fill below the horizon, not a tiled floor.
- Drawn back to front with no depth buffer: the track is a list of segments, drawn from the
  farthest visible one towards the car; each scenery object is bucketed into its segment and drawn
  with it. Visible: about 30 segments of 2-4 quads, about 20 objects of up to 8 faces, the car ~20.
- Fixed-point 16.16 maths and the 4096-entry sine table from M8SHOW; no FPU in the frame loop.
- Speed lines, a horizon that tilts with the banking, and flat shading by face angle give the look.

## The track and the physics

- The track is a centre-line of points (x, y, z) with width and bank per point, from a small table;
  segments and kerbs are generated from it at start-up.
- The car lives in track coordinates: distance along the track and offset across it. Steering moves
  the offset; leaving the tarmac adds drag; slope changes speed. This makes collision free and the
  ghost a list of (distance, offset) per frame.

## Sound

- Sound Blaster Pro, 8-bit mono, ~11 kHz, one auto-init DMA buffer in two halves (worklist S2): an
  engine tone whose pitch follows the revs, tyre noise when sliding or off track, beeps for laps and
  the start. Mixed on the 386 into one stream (worklist S4), so the bus carries one stream.
- DOS: the DSP directly. Windows: waveOut first (DirectSound is not done, #58), DirectSound later.

## One core, three front ends

The game (track, physics, camera, projection, sorting, HUD logic, sound mixing) is one C core with
no platform calls. Each front end supplies: begin frame, fill rectangle, draw polygon (flat colour
or Gouraud ramp), text, flip, keys, timer, sound buffer.

1. **DOS - DirectXT (m8dos).** Polygons by the engine's polygon fill, sky kept in each page, the
   ground one fill. Built first: quickest to change, measured on the card.
2. **Windows - DirectDraw.** Exclusive full screen 640x480x8, a flipping chain; polygons broken into
   spans drawn as colour-fill Blts, so every pixel is drawn by the engine through our DirectDraw
   layer and nothing is Locked. Exercises the HAL the way MTM did not.
3. **Windows - Direct3D.** The same scene as flat and Gouraud triangles through our Direct3D layer
   (the M8D3D test programs show the set-up), no Z buffer, no textures, back-to-front order.

The same frame counter and write counts on all three give the comparison X36 asked for: what
DirectXT gains over Windows' own path, on the same game.

## Steps, each with its check

1. Core + DOS front end: drive round the track, no sound. Card fps and writes a frame.
2. Sound on DOS. Check: no drop in fps beyond what the DMA costs, measured.
3. DirectDraw front end. Card fps against step 1.
4. Direct3D front end. Card fps against steps 1 and 3.
5. Title, results, ghost, packaging as a playable demo (`dist/`), with a short readme.

## Risks to settle early

- **DirectDraw full screen from the 16 bpp desktop**: the 8 bpp mode change has blue-screened
  before (worklist X19). Step 3 starts by proving a plain flip at 8 bpp is stable from this desktop.
- **5 MB RAM**: keep the game small (tables, no bitmaps beyond the HUD font); measure paging.
- **The keyboard**: XT scancodes, no separate arrow keys; read them the way M8SHOW does.
- Sound DMA must stay below 1 MB (channel 1 on this machine, technique 62).
