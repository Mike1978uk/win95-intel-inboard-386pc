# Next session - 2026-10-04

Supersedes `next_session_2026_10_03.md` (still the full outstanding list: sections 2, 3 and 6
there stand). This is what changed on 2026-10-03 and what to do first.

## Done 2026-10-03

- **`DMABufferSize=64` removed** from the card's `SYSTEM.INI` (default 16 KB; `DMABufferIn1MB=True`
  kept). Revert: `copy C:\WINDOWS\SYSTEM.BSR C:\WINDOWS\SYSTEM.INI`; pre-SHADRAM copy is `SYSTEM.B02`.
  Locked memory less cache ~40 KB lower - consistent with 48, not a clean measurement.
- **Read-ahead OFF** on the card, Hard Disk and CD-ROM (owner). Frees no locked memory
  (1,260 -> 1,264 KB). Registry starting values: `docs/settings_review_2026_10_03.md`.
- **SHADRAM: 0 of 16 pages on the 5160.** `PHYSPROBE` (unreal-mode DOS probe, `tools/physprobe/`)
  shows why: after boot the card keeps its reserved 128 KB out of the CPU's map - `0x5E0000` and
  `0x5F0000` read FF and writes do not stick, also with port `A0h` bit 7 set. The BIOS copy is
  served at `F0000`. `docs/shadram_5160_2026_10_03.md`.
- **#34 closed** at 800x600 (owner's decision, benchmark not run).
- Issues posted: #29, #35 (twice, latest has the probe result), #41, #45 status blocks; #48 comment.
- Checklist item 1 done (`C:\MEMMIKE.TXT`, 2026-09-29: INBRDPC 3,760 B, 578 KB free; video ROM
  `55 AA 40`).
- COMrade95 staged at `C:\CMR95\COMR95.EXE`, MCP `comrade95` registered; one bridge on COM2 at a
  time. Not run yet.

## ⚠ Alert, not fixed (owner's call)

`86box_full/src/device/inboard386.c` maps `0x5E0000` and `0x5F0000` permanently; the real card
does not after boot. That is why the bed gave SHADRAM 16 pages. Any bed result that depends on
that window is suspect.

## First things

1. **DMA buffer check, 5160 in Windows:** a DOS game with **sampled** sound in a DOS box
   (FastDoom, Wolf3D, Keen 4-6 - not Monkey Island, FM only), then a floppy copy. If DOS-box
   sound breaks, `DMABufferSize=32`.
2. **#45 row 3 speed:** `T130AB` + `PERFLOG` with read-ahead None (current), then Full.
3. ~~SHADRAM~~ **removed** from `SYSTEM.INI` by the owner, 2026-10-03 (it adds nothing). `SHADRAM.VXD`
   stays on `C:\` unloaded.
4. **Desk:** how `INBRDPC.SYS` opens and closes the reserved window while copying the BIOS
   (not port `A0h` bit 7 on its own). Only then is the EGA half reachable,
   and Windows would need the window left open. Not a quick lever - rank it against #46 and #41.
5. Then section 2 of the 10-03 handoff: #46 (LS-120 boot stall - the LEAN entry removes it on
   every boot, confirmed by the owner over several boots; LEAN also drops Nero, but `BOOTLOG.TXT`
   puts the wait in `sd120ppd.mpd`. The patch is still the fix), #41 re-scope incl. ELNK3 buffers,
   SWAPCOMP `HASH_LOG 10` (5160-only A/B).

## COMrade notes

- Keystroke injection timed out repeatedly on 2026-10-03; file read/write worked. Write a batch
  out-of-band, start it with one short command (or the owner types it).
- `MEM` is not on the path in a command-prompt-only boot (`CHK.BAT` produced nothing).
- Posting: one plain `gh issue edit|comment|close` per call matches the allow rules; a python
  wrapper or an `&&` chain gets blocked.

## State of the CF

`C:\PHYSPROB.COM`, `C:\CHK.BAT`, `C:\PP*.TXT`, `C:\CHKDONE.TXT` (probe and checks, all captured in
`docs/captures/2026-10-03_physprobe/`), `C:\CMR95\`. Last image `win95_shdw_swp.img` predates the
`SYSTEM.INI` edit and read-ahead change. `C:\SNP92.TMP` (400 KB) still there, deletable.

## Done 2026-10-03, evening (owner at the 5160)

- **ROMPROBE: parity error, no output.** Read never-written RAM at `5E0000` with port 670h bit 0
  clear - see `docs/romcache_decode_2026_10_03.md` 3a. `ROMPROB2` staged (`C:\RP2.BAT`), command
  prompt boot.
- **Read-ahead, hard disk:** `T130AB` on `C:` (the CF) - off 7.20 + 7.20 + 6.38 = 20.78 s, on
  7.09 + 6.43 + 6.86 = 20.38 s. No difference beyond the scatter. The owner prefers off; it stays
  off. The PERFLOG files ran after the copies (counters flat), so they hold memory state only.
  Raw: `docs/captures/2026-10-03_readahead/`. `t130ya` (`G:`, a changer LUN) failed in 0.4 s - not
  a result.
- **CD-ROM read-ahead on:** Explorer hung after a CD was inserted (once). Left at "No read-ahead".
- **DMA buffer (default 16 KB since 2026-10-03):** Wolf3D in a DOS box plays sampled sound. FastDoom
  `-xt` with sound hung the machine (#47 settings, not a verdict on the buffer). Floppy in Windows
  unreliable: `B:` copied once, then not; `A:` difficult. In DOS both work - `B:` formatted, copied
  and `FC` identical. Open: was Windows floppy reliable with `DMABufferSize=64`?
- **SAFE boot / Device Manager phantoms:** not reported yet.
