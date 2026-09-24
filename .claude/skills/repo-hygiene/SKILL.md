---
name: repo-hygiene
description: Keep this repository legible to outside contributors - short commit messages and PRs, a clean git status, current issue state, and every shipped fix actually published. Use when writing a commit or PR, when git status has drifted, when the README or issues have gone stale, or when asked to tidy the repo.
---

# Repo hygiene

A public repository that other people are expected to contribute to has a second job beyond
being correct: being **readable by someone who was not here**. This skill is the standing
procedure for that.

Origin: @andrew-hoffman raised it on
[issue #3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3) — the git history
had become impossible to follow. He pointed at
[Fabien Sanglard's `agent.md`](https://fabiensanglard.net/agent.md/index.html). The writing rules
below are adapted from it and also live in `CLAUDE.md`, which is always loaded; this skill adds
the periodic tidy-up pass, which is not.

---

## 1. Writing rules

### Commit messages

| Rule | |
|---|---|
| Subject | **50 characters**, 72 absolute maximum |
| Style | Capitalised, no trailing period, imperative — "Fix the alias", not "Fixed the alias" |
| Body | **5 lines or fewer**, wrapped at 72 |
| Content | *What* and *why*. Never *how* — the diff already says how |
| Overflow | If it needs more than 5 lines, it needs a doc. Commit links to it |
| Scope | One logical change. Never touch unrelated code in the same commit |

**Ramble in the plan files, not in the history.** Elimination logs, measurement tables,
handoff notes and reasoning go in `docs/`.

### Pull requests

- Title and opening paragraph stand alone. A reviewer knows what broke, why, and what the
  fix is, without reading the diff.
- State what was tested, on what. State plainly what was **not** tested.
- Minimise changed lines. No drive-by reformatting.

### Prose

Fewest words that carry the meaning. No superlatives, no self-congratulation. Negative
results are valuable but get one line, not a page.

---

## 2. The staging trap — read this before committing

`git commit` commits **everything in the index**, not just what you passed to the `git add`
immediately before it. If an earlier step left renames or `git rm --cached` staged, they get
swallowed by the next commit and its message becomes a lie.

This has already happened once here and needed a `reset --soft` and a redo.

```bash
git status --porcelain          # look before every commit
git diff --cached --stat        # and confirm the index is what you think
```

Also: **`git reset` (mixed) undoes a previous `git rm --cached`.** If you reset to redo a
series, re-apply the untracking afterwards, or you will silently recommit the artefact you
meant to drop.

**Never `git add -A`.** This tree carries large untracked VM directories by design.

---

## 3. The tidy pass

Run when `git status` has drifted, or before pointing anyone new at the repo.

### git status → 0

Every entry is either a deliverable or noise. Noise here has three shapes:

1. **Per-experiment VM directories** (`vm_qbr/`, `vm_shadow/`, `vm_test_*/`…) — throwaway
   86Box working dirs. Ignore the pattern; record the config that mattered in `docs/`.
   **Exception: `vm_win311/` is tracked** — it holds the Win 3.11 Inboard drivers
   (`IBKBD.DRV`, `IBVKD.386`) and the INT 15h shim source.
2. **Per-run captures** — `stderr_log*.txt`, `vram_dump*`, `live_*.txt`, `*_dump.bin`.
3. **Regenerated files that are tracked** — a dump or `.pyc` rewritten by every run shows
   as modified forever. Untrack it; the file stays on disk.

Before ignoring a directory wholesale, check what is *tracked* inside it:

```bash
git ls-files <dir>              # anything here is someone's deliverable
```

### Front page

Root-level `.md` files are the shop window. Session notes, handoffs and "NEXT_SESSION_*"
files belong in `docs/archive/` with a README saying which documents win when they disagree.

### README

- **Two lines per contributor, maximum.** Detail goes in its own doc under `docs/`.
- The "tractable issues" table goes stale fast — regenerate it from `gh issue list --state open`.
- State what is merged upstream, and separately, **what is still not upstream**. That second
  list is the one outside contributors can act on.

### Issues

A reader should not have to scroll a comment thread to learn the current state. Prepend a
compact `> ### Status — <date>` block to the body; leave the original text below it intact.
Say honestly when something is deployed but unmeasured.

### Publication check

Any fix that shipped must be reachable by someone who does not have this working tree:

```bash
find vxd-patches custom_vkd ivt68fix dist -type f \
  \( -iname "*.VXD" -o -iname "*.DRV" -o -iname "*.SYS" -o -iname "*.PDR" \) |
  while read f; do
    git ls-files --error-unmatch "$f" >/dev/null 2>&1 || echo "UNTRACKED: $f"
  done
```

Then cross-check `FIXES.md` lists each one. `HSFLOP.PDR` was deployed to real hardware and
absent from `FIXES.md` and the post-install bundle for a full session — nobody outside could
get it.

**Also check the emulator side.** A fix in `86box_full/` that has general value beyond this
project belongs upstream. Diff against the stock clone rather than trusting memory:

```bash
diff 86box_upstream/src/<file>.c 86box_full/src/<file>.c
```

⚠ **Corrected 2026-09-10: `86box_upstream/` in this tree is NOT a pristine clone.** It carries our
own diagnostic commits on top (`c8d05f2`, `e56c90d`, `bc617db`, `3d26999` — trace toggles, a write
watchpoint, the XT-CF stride-2 work) and is several upstream releases behind, so the diff above
reports hundreds of unrelated files and proves nothing. It is gitignored, so this is local-only
state and nobody cloning the repo sees it. Before trusting that diff again, check the tree is clean:

```bash
git -C 86box_upstream fetch origin
git -C 86box_upstream log --oneline origin/master..HEAD    # MUST be empty
```

If that prints anything, clone `https://github.com/86Box/86Box.git` fresh to a scratch directory
and diff against that instead.

That is how the XT 4-bit DMA page latch was found to be missing from 86Box — `dma_force_xt`
had gone up, but `dma_page_is_xt()` and the `val & 0x0f` truncation had not.

---

## 3b. Line endings and binaries

`.gitattributes` at the repo root pins this. Two things it protects, both suggested by
@andrew-hoffman on issue #3:

1. **`*.sh eol=lf`** — the deploy scripts were LF only by accident. With `core.autocrlf=true`
   and no rules, a fresh Windows clone hands you CRLF copies bash refuses to run.
2. **Explicit `binary`** for `*.VXD`, `*.DRV`, `*.SYS`, `*.PDR`, `*.386`, `*.COM`, `roms/**`.
   Autodetection was getting these right, but it works by sniffing for a NUL byte early in
   the file. A patched VxD that happens not to have one gets silently mangled. This project's
   whole value is byte-exact patches; do not leave that to a heuristic.

CRLF is forced for anything DOS, Windows 3.x/9x or the 1995 DDK MASM toolchain consumes:
`*.BAT`, `*.CFG`, `*.INI`, `*.INF`, `*.ASM`, `*.INC`, `*.DEF`, `MAKEFILE`.

**`86box_full/` is deliberately excluded** — it ships upstream 86Box's own `.gitattributes`,
and a subdirectory's rules win. Leave it that way so the tree stays diffable against master.

### Before changing these rules

```bash
git check-attr binary text eol -- <a few representative paths>   # rules resolve as intended?
git add --renormalize .
git diff --cached --numstat | grep -P '^-	-	'                 # MUST be empty
```

That last line lists binary blobs the renormalize would rewrite (git prints `-` for both
counts on a binary). **If it prints anything, stop** — you are about to corrupt a patch file.

(It deliberately avoids `awk` positional variables: **`$1`/`$2` in a skill file are substituted
with this skill's own invocation arguments when it loads**, silently corrupting the command.
Found the hard way — the line used to read `awk 'everything=="-" && is=="-"'` at load time.)

### The policy being right does not mean the file on disk is right

`eol=crlf` is applied **on checkout**. A file created in the working tree and committed from
there never gets that checkout, so it keeps whatever endings it was written with — while
`git check-attr` and a fresh clone both look perfect.

`PORT.INF` in the XT-IDE port driver (retired to `docs/archive/xtide_pdr_retired/`) sat
LF-only in this working tree for days under a correct
`*.INF text eol=crlf` rule. Windows 95 parses INF files line by line: it read the whole file
as one line and refused the install with *"does not contain information about your hardware"*.
Nothing in git was wrong, and nothing in git would have told you.

Before deploying any text file to a DOS or Win9x guest, look at the bytes, not the rules:

```bash
python -c "d=open('FILE','rb').read(); print(d.count(b'
'), d.count(b'
')-d.count(b'
'))"
```

Zero CRLF and a non-zero second number means the guest will reject it.

#### Bound on that claim, measured 2026-09-05

An **LF-only `XTIDEMP.INF` installed successfully** on Windows 95 - Add New Hardware read it,
created the device node, and copied the driver. So "Windows 95 reads an LF INF as one line" is
not universal; whatever defeated `PORT.INF` had a narrower cause, or depends on the path the
file arrives by. Do not use that story to explain away a failed install without checking the
bytes for yourself.

**CRLF is still the right answer** - `.gitattributes` mandates it, DOS tooling needs it, and the
cost of being wrong is a mystery install failure. But the rule to carry forward is the general
one, not the specific claim: *check the bytes on the artefact you are about to deploy.*

And note what git will and will not tell you here. With `core.autocrlf=true` the object database
stays LF, so converting a working-tree file to CRLF produces **no diff and nothing to commit** -
`git add` reports "no changes added". That is correct behaviour and it means a `git status` of
zero says nothing at all about the bytes you are about to copy onto a card. Deploy from a file
you have just inspected, not from one git says is clean.

### Never write `.gitattributes` with a truncating redirect

`cat > .gitattributes` destroyed 127 lines of pinned binary and line-ending policy here, to
add four lines that were **already in the file**. Read it first; append or edit in place. The
rules above exist precisely because losing them corrupts patch files silently.

A directory-wide `binary` macro will also catch READMEs living in that tree. Use `-text`
instead, so the `*.md` / `*.txt` rules further down the file can still win. That mistake was
made and caught here by `check-attr`, not by eye.

**On this repo the renormalize was a no-op** — `core.autocrlf=true` had already kept the
database LF-clean, so there was no mass normalising commit to make. Check before assuming
you need one.

---

## 4. Verify after writing

Same rule as the hardware work: **a confirmation that looked good is not a confirmation.**

- After any `gh` write, re-read the state. A `gh issue reopen` here reported success and
  did not stick.
- Before trusting a test run, `stat` the built exe against `git log`.
- After a patch script, confirm `Patched: N` with N > 0.

---

## 5. Close the loop

When a contributor's input is verified, shipped, or disproved, **telling them is a
deliverable**, not a nicety. Record it in `docs/contributor_input_ledger.md` with its
`[PRIMARY]` / `[AI-SOURCED]` tag and whether they have been told yet.

---

## 6. "Behind by N commits" does not predict conflicts — apply it and see

2026-09-20: `lpt-epat-bridge` was **34 ahead of and 21,833 behind** 86Box
master, which looked fatal. The submittable subset then applied to current
master with **zero conflicts** — nine files, 1,963 lines, `git apply --3way`.

A big commit count means the *project* moved, not necessarily the files you
touched. Measure the thing you care about:

```bash
git -C <clone> worktree add --detach /tmp/master origin/master
git diff $(git merge-base origin/master HEAD)..HEAD -- <the files you'd submit> > /tmp/subset.patch
cd /tmp/master && git apply --check --3way /tmp/subset.patch
```

Generating the patch from `merge-base..HEAD` **restricted to the submittable
files** is what keeps cherry-picked upstream fixes, local diagnostics and
project-specific code out of it automatically.

### The drift shows up at COMPILE time, not apply time

The same patch then failed to build on one line:

```
src/disk/hdd.c: error: duplicate case value
    case TAPE_BUS_LPT:        <- upstream added this since our merge base
    previously used here: case CDROM_BUS_LPT:
```

Both are `6`. Upstream had independently grown LPT-attached device support and
already handled that value, so **our case was redundant** and the fix was to
delete ours — which is also the minimal diff the PR rules ask for.

**So the order is: apply, build, then run.** A clean apply says nothing about
semantics, and "it applies" is not a validation anyone should be told.

### Check whether upstream already solved it before porting anything

Before the port, confirm the gap still exists on master and that nobody has
filled it:

- our three `ecp_*` device callbacks: **still absent** on master, so still novel;
- a parallel-port ATAPI bridge: **still absent** (the only similar device is
  `lpt_ditto.c`);
- but `lpt_device_t` had gained `strobe`, `read_ctrl`, `epp_write_data` and
  `epp_request_read`, so the device should now *use* those rather than
  reimplement them.

⚠ **Hooks with no consumer get rejected.** An additive callback is only
justified by a device that calls it, so a "small safe interface PR" split out
on its own may be the wrong shape. Decide that before writing the PR, not after.

---

## 7. For a device submission, the acceptance test is a working device

2026-09-20, caught by the owner before a PR went out: *"it has to work and the
drive has to be there for this to be a submission, or people will try it and
say it's failed."*

The evidence assembled for "runs on current master" was:

- `Init Success ls120mp.mpd` in `BOOTLOG.TXT`, and
- `EPAT: device reset: status 50, signature 14 EB` in the emulator log.

Both true. **Neither is a working drive.** `HwInitialize` is non-vetoing, so
`Init Success` says the driver loaded, not that it found anything - which is
already written down as a lesson in `inboard-hw-debug` and was cited anyway.
A bridge answering a reset says the model responds to a register poke, not
that a volume mounted.

**The acceptance test for a device model is: attach it, boot, get a drive
letter.** In this bed that is `tools/fixtures/LSWRITE.BAT` via the runner's
`-Startup`, which writes `drive X` or `no ls120 volume` into `C:\LSPROBE.TXT`
from inside the guest. Unambiguous, and generated by the guest rather than
inferred by us.

⚠ **And test it with the driver a stranger would use.** Every bed run here
used *our* miniport, because the runner's `-Driver` deploys to a hardcoded
`LS120MP.MPD`. The driver a person reproducing the work would reach for is the
vendor's `sd120ppd.mpd`, which sits unused in the bed image at
`C:\LS120FIX\`. Demonstrating the model with a project-local driver does not
support a claim that the device works.

### ...and a stranger has to be able to attach it from the Settings dialog

#8010 (LS-120) and #8012 (BackPack) merged on 2026-09-21 with nothing under
`src/qt/`. Every test here set the bus by hand in `86box.cfg`, so the missing
menu entry never showed up. Users then asked how to configure the devices, and
OBattler added "LPT" to the CD-ROM and removable-disk bus lists the next day
(`90baccc21`).

Before submitting a device, **open Settings in a fresh build and attach it from
there**, starting from an empty config. If it can't be reached, the PR is not
finished: either add the UI or say in the PR that it is config-file only.

### The general rule

Before writing "tested" in a PR, name the **observation** that would convince a
stranger, and check you have it. "The subsystem initialised" is not it. A
submission that fails on first contact costs more than one never sent.

---

## 8. Evidence in a public claim must not be a property of YOUR machine

2026-09-21, caught by the owner on 86Box#8010: the PR said *"the drive
enumerates as `J:`"*. `J:` is not a property of the bridge. It is a property of
this machine's SCSI chain - a Zip at `D:`, a Nakamichi changer's five LUNs at
`E:`-`I:` - so the SuperDisk lands at `J:`. Nobody reproducing it gets `J:`.

The bed image replicates that chain, which is why `LSENUMJ.BAT` probes `J:` and
nothing else, so the bed happily confirmed a letter that means nothing to a
reader.

**Before a claim goes public, separate the observation from the configuration
that produced it.** The claim that travels is *"the drive enumerates, takes a
drive letter, its directory reads, and a write lands"*, with the free-space
delta as evidence - none of which depends on which letter it got.

A useful test: would a stranger who reproduced this correctly still see the
number you quoted? If not, quote the thing they would see.

---

## 9. A rebase moves the branch ref out from under a sibling worktree

Rebasing a branch that another worktree has checked out updates `refs/heads/`
for both. The other worktree's **files are not touched**, so its `git status`
suddenly shows a large staged diff that is really just old-base vs new-base -
here, 660 deleted lines across the sound backend and `machine_table.c`, none of
it anyone's work.

It looks exactly like destroyed local changes, and the reflex is `git reset
--hard`. Check first - it costs one command:

```bash
git diff            # UNSTAGED: real local work. Empty means nothing is at risk
git status --porcelain | grep '^??'   # untracked
git stash list
```

If `git diff` is empty, nothing is at risk and the fix is not a hard reset:

```bash
git checkout HEAD -- .    # refresh index+worktree from the new HEAD
```

**Rebase in a throwaway worktree** (`git worktree add -f <tmp> <branch>`) and
this does not arise - but the branch ref still moves, so the main checkout
still needs the refresh afterwards.

---

## 10. The upstream submission gate - every item, every PR

Each follow-up patch after an 86Box submission traced to a check nobody ran:

| Escaped | PR | Gate that would have caught it |
|---|---|---|
| No Settings entry; OBattler added one (`90baccc21`) | #8010, #8012 | G3 |
| `J:` quoted as evidence - a property of this machine | #8010 | G6 |
| Comments about the LS-120 and `rdisk_*` in a CD-ROM bridge | #8012 | G5 |
| `#define ENABLE_LPT_BPCK_LOG 1` / `ENABLE_EPAT_LOG 1` left forced on | #8010, #8012 | G1 |
| `case` fall-through: every IDE/SCSI CD-ROM also adds a BackPack that claims LPT1 | #8012 | G2 |

Tick all of these, in the PR's worktree, before the owner is asked to open it:

- **G1 No debug left on.** In every touched file, no `#define ENABLE_*_LOG 1`, no forced
  log define, no `pclog` outside a `*_log()` helper, no `#if 1`/`#if 0` experiments.
  `git diff origin/master -- <files> | grep -nE '^\+.*(define +ENABLE_|_LOG +1|#if [01]|pclog)'`
- **G2 Absent means unchanged.** Boot a config that does NOT use the new code (same class
  of device on another bus, nothing on the port) on master and on the branch, both built
  with the new module's log enabled. The logs must match: no new attach, no claimed port,
  IRQ or I/O range. Read every `switch` the diff touches for fall-through into new cases.
- **G3 Settings attach** from an empty config in a fresh Qt build (section 7).
- **G4 Working device** with the driver a stranger would use (section 7).
- **G5 Every comment re-read** against this file, not the one it was ported from
  (`CLAUDE.md`, "Comments are a canary").
- **G6 Every claim reproducible** by a stranger (section 8); what was not tested, said plainly.
- **G7 Builds on current master** with no new warnings: apply, build, run (section 6).
  Re-fetch first - upstream may have fixed or moved it (`git log origin/master -- <files>`).
- **G8 Minimal diff.** No reformatting, no project-local diagnostics, one change per PR.

Record the G1-G8 result in the handoff next to the PR number. An item not run is written as
not run, never as passed.
