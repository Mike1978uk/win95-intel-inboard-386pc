# Fable review, Part 2: swapcomp - 2026-11-13

Brief: `docs/fable_driver_review_brief.md`. Reviewed: `drivers/swapcomp/src/SWAPCOMP.ASM` as built by
target `SWAPZF` (`-DSC_ZERO -DSC_FOLD`) of `drivers/swapcomp/build.ps1`. Baseline build in this worktree:
21,332 bytes, md5 `C2585E38...`, the card's file. Line numbers are this branch's head.

Method: source read against the DDK's own pager source (`BASE\SAMPLES\DYNAPAGE\PAGEFILE.ASM`) and
`PageSwap.Inc`; the LE object table of the built file; the repo's captures. Nothing was run: no bed, no card.

**No correctness defect found.** One RAM fix committed, one comment fix committed, one gap in the
evidence, four levers that need a measurement first.

## Findings

| # | Kind | Finding | Outcome |
|---|---|---|---|
| 0 | Known | The card's file is not in `dist/` | none: waits for the soak |
| 1 | Evidence | Page integrity of `SWAPZF` is proven in the bed only | probe |
| 2 | RAM | Locked object 32 bytes over four pages | fixed, `e020fcc` |
| 3 | Comments | Two comments wrong or unclear; two more noted | fixed, `832bb5d` |
| 4 | RAM | `SlotTab` at 4 bits a slot would free another page | probe |
| 5 | Bus | Pages that compress to 7 sectors are stored raw | probe |
| 6 | CPU | An incompressible page pays a full compress, then a raw write | probe |
| 7 | CPU | `LZ_Hash` is a call per position; loop limits live in memory | probe |

### 0. The card's file is not in `dist/` (known)

