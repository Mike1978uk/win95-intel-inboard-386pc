# Next session — 2026-09-24

## 3C509B emulation — built, every gate passed, awaiting the owner's submission

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
