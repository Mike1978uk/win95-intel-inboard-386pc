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
| 64 | the card's RAM under the planar's 64 KB - one copy of that range is redundant |
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
