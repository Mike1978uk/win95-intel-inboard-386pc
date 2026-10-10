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

XTWLAT safety run, same bed, plain DOS (F8, command prompt only), 2026-10-10 (`XTWLAT_bed.BIN`): 32 of 32
commands written and read back identical. Image against a snapshot taken just before: none of the 256
rewritten sectors changed; the 7 that did are XTWLAT.BIN (root entry, both FATs, data) and a timestamp
`CPUSET.BAT` touches at boot. The bed's timings are the model's, not the card's; the model rejects FLUSH
CACHE, which exercised the probe's declined path. Ready for the card.

## Writes on the 5160, 2026-10-10 (`XTWLAT_5160.BIN`)

Same card, plain DOS (F8), 32 rewrites of 8 sectors with their own data, all read back identical. ~5.8 us
per poll:

| wait | result |
|---|---|
| command to first DRQ | 0.8-3.3 ms, median 3.0 ms (139-569 polls) - 13x a read |
| between sectors | ready at once, except ~1.1 ms (185-195 polls) at a **32-sector (16 KB) boundary**: commands 9-15 cross one at sectors 1-7, and each stalls exactly there |
| commit, BSY after the last sector | **~110 ms** (18,865-19,252 polls) in 30 of 32; 0.8 ms in 2 (LBAs 1000, 17382) |
| FLUSH CACHE | accepted, 23-29 us - nothing left to flush once BSY clears |

The commit is the card's flash management (16 KB units; an erase on most rewrites, not on the two fast
ones). A synchronous driver holds the CPU ~113 ms per write command and, polling, fills that time with
status reads. Not separated: whether rewriting identical data or this card's free-block state sets the
110 ms; a write to free space may differ.
