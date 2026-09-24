# Next session — 2026-09-24

## Priorities (owner, end of session)

1. **The 86Box LPT bridge fixes**: test (plan below), then the owner opens the PR.
2. **#42: IRQ 9 → 2 on the XT.** Card at 9, stock ELNK3, VPICD dispatching master IRQ 2 as
   IRQ 9. The bed already reproduces the fault; trace it, then write the patch.

## Update, end of session

- **3C509B submitted: 86Box PR #8076** (`Mike1978uk:3c509b-isa`, commit `7e32b6ce8`, one
  commit on master `715a1c1ca`). Wording is the owner's. Announced on discussion #6447
  (comment 18584701). **Watch both; report new comments to the owner; never reply.**
- Open owner decision: EEPROM words 4-6 (the card's manufacturing data) are in the template;
  zero them and force-push if the owner prefers, ideally before review.
- **LPT bridge fix still untested.** Two attempted runs produced no evidence: the Inboard bed
  and a copied 486 config both reached ROM BASIC / "no ROM BASIC" on the fix build (the copy
  pointed at `../at95/win95_at.img`; the disk was never seen). Both grabbed the owner's mouse.
- **Before any VM run:** ask first; set `mouse_type = none` in test configs (86Box captures
  the mouse only on a click in the window when a mouse is configured); keep the disk image in
  the bed's own folder and check it resolves; close only the PIDs launched.
- Test plan for the fix: (1) SCSI/IDE CD-ROM, nothing on LPT, `build_log` variant of unfixed
  vs fixed - `BPCK: attached` present then absent; (2) owner configures an LPT CD-ROM **through
  Settings** in the fixed Qt build (checks the menus still offer LPT - the gap missed on
  #8010/#8012) and confirms the drive letter in `vm_bpck`. The fix does not touch `src/qt/`.

## 3C509B emulation — built, every gate passed

Branch `3c509b-isa-pr` in worktree `86box_3c509b_pr/`: one commit on 86Box master `715a1c1ca`,
builds with zero warnings. Working branch `3c509b-isa` (worktree `86box_3c509b/`) also carries
the `pic.c` commit below. Nothing pushed.

| Gate | Result |
|---|---|
| Win95 + stock ELNK3 on the Inboard XT bed: DHCP, web, clean shutdown | pass |
| DOS + Nestor packet driver + mTCP on the Inboard: DHCP, `HTGET` whole page | pass |
| Win95 on a 486 AT (AMI 495): manual install from its CABs, DHCP, web, restart after a network change | pass |
| Attach from Settings on an empty config (Qt) | pass |
| Regression of all three on the final binary | pass |

- EEPROM template = the owner's real card, read through the ID port (`drivers/3c509b/eeprom/`).
  Boot-ROM bits cleared; persisted per card as `nvr/eeprom_3c509b_<n>.nvr`.
- Defaults COMBO / `0x300` / **IRQ 3** (owner: IRQ 10 is unreachable in an XT slot).
- Windows 95 detection records the IRQ, which `NET3COM.INF` does not allow for the legacy card:
  "resources do not match any known configuration". Install from the list instead. Windows
  behaviour, not the model.
- MicroWeb hangs drawing in 256-colour mode on the emulated Mach8 after the page has arrived;
  `HTGET` of the same page is clean. Mach8, not the NIC — unexplored.
- One shutdown hang on the Inboard bed after a network change, not reproduced since.

**Owner decisions before posting:** a line on discussion #6447 (@wavz217 said 2026-08-12 they
were looking); whether the card's manufacturing words (EEPROM 4-6) may stay in the template;
PR text — draft notes only, to be rewritten by hand.

`pic.c` IRQ 9 → 2 on single-PIC machines is a separate commit and a separate PR.

## Upstream regression from #8012 — fix built, NOT yet tested

`src/cdrom/cdrom.c` adds the BackPack bridge in a `case` shared with ATAPI and SCSI, so every IDE
or SCSI CD-ROM creates a BackPack that claims LPT1. `lpt_bpck.c` and `lpt_epat.c` force their
logging on. Branch `fix-lpt-bridges` (worktree `86box_lptfix/`): 5 lines, warning-free.
Confirmed on master: the Inboard bed (SCSI CD-ROMs, nothing on LPT) logs
`BPCK: attached as unit 07 on LPT port 0`.

Still to test, **asking before each VM run**: (1) no BackPack with an IDE/SCSI CD-ROM, using the
`build_log` variant (`-DENABLE_LPT_BPCK_LOG=1`) on a config that boots on plain master — the
Inboard bed reached ROM BASIC on that build, cause unknown; (2) `vm_bpck` still gets its CD-ROM.

## #42

IRQ 9 fault reproduced in the bed: `BOOTLOG.TXT` ends at `Dynamic load success elnk3.vxd`, as on
the 5160. Next: trace the faulting instruction, then the VPICD dispatch patch.
