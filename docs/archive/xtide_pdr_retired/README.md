# RETIRED — the XT-IDE IOS port driver (`PORT.PDR`)

**Do not build this. Do not install these binaries.** Superseded 2026-09-05 by the SCSI
miniport, [`drivers/xtide_mpd/`](../../../drivers/xtide_mpd/), which is confirmed on the real
5160 and published in [`FIXES.md`](../../../FIXES.md).

Kept because it is cited evidence, not because it is a fallback.

## Why it was retired

It reached the disk and served it, then **wedged Windows at shutdown** — four sessions of
bisecting, ending in a VMM spin our driver was not even in. The miniport deletes the whole layer
rather than debugging it: SCSIPORT owns the polling contract, the DCB lifecycle and
scatter/gather, and every bug in that investigation lived in one of those three.

Costing and the control that justified the switch: [`../../scsi_miniport_costing.md`](../../scsi_miniport_costing.md).

## `binaries/` — why these still exist

Moved out of `dist/` deliberately. `dist/` is where someone downloads a working fix, and
**`PORT_claim_master_stride2_rw.pdr` (md5 `0fe2431a`) hangs Windows at shutdown on real
hardware.** It must not be reachable as a published artefact.

It survives here because it is the **tracked, reproducible control** that made the Technique
88/89 retraction defensible — the hang was first diagnosed from a binary that could not be
rebuilt from any commit, and this one could. Deleting it would remove the evidence for a
correction the debug skill still relies on.

`source/build_ledger.tsv` is kept for the same reason: it maps md5 → commit for every binary
this line ever produced, including the unreproducible `70298a8f`. If one of these files turns up
on a card, that ledger is what identifies it.

## What came out of it that is still live

The dead end paid for a large part of the debugging methodology — Techniques 78-91 in
`.claude/skills/inboard-hw-debug/SKILL.md` are mostly from this work: the DDK sample's missing
stack frame, the scatter/gather descriptor layout, the IOS polling contract, the appy-time rule,
and the "substitute a known-good component as a control" move that produced the miniport.

`source/src/XTIDETR.ASM` is the **ancestor** of the shipped transport, not a copy of it — the
two have since diverged. The live one is `drivers/xtide_mpd/src/XTIDETR.ASM`.
