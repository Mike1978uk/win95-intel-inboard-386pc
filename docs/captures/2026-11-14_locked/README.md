# Card, 2026-11-14: the swapcomp and XT-IDE review builds

Boots 384-386, V9x agent. Logs here are PERFLOG and PAGETIME output, cut to the run named.

| Boot | Drivers | Result |
|---|---|---|
| 384 (B1) | `SWAPZF` `C2585E38`, `XTBOTH` `DAEF6830` | locked 1,736,704 B, disk cache 241,664 B |
| 385 (B2) | `SWAPZFC` `349776FD`, `XTIDEMP` `A3F3EEAD` | PAGETIME: 7,941 page reads, ChecksumErrors 0, Errors 0, Fallback 0; XTSECT `B2FAB` within noise of `abA`/`abB`; KERNEL32 `2AFBC674`, VMM32 `F4933989`, EXPLORER `40978DF8` read back identical |
| 386 (B3, B3b) | `SWAPZF` `07154EB7`, `XTIDEMP` `A3F3EEAD` | locked 1,699,840 B at cache 163,840 B |

The 8 KB saving (one page from each driver, `docs/le_census_2026_11_14.md`) is not shown by
`VMM\cpgLocked`: within one boot it moves 1,628-1,800 KB with open programs and cache size, and
cache pages are only partly counted as locked, so "locked less cache" is not a fixed baseline.
The saving stands on the object page counts only.
