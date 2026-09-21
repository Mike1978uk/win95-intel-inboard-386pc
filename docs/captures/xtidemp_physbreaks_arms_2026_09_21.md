# The `-PhysBreaks 17` A/B pair is valid — verified byte-by-byte, 2026-09-21

Verified **offline**, from the owner's disk backup
(`OneDrive/Desktop/latest_vendorls120.img`, 2026-09-20), without the CF or COMrade.

## The two arms, as they sit in `\WINDOWS\SYSTEM\IOSUBSYS\`

| file | md5 | what |
|---|---|---|
| `XTIDEMP.MPD` | `db88f64d` | **baseline** — the shipped driver, code `20535159` |
| `XTIDEMP.PB` | `a6bfb48b` | **test** — `-PhysBreaks 17`, code `01be936b` |
| `XTIDEMP.B20` | `db88f64d` | backup copy of the shipped build |
| `XTIDEMP.ORG` | `561fb45b` | the original, pre-word-transfer |

## Exactly one functional byte differs

```
0x0D57   c7 46 1c 00    ->    c7 46 1c 11
         mov word [bp+1Ch], 0      mov word [bp+1Ch], 17
```

That is the `NumberOfPhysicalBreaks` store in `PORT_CONFIGURATION_INFORMATION`.

⛔ **The offset was recorded as `0x0D5E` in `next_session_2026_09_20c.md`. It is `0x0D57`** —
seven bytes out. A spot-check against the wrong offset reads `0x02` and looks like the staged
file is the wrong build, which is exactly what happened on 2026-09-21 before this diff was run.

The other **16** differing bytes are all build metadata, none of it code:

| offset | |
|---|---|
| `0x0088`, `0x1204`, `0x1220` | PE timestamp, three copies (header + debug directory) |
| `0x00D8` | PE checksum |
| `0x2809` | `"Reg"` → nulls, a CodeView debug-path fragment |
| `0x291C` | the `NB10` CodeView **age** field |

⚠ The handoff said *"the other five differing bytes are the PE timestamp and checksum"*. There
are **16**, and they include CodeView records as well. The conclusion was right; the count was not.

## ⭐ Why this matters beyond the offset

The two arms were built from **different trees** (`ecb53d4` clean, `33684e1` DIRTY), which looked
like a two-variable A/B and a reason not to trust it. **It is not**: the trees produced byte-identical
code, and every difference outside `0x0D57` is metadata. **The A/B is clean and ready to run.**

The general point is the one technique 130 already makes, sharpened: *check what actually differs
in the artefacts, rather than reasoning from their provenance.* Two builds from different trees can
still be a valid single-variable pair, and two builds from the same tree might not be.

## What has NOT been done

⛔ **Neither arm has been run on hardware for throughput.** The A/B is staged and never executed.

⚠ `XTIDEMP.MPD` serves the **boot volume**, and 64 KB SRBs have never been exercised on it —
`NumberOfPhysicalBreaks = 0` has capped every request at a page since the driver was written. Swap
by rename so a bad boot is a rename back, and keep `XTIDEMP.B20` as the known-good copy.

⚠ `a6bfb48b` exists **only on the card and in this backup** — it is not in
`drivers/xtide_mpd/build/`, and its tree was DIRTY, so it cannot be rebuilt bit-for-bit. That is
fine for running the A/B, because the byte diff above establishes exactly what it is. It would not
be fine as the basis for a shipped driver: rebuild from a clean tree before shipping.
