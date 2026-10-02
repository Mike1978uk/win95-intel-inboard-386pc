# Next session - 2026-10-03

Supersedes `next_session_2026_10_02.md` (swap compression: released, see there and
`docs/swapcomp_bed_results_2026_10_02.md`). This one is the full outstanding list after the
owner's 2026-10-02 brain dump, sorted by where the work runs.

## Done 2026-10-02

- `SWAPCOMP.VXD` released (`dist/swapcomp/`): -4.5% over the whole workload on the 5160.
  It is **loaded on the card now** (`DEVICE=c:\swapcomp.vxd`, left in after run B2).
- `PAGETIME.EXE` / `PAGERUN.BAT`: the timed paging workload (`tools/perflog/`).
- `SHADRAM.VXD` built and bed-tested - see below.

## 1. Shadow RAM for Windows - `SHADRAM.VXD` (stage 3b)

`drivers/shadowram/`. At `Sys_Critical_Init` it checks Windows does not own pages
`5E0h`-`5EFh` (`_GetPhysPageInfo`), pattern-tests each page, refuses any page whose writes
appear in `F0000` or `C0000` (alias of live ROM shadow) or alias each other, and gives the
rest to the VMM with `_AddFreePhysPage`. Counters `SHADRAM\PagesTested/Added/Failed/Status`.
Load: `device=C:\SHADRAM.VXD` in `[386Enh]`.

Bed result (`docs/shadram_2026_10_02.md`): Status 0, 16/16 pages added, PAGERUN clean; confirmed on a repeat (2 runs per arm): page-ins -13.8%, program open -11%, no extra locked RAM.

**On the 5160:** the address is from the emulator model and UniPCemu, not the real card. The
driver's checks make a wrong address cost the pages, not the machine - except one case they
cannot see: the window aliasing live RAM other than the two ROM areas. Image the CF first;
one boot; read `SHADRAM\*` with PERFLOG. `EGACACHE` must stay off (it uses this half).

Windows rather than DOS, as the owner asked: DOS has 578 KB free and is not short; Windows is.

⭐ **This re-ranks the RAM track.** The pageable pool is only ~1.3 MB (most of the 5 MB is
locked), so every KB returned to it is worth ~4x what its share of total RAM suggests. Raise:
`DMABufferSize` 64 -> 16 (up
to 48 KB, section 4), and the second idle 64 KB (the card's RAM under the planar's, inferred
only - `docs/inboard_memory_layout_2026_09_30.md`).

## 2. Desk and bed - can be done without the 5160

| item | what | close condition |
|---|---|---|
| **#41 driver polling** | Re-scope first: the T130B now runs on IRQ 3 (#42), so A18's "polls throughout every transfer" premise is stale. Re-read `T130.MPD`'s loops with the IRQ path in mind. A21 `ELNK3.VXD` is **unblocked** - the 3C509B is in 86Box. Static: `tools/le_poll_scan.py`, then bed | each target measured or retired |
| **#46** | find `SD120PPD.MPD`'s ~49 s wait (static), model the unpowered drive (`80h`) in the bed, patch | boot stall gone in the bed, then one 5160 boot |
| **#28** | six-question walk per driver; the locked-memory audit is done, the walk is not | the walk table filled |
| `DMABufferSize` | see section 4 - a settings A/B, not a code change | #45 row |
| #47 FastDoom | settings sweep in the bed | settings found |
| #48 | optimisation timeline; add SWAPCOMP | written |
| #10 GLaBIOS | owed to Andrew once tried: bed, 5150 and 5160 types | tried |

## 3. At the 5160 (owner)

