# Next session - from 2026-10-20

Supersedes `docs/next_session_2026_10_19.md`.

## Done 10-20

- **DIBENG polygon fault fixed** (`097fc8a`). GDI fills polygons with `Output` style 5,
  `OS_POLYSCANLINE`, and a NULL clip. Our `Output` gave it a full-screen strip, shifted its records as
  points, and read `wCount` as points; it counts records (each: n, top, bottom, n x values, n). The
  DIB Engine does not clip scan rows to the device, so it wrote past the strip's selector limit
  (fault EDX = 268 x 800, EDI = EDX + 421: screen pixel 421,268). Found by a tagged copy of the last
  `Output` call in driver data, read from the faulted guest with `find_memory` (86box-mcp). M8BENCH
  now runs all 35; tests 31-35 hash equal to ATI's. The same bounding for `OS_SCANLINES` is in but
  no bench test sends that style.
- **Bench bed on the MCP:** `vm_5160_mcp` (copy of `vm_5160_now`, gitignored), build `86box_auto`
  branch `auto-irq9` = master `5468d5952` + `ef082884b` (IRQ 9 delivered as IRQ 2 on the XT; without
  it the 3C509B never interrupts). Image holds the plain `097fc8a` build. Run `mcp6` (diag build, same
  logic): `C:\M8BENCH.TXT` in the image; polygon 11.3/s, floodfill 1.9/s, ellipse 14.5/s.
- **V9x Remote Agent** (Michael Dale) installed in `vm_5160_mcp`: 3C509B + TCP/IP added by the owner,
  SLiRP forward 9869, host firewall blocks 9869 inbound. `v9x` MCP connected: ping, info, shell work.
  Boot shows a Microsoft Networking password prompt (Client for Microsoft Networks); Enter clears it.
  Removing that client would make boots unattended.
- Issue #50 posted: BlueSCSI as a SCSI bridge (storage, DaynaPort, USB), parked.

## Next, in order

1. **New baseline on `86box_auto`**: M8BENCH under ours and under ATI's on `vm_5160_mcp` (driver switch
   `C:\ATIM8.REG` / `C:\M8DISP.REG`), so speeds compare on one build. Results via `v9x_get_file
   C:\M8BENCH.TXT`, then `tools/m8_bench_compare.py`.
2. **Cheap 32-bit hashing** for the string and bitmap caches (bmp_card, text_sys).
3. **Brushes and dashed lines on PATT_DATA** (fill_hatch, lines_dash).
4. Polygon/ellipse/floodfill speed: they fall back through strips (polygon 0.1 of ATI's old figure).

## Driving the bed

`vm_start` with `vm_dir` = `vm_5160_mcp`, `window=True`; step ~120 s to the desktop. Start menu: press
`ctrl+esc`, step 3 s, then `r` (keys sent too early open Help or the Task List). After boot `release`,
then `v9x_*` tools work. A stuck run: the fault dialog's Details give the registers.
