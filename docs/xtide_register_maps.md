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

## What has and has not executed the stride-1 path

Worth stating precisely, because the short version ("stride 1 was tested in the emulator") is
half true and misled this project's own drafts on 2026-09-07.

- **86Box's stock `hdc_xtide.c` decodes `port & 0xf`** — a stride-1, consecutive-register map. So
  the emulator does model a Compatibility-shaped card, and it is the *default* there.
- **The retired `.PDR` ran against that bed for weeks.** Its stride-1 code path genuinely executed.
  But it never correctly claimed the disk on it: the driver fell back to stride 1, `RMM.PDR` stayed
  and served the volume, and the shutdown defect never triggered *because the driver was not really
  doing the job* (technique 88). Code executing is not the map being driven.
- ~~**`XTIDEMP.MPD` has never run at stride 1 at all.**~~ **Superseded 2026-09-07 — it has now.**
  The miniport was developed and confirmed against stride 2 only, and that stayed true until the
  test below.

## ✅ Stride 1 executed and passed in emulation, 2026-09-07 (issue #24)

The shipped binary — md5 `db88f64d9a0500d13690032445ad4e31` (was `561fb45b...` before the
2026-09-10 word-transfer revision) — byte-identical to
`dist/xtide_mpd/XTIDEMP.MPD`, taking its base from `AdapterSettings PORT=0x300` — **autodetected
stride 1 and claimed the boot disk.** Bed `vm_xtide_stride1`, emulator reporting
`XTIDE: base 0300 stride 1 bios xt_plus` at init (verified at runtime, technique 69).

```
[00000226] Initing xtidemp.mpd        [00000228] Init Success xtidemp.mpd
rmm.pdr  Dynamic load success ... and NEVER reaches INITCOMPLETE   <- boot-disk takeover
INITCOMPLETESUCCESS = SCSIPORT / DiskTSD / DiskVSD / VFAT / IFSMGR / IOS
ESDI_506 absent     IOS.LOG absent     7 teardown stages started, 7 closed, none unpaired
```

Write verified **host-side, off the image**: `C:\HELLO.TXT`, 5 bytes, `hello` — so the write path
and the shutdown cache flush both work on this map (technique 79's discipline). Log kept at
`docs/bootlogs/BOOTLOG_2026-09-07_stride1_emulation.TXT`.

**It also exercised the 16-bit high-byte latch transport**, the other never-run path. At stride 1
`hdc_xtide.c` models `reg 0x8` as the high-byte latch and only goes byte-wide when the BIOS sends
`SET FEATURES 01h`, so the driver's latch path is what ran.

### Reproducing the bed

The bed is a throwaway 86Box working dir and is not tracked (repo-hygiene §3). It is three
changes to a copy of `vm_xtide_mpd2`'s config:

```
[PC/XT XTIDE]
bios   = xt_plus      # was xtcf_lotech - that ROM drives stride 2 and cannot boot this bed
base   = 300
stride = 1            # was 2
```

Image: a copy of `vm_xtide_mpd2/xtidemp2.img`, which holds the shipped `XTIDEMP.MPD` installed
with `AdapterSettings PORT=0x300` and the node auto-assigned. Delete `BOOTLOG.TXT` from it first
(`python tools/fatcp.py <img> --rm BOOTLOG.TXT --yes`) - a stale one names a driver from two
images ago and reads exactly like a result (technique 23/94). `MSDOS.SYS` already has
`BootMenuDefault=2`, so the boot is logged without anyone pressing a key, and
`mouse_type = msserial` keeps the modal mouse dialog off the shell (technique 87).

**What this does not prove.** It is a faithful model of the *register map*, not of a real card: no
XT bus timing, no real option ROM, no partial address decode. The ask for a physical
Compatibility-mode card stands — but the path is no longer unexecuted, and a stranger's disk is no
longer the first thing it will ever touch.

## Consequence for the ask

The valuable request is no longer "does anyone still own a 2009 Rev 1 card". **Any XT-IDE Rev 2, 3
or 4 switched to Compatibility mode presents the stride-1 map**, with XUB set to device type
`XTIDE rev1`. That is a jumper and a BIOS setting on hardware plenty of people have, and it
exercises a path in the shipped binary that has never executed.

## ⚠ STALE ROM DUMP — the card was reflashed, found 2026-09-11

`roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin` is **XTIDE Universal BIOS 2.0.4**.
Read live from the running machine over COMrade on 2026-09-11, `D8000` now holds:

```
XUB212-=XTIDE Universal BIOS (XT+)=-
r638 (2026-06-09)
```

**XUB 2.1.2, build r638, the XT+ variant.** The card has been reflashed since the dump.

### What this puts in doubt

`bDevice = 0x0A` — the byte that made "XTIDE rev 2 (latched) or Lo-tech XT-CF" ambiguous
and was settled by photographing the board (technique 95) — was decoded from the **2.0.4**
image. The ROMVARS layout is not guaranteed stable across a major version, so that offset
may not mean the same thing in 2.1.2.

The board photograph still settles the hardware question: no 74x373/573/574, so no
high-byte latch, so it is an XT-CF. That conclusion does not depend on the ROM. But any
statement about **what the ROM is configured for** needs re-reading from the live image.

### Do not decode it by eye

The live config block shows four IDEVARS-shaped entries - one near `0x0300` and then the
standard `0x170/0x370`, `0x1E8/0x3E8`, `0x168/0x368` pairs - but the 2.1.2 struct is not
held locally and guessing offsets is how the `bDevice` ambiguity arose in the first place.
**Read it with `XTIDECFG`**, which loads the ROM and displays it, or fetch the XUB 2.1.2
`ROMVARS.INC` before decoding anything.

### And re-dump the ROM into the repo

The stored image is now known-stale and is cited by this document and by #23/#24. Replace
it with a live 8 KB read of `D8000-D9FFF` next time the machine is up, and keep the old one
under its dated name rather than overwriting - the two versions are evidence of a change
that nothing else recorded.
