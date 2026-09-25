# Next session - midday handoff from 2026-09-25

## ✅ Update 2026-09-25 afternoon: #44 SUBMITTED as 86Box#8099

Head `f361a8a02` (three commits on `lpt-epat-vendor-dos`, fork `Mike1978uk/86Box`), opened by
Claude with the owner's go-ahead; the owner overrode "never post" and the full not-tested list.
The engine was not the blocker: a lost interrupt, an arm-gated unit query, and register value
22h eaten by the unlock matcher. Detail: `docs/sd120ppd_sys_load_path_2026_09_25.md`.

| gate | result |
|---|---|
| G1 | pass |
| G2 | **not run** |
| G3 | not run (no UI change in the diff) |
| G4 | pass - vendor DOS (default and `/di`) and vendor Windows MPD from `dist/ls120_vendor`, writes checked on host |
| G5 | pass |
| G6 | pass |
| G7 | built on base `1c7e3a573` only; **not rebased** onto current master (owner: push as-is) |
| G8 | pass |
| G9 | source at HEAD byte-identical to the tested source; exe predates the commits |
| G10 | partial - owner drove all runs; Settings/Media menu/Device Manager names not checked |

Serial `X713CA0B4594` kept (owner). Test 3 first ran on our RETIRED `LS120MP.MPD` because this
handoff named it - the regression bed is now on the vendor MPD (`vm_ls120win`, backups
`ls120win_preVendorStage.img`, `rd_preRegr44d.img`). `vm_ls120dos44` is on the default line.

## (superseded) #44 was HELD - one task closes before it can be submitted

The vendor LS-120 DOS driver works in 86Box **only with `/di`** (polled). With the line a
stranger copies (`/IRQ:7`, no `/di`) the driver hands transfers to the EPAT's own **transfer
engine**, which is not modelled, and hangs on the splash screen. That breaks repo-hygiene
section 7 ("people will try it and say it's failed"), so the owner held the push.

**Task 1: model the transfer engine.** Read it statically first and write it into
`docs/sd120ppd_sys_load_path_2026_09_25.md` before any code:
- setup `0x3357`: internal regs 6/7 (size), reg `0x14` = bytes/2 - 1, reg `0x12` bits 0-1
  (direction + start), window reg `0x18` = direction code (`0x26`/`0x22` on the `E2` chip);
- wait/finish `0x368C`, `0x36B6` (reg `0x12` bit 1, reg `0x13` bit 1), and how the data is
  then collected from the bridge;
- completion by interrupt: already modelled (`CPP 48` arm, INTRQ -> IRQ 7, `CPP 08|u` byte).
Then implement, and run: default line (pass = one drive letter, files read), then `/di`,
then the Windows regression. Owner drives every check (G10), **no startup batch**.

## State of #44

| | |
|---|---|
| PR branch | `lpt-epat-vendor-dos` in worktree `86box_epat44`, one commit `876b6c53b` on master `1c7e3a573`. **Not pushed** |
| builds | yes, on current master, no new warnings (G7) - `86box_epat44/build_log`, 11:13 |
| tested | on `b3259c62d` (same code before three comment fixes and a dead-code removal): DOS `/di` one drive `D:` reads (B15); Windows `LS120MP.MPD` mounts `J:`, reads, a write lands. **Retest on the final HEAD (G9)** |
| not run | G2 (nothing on LPT, and the BackPack, unchanged) |
| open decision | the identify block carries the owner's drive serial `X713CA0B4594` - keep or genericise |
| PR text | drafted in the session; drop the ECP and LPT2 lines (owner) |
| the read | `docs/sd120ppd_sys_load_path_2026_09_25.md`; measured values `drivers/imation_ls120/MEASURED_FACTS.md` sections 8-9 |

Old branch `epat-cpp-40-50` (`35bf8fe5c`, `b3259c62d`) is the tested history; keep it until
the PR merges.

## After the PR merges

Add it to the README's merged table and the contributor ledger: what was submitted, what merged.

## Other state

- **5160:** vendor DOS driver lines are un-REM'd in `CONFIG.SYS` (for the reads); owner to
  restore. `C:\MEMD.TXT` left on the card.
- `vm_ls120win\86box.cfg.master` corrected from type 6 (SparQ since the renumber) to 7.
- WfW 3.1 report on #8076: not started; our 3.11 image shares the driver base.
- README updated this morning (`d816909`): #8078 merged, IRQ 9 -> 2 is Windows-side, XT caveat.
