# Session start — 2026-09-08

Written at the end of 2026-09-07. Read this top to bottom before touching anything; it is ordered
by what to do first.

**Two jobs to finish: the floppy (#25 + #18) and the LS-120 (#22).** Everything else is parked.

---

## 1. Where the machine is

The real 5160 runs **the boot disk and the entire SCSI chain in 32-bit protected mode**, and the
floppy driver loads. Confirmed on hardware, read off the CF rather than the screen.

```
xtidemp.mpd   Init Success     <- the XT-CF, C:          (#21, closed)
t130.mpd      Init Success     <- the SCSI chain         (#19, closed)
hsflop.pdr    Init Success + INITCOMPLETE = HSFLOP
ls120mp.mpd   Init Success     <- our phase-0 skeleton   (#22, phase 0 passed)
rmm.pdr       loads, never reaches INITCOMPLETE          <- boot-disk takeover
IOS.LOG       does not exist
```

**Three issues closed in three days** (#21, #19, #17) plus **#24**.

### Standing facts you will need

- **Every boot now logs automatically.** `MSDOS.SYS` has `BootMenu=1` / `BootMenuDefault=2`
  (= Logged) / `BootMenuDelay=5`. Revert with `C:\MSDOS.BAK` on the card, or
  `docs/evidence/MSDOS.SYS.before_bootmenu_2026-09-07`.
- **There is no Imation code left.** Registry: zero hits for `SD120PPD`, `Imation`, `Shuttle`,
  `OEM0`. `IOSUBSYS\SD120PPD.MP_` is renamed and cannot load. **Both** DOS lines in `CONFIG.SYS`
  are REM'd out, so the LS-120 has no driver at all — that is expected, not a symptom.
- **Floppies are READ-ONLY** until #18 is understood. #18 corrupted an image in emulation after a
  few disk swaps.
- The SCSI chain was powered **off** for the last boot, so drive letters will differ when it is up.

---

## 2. FLOPPY — cause found, fix not chosen (#25), and #18 is the real blocker

### What is settled

`INT 13h AH=08h` returns **drive type 3 (720K 3.5") for BOTH drives**, identically:

```
BX=0003  CX=4F09  DX=0102     BL=3, 80 cyl, 9 sect, 2 heads, 2 drives
80 x 2 x 9 x 512 = 737,280 = 720K
```

The machine has a **TEAC FD-505**: 1.44 MB 3.5" as A:, 1.2 MB 5.25" as B:. So the BIOS reports the
wrong geometry for A: and the wrong drive class entirely for B:.

**This accounts for every symptom with nothing left over** — both labelled 3.5" in Explorer, B:
formatting at 720K, both saying "needs formatting" with real media, and DOS reading them fine
(DOS uses the inserted disk's own BPB, not `AH=08h`). Verified the other way too: `dir_list A:\*.*`
returned **22 real entries** off the exact disk Windows called unformatted.

**So Windows is not mis-detecting — it is being told the wrong thing, and `HSFLOP.PDR` is innocent
of this.** `AH=08h` is an **AT-era service** a stock 5160 BIOS predates: the same family as the 8259
aliasing across `0x20-0x3F`, the 20-bit DMA page latch, and the uninitialised `INT 68h` vector.

Evidence: `docs/evidence/fdtype_int13_ah08_2026-09-07.txt`. Probe: `tools/fdtype/` (staged on the
card at `C:\FDTYPE\`, runs from real-mode DOS, writes `FDTYPE.TXT`).

### Already ruled out — do not spend a boot re-deriving these

- **`DETLOG.TXT` cannot answer it.** Pulled and CRC-verified
  (`docs/evidence/DETLOG_2026-09-07.TXT`, 14,097 bytes). 22 `Detected:` lines, **zero** mentions of
  floppy / FDC / `PNP0700` / diskette — Windows' PnP never enumerated the controller, because it
  was installed by hand. Clean negative.
- **`INT 13h` is hooked at `0575:0122`** — low DOS memory, not `F000`, not a `C800`-`DE00` option
  ROM window. Consistent with `INBRDPC.SYS`, which is known to hook `INT 13h` to adjust wait states
  via port `0x670` and then chain (it is *not* in the data path — #17).

### FIRST ACTION — SUPERSEDED 2026-09-07: the chain walk is not needed

Answered statically off the ROM dumps already in the repo. Full working:
[`evidence/int13_ah08_who_answers_2026-09-07.md`](evidence/int13_ah08_who_answers_2026-09-07.md).

It lands in **neither** box this briefing predicted. Settled:

- **The 1986 system BIOS does not implement `AH=08h`** — no `cmp ah,08` in either U18 chip
  (`F800`, where the diskette BIOS lives), and the call returned success. Not it.
- **Sergey's ROM is doubly exonerated.** It *does* implement `AH=08h` (dispatch `0x1119` ->
  handler `0x98f`), and its per-drive config table at `0x1f81` reads **type 4 (1.44 MB)** for
  A: and **type 2 (1.2 MB)** for B: — the correct drives. Answering from that table it would
  not say 720K. Its type-3 branch does produce `CX=0x4F09`, but returns `ES:DI = <ROM>:13D6`.
- **XTIDE Universal BIOS intercepts `AH=08h` for drives it does NOT own** — proven in the dump
  taken off the real card, and identical in the r638 XT+ image. `AH=00h` and `AH=08h` go to
  XTIDE's own function table; everything else chains onward. Its floppy path calls the previous
  handler and then rewrites the drive count. It also carries a **byte-identical copy of the
  `F000:EFA0` 9-sector table** at ROM `0x166d`.
- **No DOS floppy driver is loaded at all** — `CONFIG.SYS` on the CF is clean.

⚠ Still a hypothesis, not a finding: `ES:DI = F000:EFA0` fits neither candidate cleanly.
And this is static analysis, which this project has been burned by (techniques 29, 44, 63, 81).

**Do these two read-only inspections before anything else. Both are at the machine, minutes.**

- [ ] **`xtidecfg.com`** — already on the CF at `D:\xtide\`. Read XTIDE's floppy settings.
- [ ] **F2 at boot** into Sergey's Multi-Floppy BIOS utility, then `p` to print the live EEPROM
      configuration. The image in the repo is correct; the EEPROM on the card may not be.

Whichever one reports 720K owns the bug, and the fix is a configuration change in that utility.

### THEN the fix, cheapest first (full costing in #25's comments)

- [ ] **Route 1 — registry override.** The `ROOT&FDC` nodes hold no geometry field (confirmed
      twice, and there is **no** `Cylinder`/`Heads`/`SectorsPerTrack`/`Geometry` value anywhere in
      the hive for any device). But the **driver** keys — `Services\Class\fdc\0000`,
      `DiskDrive\0000`/`\0001` — have not been searched. If `HSFLOP.PDR` reads an override this is
      a registry edit and nothing more.
      ⚠ **Not** by hand-editing `SYSTEM.DAT` — technique 99, CREG validates more than length and a
      byte edit cost a corrupted hive. Use Device Manager or an INF.
- [ ] **Route 2 — a tiny `INT 13h` shim** correcting **only** `AH=08h` for `DL=0`/`DL=1`, chaining
      everything else untouched. Direct precedent: `IVT68FIX.COM`, a 20-byte hand-assembled `.COM`
      already called from the last line of `AUTOEXEC.BAT`. Attractive because it is **not in the
      data path** — it corrects one query at detection time and is irrelevant once protected mode
      takes over.
      ⚠ A resident `INT 13h` hook is what makes IOS refuse every miniport. It needs a `[SafeList]`
      line — `docs/ios_safelist_howto.md`.
      ⚠ Timing matters: `IVT68FIX.COM` only worked from the **last** line of `AUTOEXEC.BAT`
      (technique 38). Whether Windows' detection runs late enough to see the hook is open.
- [ ] **Route 3 — patch `HSFLOP.PDR`'s drive-type determination.** Most work, best scoped:
      protected-mode only, no real-mode component. The right answer if this ships to anyone else.
      `tools/vxd_disasm.py` handles the `CD 20` + inline service-id encoding, and this file has
      already been patched successfully once (`maxPhys` 0x1000 → 0xFF).

**REJECTED — do not re-suggest:** `DEVICE=C:\DOS\DRIVER.SYS /D:0 /F:1`. It adds a *second* drive
letter rather than fixing either existing one, and it is a real-mode block device driver — the exact
category #17 existed to eliminate. It would trade a wrong geometry for a real-mode driver.

### AND: #18 is the actual blocker, not #25

Both drives return **wrong data** — "needs formatting" with good media that DOS reads fine. This is
@andrew-hoffman's 86Box symptom, now reproduced on hardware for the first time (the 2026-08-25
result was **void** — it predates the IOS punt clearing, so the driver was never loading).

**A correct drive type will not help while the transfer is still wrong.** #18 is unfixed, is a
data-loss bug, and owns this. Do #18 before polishing #25.

---

## 3. LS-120 — phase 0 passed, one decision blocks the rest (#22)

### What is settled

`Init Success ls120mp.mpd` **with the keyboard working throughout**. So the fault in Imation's
`SD120PPD.MPD` is **in their code**, not structural to a polling parallel-port miniport on an XT.
That was the one outcome that would have killed the plan.

The driver that passed contains **one port instruction and no `OUT` at all**, so nothing it did
could have masked an interrupt even in principle.

Also settled: the vendor binary's **entire `AdapterSettings` surface is exhausted** — nine keywords,
four parsed and never read, none gating the destructive writes. So "persuade Imation's binary to
behave" is dead from both ends (six patches six nulls, and now its configuration).

Resource note: CONFIGMG forced the node to `0278-027F` while `AdapterSettings` reads `PORT=0x378`,
and the driver used 378 and ignored the node — by design. **Do not narrow the INF's `LogConfig`
to "fix" that**; doing so on 2026-09-05 caused a run of wedged shutdowns (technique 97).

### Built and waiting

`-Phase 2` links three layers, **all untested, nothing has executed**:

| layer | what | status |
|---|---|---|
| 1 | parallel port registers (IEEE 1284) | written |
| 2 | **EPAT bridge register access** | **STUBBED — blocked** |
| 3 | ATAPI packet commands | written |
| — | SRB → ATAPI dispatch | written |

An ATAPI packet **is** a 12-byte SCSI CDB, so the dispatch is a pass-through with no command
translation. Phase 0's code hash is `7bfc5476` and is unchanged by any of this — verified, and the
binary on the card matches.

### THE DECISION — owner's call, nothing proceeds without it

| | route | cost | licence |
|---|---|---|---|
| **A** | licence `drivers/imation_ls120_mpd/` **GPL-2.0**, port layer 2 from Linux `paride/epat.c` | days | that directory GPL-2.0, rest of repo MIT |
| **B** | recover EPAT from `SD120PPD.SYS`, stay MIT | multi-session, may not converge | all MIT |

Only **layer 2** would be GPL-derived under A — layers 1 and 3 and the miniport stay MIT either way.
The owner has said the driver should be open source and live in this repo, which GPL-2.0 on that one
directory satisfies, but has **not** explicitly authorised GPL. **Ask once, plainly, and do not
assume.**

⚠ **Do not read `epat.c` and then write layer 2 "from memory" under MIT.** That is worse than
either route and easy to drift into.

---

## 4. Parked — do not start these before the two above

- **#23** — `XTIDEMP.MPD` cannot drive the XT-IDE **Hi-Speed** map (an A3/A0 line swap; a
  permutation no stride can express). Not urgent, and gated on the maintainers confirming the map.
- **#26** — owed VOGONS replies. **@disruptor asked about the ST01** (2026-09-01) — an 8-bit ISA
  SCSI card, so the shipped miniport template applies directly; **read his actual post first**, an
  automated summary of that thread missed it once. **@red-ray's SIV test** — its precondition
  (drives out of real mode) is now met.
- **The guide.** The owner's own idea and a good one: publish *how to build a Win9x SCSI miniport
  for odd/8-bit hardware* rather than writing drivers to order. Most material already exists in
  `drivers/xtide_mpd/README.md`, `docs/scsi_miniport_costing.md` and the skill — it needs
  collecting, not researching.
- #20, #14, #15, #10, #8 — older emulator issues.

**Release gate**, per the owner: a release once the stack is clean — **#18, #22, #25**.

---

## 5. State

Tree clean, everything pushed. `86box_upstream` is on branch `xtcf-lotech-stride2`; a stash holds
the retired `.PDR` wedge diagnostics.

Evidence added this session: `docs/bootlogs/BOOTLOG_2026-09-07_ls120_phase0_pass.TXT`,
`BOOTLOG_2026-09-07_stride1_emulation.TXT`, `docs/evidence/fdtype_int13_ah08_2026-09-07.txt`,
`docs/evidence/DETLOG_2026-09-07.TXT`.

New docs: `docs/xtide_register_maps.md`, `docs/ios_safelist_howto.md`,
`drivers/imation_ls120_mpd/README.md`.

**Four stale claims corrected**, and the floppy drive-letter theory retired — both `ROOT&FDC` nodes
hold correct letters (`A`, `B`) and `GENERIC NEC FLOPPY DISK` is what Windows calls every floppy,
not a detection failure.
