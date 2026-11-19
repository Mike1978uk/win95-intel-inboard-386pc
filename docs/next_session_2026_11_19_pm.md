# Next session (2026-11-19, from 14:20)

## START HERE

**Card state:** 5160 in **Windows**, CONFIG.SYS menudefault **LEAN**, display driver **`M8TL.DRV`** (top-left rule,
md5 DLL `22F9F173...`, DRV `8F8F44B4...`). `tools\dosagent\bootmode.ps1 dos` switches to the DOS agent
(`dev-dos-con3`, Esc and idle drop now work while a client is connected).

**Owner's direction (11-19):** the whole card is the primary focus (worklist **L6**,
`docs/l6_card_for_3d_2026_11_19.md` holds every result); every new lever gets a retrospective across m8dos AND the
Windows driver; **DirectXT** (our own DirectX on Windows and DOS) is the end goal; aperiodic tilings and card-side
dither are owner ideas to probe (P5, P5b, P6). Owner approval of a draft's wording = post it.

## Done today (all card-proven, pushed)

| Item | Result | Commit |
|---|---|---|
| DOS agent: Esc and 120 s idle drop while connected | card + PR #2 updated | `f221774`, dist `6e5637c` |
| C3 perf mailbox (`M8PF` at 40:F0) + PERF.CSV | matches printout | `91f7a12` |
| DirectXT step 1: HX runs Win32 on the 5160 | 2,836 KB free, 2 MB clean | `8e8acf3` |
| L6 P2 closed scratch outline (DOS + Windows triangle) | -14 % writes | `5a97d35`, `0cd2edc`, `faed345` |
| L6 P4 hardware depth test (`m8_depth`) | exact, control differs | `4859836` |
| L6 page clear sent early | DOS bench 37.9 -> 46.7 fps | `0dfa59d` |
| L6 P3b top-left fill rule + shared batches (DOS) | mesh -41 % writes, no holes, order-independent; bench **55 fps** (from 34.7) | `3ddd594`, `cd7a1a6` |
| Top-left rule in Windows `m8d3d.c` | twist Flip 32.5 -> 24.9 ms, 6.61 -> 6.87 fps, owner: faces clean | `34a07be` |
| Register probe: only SRC_X_START persists between fills | -1 write a fill | `cd7a1a6` |
| Simon #10: buffered VMM32 passes the mode switch; stops at "Insufficient memory" | bed: his image boots on the 1986 ROM; his Super PC BIOS black-screens the bed | `e26c099` |
| Watt-32 #133: reply posted (fast path size check + 64 KB) | waiting on gvanem | comment 7007279035 |

Disproved (do not retry): P1 (fill registers do not persist, except SRC_X_START); P3 as posed; CUR_X /
DEST_X_START / DESTX_DIASTP / DESTY_AXSTP once a batch; M8D3DF **grab** mode hung the card once - not unattended.

## Next, in order (owner's order)

1. **Joined same-colour patches in m8dos** (Windows already has them, `m8d3d_join` ~3063): same colour and z, share an
   edge with third corners on opposite sides, box-disjoint from other plane members, no overlap with later fills;
   one plane, shared edges cancel by XOR, one fill. Probe: a mesh of same-colour quads, pixels = unjoined, writes.
2. **P5 checkerboard** (lines once into one plane + one fill), then **P5b** Penrose with 3 planes and WRT_MASK edges.
3. **P6 card dither** through a cached mask plane (fill A, blit B through the mask).
4. Overdraw audit leftovers (`docs/overdraw_retrospective_2026_11_19.md`): mask builder and gradient triangles to
   top-left; stale thresholds; equal-row span merging.
5. Twist in DOS: first a native m8dos port of the scene (yardstick), then DirectXT steps 2-3 (VESA32.DLL design in
   `docs/hx_dos_directx_2026_11_18.md` s8; Direct3D DLL) for the same `twist.exe`.
6. Windows levers now: our describe/feed code (~33 ms of twist's 146) and Microsoft's runtime (~100 of tunnel's 346 ms,
   D14) - DirectXT.

## Waiting on others

- Simon: MEM /C, SYSTEM.INI, Inboard memory, network card / remote agent answer (sent 11-19).
- gvanem: Watt-32 #133 fix for the non-fast path.
- Michael: PR #2 review (now includes the Esc/idle fix).

## Tools

`tools/dos3d/M8DTEST.EXE` modes: `card`, `bench`, `probe` (outline rules), `zprobe` (depth), `costs` (frame time),
`edgeprobe` (fill boundary), `tileprobe` (meshes, coarse + fine). Card runs: put files to `C:\M8C3`, run a `.BAT`
detached through the DOS agent, fetch `M8PROBE.TXT`. Windows: `tools\m8cardscripts\install_h.ps1` then `twist_card.ps1`.
