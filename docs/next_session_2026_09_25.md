# Next session - handoff from 2026-09-24, end of day

The owner picks tomorrow's priorities. Two are already agreed: **project work now the 5160 has
networking** (the 3C509B, #8076 merged), and **#44, the vendor LS-120 DOS driver on the EPAT
model**. Detail for today is in `docs/next_session_2026_09_24b.md`.

## Where things stand

| | state |
|---|---|
| 86Box #8076 (3C509B) | **merged** 2026-09-24 by OBattler. README and #20 updated. Tested by others under NT 3.5 and Linux |
| 86Box #8078 (LPT bridges) | **open**, 8 commits, owner's wording, every gate G1-G10 passed. No standing watch: check it when starting upstream work; the owner writes every reply |
| `pic.c` IRQ 9 as IRQ 2 | local branch `3c509b-isa` (`ef082884b`), not submitted - #42 |
| #44 LS-120 DOS driver | open, below |

## Close the loop with @andrew-hoffman, as each input is tested

His inputs from #42 and #35 (ledger: `docs/contributor_input_ledger.md`, "#42 and #35, 2026-09-22
to 09-24"). When one is tested, tell him the result - with thanks for his continued input. The owner
writes the reply; draft it on request.

| input | tested? | owner replied? |
|---|---|---|
| upstream's `net_3c59x_eisa.c` before building a 3C509B | built; #8076 merged | yes, 09-24 - a closing note that it merged is still owed |
| VPICD must take IRQ 2 from the master, not as the cascade | confirmed by reading our `VPICD_INBOARD.VXD`; patch not written | yes, 09-23 |
| A/B the T130B with and without an interrupt, no NIC, first | **not yet** - do this before any #42 work | **no** |
| WDEB386 over serial for the Protection Error | **not yet** | **no** |
| does `INBRDPC.SYS` refuse to shadow a non-IBM EGA ROM? disassemble it (#35) | **not yet** | acknowledged 09-22 |

## #44 - where to start

`SD120PPD.SYS` + `ASPIHDRM.SYS` report "Error initialising adapter" on master and on #8078
alike. The bridge log shows the preamble and the chain scan (unit 0 answers `FFAA`), then no
connect; `CPP(0x40)` and `CPP(0x50)` are logged as unknown. Implement them in
`src/device/lpt_epat.c` from `drivers/imation_ls120/TRANSPORT_SPEC.md:846-866`, then trace past
the scan. The real driver runs ECP on the Intek21, so an ECP or EPP step may follow. Estimate:
one session for the first gate, unknown tail. It becomes its own PR, gated G1-G10.

Bed: `python tools/bed_stage_at.py <dir> --lpt1 ls120-dos --dos`, then
`tools/bed_launch.ps1 -Exe <exe> -VmPath <dir> -Seconds 0 -AllowMouse`. Master control first.

## Networking - notes for picking priorities

- The real card is at `0x320` / IRQ 3, installed from the list (Windows' hardware search records an
  IRQ `NET3COM.INF` rejects). mTCP works from DOS.
- #42 (IRQ 9 -> 2) exists only to free IRQ 3 for the T130B. Andrew's steer stands: A/B the T130B
  with and without an interrupt first (the bed's T130B model offers IRQ 3/4/5/7) - it may make
  #42 unnecessary. WDEB386 over serial for the Protection Error if it goes ahead.
- #41: `ELNK3.VXD` polling can now be measured in the bed, since the card model is upstream.
- The NIC's EEPROM enables a boot-ROM window at `D0000h`, where Sergey's floppy BIOS lives (#35).
  `3C5X9CFG` can disable it; not yet done.

## How to run the bed now

- **Launch only through `tools/bed_launch.ps1`** (inboard-hw-debug 131 section 0): it refuses what
  put dialogs on the owner's desktop today and warns when the exe predates HEAD.
- Staging traps are in technique 132. Removable-disk types are positional: LS-120 is **7** on
  current master.
- `mouse_type = none` except on a Windows bed that runs a StartUp probe (technique 87).
- The owner drives every check a submission depends on (gate G10).

## Reported, not fixed

- Upstream renumbered removable-disk types: an old config's 5/6/7 silently becomes another drive.
  Owner to decide whether to report it.
- `tools/ls120_bed_run.ps1:95` stops every 86Box.
- `vm_bpck/86box.cfg.master` is the old Ditto config.
- `drivers/3c509b/3c509.asm` shows modified: line endings only.
