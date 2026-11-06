# Next session, after 2026-11-06

Order of work: `docs/mach8_frame_rate_plan.md`, "Order of work (2026-11-06)" and the lever table. **The cloud session's tests are in #59**: carry this line into new handoffs until every box there is ticked. Earlier handoff: `docs/next_session_2026_11_06.md`.

## Card

Boot 232 or later, `m8d3dd.drv` at 800x600x16. `M8DISP.DRV` md5 4263513518f4ed7af787850622650cae, `M8DD32.DLL` md5 69ff48e5f3946a4bcda63e5408ddab1c, commit afa46aa.

Way back to the build before V10: `C:\V9XREMOTE\REG\BACKV10.REG` (`m8d3d8.drv`) and `C:\V9XREMOTE\REG\M8DD32.D8`, renamed in by WININIT.INI. `install_drv.ps1` in the old scratchpad is not in the repo: model the next one on `tools/m8_card_cycle.ps1`.

**State when this was written:** agent ON, boot 233. Owner on the agent-free run of `M8D3DD`: "equally good, no stutters, felt smooth, general performance good". No black flashes reported.

## `M8D3DD` agent-free: the figures to beat

| fps | hal_tiny, first | hal_tiny, last | hal_small | hal_large | hal_static | hal_colour | hal_many |
|---|---|---|---|---|---|---|---|
| `M8D3D8`, tearing, black overlaps | 70-71 | - | 70-73 | 40 | 207 | 75 | 22.5 |
| **`M8D3DD`, whole frames, labels `v10c`, `v10d`** | 63-67 | **71.5-71.8** | 66-67 | 36.0 | 206-208 | 71.6-71.7 | 23.5-23.7 |

- The over-wait fix took `hal_small` from 35 to 66 fps, page wait 13-14 to 0.6 ms.
- 72 Hz is reached: `hal_tiny` last and `hal_colour` lock at it. `hal_large` is locked at 36.0.
- The first test carries the mode set's paging: `hal_tiny` 63-67 first, 71.5-71.8 last, worst frame 207-356 against 31 ms. Worst frames elsewhere 31-71 ms.
- `hal_small` needs ~1.2 ms less to reach 72 (15.0 to 13.85 ms). Spans (V6) gave `hal_many` 22.5 to 23.6 only: its `d3d` is 8.5 ms and engine 31 ms.

**M8GTRI, gradient against flat, agent running:** 32 px 654 against 482 us, 1.36x; 100 px 1,227 against 1,081 us, 1.14x. Centres read back 0 of 16 wrong, plane 7 left clean 0 of 16, no wait ran out. Owner: smooth. So gradients cost little, and the area costs little more: the extra is mostly the second XOR of the edges.

## Results of 11-06

| Finding | Evidence |
|---|---|
| V10 two pages at 640x480: black overlaps gone | Owner's eye; `docs/captures/2026-11-06_v10/README.md` |
| Stutter comes with the agent. Agent off: none | Owner, two builds. `M8D3DA` agent off 50-60 / 35 / 36 / 208 / 65 / 21-22 fps |
| 892 KB less locked RAM on the card | PERFLOG 3,020 to 2,128 KB: moveable + `GlobalFix` in place of `GMEM_FIXED`. Paging effect not yet measured |
| Polygon fill takes stored colours per pixel (6213h, A213h, E213h) | `docs/captures/2026-11-06_probes/README.md` |
| Gradient triangle on the card, smooth, 1.14-1.36x a flat one | M8GTRI, owner's eye and read-back |
| No memory path between the VGA and 8514 halves; shared DAC only | `docs/mach8_vga_half_findings_2026_11_06.md` |
| VGA half can flip; latch copy 0.65 us a pixel; BDh bit 2 separates A000 and B000 | Same probes. Which memory B000 reaches is not shown |
| Hangs: two with the agent on, none agent-free so far | Below |

## Open

