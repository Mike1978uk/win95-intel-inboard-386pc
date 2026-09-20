# EPP Fast (dword) unlock for the vendor LS-120 miniport

Put the CF in a reader. `<CF>` below is whatever letter it takes.

| file here | what it is | md5 |
|---|---|---|
| `SD120PPD.MPD` | **patched** Windows miniport — EPP dword unlocked | `4f1fb59cb7dda09d1002c34bfe32ef0f` |
| `SD120PPD.STK` | stock miniport, for revert | `08104ffb559ae4b47b84377daee473bc` |
| `SD120PPD.SYS` | **patched** DOS driver — same gate | `96018ce66d2312a26fa16193b5273a37` |
| `SD120PPD.SYK` | stock DOS driver, for revert | `cbb42e8eb7847869e274e45f258cf718` |

## Do the DOS one FIRST

Both drivers carry the **same gate from the same source** — the 16-bit build
ends on the identical `3c 03 72 04 b6 0b b2 04`. But only the DOS driver
**prints the mode it chose**:

```
    Read  Mode : EPP Fast      <- dword, the gate opened
    Read  Mode : EPP Normal    <- byte-wide, it did not
```

So patch the DOS driver, add `/fe` to its `CONFIG.SYS` line, boot to DOS, and
**read the banner**. That is a direct readout of whether the patch does what it
claims, on a driver that cannot damage the Windows install — instead of
inferring it from a stopwatch afterwards. Only then do the miniport.

⚠ `/fe` on the DOS line is for **this test only**. The verification copy wants
the DOS side on its normal transport so it stays a different path from the
Windows write.

## What it changes

Two bytes, at rva `0x80B2`, `75 0d` -> `90 90`.

The miniport defaults to byte-wide EPP and upgrades to dword only if a gate at
rva `0x80AB` passes. That gate reads a byte set **only** as a by-product of a
successful chipset detection — and detection is suppressed here (`/de /db /ni`)
because it writes `0x22`/`0x23`, which alias onto the 8259 on an XT. So the byte
stays 0 and dword can never be selected. Nopping the `jne` drops that one test
and leaves the CPU-class test below it intact.

Read mode goes 10 -> 11 and write mode 3 -> 4: `ScsiPortReadPortBufferUchar`
becomes `...Ulong`. **One I/O access carries four bytes instead of one.**

Adds no port access of any kind. With `/fe` the transfer routines take the
`[0x20BD9]` branch at `0x8D81` and never reach the `0x22`/`0x23` writes.

## Where it goes

⚠ **Confirm both paths on the card before copying — not yet verified this
session**, because FC had the disk locked when this was written.

1. `<CF>:\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD` — the driver Windows loads.
2. The vendor install source (believed `<CF>:\LS120VEN\`). Deploy to **both**:
   a driver refreshed from a stale install source silently reverts, which has
   already cost this project a week once.

**Back up what is there before overwriting**, and after copying, md5 the file
**in its destination** — not the staging copy. That is the check that catches a
half-finished write.

## Revert

Copy `SD120PPD.STK` over both destinations as `SD120PPD.MPD`. Or, from the repo:

```
python patch_sd120ppd_eppfast.py --revert <patched> <out>
```

which refuses a no-op and reproduces `08104ffb...` exactly.

## What to look for after booting

- **Drive letter present.** If it is gone, revert.
- **Time a copy of a known file** and compare against the byte-wide baseline:
  36,735,152 bytes in 7 min **by wall clock, +/-59 s** = **75-99 KiB/s**. The
  measured cost model says a dword access should take 5.77 us/byte to about
  2.85 — call it up to 2x.
- **Verify the bytes, not just the clock.** `FC /B` against the source, with the
  DOS driver reading it back, so the write path and the read path are different
  code.

## If dword does not work

The driver catches it itself: the recovery ladder at `0xAEEC` calls `0xABC6`,
latches `[0x20C7C]=1` and drops back to byte-wide for the rest of the boot. So a
wrong answer degrades to today's behaviour rather than corrupting anything.

That latch is one-way and never cleared, so **a transient error costs you the
width until the next reboot** — which is also why a re-timed copy should be run
on a fresh boot with nothing else having touched the drive first.

---

# BOUNDARY.BIN — the test file

`BOUNDARY.BIN`, 4,000,000 bytes, md5 `a2ea9a7af4c73214840b2988d334a353`.
Copy it to `<CF>:\` while the card is in the reader. Test cycle is then
`COPY C:\BOUNDARY.BIN D:\` and `FC /B` — about three minutes, not an hour.

## Why 4 MB

| | |
|---|---|
| at today's 75-99 KiB/s | 41-55 s |
| at dword, if it works | ~23 s |

A 2x difference against a stopwatch error of a second or two is unmissable.
Smaller than ~2 MB and the reading gets ambiguous; larger and the loop stops
being repeatable, which is what we actually need now.

## What it crosses

| boundary | |
|---|---|
| 512-byte sectors | 7812.5 -> **7813, last one partial** |
| ATAPI bursts (3584 B = 7 sectors) | 1116.07 -> **1117, last one short** |
| 64 KB | **61** crossings |
| dwords | 1,000,000 exact — and since a sector is 128 dwords, `epat.c` mode 5's tail path (`count/4-1` dwords, then 3 bytes, then the last byte) runs on **every sector** regardless |

## Why the content is a counter, not a slice of the video

Dword N holds the value N, so **every dword names its own address**.

- Read any dword, multiply by 4, and that is where it should have been. A
  mismatch is self-locating instead of "FC says byte 1,234,567 differs".
- A **byte-lane swap inside the dword** — the classic failure of a 32-bit
  transport on an 8-bit bus, and exactly what this patch risks — is visible at
  a glance rather than needing to be inferred.
- A repeated or dropped burst shows as a clean step in the counter.

Compressed data (an MPEG) does the opposite: every corruption looks like noise,
and the decoder hides the rest. Technique 109f's incrementing-pattern rule,
applied at file scale.
