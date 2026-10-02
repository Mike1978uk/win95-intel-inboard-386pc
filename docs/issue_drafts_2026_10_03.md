# Issue updates - 2026-10-03 (approved by the owner; not yet posted)

Each status block replaces the existing `> ### Status` block at the top of the issue body.
Posting from the session was blocked by the tool's permission check, so the owner posts these.

## #29 - status block

> ### Status — 2026-10-03: reopened, one question open
> - **Open: can a DMA transfer land in 0-64 KB, and which copy does it reach?** The Inboard's RAM answers the CPU there; the planar's 64 KB sits hidden behind it. The 8237 runs on the XT bus and may reach the planar copy instead. Nothing seen lands there yet (SB Pro buffer at `0x09xxxx`). [comment](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/29#issuecomment-5959939970)
> - **Next:** where VDMAD's buffer sits (now 16 KB, `DMABufferSize` removed, #45 row 7); then, if anything can land below 64 KB, one DMA read there on the 5160.
> - Settled 2026-09-29: SB and floppy DMA reach the Inboard's conventional memory; channels 0 refresh, 1 SB Pro, 2 floppy, 3 parallel card; the XT-CF has no DMA; items 2 and 4 not planned.
> - Thanks to @andrew-hoffman, whose point that the parallel card claims channel 3 was right.

## #35 - status block

> ### Status — 2026-10-03: SHADRAM loads on the 5160; page count not yet read
> - **`SHADRAM.VXD` gives the idle 64 KB EGA half (`0x5E0000`) to Windows** as 16 pageable pages, acting on @andrew-hoffman's point that even 128k could make a big difference. Bed, two runs per arm: 16/16 pages, page-ins -13.8%, programs open 11% faster. [`docs/shadram_2026_10_02.md`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/docs/shadram_2026_10_02.md)
> - **5160:** loads and initialises, Windows normal. Pages added not read yet; that confirms the address on the real card.
> - **The other 64 KB, under the planar: not reclaimable** - it is the planar's own RAM behind the card. [`docs/inboard_memory_layout_2026_09_30.md`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/docs/inboard_memory_layout_2026_09_30.md)
> - Part 1 closed (only `F000` shadowed). `EGACACHE` stays off: SHADRAM uses that half. The `D0000h` window stays enabled. Part 2 open, low priority.

## #41 - status block

> ### Status — 2026-10-03: re-scope before measuring
> - **`HSFLOP.PDR` (A17): not a target.** Completion comes on IRQ 6. `docs/hsflop_poll_audit_2026_09_27.md`.
> - **`T130.MPD` (A18): premise stale.** The T130B now runs on IRQ 3 (#42), so "polls throughout every transfer" no longer holds. Re-read its loops with the IRQ path in mind before measuring.
> - **`ELNK3.VXD` (A21): unblocked** - 86Box now models the 3C509B. Its buffers (22 KB locked) go in the same pass.
> - The LS-120 driver's boot-time wait on an unpowered drive also costs bus cycles; tracked as #46.

## #45 - status block

> ### Status — 2026-10-03: rows 1, 2, 5, 6 settled; rows 3, 4, 7 open
> - Row 1, VCACHE cap: kept (copies 8% faster). Row 2, auto insert notification off: kept. Row 5: stays on Network server. Row 6, the 1 ms tick: no gain, not adopted.
> - **Row 3, hard-disk read-ahead:** at the default (full). A/B at None next.
> - **Row 4, CD-ROM cache:** CDFS `CacheSize` 32, `Prefetch` 27 on the card. The drives buffer in their own RAM, so Windows' read-ahead may spend bus time and memory for nothing. A/B at "No read-ahead".
> - **Row 7, new: `DMABufferSize=64` removed** (Windows default 16 KB; 48 KB less locked below 1 MB). The 64 came from an AI suggestion, never from a need. `DMABufferIn1MB=True` stays: the 8237 reaches only the first 1 MB. Check: SB sound in a DOS box, a floppy copy, locked memory.
>
> Figures: [comment](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/45#issuecomment-5860684281), `docs/settings_review_2026_10_03.md`.

## #48 - comment

Entry to add: **`SWAPCOMP.VXD`, 2026-10-02.** Compresses swap pages before they reach the CF. Paging workload 4.5% faster on the 5160 (runs A1 B1 A2 B2, every step faster), 2.64:1, no errors, 22 KB more locked. [`dist/swapcomp/`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/dist/swapcomp), [`docs/swapcomp_bed_results_2026_10_02.md`](https://github.com/Mike1978uk/win95-intel-inboard-386pc/blob/master/docs/swapcomp_bed_results_2026_10_02.md).

`SHADRAM.VXD` follows once it is measured on the 5160 (#35).
