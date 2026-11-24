# Next session: T5 DirectSound, after the FPU finding (written 2026-11-24)

Started from `next_session_2026_11_24_t5.md`. Design and the finding: `sbds_design_2026_11_23.md`
("DirectX 7 mixes on its own only with a 486-class FPU"). Evidence: `captures/2026-11-24_sbds_dxdiag/`.

## Result

The 11-23 dxdiag failure was the bed: no bed had an FPU, so VMCPD reported coprocessor type 0 and DirectX 7
used its app-driven mixer, which leaves a sound that is played and left alone unmixed. The card has a 387
(type 3) and runs DSOUND.VXD's timer mixer with the stock DirectX files.

- Bed with `fpu_type = 387`, SBDS diagnostic build: dxdiag **all four tests successful**, owner heard all four.
- Card: `DSTEST onceq` one-shot ran through the timer mixer and stopped at 1,144 ms (trace build, boot 522).
- Bed, release build (6,612 B, boot 305): `DSTEST once` secondary cursor advances ~22 B/ms and ends at
  ~1,100 ms, `status 0`.
- Seen once: restarting the bed onto the release build stopped at a blue screen, *fatal exception 0D at
  0337:0000235x* (16-bit fault, last digit cut off). Hard reset, then it booted clean. Ten earlier reboots
  today were clean. Not attributed; it fits the open Msgsrv32 16-bit sound fault
  (`project-swapcomp-hang` memory). Note the full address if it recurs.
- 53 bed configs gained `fpu_type = 387` (backups `*.prefpu`); `bed_launch.ps1` now refuses a BL3 Inboard bed
  without it (`-NoFpu` for a deliberate test). The `i386dx` beds are other people's machines, unchanged.

- **Card baseline, SBDS trace build, boot 524 (11-24 evening), Mode X ops/s box / same:** no sound 14.95 / 14.36
  (11-23); sound open idle: emulation 12.49-12.59 / 12.40-12.69, **SBDS 13.31 / 13.70**; playing 11 kHz
  (`modexd`): emulation 11.8 / 8.9-11.8, SBDS 10.55 / 10.53; playing 22 kHz (`modexdf`): emulation 12.5-12.9 /
  12.7, SBDS 11.78 / 11.99. Idle cost about halved; playing is no cheaper yet (DX7's own mixer does the work) -
  this is the "before" for the mixer plan. **Caveat:** the M8IDLE `idle:` line was not captured (fix the capture
  before the stage-6 bench); 11-23 repeats vary widely. Files: `captures/2026-11-24_sbds_dxdiag/
  M8DDB_card_sbds1124.TXT`, trace `SBDSM_card_m8ddb.REG` (not yet decoded).

## State

- **Card:** `SBDS.VXD` = trace build with a 512-entry ring (15,832 B), for the census; loads from the restart after
  boot 523 - loaded at boot 524 (empty trace confirmed). Kept beside it: `SBDS.REL` (release, 6,612 B), `SBDS.DG` (diagnostic probes), `SBDS.V1` (first build).
  `C:\DSTEST.EXE` present. Revert to no SBDS: `copy C:\WINDOWS\SYSTEM.SBD C:\WINDOWS\SYSTEM.INI`.
- **Card dxdiag Test DirectSound: passes** (owner, 11-24 afternoon).
- **Bed** `vm_5160_dbg`: release build, stock DSOUND.DLL (copy `DSOUND.STK`), 387 fitted, 800x600x16 on
  `m8pcl.drv` (same code as the card's `m8cw.drv`; only the version stamp differs), `DXTWALL.BMP` wallpaper.
  `C:\DSTEST.EXE` there too (`tools/dstest/`: `once`, `onceq`, `loop`, `mono`, `mmtimer`).
- `drivers/sbds/build.ps1 -Trace` builds the call-trace variant; plain build is the release.

## Parked, not T5: dxdiag Music tab crash (card only)

Worklist, Parked. Dragging the Music Ports list slider: "DXDIAG caused an invalid page fault in module USER.EXE
at 0004:00004e8e", `test byte ptr [ebx+33h],10h` (bytes `67 f6 43 33 10`), EBX=EDX=ESI=EDI=000630ac DS=07c7: USER
was asked whether a window that is not there is visible. Rows drew doubled at the bottom first. Intermittent
(worked once, broke on repeat), also with the SBDS release build. The bed, with the same driver code, mode and
wallpaper, does not reproduce it. Photos: `XT_project\photos\dxdiag_music1-4.jpg`. Owner: may come out in the
wash with other titles.

## Next - the aim: SBDS mixes (`docs/sbds_mixer_plan.md`, worklist S6)

Owner, 11-24: "exactly the design we aim for". Stereo is never given up; mono only when nothing playing needs it.
Benches: prove the card idle, then run once; build levers before benching.

1. **Census + baseline** (card is set up for it): owner starts Dr. Watson, plays Doom 95 with sound (shooting,
   doors), quits; pull `HKLM\Software\SBDS` with `regedit /e` and decode - does it set the primary format, which
   buffer formats, any pan. Doom 95 also tests the Msgsrv32 sound-loop hang with sound on SBDS. The M8DDB
   baseline is done (above); decode its trace `SBDSM_card_m8ddb.REG` for how M8DDB drives DirectSound.
2. Measure the store path (bus crossings per mixed byte).
3. Channels + mono/stereo per moment, `ForceStereo` override.
4. Feeder + own timing. 5. Display handshake. 6. One bench (idle proven).
7. Owner, 11-24: once the mixer stages are in, try a higher resolution than Mode X and see whether the bus time
   sound gives back makes it playable.

Smaller: stereo channel order by ear (design risk 3); caps oddity (M8DDB reads DSCAPS flags 0, DSTEST 7 -
compare the DSCAPS sizes the two pass).
