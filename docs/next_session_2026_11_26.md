# Next session: SBDS stage 5, sound and display cooperating (written 2026-11-25)

Started from `next_session_2026_11_25.md`. Plan `sbds_mixer_plan.md`, stage design
`sbds_stage3_design_2026_11_25.md`, worklist S6. Owner, 11-25: bed first until sound and graphics cooperate,
then the card - no card trip before stage 5.

## Done 11-25 (all on the bed `vm_5160_dbg`, 387 fitted)

- Issues: #10, #55, #56 closed; #58, #49, #48 status posted; README #60 row; video carry-over in
  `video_playback_plan_2026_11_12.md` (feeder applies to Cinepak frames; AVI audio may be waveOut - check).
- Census (`captures/2026-11-25_sbds_census_bed/`): Doom 95 sets an 11 kHz 8-bit stereo primary; holds one
  buffer per sound (43), replays it, no duplicates; pans; changes pitch per play.
- SBDS is now the mixer (commits 4766456, 804d588, 248c971, 55d37f3):
  - 64 channels over DSOUND's memory (shared arena, safe at interrupt time), pages locked per play;
  - 16 voices with 256-word volume/pan tables, 16.16 pitch step, no FPU;
  - primary is ordinary memory; a 20 ms global time-out mixes primary + channels into the DMA ring;
  - DMA stops after 300 ms of silence (the census's 49 min of silent DMA), wakes on the next sound;
  - feeder: ring written a dword per store; a lone mono sound mixes straight to the ring;
  - lead adapts: +20 ms per underrun up to 200, back to 60 after 250 calm ticks;
  - `HKLM\Software\SBDS "Stats"` written at each engine stop; `sbds_trace.py` prints it.
  - Mono per moment DROPPED (owner): ~1 % of the bus, clicks; silence stop saves more.
- By ear (owner): DSTEST and Doom 95 effects right, no pops or delay. Feeder run: 25 underruns, 21 sleeps.

## Adaptive-lead run (done, `captures/2026-11-25_sbds_lead_bed/`)

Underruns 7 (was 25), lead grew to 180 ms and stayed there - a new sound can start ~180 ms late. Stage 5
should keep ticks on time so the lead stays low. Also: on Play, add the new sound into the ring bytes
already written ahead of the 8237 (clipped add), so its start does not wait for the lead.

Bed how-to: `tools\bed_launch.ps1 -Exe 86box_auto\build_gdb\src\86Box.exe -VmPath vm_5160_dbg -AllowMouse` - no
`-Automation`: it holds the VM at t=0, and the owner runs the test.
Put files with `tools/fatcp.py`. Pull exports with
`tools/fatls.py --get` and check the md5 is new. Kill the bed's 86Box only after the owner says it is closed.

## Stage 5 built, not yet working (`captures/2026-11-25_sbds_stage5_bed/`)

Built: SBDS `SBDS_IoCtl` (IOCTL 1 = hot data address, page locked) and a tick that defers 5 ms while
`H_BUSY` is set and 40 ms+ is queued; M8DD32 `m8dd_sbds_hand()` in `DriverInit` opens `\.\SBDS` and
passes `m8dd->hot`. Bed run: no crash (after moving the handler out of the init segment - it BSODed there),
underruns 5, lead 140 ms, **but 0 ticks deferred and 0 busy**: SBDS most likely never got the address.
First job next session: count IoCtl calls and hot-set in the stats, trace CreateFile's result in M8DD32,
fix the hand-over, re-run. Then the "start a sound on Play" change to bring the lead down.

Graphics retro candidates from the sound work (owner asked, 11-25): (1) M8FEED's fixed 1 ms / 512-write
budget could adapt like the lead, fewer bigger bites when the queue is long - each tick empties the cache;
(2) end-of-run stats in the registry for the display path; (3) skip unchanged DirectDraw frames (check #54's
hashing covers full screen); (4) handshake the other way, M8FEED avoiding SBDS's tick; (5) draw small changes
into the frame already sent, as "start on Play" adds into the queued ring.

## Stage 5 design notes

Found 11-25: the busy flag already exists. Both display halves set `H_BUSY` (byte 12 of the display
driver's hot data, `M8_HOT` in `m8.h`) around engine work: `blt.asm` (16-bit, lines ~170-238, ~335) and
`m8dd32.c` (`hot[H_BUSY] = 1/0`, many sites). M8FEED already reads that hot data: M8DD32 passes its linear
address with `IOCTL_INIT` (`drivers/m8feed/src/M8FEED.ASM`), and M8FEED's `Q_STATE` = 1 while its 1 ms tick
feeds the FIFO.

Design to write first (short doc), then build:
1. SBDS learns the hot data's address: M8DD32 opens `\\.\SBDS.VXD` (SBDS needs a W32_DEVICEIOCONTROL handler)
   and passes it, as it does for M8FEED. Desktop-only (16-bit, no M8DD32) case: decide whether it matters.
2. SBDS tick: if `H_BUSY` (or M8FEED feeding) and the ring still holds more than ~2 ticks, re-arm 5 ms
   without mixing; if idle, mix deeper (lead + 40 ms) to bank time. Never let the margin drop below one tick.
3. Optional: M8DD32 calls SBDS "mix now" at Flip, so mixing lands in the frame gap, not mid-blit.
4. Why it pays: the 486 cache is emptied by port I/O (M8FEED's note) and stores are write-through
   (`project-write-through-stores`); a tick landing mid-blit costs the blit its cache. Counters to add:
   ticks deferred, ticks mixed while busy.

Then (owner's order): the card - release build, Dr. Watson Doom run, M8DDB bench against the 11-24
baseline (`captures/2026-11-24_sbds_dxdiag/M8DDB_card_sbds1124.TXT`), stage 2 store cost. Card files:
`SBDS.REL` older release; revert `copy C:\WINDOWS\SYSTEM.SBD C:\WINDOWS\SYSTEM.INI`.

## Parked

- Cap on simultaneous plays of one sound (owner's idea; Doom 95 does not overlap, so low value now).
- FM music (OPL) stalls on scene changes: Doom's own timing, not SBDS; its I/O cost is a separate item.
- dxdiag Music-tab crash (card only).
