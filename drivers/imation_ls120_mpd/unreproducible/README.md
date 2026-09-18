# Unreproducible LS-120 miniport builds

These binaries are here because they **cannot be rebuilt**. Every one was linked from a
working tree with uncommitted changes, or is not in `build_ledger.tsv` at all, so no
`git checkout` regenerates them. Technique 89: an artefact under test must be traceable to a
commit, or its results are not evidence.

**Clean-tree builds are deliberately NOT kept here.** The toolchain is deterministic in the
sense that matters: the same source links to the same **code hash**. So a clean-tree row in
`build_ledger.tsv` plus its commit regenerates the driver:

⚠ **It does not regenerate the same md5, and it never will.** A PE header carries a
`TimeDateStamp` that the linker writes per link, so the file hash moves every time even when
nothing in the source changed. Measured 2026-09-18 - three links of identical source gave code
`a98157ec` every time and md5 `3c98b759`, `87ced927`, `ac5cf686`. **Identify a build by its code
hash; use md5 only to match a specific file back to a specific ledger row.**

```
git checkout <commit>
./build.ps1 -Phase 2 -Mode <spp|ecp>
```

That covers 30 of the 44 LS-120 binaries found on the CF card on 2026-09-18. Storing those as
blobs would add nothing and go stale.

| md5 | commit | built | flags | found on the card as |
|---|---|---|---|---|
| `20dea711` | `616744d` | 2026-09-18 17:42 | `-DLS_PHASE2 -DLS_FORCE_MODE=2` | `LS120MP.ECP` |
| `220be39e` | `d21d4a6` | 2026-09-13 22:19 | `-DLS_PHASE2` | `LS120MP.B14` |
| `508ab8d3` | `6db3616` | 2026-09-18 15:44 | `-DLS_PHASE2` | `LS120MP.AUT` |
| `55327f8e` | `3c362a6` | 2026-09-17 23:49 | `-DLS_PHASE2 -DLS_FORCE_MODE=1` | `LS120MP.B26` |
| `5b7b88d2` | `5ad01a8` | 2026-09-14 22:01 | `-DLS_PHASE2` | `LS120MP.bad` |
| `976e4114` | `bc453ca` | 2026-09-11 20:14 | `-DLS_PHASE2` | `LS120MP.B13`, `LS120MP.NEW` |
| `a167d82d` | `9ae99a9` | 2026-09-10 18:18 | `-DLS_PHASE2` | `01_7c83cb6_LS120MP.MPD` |
| `bfc8ef7c` | `b003e66` | 2026-09-14 15:58 | `-DLS_PHASE2` | `LS120MP.B19` |
| `c9fc08b4` | **orphan** | unknown | unknown | `LS120MP.OLD` |
| `fba7dfd6` | `a8aa1f2` | 2026-09-14 16:40 | `-DLS_PHASE2` | `LS120MP.B23` |
| `fd1fce25` | `b256261` | 2026-09-14 19:23 | `-DLS_PHASE2 -DLS_FORCE_MODE=1` | `LS120MP.B24` |

`c9fc08b4` is in no ledger row. It is kept because it is the only copy that exists, not because
anything is known about it. Do not draw a conclusion from it without identifying it first.

## The gap this does not close

`build_ledger.tsv` records what was **built**. Nothing records what was **deployed** - which
md5 sat in `IOSUBSYS` during which boot. That mapping has been reconstructed by hand from
filename conventions (`.B13`...`.B28`) every session, and the conventions are not consistent.
A one-line-per-deployment log would fix it; it does not exist yet.
