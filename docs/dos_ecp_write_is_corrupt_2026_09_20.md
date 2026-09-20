# The DOS+ECP write is corrupt, and the "Windows stack" figure rested on it

Real 5160, 2026-09-20. Stock vendor DOS driver (`SD120PPD.SYS`, crc32
`13bf5b05`, verified byte-identical to our `.SYK` before the run), `/fe` on the
`CONFIG.SYS` line, LS-120 at `D:`. Banner reported **`ECP Read` / `ECP Write`**
- `/fe` is not a transport override, so EPP was tried, rejected, and the walk
fell through to slot 3.

## What happened

`TBD.BAT`: two timed copies of `BOUNDARY.BIN` (4,000,000 bytes) to `D:`, then
`FC /B`.

| | |
|---|---|
| copy 1 | **35.10 s** |
| copy 2 | **42.02 s** |
| `FC /B` | **interrupted at 32%** after 7 minutes, having written 13.3 MB of difference records |

The log, read off the CF in a host reader rather than over the serial link:

```
Comparing files c:\BOUNDARY.BIN and D:\boundary.bin
00004001: 10 F2
00004002: 00 0E
00004005: 10 F2
...
0013497A: ^C          <- the interrupt, at 1,263,482 of 4,000,000 bytes
```

- **The first 16,384 bytes match exactly.** The first difference is at `0x4001`.
- From there, **785,053 differing bytes in the 1,263,482 compared - about 62%.**
- The differences are spread across all four byte positions of a dword
  (182k / 287k / 254k / 62k at offsets 0-3 mod 4), so this is **not** a
  byte-lane swap. It is gross corruption.

✅ **The source is not the problem.** With the CF in a host reader,
`C:\BOUNDARY.BIN` hashes to `a2ea9a7af4c73214840b2988d334a353` - byte-identical
to the known-good copy in `drivers/imation_ls120/eppfast_deploy/`. FC compares
`C:` against `D:`, so this rules out a corrupt source and puts the fault on the
LS-120 side. Checked before the conclusion was drawn on, not after.

⚠ This is the state left by copy 2, which **overwrote** an existing 4 MB file.
Copy 1's output was overwritten and cannot now be checked, so whether the
create path is also broken is **unknown**.

## The part that matters

`T2.TXT` on the same card is last night's DOS run:

```
copy 1   1:00:59.24 -> 1:01:33.79 = 34.55 s
copy 2   1:01:34.12 -> 1:02:09.82 = 35.70 s
```

Those are exactly the **34.55 / 35.70 s** recorded as "DOS + ECP" in
`next_session_2026_09_20b.md` section 9. **The file contains no `FC` line.
That run was never verified.**

`T3.TXT`, the Windows run, does contain one:
`Comparing files c:\BOUNDARY.BIN and j:\boundary.bin` ->
`FC: no differences encountered`.

So the comparison that has been steering this work was:

| arm | verified |
|---|---|
| Windows + EPP, 43-52 s | **yes**, byte-identical |
| DOS + ECP, 35 s | **no** - and the repeat is corrupt |

## ⛔ Retractions

- **"DOS+ECP does the same 4 MB in 35 s"** - it does not do the same 4 MB. It
  moves 4 MB of *something* in 35 s. Do not quote this figure.
- **"The Windows stack costs ~10-18 s of a 44-53 s copy, and is the bigger
  lever than the transport"** - this was the difference between a verified
  transfer and an unverified broken one. **It is withdrawn.** The Windows-side
  cost may be real, but nothing measured so far establishes it.
- The same applies to today's restatement of it as "~8.9 s, about 20%",
  derived from the same DOS number by access arithmetic. Also withdrawn.

⚠ **Not retracted:** the ECP *read* result. `GOODTIME.MPG`, 36,735,152 bytes,
was written over EPP by the Windows miniport and read back over ECP by the DOS
driver with `FC` reporting no differences. That verifies the **read**
direction, and it is untouched by this. The memory entry
`ls120-ecp-bulk-works-dos-driver-2026-09-20` says the drive "serves" a 36 MB
file - serving is reading. **The ECP write direction had never been checked at
all.**

## What this costs and what it buys

It removes the basis for prioritising the Windows stack over the transport, so
the ranking of levers has to be rebuilt from verified numbers only. Currently
that leaves exactly one verified end-to-end figure for a 4 MB write to this
drive: **Windows, 43.45-51.90 s, byte-identical.**

It also means the LS-120 has a **correctness** defect on the DOS+ECP write path
that nobody had found, because no DOS write had ever been verified.

## Method note

This is `CLAUDE.md`'s own rule - *"a negative result from an unverified run is
not a result"* - applied to a **positive** one, and it cost a session's
priorities. Technique 79 exists for exactly this: the read-back must come from
outside the code under test. The Windows arm had it, the DOS arm did not, and
the two were compared anyway.
