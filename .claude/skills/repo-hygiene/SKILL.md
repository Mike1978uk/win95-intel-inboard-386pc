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
