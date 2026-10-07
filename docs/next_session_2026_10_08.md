# Next session - Mach8 model: TS1 from command 142, then the PR

Supersedes `docs/next_session_2026_10_07b.md` for order. Results and rules: `docs/captures/2026-10-07_m8txt/README.md`.

## Where it stands (2026-10-07)

- Owner's direction: understand the whole card, not only what drivers use; TEST.COM fully clean before the
  86Box PR; then ask 86Box#6695 to retest on that version. Write-up of undocumented Mach8 behaviour planned
  (Michal Necasek, OS/2 Museum, is the obvious reader; his 8514/A article is in our sources).
- Fork branch `inboard-ext-256k-diag` (86box_3c509b), today: `02c3b70cb` (monochrome host data byte order -
  fixed our own Windows 3.1 8514.DRV text bug from `a0bad65cc`), `96859258f`, `ecea8636e`, `0e9796012`
  (monochrome host-data rules from M8LINE), `8283ed2bc` (source colour compare 92EEh/EAEEh). Diagnostics
  `cdfb0657d` (MACH8_SEQ logger). Main repo and fork pushed to `8283ed2bc` mid-afternoon; later commits local.
- Card and bed agree on: M8LINE, M8MONO, M8TXT, M8SCMP, M8ROW5, M8ROW6, and TS1 everywhere except source 4 (commands 145-148). TEST.COM: Register,
  FIFO, RAMDAC, Video RAM pass; TS1 14216 27DB 0016 and TS2 14204 0602 0004 still fail.
- 86Box#6695 does not reproduce on master or ours (Windows/386 2.11 bed `vm_6695_w2`). No comment posted:
  owner's order is build, PR, then ask them to retest. Draft kept in the 10-07 conversation; rewrite then.

## Next, in order

1. **TS1 command 145: foreground source 4** - the last TS1 feature. Card data so far in
   `docs/captures/2026-10-07_m8txt/README.md` (M8SRC4, M8SRC4B, M8SRC4C). M8SRC4C: a 4:1 reduction, each
   destination pixel = 11h x set pixels in source pixels 4n-4n+3. Explain the constant 44h at 31/47/63, then
   model it in the BitBLT.
2. Re-read M8ROW7's third area (X >= 1024) with a wider scissor; and model reads outside the scissor as FFh.
3. Bisect TS2 the same way (M8SEQ5-style replay of its table). Goal: TEST.COM all stages pass.
4. Regression after every fix: `vm_6695` boots straight into `C:\M8SEQ\REGR.BAT` (all probes); compare each
   probe with the previous build's results; only the intended probe may change. Check Windows 3.1 text.
5. Then: clean upstream branch (drop DIAGNOSTIC commits), regress Mach32 and plain 8514/A, TC1995 first, PR.
6. Then the driver (handoff 07b: velocity9x vs NOT_FRAMEBUFFER).

TS1 progress, 2026-10-07: commands 133-144 and 149-172 match the card; only 145-148 (source 4) remain.
Fork commits today after the morning: `554cfeddc` (source 7), `de789efb6` (mono host bits), `4e9ef658d`
(POLY_MODE lines). Main repo and fork pushed to these at the end of 2026-10-07.

## Beds and tools

- `vm_6695`: Win 3.11 + Microsoft 8514.DRV (ATI's SYSTEM.INI kept as `WINDOWS\SYSTEM.ATI`), boots to DOS,
  probes in `C:\M8SEQ` with TEST.COM. `vm_6695_master`: same on upstream (`86box_master`). All four 6695 beds
  now have the FlexView EEPROM.
- `tools/m8seq/`: M8TXT, M8MONO, M8LINE, M8SCMP, M8SRC4/4B, M8ROW4-7, M8SEQ5; `m8txt_decode.py` (read-back is low byte
  first), `m8seq_diff.py`, `REGR.BAT` (DOS batch lines must stay under 127 characters).
- COMrade: file_write and hash sometimes time out at 8 s but the copy lands; verify by reading the file back
  BEFORE running it (a truncated M8ROW6 ran once because the two were done in parallel).
  `run_command` cannot use `&&`.
- Window capture of a bed for checking the screen: a PrintWindow script (worked even with the bed behind other
  windows); not yet in `tools/`.

## Rules

- Ask before every VM launch; owner judges screens. Alert, don't fix, for committed work. Re-read comments in
  generated files before committing.
