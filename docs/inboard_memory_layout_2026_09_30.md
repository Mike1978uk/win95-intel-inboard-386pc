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

Both #7638 reports are this one defect. Only the 5 MB row is measured on hardware.

Cause: 86Box's generic `mem_reset()` (`src/mem/mem.c`) maps `mem_size - 1024` KB at 1 MB, treating
640-1024 KB as a hole. The card has only its 128 KB reserved block there, so 256 KB is lost at
every size. QuantumByteRider's workaround (`mem_size = 1280`) works for that reason, not because
the planar keeps 256 KB: on the 5160 the card serves 0-64 KB (measured above). The machine entry
allows 1024/3072/5120 only (`.step = 2048`, `.max = 5120`), so the workaround cannot reach 5 MB.
Fix belongs in the Inboard device or machine entry: map `mem_size - 640 - 128` KB at 1 MB.

### Fixed in the emulator, 2026-10-05 - local, not yet submitted

`86box_upstream` branch `inboard-ext-256k`, commit `4f18c5b98`: `inboard386_reset()` calls
`mem_remap_top_ex_nomid(256, mem_size)`, placing the RAM behind the hole directly after the
(mem_size - 1024) KB mem.c maps, plus an A31 alias for that 256 KB. One file, 59 lines.
Beds in `vm_ext7638/`: QuantumByteRider's image, stock `INBRDPC.SYS` with `NOPAUSE` and
`NODIAGS` blanked.

| board | before (control exe) | after (detected / diagnosed / functional) | real 5160 |
|---|---|---|---|
| 1 MB | 0k detected, bad 10560k | 256k / 256k / 256k, bad 0k | - |
| 3 MB | not run | 2304k / 2304k / 2304k, bad 0k | - |
| 5 MB | 4096k | 4352k / 4352k / 4352k, bad 0k | 4352k |

Windows 95 on `vm_magnaram_off` (same commit cherry-picked onto `86box_3c509b`, branch
`inboard-ext-256k-diag`): boots, System Properties 5.0 MB (owner). No new setting is needed:
the RAM box steps 1/3/5 MB and snaps typed values, so the 1280 KB workaround was only
reachable by editing the config file. Not tested: Intel's SETUP at 1 MB, the dynarec, the
AT Inboard (untouched - the change is `is_xt` only).

**Pre-PR checks, same day.** The commit cherry-picks cleanly onto upstream master `ed1b5209b`
(worktree `86box_ext256k`, branch `inboard-ext-256k-master`, commit `b2a016f77`).

- Master + fix, interpreter, 5 MB: 4352k / 4352k / 4352k, bad 0k.
- Hard reset: no separate run needed. The SDL front end starts every VM through
  `pc_reset_hard_init()`, the same function as the menu's Hard Reset, and its order is
  `machine_init()` -> `mem_reset()` -> Inboard init -> remap. Every bed run took that path.
- Dynarec, 5 MB: the memory diagnostic stalls with **and without** the fix (owner read both
  panels): detected and diagnosed fill in, functional and bad stay blank. Older than this
  change; not investigated. `PrintWindow` returned stale frames under the dynarec - use the
  owner's reading, not a capture.

### The dynarec stall, root-caused and fixed, 2026-10-05

With the dynarec on, the full memory check stopped after "diagnosed" at 3 and 5 MB, with or without
the 256 KB fix. Cause, from a timer-driven CS:EIP heartbeat, a CS-load hook and a write watch on the IVT:
the low BIOS window's **exec pointer** always named `bios_shadow_ram`, while its read handler returns
the ROM until shadowing is switched on. `INBRDPC.SYS`'s reserved-block test writes patterns into that
buffer through `0x5F0000` while shadowing is off. The interpreter (read handler) kept running the ROM;
the dynarec (exec pointer) compiled the patterns as BIOS code, ran past `F000:FFFF` into address 0 with
A20 off, rewrote vectors 4-9, and the next timer tick went to `5EDD:FF23`. At 1 MB the test path that
reaches it does not run.

Fix, second commit on the PR branch (`32013c53f`): `inboard386_apply_rom_shadow()` points the exec
pointer at the ROM snapshot or the shadow buffer to match the read handler. Clean build, both cores,
1 MB and 5 MB: 256k and 4352k detected/diagnosed/functional, 0k bad. Branch
`inboard-ext-256k-master` is two commits, one file, +66/-5. Diagnostics kept on
`inboard-ext-256k-heartbeat` (`4f3738caf`), not for upstream.

### The 08NOV82 XT BIOS, offered again - 2026-10-05

The Inboard's BIOS list was cut to the two 1986 ROMs in August on the claim that `INBRDPC.SYS` needs a
signature at `F000:E05B`. That was the shadow-window bug (`docs/issue10_old_bios_2026_09_27.md`), and
Cimon's real 5160 runs Windows 95 on the 08NOV82 ROM. Branch `inboard-1982-bios` (`0d234a599`, off
upstream master): one list entry, identical to `ibmxt_config`'s, and the August comment replaced.
Only this ROM is offered - 16AUG82, the Alt BASIC pairing and the 5150 ROMs have no Inboard evidence,
and the owner does not want to support ROMs that are not known to work.

Tested with all three commits together (branch `inboard-test-all`):

- the 1982 ROM is the one running: DEBUG in the guest read `B1 05 D2 EC ...` at `F000:E07E` and
  `11/08/82` at `F000:FFF5`;
- memory panel, both cores, 1 MB and 5 MB: 256k and 4352k detected/diagnosed/functional, 0k bad;
- Windows 95 on `vm_magnaram_off` (`86box_3c509b`, branch `inboard-ext-256k-diag` with the exec fix
  merged into its local port-670h window gating): 5.0 MB on 09MAY86 and on 08NOV82, owner-read; one
  hard reset at power-on in each log. The bed's PERFLOG startup was off for the 08NOV82 boot only and
  is restored.

ILIM386 on a 1 MB board reported "Total available = 256 KB": independent confirmation of the 256 KB fix.

### PR layout, 2026-10-05 (local, nothing pushed)

Two PRs, at the owner's request (one per subject, not one per fix):

1. `inboard-memmap` (`86box_ext256k`): `b2a016f77` the card's 256 KB, `9c299d7cc` the 670h bit 0
   window gating (local commits `696cd8cd1` + `32bbc97c0`, squashed to their net change), `842b7b82d`
   the exec pointer. One file, +79/-5. Panels pass on both cores at 1 and 5 MB; Windows 95 at 5.0 MB
   on the same `apply_rom_shadow()` in `86box_3c509b`, on both ROMs.
2. `inboard-1982-bios`: `0d234a599`, the 08NOV82 entry.

Each matches the real 5160: 4352k at 5 MB (driver panel), `FF` at the windows once shadowing is on,
the full check completing (owner photo), and Windows 95 on 08NOV82 (Cimon's 5160).