- **Hangs, cause open, V10 not cleared.** (a) `M8D3D9` boot 226: egg timer, black screen, after a high-priority M8D3DF finished, started by the agent. Same symptom as `M8D3D6` on 11-05. (b) `M8D3D9` boot 227: `AGDISK` with the agent on, in PAGETIME with Word, Paint, PSP open; mouse and ping dead, no sound loop. The same list agent-free finished that morning. Do not run PAGETIME with the agent on unattended. Owner's theory: the agent is implicated. For: all hangs today had it. Against: its load is not proven the cause.
- **Pick-up risk in the over-wait fix:** a CRT_OFFSET write just before the card's pick-up, with the blank flag following, counts as taken. M8PAGE places pick-up between 0.9 of a frame and just after the flag. Would show as a black flash. None reported in two agent-free runs; no frame-by-frame check made.
- `M8D3DA` and later changed D3D paths for 8 bpp too (scratch corner, spans). Not rerun at 8 bpp. Desktop bench `M8BENCH` not rerun on any V10 build: pixels by hash against `noag1`, before this ships.
- M8VGAW fails to start, error 31, from the agent and from Start > Run. Not needed yet: M8LATCH gave the write cost (~1.07 us). If the read cost is wanted, add it to a DOS probe.
- Bed `vm_5160_mcp`: on `m8d3dc.drv`, registry and files changed by this session; image backup `card.img.pre_v10` (2 GB, delete when no longer wanted). Still holds the error-tagged `DDRAW.DLL`: restore md5 EEDE14A8FB89A725C903E33A6A0F9F28. Its M8D3DF is too slow to show a page wait: the card is the test.
- Disk baseline with the agent on (`AGDISK`) not captured: it hung. Do not repeat as is.
- #55 and #59 updates not posted. 15 local commits, nothing pushed: owner's call.
- Owner to bring the CF to the host reader once, card shut down: `sectors.py` over `WIN386.SWP` (P-a). Never commit that file.

## Order of levers

1. **Paging effect of the 892 KB:** agent-free `NOAGDISK` with a new label, against `noag` (open 64.6 s, switch 12.5 s, page-ins 10,619). Started by the owner's double-click. Then R3: the same at 8 bpp.
2. **Colour: V25 with C3 and C2, the owner's colour blocks.** Needs a design: M8GTRI proved the fill, but a stored ramp is a 1:1 shift, 7 bits, and DirectX gives three corner colours. M8GTRI's 1.14-1.36x says the stored fill is affordable, so favour it. Candidates: stored ramps per hue with an adaptive palette; row bands by spans (4 writes a row, any vertical gradient); A213h per row for a horizontal ramp.
3. **Frame cost:** `hal_small` 15.0 ms against 13.85 ms a refresh. V13 (DirectX draw calls, ~3 ms a frame), V3+ (16 erase boxes, ~130 writes), V18 and V12 (probe first). `hal_large` at 24 ms cannot reach 72: aim 36 steady.
4. **RAM:** R2 fingerprints, R4, R6; the other 8 bpp buffers (`screen.c:201`, GMEM_FIXED) if 8 bpp stays. Back buffer R1: now pageable, so reserve-on-Lock may add little; measure before building.
5. **Paging:** P-b `LZ4B12FN` (18 % cheaper compress, ratio 2.26 to 2.25) or `LZ4B10FN` (6 KB less table). A third of swap reads are raw though 8 % of writes are: find out why. `SWAPCOMP\Contention` 918 in one run, 0 before.
6. **XT-CF:** four test builds, `docs/next_session_2026_11_06.md`. Expect little: 15-40 us of 10,000.
7. **VGA half (V28):** a Mode X path for programs that write pixels: flip, latches, ~1.07 us a write against 7.64 us. Full-screen only: the output switch blanks over a second. **Ring 0 (O18):** no faster OUT. Adds flip queue, frame timer, page dirty bits. Hold until the hangs are understood; re-measure page wait first.
8. **Textures:** CT1, CT3 (owner's ROM font), V9, V21, V26, V27. M8GRAD's E213h recolours a stored texture.

Probes built and not run: D6 pass-through mix, D8 AB bits 3/6 and AC bit 6 (highest hang risk): `docs/mach8_vga_half_findings_2026_11_06.md`.

## Tools

`tools/m8seq/` M8GRAD, M8DACX, M8BNDRY, M8LATCH, M8VFLIP, M8W128, `m8vga_decode.py`, `M8PGO.BAT` (on card as `C:\M8P\GO.BAT`, Command Prompt Only). `tools/m8probe/` M8PAGE, M8GTRI. `tools/m8d3dtest/M8D3DF` gained a page wait column and `again`. Desktop on the card: `AGENTOFF`, `AGENTON`, `NOAGWTCH`, `NOAGDISK`, `V10TEST`, `M8GTRI`, `M8PAGE`, `M8VGAW` (fails). Lists in `C:\V9XREMOTE\*.LST`.

Benches: agent-free runs are the owner's. `M8D3DF` at `high` hung once started by the agent. Build order for a Windows program: `-fe=NAME.EXE` must be quoted in PowerShell.
