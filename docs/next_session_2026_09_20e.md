# 2026-09-20e — EPAT submitted; optimisation track opened as issues

## Shipped

- **86Box PR [#8010](https://github.com/86Box/86Box/pull/8010)** — the parallel-port LS-120.
  Verified OPEN, 9 files, +1,578 / −21. Framed as one transaction: the bridge, the
  `RDISK_BUS_LPT` attach path and the SuperDisk type are each inert alone.
  Tested on master: drive enumerates at `J:`, directory reads, a 92,870-byte write lands;
  a non-LPT machine is unaffected; the SuperDisk also mounts on **SCSI with no bridge**.
  Disclosed in "NOT tested": `RDISK_TYPE_ZIP_750` is enabled as a side effect of the
  contiguous enum block, and the Qt new-image dialog still cannot create SuperDisk media.
- **[`dist/ls120_vendor_spp/`](../dist/ls120_vendor_spp/)** — the vendor Have Disk package.
  One INF line applies the probe suppressors at install, so the keyboard survives it.
- **Andrew's loop closed** on [#23](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/23),
  where he actually asked (2026-09-12, naming BackPack = `bpck`, one of the `paride` bridges the
  PR cites). Ledger updated.
- Console box gone: `tools/pe_subsystem_gui.py`, re-run after each rebuild.

## Issues opened for the optimisation track

**#28** driver + VxD audit (umbrella, six questions, device register) · **#29** DMA: reach,
µs/byte, channel inventory, CPU overlap · **#30** XT-IDE request merging · **#31** SCSI
disconnect + cache pages · **#32** 3C509B · **#33** DRAM refresh · **#34** display mode ·
**#35** shadow RAM + memory-mapped storage · **#36** the FAT12 tooling bug.

⭐ **#29 is the real gap.** Every other path has a measured µs/byte; DMA has none, so it
cannot be ranked. Andrew's OS/2 Museum method has never been run.

## Answered today, do not re-ask

- **`T130.MPD` already uses string I/O.** `ScsiPortReadPortBufferUshort` (2 sites) and
  `WritePortBufferUshort` (1) carry bulk; `...PortUchar` (28/32) is register traffic.
  ⚠ `pedis.py io` shows one `in al, dx` — a SCSIPORT miniport does I/O through imported
  helpers, so **resolve the IAT thunks and count call sites**, or the instrument lies.
- **The card carries `5140442c`**, a 09-19 trace build of `LS120MP.MPD`, not the enumerating
  `d8154f1d`. Restore from `dist/ls120_mpd/` before any hardware test.
- **The `[0117:0000B929] Illegal instruction` is not ours** — it appears with no EPAT device.

## #18 — PARKED, and the owner's machine is fine

The card runs the **patched** `HSFLOP.PDR` (`8e695d00`) and it loads (`INITCOMPLETESUCCESS`).
Not a fault the owner has.

⛔ **Two bed runs withdrawn.** The medium was a synthetic image built by a brand-new tool and
checked only by that same tool. The method for circling back is recorded in the issue's status
block: make the media with a trusted writer, prove a known-good boot reads it, **then** do
Andrew's actual test — boot and *swap disks* — on `monster_fdc`.

## Bed hygiene

`ls120win_clean.img` now carries `d8154f1d` so it always boots; the old 7168-byte miniport
looped forever on the 2500 ms settle and ate two runs. A repeating `SRST released` in the log
means that, not the thing under test. Technique 131 extended with both of today's repeat
offences.

---

# Measurement session, 2026-09-20 evening

## Answered

- **#32 — 3C509B drains in bulk, no lever.** `ELNK3.DOS` (`78242932`) and
  `ELNK3.VXD` (`301b14c4`), extracted from the card image, both carry `REP`
  string I/O: 22/19 and 8/10 `insw`/`outsw`, the VxD including `o32` 32-bit
  variants. Still open: the on-card buffer size, which `3C509B_PORT_SPEC.md`
  does not state.
- **#35a — only `0xF000` is shadowed.** Settled from the existing five-window
  probe, no new run: a shadowed region's per-cycle cost collapses below one
  ISA cycle (`0.151` us vs `1.978`-`3.805` for the option ROMs and video).
  ⭐ **Lever exposed:** the XT-CF option ROM at `0xD8000` runs at 2.861 us/byte
  against the shadowed ROM's 1.113 - a 2.6x gap on the **DOS** INT 13h path.
  Worth nothing under Windows. Open question: can the Inboard shadow regions
  other than `F000`?
- **#31a — `T130.MPD` shows no disconnect grant.** No literal `0xC0`
  (`IDENTIFY | DiscPriv`) anywhere, no `0x40` OR'd into a message byte.
  Indicative, **not proof** - the value could be computed. The definitive
  test is dynamic.

## Method corrections - both change what the issues asked for

- **#29 A5 is impossible as written.** The 8237A's mask and mode registers are
  **write-only**, as are the XT's 74LS670 page latches. Channel assignments
  cannot be read back on a 5160. Status port `0x08` read `0x81` twice: bit 0
  is refresh on channel 0, bit 7 is a channel-3 request that almost certainly
  reflects an **undriven DREQ3 floating**, since nothing on this machine uses
  channel 3. Recorded as inconclusive.
- **#29 A4 cannot use the floppy.** A track read is rotation-limited - about
  4,608 bytes per 200 ms revolution, roughly 43 us/byte - so it measures the
  disk spinning, not bus transfer cost. ⭐ **The owner's suggestion is the way
  in: the Sound Blaster Pro's DMA channel 1 transfers asynchronously**, so a
  PIT-timed CPU loop can be compared with DMA running and idle. That measures
  bus cycles stolen, which is what the cost model actually ranks. The floppy
  can never do this because BIOS blocks until the read completes.

## State of the machine

⛔ **DEBUG is wedged at its `-` prompt** and needs a physical Ctrl+C or a
reboot. `DEBUG < file` redirects stdin from the file, so at EOF no keystroke
can reach it - see [[comrade-debug-redirect-wedges]]. COMrade is unaffected;
`io_in`, `mem_read` and `dos_status` need no shell.

A3 (can the 8237 reach the 384 KB served from the Inboard?) is **still
unrun**. Rebuild it as a `.COM`, not a DEBUG script. Note the confound: DOS
INT 25h and BIOS INT 13h both pass through `INBRDPC.SYS`, which may
bounce-buffer through low memory and mask the answer. A failure would be
decisive; a success would not.

---

# Evening session 2: EGACACHE closed, BackPack opened

## ⛔ EGACACHE does nothing — lever closed, measured

A/B on the real 5160 with `PC000.COM`:

| | byte | word | dword |
|---|---|---|---|
| baseline | 1746 | 1476 | 1340 |
| `EGACACHE` on the `INBRDPC.SYS` line | 1746 | 1478 | 1336 |

±2 ticks is this harness's own noise. `0xC0000` stays at **2.858 us/byte**.

Both preconditions verified, so it is a result and not a failed test:
`CONFIG.SYS` line 1 really is `DEVICE=c:\INBRDPC.SYS EGACACHE NODIAGS NOPAUSE`,
and `C000:0000` reads `55 AA 40` - a real 32 KB option ROM. ⛔ Do not
re-propose EGACACHE without a new mechanism to test.

**Shadow track is closed**: five regions measured, only `0xF000` is shadowed,
and no configuration on this machine moves an option ROM off the bus.

## ⭐ The probe harness that made it possible

`tools/gen_memwidth_com.py` emits a **`.COM`**, not a DEBUG script, and leaves
its three PIT deltas at `0040:00F0` so the console is never needed to read
them. A DEBUG script wedged the 5160 twice in one session with no remote
recovery. Cross-validated: it agreed with the older DEBUG-script harness to
**2 ticks on all three widths** against an independent ROM.

## BackPack (#37) - phase 1 done offline

`tools/dosdrv_disasm.py` full-sweeps a raw DOS driver, emitting `db` and
resyncing instead of stopping at the first undecodable byte - the trap
recorded at the head of `SD120PPD_SYS.asm`, where an earlier dump silently
covered 21% of the file.

`BPCDDRV.SYS` (53,090 B, v4.02.CB): **23,591 instructions, 48 `db`** (99.8%
decoded), **1,291 I/O instructions** - 598 `out dx,al`, 394 `in al,dx`, first
cluster at `0x09F6`.

🔑 **It writes `0x22` (x42) and reads `0x23` (x23)** - the ports that alias
onto the 8259 on an XT bus, the same mechanism that made the LS-120 vendor
driver kill the keyboard. Harmless for 86Box modelling; relevant if a
BackPack is ever put on the 5160.

⭐ Owner's correction, and it reshapes the plan: **BackPack is not XT-specific,
so the whole model can be built and evaluated in a VM** on a stock machine
with an ordinary parallel port - no 5160, no Inboard, no floppies. The
physical drive becomes confirmation, not a prerequisite: `paride/bpck.c` plus
the vendor driver's own validation checks supply the expected responses, which
is exactly how the EPAT's chip-version check was found.

## Next

- CF comes local → add `/fe` to the INF `AdapterSettings` for EPP.
- #29 needs an **SB Pro DMA** harness written before any more machine time.

---

# The standard for the parallel-bridge work

Owner, 2026-09-20: *"it's not linked to an xt and the inboard or project... if
we can test and it works in vm then it's a pass"*.

**None of the parallel-port work is Inboard-specific** - not the EPAT bridge,
not EPP, not BackPack. Framing it around this project is a weakness, not
provenance:

- a reviewer should not have to evaluate an Inboard-equipped XT to judge a
  parallel device
- the XT bed carries confounds that have nothing to do with the bridge - 8259
  aliasing, a 4.77 MHz bus, the Inboard's own quirks
- a **stock emulated PC with an ordinary parallel port** is both the cleaner
  claim and the cleaner test

PR #8010 edited accordingly, in one batched edit: the `ibmxt_inboard386`
reference and the "author's own machine" sentence are gone.

## What this means for testing

⭐ **Acceptance is a generic VM**, not the 5160 and not the Inboard bed. The
cheapest route with assets already held: boot the existing disk image to
**DOS** on a generic machine type, load the vendor driver, and look for the
drive. No Windows re-detection, no Inboard, no floppies.

Applies equally to BackPack (#37) and to the EPP wiring (#38, branch
`epat-epp`, `d38a636c3`).

## ⛔ EPP is implemented and is NOT the blocker

Direct test against a registry that forces `/fe`: **zero EPP lines**. The
driver stops after the CPP chain scan (`unit 0 -> FFAA`, `1-7 -> 0000`) and
never issues `CONNECT`. Three theories for this one symptom have now been
wrong - trace build, then EPP, now the scan - and only this elimination was
by test rather than argument.

Next, both offline: what the vendor driver compares the scan ID against
(`SD120PPD_SYS.asm` at `0x2767` stores it at `[0xbbe]`), and ruling out the
boring explanation that the bed's LPT address does not match `PORT=0x378`.
