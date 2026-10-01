# Next session - 2026-10-02

Supersedes `next_session_2026_09_30.md`. Findings: `docs/magnaram_and_swap_compression_2026_10_01.md`.

## Done this session

- **MagnaRAM 97: no.** Bed A/B, same image: workload 8.9 min against 4.4, +0.5 MB locked,
  CPU pinned, guest clock stalled. Not going on the 5160.
- **Swap compression: the direction.** The 5160's `WIN386.SWP` compresses 2.1:1 with a simple
  LZ (WKdm 1.5:1, deflate 2.9:1). Under Windows the bus costs per byte, not per command
  (#30), so cutting bytes is the only lever left on the paging path.
- **Stage 1 built and run: `drivers/swapcomp/` (SWAPCNT.VXD).** Hooks `PageFile_Read_Or_Write`
  and counts. Bed: 62% of page-ins come from swap, one page per call, each written page read
  ~2.9 times. Boots and runs clean in the bed. **Not yet on the 5160.**
- `RAMBASE` now tears down after itself (`WINCLOSE.EXE`); copied to the CF.
- Swap file: keep 32 MB fixed. The 09-30 "8 MB" advice was wrong (sized from pages written,
  not commit).

## Unpushed (owner's go-ahead)

`144afc9` boot-file trims (held for the 5160 FULL boot), `7220494`, `6a1b7ee`, `5e6fe61`,
`34d2c03`.

## Next, in order

1. **5160 FULL boot** with the trimmed boot files - the `144afc9` checks (menu, no KEYB or
   COUNTRY warning, the pound key in a DOS box, `BOOTLOG.TXT` without DISPLAY.SYS, SETVER,
   DBLBUFF). Then push.
2. **SWAPCNT on the 5160** (optional, ~10 min): same `RAMBASE`, to get the real swap share.
   Add `device=C:\SWAPCNT.VXD` to `[386Enh]` - the owner edits SYSTEM.INI.
3. **Stage 2:** LZ4-class decompression timed on the Blue Lightning - a DOS program over real
   swap pages, PIT-timed. Kills the idea if a page costs more than ~5 ms.
4. **Stage 3:** the compressing hook. The hard part is a page-to-location map and its own file
   I/O, because compressed pages are not 4 KB slots. Checksum every page. Bed first.
5. #46 (the ~49 s LS-120 wait) and #41 are still open; the owner chose swap compression first.

## Beds

`vm_magnaram/` (MagnaRAM installed) and `vm_magnaram_off/` (control, now also carries
SWAPCNT in SYSTEM.INI, `CTL3D.DLL`, the `[vcache]` cap and a RAMBASE StartUp entry). Both
ignored by git. The bed image lacked `CTL3D.DLL` and the 512-1024 KB VCACHE cap that the
real card has - copy both into any new bed built from `vm_vpicd_irq2`.
