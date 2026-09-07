# Next session — from 2026-09-08

Written at the end of 2026-09-07. **Three issues closed in three days** (#21, #19, #17), plus #24,
and two new findings that each answer a long-standing question.

---

## Closed today

- **#17** — all drives out of MS-DOS compatibility mode. `INBRDPC.SYS` was never the cause, only
  what Windows could name; the class-wide blocker was an unrelated parallel-port ASPI driver.
- **#24** — **stride 1 PASSES.** The shipped `XTIDEMP.MPD` autodetected stride 1, claimed the boot
  disk and tore down cleanly on a faithful Compatibility-map bed, write verified off the image.
  Cleared the 16-bit latch transport in the same run. **86Box already modelled that map** — the
  test bed had been in the tree the whole time.

## The two findings that matter

### 1. Floppy: the BIOS reports both drives as 720K 3.5" (#25 — ANSWERED)

`INT 13h AH=08h`, real-mode DOS over COMrade, **identical for both drives**:

```
BX=0003  CX=4F09  DX=0102     BL=3 (720K 3.5"), 80 cyl, 9 sect, 2 heads, 2 drives
```

Accounts for every symptom with nothing left over: both shown as 3.5", B: formatting at 720K,
both saying "needs formatting" with real media, and DOS reading them fine (DOS uses the disk's own
BPB, not `AH=08h`). Verified the other way too — `dir_list A:\*.*` returned 22 real entries off the
exact disk Windows called unformatted.

**So Windows is believing what it is told. `HSFLOP.PDR` is not at fault for this.**

⚠ **NOT yet established: who answers `AH=08h`** — Sergey Kiselev's floppy ROM, or the 1986 system
BIOS with a stock table. `ES:DI = F000:EFA0`, which is the system BIOS segment. **Do not conclude
the ROM is misconfigured.** A stock XT BIOS predates `AH=08h` for floppies, so a 720K table is
plausible from either. Next: is the ROM hooked into `INT 13h`, does it implement `AH=08h`, and does
it carry a drive-type setting?

Raw capture `docs/evidence/fdtype_int13_ah08_2026-09-07.txt`, probe `tools/fdtype/`.

### 2. LS-120: phase 0 PASSES — the miniport shape is safe here (#22)

`Init Success ls120mp.mpd` with **the keyboard working throughout**. So the fault in Imation's
`SD120PPD.MPD` is **in their code**, not structural to a polling parallel-port miniport on an XT.
That was the one result that would have killed the plan.

The driver that passed has **one port instruction and no `OUT` at all**, so nothing it did could
have masked an interrupt even in principle.

Also settled today: the vendor binary's **whole `AdapterSettings` surface** is exhausted — nine
keywords, four of them parsed and never read, none gating the destructive writes. So "persuade
Imation's binary to behave" is dead from both ends (six patches, six nulls; and now its config).

**Built and waiting:** `-Phase 2` links a written parallel-port layer and a written ATAPI packet
layer either side of a **stubbed EPAT bridge**. An ATAPI packet *is* a 12-byte SCSI CDB, so the SRB
dispatch is a pass-through. All untested.

**THE ONE DECISION BLOCKING IT** — owner's call, recorded in
`drivers/imation_ls120_mpd/README.md`:

| | route | cost |
|---|---|---|
| **A** | licence `drivers/imation_ls120_mpd/` **GPL-2.0**, port layer 2 from Linux `paride/epat.c` | days |
| **B** | recover EPAT from `SD120PPD.SYS`, stay MIT | multi-session, may not converge |

Only layer 2 would be GPL-derived under A; layers 1 and 3 and the miniport stay MIT either way.
The owner has said the driver should be open source and live in this repo — which GPL-2.0 on that
one directory satisfies — but has **not** explicitly authorised GPL. Ask once, plainly.

---

## Also done

- **XTIDE submission is OUT.** Two VCFed posts live, both since amended with the #24 result; the
  maintainer email sent, and a follow-up sent correcting the stride-1 claim.
- **#23** raised — `XTIDEMP.MPD` cannot drive the XT-IDE **Hi-Speed** map (an A3/A0 swap, a
  permutation no stride expresses). Not urgent; gated on the maintainers confirming the map.
- **#26** raised — owed VOGONS replies: **@disruptor asked about the ST01** (8-bit ISA SCSI, so the
  shipped miniport template applies directly) and **@red-ray's SIV test**, whose precondition
  (drives out of real mode) is now met. **Read disruptor's actual post before drafting** — an
  automated summary of that thread missed it once.
- **`docs/ios_safelist_howto.md`** — the `IOS.INI` `[SafeList]` hand edit, which gates every 32-bit
  storage driver here and had been referenced from six places and explained in none.
- **Every boot now logs.** `MSDOS.SYS` gained `BootMenu=1` / `BootMenuDefault=2` / `Delay=5`.
  Revert: `C:\MSDOS.BAK` on the card, or `docs/evidence/MSDOS.SYS.before_bootmenu_2026-09-07`.
- Four stale claims corrected, and the **floppy drive-letter theory retired** — both `ROOT&FDC`
  nodes hold correct letters and there is **no geometry field anywhere in the hive**, for any device.

## Release gate (#17's successor)

Owner wants a release once the stack is clean: **#18 (floppy wrong data — still the live bug, keep
floppies read-only), #22 (LS-120), #25 (drive types)**. #25 now has its answer and needs the ROM
question settled.
