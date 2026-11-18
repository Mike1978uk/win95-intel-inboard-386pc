# Next session, after 2026-11-18

Earlier: `docs/next_session_2026_11_18.md`. Newest wins.

## START HERE

**First job (owner brings the CF card up):** back up the whole CF card on the host (it mounts at `D:\`), then put the
buffered-mode VPICD on it offline and boot the 5160 with it. Spec, deploy and rollback steps:
`docs/issue10_vpicd_buffered_2026_11_18.md` (written by the agent that built it; check it landed and read its bed result
first - the agent was still running at handoff). Patch script `vxd-patches/patch_vpicd_buffered.py` (expect `Patched: N`).
On the 5160 prove the patched VMM32 loaded (md5 on the guest), then: desktop, NIC (IRQ 9->2), T130B (IRQ 3), keyboard,
mouse, floppy, twist once. If it holds, the next step is Simon's 5150 (one batched ask; his file transfer is slow).

**State.** Card: idle at the desktop, 800x600x16, driver as in the 11-18 handoff, agent build `inboard-trayflash`.
Beds: `vm_5160_dbg` and `vm_5160_mtm` shut down. 86Box `86box_auto` has the X22 commit `26fd9ec4c` (local, not pushed)
plus the uncommitted M8STEP diagnostic. `86box_h2` worktree (branch `diag-issue10-h2`, uncommitted I/O 100-1FFh trace).

## Done 2026-11-17/18

| item | result |
|---|---|
| V9x agent | `tray_flash` PR [michaeldale/V9x-Remote-Agent#1](https://github.com/michaeldale/V9x-Remote-Agent/pull/1) open; README "Other projects" table + ledger |
| #50 | L1 on/off makes no difference: 3.93 ms per KB either way (`captures/2026-11-18_xtinsw_card`); posted to Andrew, ledger closed |
| X22 | 86Box starts the 8 bpp frame at CRT_OFFSET: bed now flips both pages (`captures/2026-11-18_x22_bed`). Not upstream: full regression first (TEST.COM, probes, four 8514-family cards, G11) - raise before running |
| X20 | twist pages 23-24 in / 10-11 out a second vs 1.6 / 1.1 idle, free RAM ~0; est. up to ~half of each second in paging I/O (assumes 4 KB per count, unconfirmed). First attempt hung (twist windowed, black, hourglass); retry did not hang |
| #10 H2 | only the VGA BIOS touches 100h-1FFh (real and V86 mode): weak (`captures/2026-11-18_h2_iolog_bed`, doc section 18) |
| #10 IBVPICD | Intel's VPICD = stock 3.0 cut to one 8259, writes ICW4 09h (buffered) like the BIOS; ours writes 01h. Lead (`docs/issue10_ibvpicd_2026_11_18.md`, doc section 19). Simon = Cimon (one person). Owner messaged Simon: was WfW 3.11 on the 5150, with Harrison's image / IBVPICD? |
| DOS driver | `docs/dos_driver_groundwork_2026_11_18.md`: DOS/32A recommended; risk = extended memory under the 1986 BIOS (HIMEM sees 0 KB); first test a DOS/32A hello under INBRDPC.SYS. Mode from DOS = engine init + shadow set (`OUT 4AE8h,3`). Z27 corrected |
| Direction | "DirectXT" (owner's): our engine takes over the runtime's work wherever an interface allows; X20 and twist's 118 ms of D3D geometry are the numbers behind it |

## Check at the start (agents running at handoff)

- **VPICD buffered** (Opus): reached the bed desktop; was proving the file loaded, then writing the deploy doc. Its files
  (`vxd-patches/patch_vpicd_buffered.py`, `VPICD_INBOARD_BUF.VXD`, the doc) are uncommitted - review, then Haiku commits.
- **MTM dashboard off: done** (`captures/2026-11-18_mtm_dashoff_bed/`): colour runs a row 124 -> 17, but the
  back buffer is still Locked whole (480 rows) every frame - cause unknown (HUD text a guess). Feeds the S9 design.

## 2026-11-19 morning

- #55 again: card locked at first boot to the desktop, startup sound looping; owner reset, next boot normal. Stack unchanged (before the buffered VPICD). Next recurrence: one boot with the sound scheme off.
- VPICD agent left no deploy doc; written as `docs/issue10_vpicd_buffered_2026_11_18.md`. Bed reached the desktop.

## Next, in order

1. VPICD buffered on the 5160 (above). Then Simon.
2. #10 desk: trace the 8259's pin 16 (SP/EN) and CAS0-2 in the 5150 and 5160 Technical Reference schematics.
3. X20 follow-up: S7b (free the 635 KB locked shadow when DirectDraw ends); confirm whether cPageIns counts pages or
   operations; cost RAM compression. Note twist full screen or windowed by eye on the next card run.
4. DOS driver: DOS/32A hello under INBRDPC.SYS on the card (memory found?), then P3.2.
5. Plan items untouched: P1.10 fused asm row pass, S9 HUD/static-art design (MTM result feeds it), X22 regression.
6. `dist/mach8` still `32bfaac`.