From `docs/5160_checklist_2026_09_29.md`, still open: `MEM /C`; Network panel (the Dial-Up
Adapter is already removed - no `PPPMAC` in the card's `BOOTLOG.TXT`); RAMBASE; LS-120 `.OFF` toggle and #46's stall with
the drive off; boot menu LEAN/FULL/SAFE; #45 rows 3-4 (read-ahead, CD cache). Then #34 display
mode, #33 DRAM refresh, #31 SCSI cache pages, #43 IOCHRDY.

Housekeeping on the CF (host, with the card in the reader): 23 retired `LS120MP.*` in
`IOSUBSYS` (backed up, not loaded); `C:\SNP92.TMP` (400 KB).

Still open from before: SETVER loads; KEYB/COUNTRY warnings and the pound key in a DOS box.

## 4. SYSTEM.INI review (the card's file, 2026-10-02)

Tidy: almost everything in `[386Enh]` is earned or measured.

| line | verdict |
|---|---|
| `DEVICE=c:\swapcomp.vxd` | keep - released, -4.5% |
| `DEVICE=C:\WINDOWS\UMAXIS11.386` | scanner VxD, 9 KB locked every boot; audit said leave |
| `DMABufferIn1MB=True`, **`DMABufferSize=64`** | **the one lever.** 64 KB locked below 1 MB for VDMAD's bounce buffer; Windows' default is 16. Chosen 2026-08-23 as the largest size avoiding the 128 KB alignment rule (technique 58), not because anything needs 64. The SB Pro (patched `MSSBLST`) and floppy (`HSFLOP`) allocate their own buffers; what still bounces through VDMAD is unknown. A/B at 32 and 16: floppy copy, SB playback, LS-120 - up to **48 KB** back. Belongs on #45 |
| `ebios=*ebios`, `device=*vshare` | duplicates of registry entries, fail harmlessly in BOOTLOG (audit) |
| `Min/MaxPagingFileSize=32768` | keep - fixed size suits SWAPCOMP, whose table covers exactly 32 MB |
| `[vcache] 512-1024` | keep - measured (#45 row 1) |
| `display.drv=ATI mach8 1024x768x256 Large Font` | #34: 800x600 would cut bitmap memory and bus traffic |

Nothing else worth adding with evidence behind it: the classic tuning keys are Windows 3.1
(`PageBuffers`) or Windows 98 (`ConservativeSwapfileUsage`). `MinTimeslice`/`WinTimeSlice`
are #45's timer question.

## 5. The owner's questions, answered

**L1 cache and storage.** No gain there. Port I/O is never cached, and the transfer is
`in`/`out` against a port, so the cache cannot shorten it; the buffer side is ordinary
memory, already cached since CMLR was fixed (2026-09-12). One real L1 cost remains: **every
I/O access flushes the L1** (`docs/captures/io_cache_flush_2026_09_21.md`), and neither SW1
nor the flush-snooping bit changes it (tested 2026-09-27, `next_session_2026_09_28.md`) - no
switch or register is left to stop it. So the only cache lever is fewer I/O accesses: each
poll removed (#41) also saves a cache refill, as did the word-wide transfers already shipped. Open, measurement only: memory above the L1 got ~2.3x slower after the CMLR fix
(red-ray's latency walk); line fills are the likely reason, unproven.

**DMA for the storage drivers.** Neither storage card can use it: the XT-CF has no DMA
lines (closed 2026-09-29) and the T130B's INF declares none. The 1 MB limit is the XT's 4-bit
DMA page latch - 20 address bits - so the 8237 cannot reach 2-3 MB at all; a buffer there
would transfer against the wrong address (technique 62). It is not a limit we can carve up.
Where DMA would win is a different card: a DMA transfer does not pay the Inboard's ~3.9 us
per-access CPU synchronisation, so 8-bit DMA could beat PIO here, with the buffer below 1 MB
and a fast local copy. That, and memory-mapped cards (~2x, measured), is card-selection
guidance - #35 part 2 already tracks the memory-mapped side.

## 6. Owed to contributors (owner writes and sends)

From `docs/contributor_input_ledger.md`:

- **@andrew-hoffman** - memory-vs-I/O hypothesis confirmed (2x, measured 2026-09-20) and his
  DriveSpace note corrected a parked decision; his "save tokens" steer adopted into `CLAUDE.md`;
  GLaBIOS (#10) once tried. And his #35 point - *"even 128k of unusable memory could make a big
  difference"* - is what `SHADRAM.VXD` acts on: tell him once it runs on the 5160.
- **disruptor** (VOGONS) - the DMA-reach warning for their ST01 work.
- **red-ray** (SIV) - the PM reply plan in memory (8259 alias on ports 22h/23h); not sent.
- **Michal Necasek** - a note is listed as owed; owner's call whether he is a contact.

No unanswered GitHub comments: Andrew's last two (#35, #42) both have replies.

## 7. Issue updates proposed (owner approves wording before anything is posted)

- **#35**: status block - SHADRAM built and bed-tested, 5160 next.
- **#41**: status block - A18 premise stale since #42 (IRQ 3), A21 unblocked.
- **#45**: new row - `DMABufferSize` 64 -> 32 -> 16.
- **#48**: add SWAPCOMP (-4.5%).
- No new issue needed: every item above fits an existing one.
