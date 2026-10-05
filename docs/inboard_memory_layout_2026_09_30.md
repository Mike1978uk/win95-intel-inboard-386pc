# Where the Inboard's 5 MB goes - measured 2026-09-29

Answers @andrew-hoffman's question on #35: how the 5 MB is laid out, and how much is unusable.

## Measurement

One boot of the 5160 with `DEVICE=c:\INBRDPC.SYS` (no `NODIAGS`, no `NOPAUSE`), the driver's own
report photographed by the owner:

| line | value |
|---|---|
| conventional memory initialized | 640k |
| extended memory detected / diagnosed / functional | 4352k / 4352k / 4352k |
| bad extended memory | 0k |
| system BIOS | 32-bit RAM |
| EGA BIOS | ROM |

## Accounting

Board: 5120 KB. Planar: 64 KB (bank 0 of a 64-256 KB board, SW1-3/4 ON).

| KB | what |
|---|---|
| 4,352 | extended memory, handed to Windows |
| 576 | conventional 64-640 KB, served from the card |
| 64 | the planar's own 64 KB, hidden behind the card - measured 2026-10-02, see below |
| 64 | `F000` BIOS shadow - in use ("system BIOS: 32-bit RAM") |
| 64 | the second reserved half, for the EGA ROM - idle ("EGA BIOS: ROM", `EGACACHE` off) |
| **5,120** | total - the report accounts for every KB |

The 128 KB reserved block matches the emulator model and UniPCemu. The 64 KB under the planar is
an inference from the arithmetic: which of the two copies of 0-64 KB is actually decoded was not
measured.

## What could be recovered

At most 128 KB (the idle EGA half and the redundant 64 KB), both fixed by the card and the driver,
not by a setting. 128 KB is 32 pages against ~3,000 page-ins to open the RAMBASE programs: about
1%. Not a lever worth a driver patch.

Windows should see 640 + 4,352 = 4,992 KB. The System Properties General tab reads **5.0 MB**, which
is 4,992 KB rounded; the tab is too coarse to confirm more than that.

## The 64 KB under the planar - measured 2026-10-02

Which copy of 0-64 KB answers was timed on the 5160 in real-mode DOS: 512 bytes copied out of
segment `0000` and `1000`, the L1 flushed (`WBINVD`) before each pass
(`tools/gen_memwidth_probe.py --flush`, raw output `docs/captures/2026-10-02_memlo_5160/`).

| PIT ticks per 512 bytes | `0000` | `1000` |
|---|---|---|
| byte / word / dword | 258 / 244 / 226 | 260 / 240 / 226 |

Identical, at 0.42 us/byte. Planar RAM across the XT bus would cost ~2.9 us/byte (~1,500
ticks). So the card serves 0-64 KB, and the hidden copy is the planar's 64 KB: slow XT-bus RAM
with no second address. Nothing to recover there. The only reclaimable reserved RAM was the idle
EGA half, which `SHADRAM.VXD` now gives to Windows (`docs/shadram_2026_10_02.md`).

The "about 1%" estimate above used total RAM as its base; the pageable pool is ~1.3 MB, and in
the bed 64 KB cut page-ins by ~14%.

## The emulator under-counts by 256 KB - 86Box/86Box#7638, 2026-10-05

Intel's manual (page A-3): the card itself has 256 KB of extended memory; the piggyback adds to it.
The real 5160 confirms it at 5 MB (4,352 = 256 + 4,096). 86Box reports 256 KB less at every size:

| board | real (manual arithmetic) | 86Box |
|---|---|---|
| 1 MB | 256k | 0k - Intel's SETUP refuses: "contains 256K ... found less than 256K" (QuantumByteRider) |
| 3 MB | 2,304k | 2,048k (Fenix770; also our 2026-08 matrix) |
| 5 MB | 4,352k (measured) | 4,096k (Fenix770, upstream master) |

Both #7638 reports are this one defect. Only the 5 MB row is measured on hardware. Not traced: where
86Box places RAM above 640 KB for this machine. First check: read `0x100000`-`0x13FFFF` at 1 MB.
