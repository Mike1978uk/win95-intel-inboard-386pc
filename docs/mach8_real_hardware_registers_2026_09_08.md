# Mach8 / 8514A real-hardware register capture — 2026-09-08

Read-only `io_in` over COMrade on the real 5160, DOS text mode, 8514/A side **not** enabled
(ordinary VGA text console). ATI Graphics Ultra, BIOS `113-11504-002`.

| port | register | value |
|---|---|---|
| `0x42E8` | `SUBSYS_STAT` | **`0x00AB`** |
| `0x02E8` | `DISP_STAT` | **`0x0001`** |
| `0x9AE8` | `GP_STAT` | **`0x0000`** |

Word reads. `0x4AE8` deliberately **not** touched — writing bit 0 hands the display to the 8514
path and blanks the screen (technique 61).

## Why this is worth having

Issue #8 is that the option ROM prints `14207 7B6A 0007  RAM Addressing` in 86Box where the real
card prints `Testing........Ok`. Michal Necasek's article establishes the 8514/A is **not**
memory-mapped at all — VRAM is reachable only through I/O via `PIX_TRANS` — so I/O is the only way
to observe the accelerator, and these are the first real-card values this project has recorded.

**Next step is a diff, not more capture:** read the same three ports in 86Box under the same
conditions and compare. A divergence in `SUBSYS_STAT`'s upper nibble (monitor ID / revision) or in
`GP_STAT` would be a concrete, reportable emulation gap for @TC1995, who maintains
`vid_ati_mach8.c` — a file this project has never touched.

## Still the cheapest untried experiment

Run the Mach8 at **512 KB** instead of 1 MB in 86Box. Michal's finding is that the engine processes
**4 or 8 bits depending on 512 KB vs 1 MB VRAM**, which 86Box models (`config1 |= 0x20`, the
`dev->bpp` branches). If the self-test passes at one size and fails at the other, the defect is the
width handling and no register documentation is needed at all. Config change, no hardware.

## Caveats

- `GP_STAT = 0x0000` is expected with the accelerator idle, but has **not** been checked against a
  known-good reference — do not read it as a fault.
- Reading a register does not mean it is readable; several 8514/A registers are write-only and
  return floating bus. `io_in 0x83` returns `0xFF` on this board for exactly that reason.
- IBM never published 8514/A register documentation. The remaining primary sources are the
  **C&T 82C480 datasheet (Aug 1991)** and *Harnessing the 8514/A* (MIPS, 1990) —
  [`8514A_Registers.pdf`](https://www.ardent-tool.com/video/8514A_Registers.pdf), still unopened.
