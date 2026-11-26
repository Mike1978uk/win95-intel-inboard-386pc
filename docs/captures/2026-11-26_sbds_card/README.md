# SBDS release on the card (2026-11-26)

`SBDS_rel1126.VXD` (14,916 B, md5 `7a68e0bd`, built from `bb25856`, no trace) replaced the 11-24 trace build
(`SBDS_card_before.VXD`, 15,832 B; on the card as `C:\WINDOWS\SYSTEM\SBDS.T24`). M8DD32 unchanged
(`22F9F173`, hands SBDS no address: busy counts 0 by design). Boot 528.

- Doom 95 with sound, owner, two runs (640x480: too slow to play; then 320x200). `SBDS_card.REG` stats:
  ticks 3,770, **underruns 2, lead 60 ms now, 100 max** (bed: 6 and 140 ms), sleeps 15 / wakes 13,
  late starts 5. By ear: good.
- M8DDB once each after `M8IDLE 10 10 600` (0 % CPU), `M8DDB_card_sbds1126.TXT` lines 1801-1920, against
  `../2026-11-24_sbds_dxdiag/M8DDB_card_sbds1124.TXT`: all 21 hashes equal; flips 30.37 -> 30.22 (modexd),
  33.91 -> 33.97 (modexdf), 39.31 -> 39.31 (modexdi); the rest within +-6 % except modexd mx_frame +35 %
  and mx_fill -10 % (single runs; 11-23 repeats varied as much). SBDS now mixes everything at no frame cost.
- `DOS_AGENT_before.INI`: the DOS agent's `C:\V9XREMOT\AGENT.INI`; `allowed_client` already blank.
