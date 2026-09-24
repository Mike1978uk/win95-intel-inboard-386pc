# Next session — handoff from 2026-09-24

## Priorities

1. **86Box LPT bridge fix** — test it, then the owner opens the PR.
2. **#42: IRQ 9 → 2 on the XT** — card at 9, stock `ELNK3.VXD`, VPICD dispatching master IRQ 2
   as IRQ 9. Trace the fault in the bed first, then write the patch.

## Rules for every VM run

- Ask the owner first; say which bed, which build, how long.
- `mouse_type = none` in test configs: 86Box grabs the mouse only on a click in the window
  when a mouse is configured.
- Keep the disk image in the bed's own folder and check it resolves before launching.
- Close only the processes launched.
- Watch 86Box #8076 and discussion #6447; report new comments to the owner; never reply.

## Done this session

- **3C509B emulation submitted: 86Box PR #8076** (`Mike1978uk:3c509b-isa`, one commit
  `7e32b6ce8` on master `715a1c1ca`). PR wording is the owner's. Announced on discussion #6447.
  - Tested in 86Box: Windows 95 + stock ELNK3 on the Inboard XT and on a 486 AT (manual
    install from the Windows CABs, DHCP, web, clean restart after a network change); DOS with
    Nestor's 3C509 packet driver + mTCP (DHCP, `HTGET` of a whole page); attached from
    Settings on an empty config.
  - EEPROM template is the owner's real 3C509B-COMBO, read through the ID port
    (`drivers/3c509b/eeprom/`). Boot-ROM bits cleared; kept per card as
    `nvr/eeprom_3c509b_<n>.nvr` so 3C5X9CFG changes survive. Defaults COMBO / `0x300` / IRQ 3.
- `pic.c`: IRQ 9 delivered as IRQ 2 on single-PIC machines — commit `ef082884b` on local branch
  `3c509b-isa`; a separate PR, not yet raised.
- `docs/rom_and_eeprom_dumps.md`: every ROM and EEPROM image from this machine, with source.
- repo-hygiene skill §7: attach a device from Settings before submitting it.

## Open

- EEPROM words 4-6 (the card's manufacturing data) stay in the #8076 template: owner's decision.
- **LPT bridge fix — built, not tested.** Branch `fix-lpt-bridges`, worktree `86box_lptfix/`,
  5 lines, no warnings:
  - `src/cdrom/cdrom.c`: `CDROM_BUS_LPT` gets its own `case`. Master adds the BackPack bridge
    for every ATAPI and SCSI CD-ROM, and it claims LPT1 (introduced by #8012).
  - `lpt_bpck.c`, `lpt_epat.c`: remove the forced `#define …_LOG 1`; log-only helpers in
    `lpt_bpck.c` moved under the `#ifdef`.
  - Evidence of the bug on master: the Inboard bed (SCSI CD-ROMs, nothing on LPT) logs
    `BPCK: attached as unit 07 on LPT port 0`.
  - Test plan: (1) a SCSI or IDE CD-ROM with nothing on LPT, unfixed vs fixed, both built with
    `-DENABLE_LPT_BPCK_LOG=1` — the attach line present, then absent; (2) the owner sets up an
    LPT CD-ROM **through Settings** in the fixed Qt build (checks the menus still offer LPT)
    and confirms the drive letter in `vm_bpck`. The fix does not touch `src/qt/`.
  - Two attempts today gave no evidence: both machines reached ROM BASIC on the fix build.
- **#42:** the bed reproduces the real machine — card at IRQ 9, `BOOTLOG.TXT` ends at
  `Dynamic load success elnk3.vxd`. `vm_3c509b/86box.cfg` is set back to IRQ 3 for the
  regression; set `irq = 9` to reproduce.
  - ELNK3 takes the IRQ from the card (`0x30f7`), not the registry; it accepts 9 but not 2.
  - `VPICD_INBOARD.VXD` NOPs the slave PIC, so a master IRQ 2 is read as a cascade with no
    slave source and dropped.

## Findings worth keeping

- Windows 95's hardware search records the card's IRQ, but `NET3COM.INF` allows the legacy
  3C509 only an I/O range: "resources do not match any known configuration". Install from the
  list instead — which is how the owner's real machine was set up (`ForcedConfig` I/O only).
- The 3C509B's boot-ROM socket on the real machine holds Sergey Kiselev's Multi-Floppy BIOS
  at `D0000h` (#35).
- MicroWeb hangs drawing in 256-colour mode on the emulated Mach8 after the page has arrived;
  `HTGET` of the same page is clean. A Mach8 question, not the NIC.
- One shutdown hang on the Inboard bed after a network change; not reproduced since.

## Beds and branches

| Where | What |
|---|---|
| `86box_3c509b/` | branch `3c509b-isa`: card + `pic.c`; builds `build`, `build_log`, `build_qt` |
| `86box_3c509b_pr/` | branch `3c509b-isa-pr` = PR #8076 |
| `86box_lptfix/` | branch `fix-lpt-bridges`; builds `build` (Qt), `build_log` |
| `vm_3c509b/` | Inboard Win95 bed (`card_master.img`, `card_dhcp.img`) |
| `vm_3c509b/dos311/` | Inboard Windows 3.11 + mTCP bed |
| `vm_3c509b/at95/` | 486 AT Windows 95 bed (`win95_at_master.img`; reapply `AutoScan=0`) |
