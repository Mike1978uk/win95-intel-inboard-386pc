# Next session - handoff from 2026-09-25

## #44 - the vendor LS-120 DOS driver works in 86Box

`SD120PPD.SYS` + `ASPIHDRM.SYS` load in the AT bed and present **one** drive, `D:`, whose files
read back correctly (owner, B15). Windows regression passes on the same build: our miniport
`d8154f1d` mounts `J:`, files read, a write reached the medium (owner, G10 by hand).

| | |
|---|---|
| branch | `epat-cpp-40-50` in worktree `86box_epat44`, **local only** |
| commits | `35bf8fe5c` (chain scan, doubled register bytes), `b3259c62d` (the rest, WIP) |
| clean build | `86box_epat44/build_log`, 10:54:55, HEAD `b3259c62d`, no trace |
| the read of the driver | `docs/sd120ppd_sys_load_path_2026_09_25.md` |
| measured on the 5160 | `drivers/imation_ls120/MEASURED_FACTS.md` sections 8 and 9 (chip id `E2`, the IDENTIFY block) |

**Passes only with `/di` (polled).** With `/IRQ:7` alone the driver uses the EPAT's own transfer
engine (reg `0x12` bit 1, count in `0x14`, internal regs 6/7) and waits for it to finish - not
modelled. Read it statically before modelling it (`0x3357`, `0x368C`, `0x36B6`).

## Before this becomes a PR (G1-G10)

- Split `b3259c62d` into one commit per fix, each tied to the vendor code in the load-path doc.
- G2: a config with nothing on LPT, and the BackPack, must be unchanged.
- The identify block carries the owner's drive serial `X713CA0B4594` - owner's call whether to
  keep it or use a generic one.
- Say plainly: works with `/di`; interrupt mode needs the transfer engine.

## Other state

- 5160: vendor DOS driver lines un-REM'd in `CONFIG.SYS` for the reads; owner to restore.
  `C:\MEMD.TXT` left on the card.
- `vm_ls120win\86box.cfg.master` corrected from type 6 (SparQ since the renumber) to 7.
- WfW 3.1 report on #8076: not started; our 3.11 image shares the driver base.
- README updated (`d816909`): #8078 merged, IRQ 9 -> 2 is Windows-side, XT probe caveat.
