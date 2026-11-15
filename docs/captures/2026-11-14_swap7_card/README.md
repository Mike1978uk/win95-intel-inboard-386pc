# swapcomp, pages up to 7 sectors stored compressed: card, 2026-11-14

Card 192.168.68.62. One PAGETIME run each (`run.log`); counters from the last PERFLOG row, cumulative
since boot, read from the row's end (each run has its own header in the appended file: see the bed README).

| run | boot | VxD | PAGETIME | raw reads / all swap reads | Errors | ChecksumErrors |
|---|---|---|---|---|---|---|
| `zfn1` | 393 (up 79 min) | `SWAPZFN` `99B7BD5B` | 139.1 s | 3,411 / 13,870 (25 %) | 0 | - |
| `z7c1` | 394 (fresh) | `SWAPZF7C` `BE561A21` | 133.5 s | 270 / 5,493 (4.9 %) | 0 | **0** |
| `z71` | 395 (fresh) | `SWAPZF7` `188AFAA0` | 128.1 s | 282 / 5,860 (4.8 %) | 0 | - |

Data intact on the card (checksum build). Raw reads fall from a quarter of swap reads to under 5 %.
The time gain is not shown: one run each, and the baseline ran on a boot up 79 minutes. Next: PAGETIME
on a fresh boot, twice per build, `SWAPZFN` against `SWAPZF7` (replaces the 45-minute soak).

The card keeps `SWAPZF7`; `C:\SWAPCOMP.B85` is `SWAPZFN`. The restart to `SWAPZF7` waited 11 minutes on
"PERFLOG not responding": the script restarted while PERFLOG's 400 s log still ran (owner closed it).
The agent's `reboot` verb did nothing on this card; Start > Shut Down > Restart by keys works.
