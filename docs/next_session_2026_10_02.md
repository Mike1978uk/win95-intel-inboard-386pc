# Next session - 2026-10-02

Supersedes `next_session_2026_09_30.md`. Findings and numbers:
`docs/magnaram_and_swap_compression_2026_10_01.md`.

## Where it stands

- **MagnaRAM 97: rejected.** Bed A/B: workload 8.9 min against 4.4, +0.5 MB locked.
- **Swap compression is the track.** Measured so far:
  - 62% of page-ins come from the swap file (SWAPCNT, bed), one page per call.
  - Real swap pages compress 2.26:1 with plain LZ4 (LZ4BENCH, 5160).
  - On the 5160's CPU: decompress 1.17 ms, compress 4.71 ms per page. Bus: ~10.4 ms per page.
  - Estimate: -36% per swap page-in, ~-25% of all paging time. **Not yet measured end to end.**
- **`SWAPCOMP.VXD` is built and has never run.** `drivers/swapcomp/`, `build.ps1` makes:

| file | what | use |
|---|---|---|
| `SWAPCNT.VXD` | counts swap I/O, changes nothing | the share of paging that is swap |
| `SWAPCMPC.VXD` | compression + a checksum per page | **first runs: bed, then 5160** |
| `SWAPCOMP.VXD` | compression | the timed A/B |

  Each page keeps its 4 KB slot in `WIN386.SWP`; only its compressed sectors are written. A
  RAM table (8 KB) says how many. PAGEFILE still does the I/O; a hook on
  `IFSMgr_Ring0_FileIO` shortens the transfer when it sees our buffer. If that hook ever misses
  a compressed write, the page is rewritten whole and compression stops for the session
  (`Fallback` counter). ~22 KB locked.

## ▶ Done 2026-10-02 - sections 1 and 2

Bed correctness **passed** and the stopwatch exists. In the bed SWAPCOMP is **slower**
(+16-30% per step): the bed charges the compression CPU in full and does not model the 5160's
bus cost, so only the 5160 can show a win. Numbers: `docs/swapcomp_bed_results_2026_10_02.md`.
Next is section 3, with the steps updated below.

## 1. Bed correctness (emulator, ~30 min, nothing at the 5160)

Bed `vm_magnaram_off`: RAMBASE autoruns from StartUp with teardown; PERFLOG logs every
counter. It currently loads `C:\SWAPCNT.VXD`.

1. Copy `build/SWAPCMPC.VXD` to the image (`tools/fatcp.py`), and in `SYSTEM.INI [386Enh]`
   replace `device=C:\SWAPCNT.VXD` with `device=C:\SWAPCMPC.VXD`. One driver at a time.
2. Delete `C:\PERFLOG.CSV`, boot, let RAMBASE finish, shut down.
3. **Pass:** boots; RAMBASE says done; clean shutdown; and in the CSV
   `SWAPCOMP\CompressedWrites` and `CompressedReads` > 0, `Errors` = 0, `ChecksumErrors` = 0,
   `Fallback` = 0.
4. Run it twice more (warm boots). Then once with a heavier load (open extra programs by
   hand while it runs) to page harder.

| if | then |
|---|---|
| `Fallback` > 0 | retail DYNAPAGE does not do its I/O the way the DDK sample does; redesign the I/O path |
| `ChecksumErrors` > 0 | a real corruption bug; do not go near the 5160 |
| hang at boot or first page-out | likely the semaphore in the pager path; read the screen, then the source |

## 2. Build the stopwatch first (desk, ~1 h)

RAMBASE waits fixed times (SLEEP 40/30/15/30), so its wall time cannot change, and its
copies do not page. It shows page **counts**, not speed. The A/B needs a clock:

- **`PAGETIME.EXE`** (same toolchain as PERFLOG): starts PSP, WordPad, Notepad and Paint one
  after another, each with `CreateProcess` + `WaitForInputIdle`, writes ms per program and the
  total to `C:\PAGETIME.TXT`, then closes them (as WINCLOSE does). Run it from RAMBASE in
  place of the START/SLEEP block, or on its own, first after boot.
- Optionally a second pass that switches back to each program in turn and times it: with all
  four open, that is where swap is read.

## 3. The A/B on the 5160

**Before the first run: image the CF on this PC.** The swap file is rebuilt every boot, so the
driver cannot damage files by design, but a crash during any disk write can hurt FAT.