`dist/swapcomp/` holds `SWAPCOMP.VXD` `9fde4934` and `SWAPCMPC.VXD` `537c5d9a`; the card runs `SWAPZF`.
Recorded in the brief and in `docs/captures/2026-11-08_tunnel/README.md` ("dist/ waits for that
measurement"). No action here.

### 1. Page integrity of `SWAPZF` on the 5160 is not shown

- `SWAPZFC` (checksum build) ran in the bed only: 25,016 reads, ChecksumErrors 0
  (`docs/captures/2026-11-07_r892/README.md:86`).
- On the card no checksum build has run: run 0 was skipped on 2026-10-02
  (`docs/swapcomp_bed_results_2026_10_02.md`, "Not shown: page integrity under the release build").
- The fold hash relies on one guard that the cleared-table build does not need
  (`SWAPCOMP.ASM:559-563`: a stale entry must point behind the current position). The guard reads
  correctly; every candidate is then compared byte for byte.
- Probe: one PAGETIME loop on the card with `SWAPZFC.VXD` in place of `SWAPCOMP.VXD`, then read
  `SWAPCOMP\ChecksumErrors`, `Errors`, `Fallback`. Cost: one restart each way, +16 KB locked while it runs.
  Fits inside the pending soak.

### 2. Locked object 32 bytes over four pages - fixed, `e020fcc`

- Evidence: LE object 1 (locked code and data) of the card's file is 4020h = 16,416 bytes: five 4 KB
  pages. `SWAPZF.map`: `_LDATA` 3A6Fh, `_LTEXT` 5AFh.
- `SC_Bounce` was 4,352 bytes. The worst case it must hold is 2 + 4096 + 4096/255 + 16 = 4,130
  (this encoder: at most 4,116), and never less than 4,096, because PAGEFILE moves a whole page through
  it when the IFS hook does not shorten the transfer.
- Fix: 4,160 bytes (`SWAPCOMP.ASM:132-137`). Object 1 is now 3F60h = 16,224 bytes: four pages.
  `SWAPZF.VXD` 20,820 bytes, md5 `07154eb7...`.
- Expected gain: 4 KB less locked RAM. Assumes the VxD loader rounds each object to 4 KB pages; the
  2026-10-02 card figures agree (+20 to +24 KB for a 22,452-byte object).
- Not measured. Confirm with `VMM\cpgLocked`, first PERFLOG sample, old file against new.
- All six targets build. No code changed, only the buffer's size.

### 3. Comments - fixed, `832bb5d` (no code change, same bytes)

- `SWAPCOMP.ASM:446-447` said the LZ4 code is "the same algorithm as LZ4BENCH.C, which round-tripped real
  swap pages on the 5160". The card's `SC_FOLD` build differs from `tools/swapcomp/LZ4BENCH.C` in the hash
  and in never clearing the table. Reworded to say so.
- `SWAPCOMP.ASM:413-414`, on a read the IFS hook did not see: "usable, but wrong" gave no reason.
  Failing is right: under the Int 21h pager PAGEFILE maps the page that contains the buffer and reads to
  its start, not to `SC_Bounce`. Reworded.
- Noted, not changed:
  - `:4` "Stage 3 of swap compression" is project history; the doc link beside it is enough.
  - `:5-7` and `:24-25` carry bench figures (4.71 ms, 17 %). They state the trade the code makes, so they
    can stay, but `:25` names `LZ4B10FN`, a bench whose source is not in the repository
    (`tools/swapcomp/` has `LZ4BENCH.C` only). Either add the source or drop the name.
- No comment describes another device or dead code.

### 4. `SlotTab` at 4 bits a slot - proposal

- `SWAPCOMP.ASM:127`: 8,192 bytes, one per slot, holding 0-6 or FFh. Four bits hold it (0-6, 7 = zero page).
- At 4,096 bytes the locked object is ~12,130 bytes: three pages, 4 KB below finding 2.
- Cost: a shift and mask at six sites in `SC_One`; a few clocks per page moved, against milliseconds of bus.
- Not committed: it rewrites the table every page-in depends on. It needs a `SWAPZFC` loop in the bed
  (ChecksumErrors 0) before the card sees it.

### 5. Seven-sector pages are stored raw - proposal

- `SWAPCOMP.ASM:64, 342-343`: stored compressed only at 6 sectors or fewer.
- A 7-sector page saves one sector on the write and on every read, and costs one decompress per read
  (~1.1 ms, less for a page that is mostly literals). One sector is 1.3 ms at 10.4 ms a page, or 1.96 ms by
  the figure in `docs/captures/2026-11-05_lz4hash/README.md`. At 1.3 ms the read is about level and the
  write gains; at 1.96 ms both gain.
- On the card raw pages are 27 % of swap reads (1,017 of 3,706, B1 run) and are read about four times
  per write, so the read side decides it.
- Probe, host only: `tools/swapcomp/sectors.py` on a copy of the card's `WIN386.SWP`, counting raw slots
  that LZ4 would bring to exactly 7 sectors. If they are few, drop this. If many, build with
  `SC_MAX_SECTORS` 7 and A/B it in the soak.

### 6. Incompressible pages pay twice - proposal

- `SWAPCOMP.ASM:336-343`: the page is compressed in full (~4.2 ms) before the size test sends it raw.
- Raw writes on the card: 258 of 2,109 (12 %, B1). About 0.5 ms a page written, averaged.
- Lever: give up once literals so far plus output exceed 6 sectors. The check sits in the per-position
  path, so it may cost the other 88 % more than it saves.
- Probe: a bench variant over `SWAPPAGE.BIN` on the card, as the hash variants were measured.

### 7. Compress inner loop - proposal

- `SWAPCOMP.ASM:550`: `call LZ_Hash` per input position; `lz_mflimit` and `lz_src` are memory operands
  (`:547, :553, :564`). A call and return per position is roughly 5-10 % of a page's compress time.
- Page-outs weigh 0.35 of a page-in, so the whole-machine gain is near 1 %.
- Not committed: small gain, and the loop has only bed evidence behind it. Bench first, as in finding 6.

## Checked, nothing found

1. **Contract.**
   - Descriptor (`PageSwap.Inc`): `Cmd`, `nPages`, `Buffer_Ptr`, `File_Page` are copied per page; `Next`
     and `Priority` are ignored by PAGEFILE. PAGEFILE writes `Status` on every path, so the stale value in
     `SC_Desc` is never read.
   - Registers: the hook saves all, as the service does ("USES: Flags").
   - Requests of more than one page are split; SWAPCNT measured 1.00 page per call, so the split costs nothing.
   - Retail PAGEFILE does not check the byte count returned (debug builds only), so the shortened
     transfer is accepted.
   - Transfers are whole sectors, as `R0_NO_CACHE` needs.
   - Failure paths: no semaphore, or a failed hook, leaves Windows paging as stock. The IFS hook goes in
     first. A failed compressed write leaves the slot marked raw and returns failure. An I/O error on a
     read returns failure. A write the IFS hook did not see is rewritten whole and compression stops.
   - Int 21h pager (not the card's): it never reaches the IFS hook, so the first write falls back and
     no compressed slot ever exists.
   - `SlotTab` is cleared before every write of a slot, so no path leaves a stale compressed or zero mark.
   - LZ4: output bound, the 16-bit position table, the stale-entry guard, the backward extension, and
     every bound in the decompressor (literal and match lengths, offset, input end) read correctly.
2. **DMA reach.** The driver adds no DMA. Its buffer goes to IFS like any page; the swap disk is the
   XT-CF, PIO only.
3. **XT port aliasing.** No port I/O in the file.
4. **Interrupts and reentrancy.** Nothing runs at interrupt time. One semaphore serialises the hook;
   Contention is 0 in every capture. The IFS hook acts only while our transfer is in PAGEFILE and only on
   our buffer, which no other caller can pass. All code and data on the paging path are locked.
5. **Time on the bus.** No IN or OUT, so no L1 flush of ours. The IFS hook adds two instructions to
   other callers' file I/O. The fold build's working set (4 KB page, 2 KB table, output) fits the L1.
6. **Comments.** Finding 3.

## Not checked

- The retail `PAGEFILE` inside the card's `VMM32.VXD`: the DDK sample source was read instead.
- A swap file that changes size, or one over 32 MB (pages past slot 8,192 go raw by design). The card's is
  fixed at 32 MB.
- The `SWAPCOMP`, `SWAPCMPC`, `SWAPZ` and `SWAPZFC` targets beyond building them; `SWAPCNT.ASM` beyond
  reading it.
- Two copies loaded at once (`SWAPCMPC.VXD` is on C:\ but not in SYSTEM.INI).
- The PERF registration structures against `VXDS.DOC`; they match SWAPCNT's, whose counters appear.
- Any run. `e020fcc` is built, not booted.
