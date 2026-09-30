# Windows 95 "lite" builds - study material for the RAM track

Not install targets. Each one is a list of what Windows 95 runs without; the removals worth
having get applied one at a time to our OSR1 and measured with `RAMBASE`
(`docs/ram_baseline_2026_09_28.md`). Media held locally in `XT_project/95_lite_isos/`, not in
the repo.

| build | source | held | notes |
|---|---|---|---|
| Mini-95 | <https://archive.org/details/mini-95> | `MINI95.ISO`, 24,100,864 bytes, md5 `df786ff1f5c7089c8ea0c4b1c1238ca7` | Setup files dated 1995-07-10 (retail, pre-OSR); a trimmed installer set with `MSBATCH.INF`. Not yet unpacked |
| Windows 95D Lite 1.5a | <https://crustywindo.ws/Windows_95D_Lite_1.5a#Software_on_the_CD> | not downloaded | Page not yet read. Microsoft shipped no "95D", so check which release it is built on before comparing |

Also held: an OSR2 install set (August 1996) on the owner's 1990s fix-it CD. OSR2 stays archived
(`osr1-pivot`, 2026-08-04); it is reference only.

## Method

1. Diff what each build removes (`LAYOUT.INF`, `MSBATCH.INF`, the files it ships) against our
   OSR1 install.
2. Keep only removals that touch something `BOOTLOG.TXT` or the locked-memory audit
   (`docs/driver_audit_2026_09_28.md`) shows loaded.
3. One removal per `RAMBASE` run, in the bed first when it touches a VxD.
