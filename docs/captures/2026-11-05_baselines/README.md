# Baselines for #59 items 1 and 2, 2026-11-05

Real 5160 (192.168.68.62, agent 0.12.0, Windows 95 4.00.950 a), current driver, nothing changed.
The owner rebooted the card first (it had locked with the startup tone looping). Whether a sound
was heard is not known to the session that took these. Uptime at the first `info`: 158 s
(BootCounter 208). Display 800x600; `info` reports BitsPerPixel 0.

Driver: `C:\WINDOWS\SYSTEM\IOSUBSYS\XTIDEMP.MPD`, 11264 bytes, md5 a67fefc44d5368c59a9736f93eea13af
(fetched and hashed locally).

Idle evidence: the agent has no process list and the screenshot helper failed (Win32 1460), so
idleness is not proven. Nothing was run on the card except the agent, the two tools and the
file transfers; uptime was under 7 minutes at the first run.

Tools: `XTSECT.EXE` (10143 bytes, sha256 cb06c717...) from `claude/xtcf-tight-taskfile`,
`IDLECOST.EXE` (9405 bytes, sha256 2fb0f8bd...) from `claude/driver-optimisation-review`;
both verified against their SHA256SUMS and copied to `C:\`.

Commands (each three times, one at a time, v9xctl `exec`, no other traffic during a run):

```
C:\XTSECT.EXE base
C:\IDLECOST.EXE base
```

Logs: `XTSECT.TXT`, `IDLECOST.TXT` (as fetched, three runs each).

## XTSECT (drive C, 500 reads a pass, us per read)

No line was marked `(cached?)`; all 18 pass lines are used.

| run | 1 sector (3 passes) | 8 sectors (3 passes) |
|---|---|---|
| 1 | 13727 11975 12161 | 22129 21931 22273 |
| 2 | 13063 11959 11813 | 22404 21941 22498 |
| 3 | 13617 12194 11663 | 21960 22055 22341 |

- One sector: mean 12464 us/read; pass range 11663-13727; run means 12621, 12278, 12491.
- Eight sectors: mean 22170 us/read (2771 us/sector); pass range 21931-22498.
- Fixed cost per command = one-sector - eight-sector/8: 9692 us; run values 9857, 9493, 9727.
- Pass 1 of each run is the slowest one-sector pass in 2 of 3 runs.

## IDLECOST (passes/s mean, 5 s per size)

| KB | run 1 | run 2 | run 3 | mean | spread |
|---|---|---|---|---|---|
| 1 | 27683 | 25955 | 27072 | 26903 | 1728 (6.4%) |
| 4 | 9824 | 9683 | 9721 | 9743 | 141 (1.4%) |
| 8 | 4982 | 5097 | 5094 | 5058 | 115 (2.3%) |
| 12 | 3446 | 3446 | 3372 | 3421 | 74 (2.2%) |

Per-second min/max within a run: 1 KB 19312-32112 (widest); 12 KB 3232-3584.

## Correction: how to read XTSECT (main session, same day)

Do not use #59's "fixed cost" (one-sector time minus one-eighth of the eight-sector time): it is 7/8 of the
intercept, and the intercept is not a command cost either. Compare builds by **min**, not mean.

| Reads | min, us (9 passes) | mean, us |
|---|---|---|
| 1 sector | 9,964-9,989 | 11,663-13,727 |
| 8 sectors | 10,778-10,792 | 21,931-22,498 |

- The min repeats within 25 us; the mean moves by 2 ms, so a 15-40 us change shows only in the min.
- One sector costs almost what eight do. 10 ms is about eight sectors at the 2.6 us/B sequential rate, so a
  one-sector INT 25h read may fetch 4 KB through the cache path. Hypothesis, unproven: a 10 ms timing floor would
  look the same. Check the sector count the miniport receives in the bed, or time 2, 4 and 16 sectors.
- This path is INT 25h through VWIN32. The pager's own path to IOS is not measured here.

## Status: these runs are not a valid baseline

Started at ~158 s of uptime with no M8IDLE proof, against the card bench rule. Pass 1 is slower in every run (13.1-13.7 ms against 11.7-12.2), which settling alone does not explain since it repeats across runs. Repeat XTSECT and IDLECOST after `M8IDLE 10 10 600`, each three times, and record the `idle:` line, before comparing any build.
