# Archive

Root-level notes from earlier sessions, moved here so the repository front page shows only
current material. Kept because they record how conclusions were reached.

**Treat as unverified.** Several were written by a session that left the tree in a broken
state, and some of their conclusions have since been overturned. Anything load-bearing has
been re-derived and now lives in:

- [`../what_worked_and_what_didnt.md`](../what_worked_and_what_didnt.md) — the current inventory
- [`../../FIXES.md`](../../FIXES.md) — the patched files, with tested status stated per file
- [`../windows95_on_inboard386pc_writeup.md`](../windows95_on_inboard386pc_writeup.md) — the narrative

If something here contradicts one of those three, those three win.

## Retired work, kept deliberately

Not session notes — these are complete, correct pieces of work that a better fix superseded.
Each is retained because it remains the right answer for someone whose machine differs.

| | why it was retired |
|---|---|
| [`fd08fix_superseded/`](fd08fix_superseded/) | `FD08FIX.COM` forwarded `INT 13h AH=08h` for floppies to `INT 40h`, fixing #25 in software. #25 was fixed **in hardware** instead on 2026-09-07 — one DIP switch moving the Trantor option ROM `CA000` → `DA000`, so Sergey's floppy BIOS claims `INT 13h` under its own install rule. Removed from the card; carrying an unused resident `INT 13h` hook is exactly what misleads a later session. **Still the answer for anyone whose option ROM addresses cannot be reordered.** Its README also records two boot hangs and the rule they earned — technique 114 |
| [`xtide_pdr_retired/`](xtide_pdr_retired/) | The XT-IDE port driver was abandoned for a SCSI miniport (`.MPD`), which IOS binds to a device node rather than by scanning `IOSUBSYS`. `XTIDEMP.MPD` shipped and serves `C:` in protected mode |
