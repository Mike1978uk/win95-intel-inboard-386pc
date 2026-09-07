# XT-IDE register maps, and which ones our driver can express

Established 2026-09-07, after @andrew-hoffman pointed at the minuszerodegrees references on
[issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).

Source: **[XT-IDE — Register map](https://minuszerodegrees.net/xtide/XT-IDE%20-%20Register%20map.jpg)**
(a JPG; it is the whole answer). Its two companion pages,
[card variations](https://minuszerodegrees.net/xtide/variations/XT-IDE%20and%20XT-CF%20variations.htm)
and [Rev 3 general](https://minuszerodegrees.net/xtide/rev_3/XT-IDE%20Rev%203%20-%20general.htm),
carry **no register-level detail at all** — no stride, no bus width, no latch. Recorded so the next
reader does not go looking.

---

## There are three maps in play, not two

| ATA register | *Compatibility* | *Hi-Speed* | Lo-tech XT-CF (ours) |
|---|---|---|---|
| Data, low byte | `300` | `300` | `300` |
| Data, high byte | `308` | `301` | — (8-bit, no latch) |
| Error / Features | `301` | `308` | `302` |
| Sector count | `302` | `302` | `304` |
| Sector number / LBA low | `303` | `30A` | `306` |
| Cylinder low / LBA mid | `304` | `304` | `308` |
| Cylinder high / LBA high | `305` | `30C` | `30A` |
| Drive / head | `306` | `306` | `30C` |
| Status / Command | `307` | `30E` | `30E` |

**Compatibility** — XT-IDE Rev 1, and Rev 2, 3 and 4 switched into that mode. XUB 1.x.x expects it;
XUB 2.x.x drives it when device type is set to `XTIDE rev1`.

**Hi-Speed** — modified Rev 1, and **the default on Rev 2, 3 and 4**. XUB 2.x.x defaults to it
(device type `XTIDE rev2 or modded rev1`). The map comes from **swapping the A3 and A0 address
lines** to the IDE circuitry, which puts the two data registers adjacent at `300`/`301` so the BIOS
can fetch 16 bits in one word read.

**Lo-tech XT-CF** — A0 simply not decoded, so register N lands at `base + 2N`. Measured on the real
card 2026-08-31 by write/readback over COMrade, not inferred (technique 78).

### The thing to notice

*Hi-Speed is a permutation, not a stride.* An A3↔A0 swap sends register index `b2 b1 b0` to offset
`8·b0 + 4·b2 + 2·b1`. Even-indexed registers do not move at all; odd-indexed ones scatter. It is
**not** `index × 2`, and no single multiplier produces it.

Hi-Speed and our XT-CF map agree on exactly two ports: data-low at `300` and status at `30E`.
Everything in between differs.

---

## What `XTIDEMP.MPD` implements

`XTIDE_SetPorts` computes every port as `base + index × XTIDE_Stride`, with the stride 1 or 2
(`drivers/xtide_mpd/src/XTIDETR.ASM`). That expresses:

- ✅ **Compatibility**, as stride 1, with the high data byte at the fixed `base+8` (`XT_HILATCH`).
- ✅ **Lo-tech XT-CF**, as stride 2. Shipped, and hardware-confirmed 2026-09-06.
- ❌ **Hi-Speed** — cannot be expressed by any stride. Not implemented.

So the shipped driver supports two of the three maps, and the one it does not support is the
**default on every XT-IDE Rev 2/3/4 card**. This was not known when the driver was written, and the
submission drafts said so incorrectly before today.

### What should happen on a Hi-Speed card

`XTIDE_DetectStride` reads Status and Alternate Status — the same register on any correct ATA map —
and compares them, writing nothing. On a Hi-Speed card:

- **stride 1 candidate**: Status at `307`, which is unmapped there, against Alt Status at `30E`,
  which is the real one. Expected to mismatch and be rejected.
- **stride 2 candidate**: Status at `30E` reads correctly, but Alt Status at `base+1Ch` falls
  outside a 16-port decode and floats to `FFh`, which the probe rejects explicitly.

So the expected outcome is that **the driver declines the card** rather than driving it wrongly.
That is the safe failure and it is by construction, but it has never been observed.

**The residual risk, named rather than assumed away:** a card decoding partially would alias
`base+1Ch` down to `base+0Ch` — which on the Hi-Speed map is LBA High. The probe would then compare
Status against LBA High and pass only if those two happened to be equal. Unlikely, not impossible.
This project measured the 5160's own 8259 aliasing across `0x20-0x3F`, so partial decode is not a
theoretical worry here (see [`xt_io_aliasing_gotcha.md`](xt_io_aliasing_gotcha.md)).

### If Hi-Speed is worth supporting

It is a small change and not a rewrite: replace the multiply in `XTIDE_SetPorts` with a lookup
table per map, and add a third candidate to `XTIDE_DetectStride`. The probe stays read-only. Not
done, because there is no card here to test it on — which is precisely what the submission asks
for.

---

## Consequence for the ask

The valuable request is no longer "does anyone still own a 2009 Rev 1 card". **Any XT-IDE Rev 2, 3
or 4 switched to Compatibility mode presents the stride-1 map**, with XUB set to device type
`XTIDE rev1`. That is a jumper and a BIOS setting on hardware plenty of people have, and it
exercises a path in the shipped binary that has never executed.
