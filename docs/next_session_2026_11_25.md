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

## State

- **Card:** boot 522, `SBDS.VXD` = trace build of 11-24 midday (10,796 B, with the diagnostic probes).
  First build kept as `C:\WINDOWS\SYSTEM\SBDS.V1`. `C:\DSTEST.EXE` present. Revert: `SYSTEM.SBD` as before.
- **Bed** `vm_5160_dbg`: release build (6,612 B), stock DSOUND.DLL (copy `DSOUND.STK`), 387 fitted.
  `C:\DSTEST.EXE` there too (`tools/dstest/`: `once`, `onceq`, `loop`, `mono`, `mmtimer`).
- `drivers/sbds/build.ps1 -Trace` builds the call-trace variant; plain build is the release.

## Next

1. Owner: dxdiag Test DirectSound on the card.
2. Card: install the release build, then `M8DDB modexd/modexdf/modexdi` twice each against
   `captures/2026-11-23_t5_split/` (target: `modexd` near PlaySound's 0.94, `modexdi` near silent).
3. Doom 95 sound on the card (the app this was for), and stereo channel order by ear (design risk 3).
4. Caps oddity: M8DDB reads DSCAPS flags 0 where DSTEST reads 7. Compare the two programs' DSCAPS sizes.
