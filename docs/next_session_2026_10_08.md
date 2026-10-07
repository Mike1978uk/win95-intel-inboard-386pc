# Next session - Mach8 model: TS1 from command 142, then the PR

Supersedes `docs/next_session_2026_10_07b.md` for order. Results and rules: `docs/captures/2026-10-07_m8txt/README.md`.

## Where it stands (2026-10-07)

- Owner's direction: understand the whole card, not only what drivers use; TEST.COM fully clean before the
  86Box PR; then ask 86Box#6695 to retest on that version. Write-up of undocumented Mach8 behaviour planned
  (Michal Necasek, OS/2 Museum, is the obvious reader; his 8514/A article is in our sources).
- Fork branch `inboard-ext-256k-diag` (86box_3c509b), today: `02c3b70cb` (monochrome host data byte order -
  fixed our own Windows 3.1 8514.DRV text bug from `a0bad65cc`), `96859258f`, `ecea8636e`, `0e9796012`
  (monochrome host-data rules from M8LINE), `8283ed2bc` (source colour compare 92EEh/EAEEh). Diagnostics
  `cdfb0657d` (MACH8_SEQ logger). Nothing pushed.
- Card and bed agree on: M8LINE, M8MONO, M8TXT, M8SCMP, M8ROW5, and M8SEQ5 to checkpoint 142. TEST.COM: Register,
  FIFO, RAMDAC, Video RAM pass; TS1 14216 27DB 0016 and TS2 14204 0602 0004 still fail.
- 86Box#6695 does not reproduce on master or ours (Windows/386 2.11 bed `vm_6695_w2`). No comment posted:
  owner's order is build, PR, then ask them to retest. Draft kept in the 10-07 conversation; rewrite then.

## Next, in order

1. TS1 command 142: colour-pattern blit (PATT_DATA 1111..8888 via 8EEEh, DP_CONFIG E211h/EA11h, then 2251h).
   Make an M8ROW variant for commands 142-150 that clears 92EEh/EAEEh each checkpoint (as M8ROW5), area near
   (74h,56h); run on the 5160 over COMrade, then in `vm_6695`; fix; regress.
2. Continue down M8SEQ5 to 172, then bisect TS2 the same way. Goal: TEST.COM all stages pass.
3. Regression after every fix: `vm_6695`, `C:\M8SEQ\REGR.BAT` (all probes); compare with the previous build's
   results; only the intended probe may change. Check Windows 3.1 text (`WIN`, the Sound Blaster dialog).
4. Then: clean upstream branch (drop DIAGNOSTIC commits), regress Mach32 and plain 8514/A, TC1995 first, PR.
5. Then the driver (handoff 07b: velocity9x vs NOT_FRAMEBUFFER).

## Beds and tools

- `vm_6695`: Win 3.11 + Microsoft 8514.DRV (ATI's SYSTEM.INI kept as `WINDOWS\SYSTEM.ATI`), boots to DOS,
  probes in `C:\M8SEQ` with TEST.COM. `vm_6695_master`: same on upstream (`86box_master`). All four 6695 beds
  now have the FlexView EEPROM.
- `tools/m8seq/`: M8TXT, M8MONO, M8LINE, M8SCMP, M8ROW4/5, M8SEQ5; `m8txt_decode.py` (read-back is low byte
  first), `m8seq_diff.py`, `REGR.BAT` (DOS batch lines must stay under 127 characters).
- COMrade: file_write and hash sometimes time out at 8 s but the copy lands; verify by reading the file back.
  `run_command` cannot use `&&`.
- Window capture of a bed for checking the screen: a PrintWindow script (worked even with the bed behind other
  windows); not yet in `tools/`.

## Rules

- Ask before every VM launch; owner judges screens. Alert, don't fix, for committed work. Re-read comments in
  generated files before committing.
