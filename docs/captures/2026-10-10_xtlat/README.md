# XT-CF wait times on the 5160, 2026-10-10

`tools/xtlat/XTLAT.ASM`, read-only, run from DOS on the boot CF (Transcend, XT-CF rev 3, base 300h,
stride 2). 64 READ SECTORS of 8 sectors, LBAs 1000 + n x 8191. Polls of alternate status (~5.8 us each)
until DRQ.

| wait | result |
|---|---|
| command to first sector | 232-313 us, median 238 (40-54 polls) |
| between sectors | ready on the first poll, all 448 |
| after the last sector | BSY clear on the first poll |

For the miniport (#21 follow-up): today's backoff spends 32 back-to-back polls (178 us) then paces at
~57 us, so a read costs ~33 polls and notices ready up to ~57 us late. Sized from this: wait ~220 us
silently (register-only loop, no bus cycle), then poll every few us - 1-3 polls per command. Between
sectors no wait is needed. Writes (flash commit) are not measured yet.

Write-path regression, bed (`vm_xtide_mpd`, `86box_xtcf` build, shipped XTIDEMP.MPD), 2026-10-10: WRTEST
copied 129 files (5.9 MB) under Windows 95 and shut down cleanly; every copy byte-identical to its source,
checked on the host.