**Copy to `C:\` from the card reader:** `SWAPCMPC.VXD`, `SWAPCOMP.VXD`
(`drivers/swapcomp/build/`), `PAGETIME.EXE` (`tools/perflog/build/`), `PAGERUN.BAT`
(`tools/perflog/`). PERFLOG, SLEEP and WINCLOSE are already on the card. The workload is
`PAGERUN <label>` from an MS-DOS Prompt, first thing after boot, hands off.

**Run 0, safety (checksum build), once:** `device=C:\SWAPCMPC.VXD`, `PAGERUN S`, check
`ChecksumErrors` = 0, `Errors` = 0, `Fallback` = 0. Only then the timed runs. Run 0 is also the
first test of PAGETIME's WinZip steps (the bed had no WinZip): if the licence dialog is not
pressed by itself within a few seconds, press I Agree by hand and note it; the timed runs then
need that fixed or the zip steps dropped.

**Timed runs, release build `SWAPCOMP.VXD`.** The owner edits `SYSTEM.INI` between runs; the
only difference between arms is the one `device=` line (put a `;` in front for A).

| rule | why |
|---|---|
| same boot entry every run (FULL), same card, swap fixed at 32 MB | one variable |
| first workload after boot, hands off | the 09-29 run 2 showed what a second run does |
| order **A B A B** (4 boots), better A B B A A B | drift, warm-up and cache state cancel |
| report each run and the median, never the best | |
| note anything that changed between runs | |

Collect per run: `PAGETIME.TXT`, `PERFLOG.CSV` (page-ins, locked, SWAPCOMP counters),
`BOOTLOG.TXT`. Summarise with `tools/perflog/perflog_table.py`.

**What counts as a result:**
- **Win:** program-open total lower with SWAPCOMP in every pairing, by more than the spread
  between the two A runs. The estimate says ~25% of paging time.
- **Cost check:** locked memory up by ~22 KB and no more; page-in count about the same in
  both arms (compression changes the cost of a page-in, not the number). The bed showed +13%
  page-ins with SWAPCOMP on one run per arm - check whether the 5160 repeats it.
- **No win:** record it in `what_worked_and_what_didnt.md` in one line, with the numbers.

## 4. Publishing (after the A/B, owner verifies by hand)

`dist/swapcomp/`: `SWAPCOMP.VXD`, a README (what it does, the one SYSTEM.INI line, how to
remove it, what was tested on what, what was not), `FIXES.md` entry. Release only after the
owner has run it on the 5160 (G10-style hand check).

## 5. Later

- **Stage 3b - the idle 64 KB, given to Windows.** The EGA half of the Inboard's reserved
  shadow block is RAM Windows never gets (`docs/inboard_memory_layout_2026_09_30.md`). Better
  than parking SWAPCOMP's ~22 KB there: donate all 16 pages to the VMM free pool with
  `_AddFreePhysPage` (VMM.INC, init-time only), so every driver and program gains 64 KB. Same
  for the second 64 KB (the card's RAM under the planar's), which is only inferred. The address
  (0x5E0000) is from the emulator model and UniPCemu, not the real card: pattern-test every
  page at init, donate only pages that pass. Expected gain ~1% of page-ins per 64 KB (the
  layout doc's estimate) - small, but the VxD to carry it now exists. The owner's question: could DOS TSRs use it too? Only if the card can map it below
  1 MB as an upper memory block; and conventional memory is not the constraint (578 KB free,
  DOS drivers 63 KB), Windows' RAM is.
- **SETVER still loads** after `144afc9`: IO.SYS loads `C:\WINDOWS\SETVER.EXE` whenever it
  exists. Owner's call: rename the file, revert, or leave. Not touched.
- Boot-file trim checks still open: KEYB/COUNTRY warnings on screen, the pound key in a DOS box.
- #46 (the ~49 s LS-120 wait) and #41 remain.

## Beds

`vm_magnaram/` (MagnaRAM installed) and `vm_magnaram_off/` (control: SWAPCNT in SYSTEM.INI,
`CTL3D.DLL`, `[vcache]` 512-1024, RAMBASE StartUp entry ending in PAUSE). Both ignored by git.
Any new bed from `vm_vpicd_irq2` needs `CTL3D.DLL` and the `[vcache]` cap the real card has.
