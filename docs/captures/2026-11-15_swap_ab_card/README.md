# swapcomp A/B on a fresh boot each: SWAPZF7 against SWAPZFN, card, 2026-11-15

Card 192.168.68.62. Order ZF7, ZFN, ZFN, ZF7, each on a fresh boot after M8IDLE (`run.log`).
PERFLOG 260 s alongside, so it has exited before the next restart. Counters cumulative since boot,
last row, read with `tools/perflog/perflog_table.py`.

| run | boot | VxD | PAGETIME | open / switch 1-3 (ms) | page-ins | raw / compressed reads | sectors saved |
|---|---|---|---|---|---|---|---|
| `ab7a` | 397 | `SWAPZF7` `188AFAA0` | 130.0 s | 66,256 / 17,527 17,505 15,787 | 10,822 | 330 / 6,514 | 30,408 |
| `abna` | 398 | `SWAPZFN` `99B7BD5B` | 124.7 s | 66,628 / 15,063 15,745 13,801 | 11,043 | 1,173 / 5,772 | 30,614 |
| `abnb` | 399 | `SWAPZFN` | 128.9 s | 66,871 / 16,514 15,098 17,417 | 11,035 | 1,174 / 5,714 | 29,579 |
| `ab7b` | 400 | `SWAPZF7` | 123.0 s | 65,148 / 16,419 16,032 16,426 | 10,898 | 332 / 6,495 | 30,337 |

Errors 0 in every run.

**No time difference:** ZF7 126.5 s mean, ZFN 126.8 s; the spread within a build (5-7 s) is larger
than the gap. ZF7 cuts raw reads by 72 % but saves no more sectors: a page that fits in 7 sectors
saves one sector of eight, and decompressing it costs CPU. The 7-sector build stores fewer raw
pages, not fewer bytes read.

The card stays on `SWAPZF7` (no loss, data intact). The first restart was missed (keys lost, card
not hung); the script now sends Escape first and retries the keys after 4 minutes.
