# 2026-09-20d — the EPAT submission is BLOCKED: no drive on master

## The bar, and the result

The owner set it plainly: *"it has to work and the drive has to be there for
this to be a submission or people will try it and say it's failed."*

**It does not work on master. Nothing has been submitted, and nothing should
be until it does.**

Two bed runs, both negative:

| run | image | driver bound | result |
|---|---|---|---|
| A | `ls120win_clean.img` | ours, by registry | `Init Failure ls120mp.mpd`, `no ls120 volume at J:` |
| B | owner's `latest_vendorls120.img` | vendor `SD120PPD.MPD` | no J drive, observed on screen by the owner |

Run A is void as a test of the vendor path: dropping `SD120PPD.MPD` into
`IOSUBSYS` does not install it. The registry on that image binds
`LS120MP.MPD`, which run A had just deleted — hence the `Init Failure`. A file
drop is not an install (technique 131; `-VendorDriver` carries the warning).

Run B is the real test, on the owner's own working image, and it is the one
that counts.

## What run B did prove

The vendor driver reaches our bridge on master and the early protocol works:

```
EPAT: DISCONNECT
EPAT: unlock frame committed with unknown command 40
EPAT: unlock frame committed with unknown command 50
EPAT: CPP init
EPAT: CPP unit 0 id -> FFAA
EPAT: CPP unit 1 id -> 0000        ... through unit 7 -> 0000
[0117:0000B929] Illegal instruction 00008B55 (FF)
```

- The unlock frame is recognised and committed.
- The CPP chain scan runs, unit 0 answers, 1-7 are correctly empty. The scan
  is **not** the fault.
- `unknown command 40 / 50` is **not** a defect. Those are bare strobes with no
  response, per SD120PPD.SYS's own decoder at `0x2767`, and `lpt_epat.c:277`
  already says so. The log wording is alarming and should be softened.

So the failure is **after** the chain scan, somewhere between CPP unit select
and the drive presenting itself. The `Illegal instruction` at `0117:0000B929`
is protected-mode and unexplained; it may be the miniport faulting, or
unrelated. **Not yet attributed — do not cite it as the cause.**

## Next

1. Find where run B stops. The bridge log ends at the chain scan, so the next
   question is what the driver asks for after unit select and what we return.
   Register `0x0B` (chip version, must be `(ver & 0xF8) == 0xC0`) is the first
   thing to confirm on this build.
2. Attribute or dismiss the `0117:0000B929` fault.
3. Only then revisit the PR.

⛔ The PR draft's "Tested" section still claims the fork run as evidence. That
claim stands, but the master gap is now measured, not merely untested — see
`docs/upstream_patches/PR_DRAFT_epat.md`.
