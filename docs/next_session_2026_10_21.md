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

- **Shutdown BSOD fixed** (fatal exception 0D at `xxxx:30B7`, twice, on reboot under ours). GDI
  still calls `BitBlt` on the screen after `Disable` has set `m8_screen` NULL; `BitBlt` read
  `m8_screen->deFlags`. Every screen entry now returns when `m8_screen` is NULL, as
  `SaveScreenBitmap` already did. Two clean reboots under ours after it.
- **Baseline on `86box_auto`** (`docs/captures/2026-10-20_auto/m8bench_auto.txt`, runs `auto_ours1`,
  `auto_ati2`; `auto_ati1` is mislabelled - it ran ours, a repeat run). Ours vs ours repeat: within
  +-10% per test, so smaller gaps are noise. ATI vs ours: engine paths 0.96-1.17x; text 5-11x;
  bitmaps 10-45x; fill_hatch 52x; lines_dash 21x (pixels differ); drag_frame 22x; polygon 9x.
  Switching driver through the agent: `REGEDIT /S C:\ATIM8.REG`, then `v9x_reboot_with_proof`.

## Next, in order

1. **Plan of attack in `docs/worklist.md`** (10-20). Start: DDK device bitmaps (item 1).
2. **Brushes and dashed lines on PATT_DATA** (fill_hatch, lines_dash).
3. Polygon/ellipse/floodfill speed: they fall back through strips (ATI 9x, 5x, 6x faster).

## Driving the bed

Two beds, same image, both on `86box_auto`: `vm_5160_mcp` (ours, agent on 9869, MCP `v9x`) and `vm_5160_ati`
(ATIM8, agent on 9870, MCP `v9x_ati`). Bench one at a time: two running VMs share the host CPU and skew
real-time results. Launch GUI programs with `v9x_exec` without `detach`: a detached launch runs hidden (a hidden
Solitaire left a second, visible one with no cards, on either driver).

`vm_start` with `vm_dir` = `vm_5160_mcp`, `window=True`; step ~120 s to the desktop. Start menu: press
`ctrl+esc`, step 3 s, then `r` (keys sent too early open Help or the Task List). After boot `release`,
then `v9x_*` tools work. A stuck run: the fault dialog's Details give the registers.
