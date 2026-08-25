# Project Context

Windows 95 on a real IBM 5160 fitted with an Intel Inboard 386/PC. This repository holds:

1. **`86box_full/`** — 86Box fork with the Inboard 386/PC hardware model (`src/device/inboard386.c`)
2. **`hardware/`** — real 5160 reverse-engineering (INBRDPC.SYS, PAL/GAL analysis)
3. **`vxd-patches/`, `custom_vkd/`, `ivt68fix/`** — the Windows 95 guest-side fixes
4. **`docs/`** — the writeup, plans, and correspondence

See `README.md` for the quick start and `docs/what_worked_and_what_didnt.md` for the fix inventory.

## Writing rules

Credit: these are adapted from [Fabien Sanglard's `agent.md`](https://fabiensanglard.net/agent.md/index.html),
suggested by @andrew-hoffman on issue #3 — the git history had become hard to read.

### Commit messages

- **Subject: 50 characters, 72 absolute maximum.** Capitalised, no trailing period,
  imperative mood ("Fix the alias", not "Fixed the alias").
- **Body: 5 lines or fewer, wrapped at 72.** Say *what* and *why*, never *how* — the diff
  already says how. If it needs more than 5 lines, it needs a doc.
- **Ramble in the plan files, not in the history.** Long reasoning, elimination logs,
  measurement tables and handoff notes go in `docs/`; the commit links to them.
- One logical change per commit. Do not touch unrelated code in the same commit.

### Pull requests (upstream 86Box especially)

- Title and opening paragraph must stand alone. A reviewer should know what broke, why,
  and what the fix is, without reading the diff.
- State what was tested and on what. Say plainly what was *not* tested.
- Minimise changed lines. No drive-by reformatting.

### Prose

- Use as few words as possible; pick every word deliberately.
- No superlatives, no self-congratulation, no "comprehensive"/"robust"/"seamless".
- Negative results are worth recording, but in one line — see `docs/what_worked_and_what_didnt.md`.

## Use the skills — without being asked

This file loads every session; the skills do not. **Invoke the matching skill BEFORE deriving an
approach, not after.** Each one exists because the approach was already worked out the hard way,
and re-deriving it wastes a session and repeats mistakes that are already written down.

**Route by what the task is, first.** The domain decides the skill; the symptoms below are
just the common cases.

| If the work is… | Use | Typical triggers |
|---|---|---|
| **Git, GitHub, docs** — commits, PRs, issues, releases, the README, anything public-facing | **`repo-hygiene`** | `git status` has drifted; raising or merging a PR; closing an issue; shipping a patched file; before pointing anyone new at the repo; asked whether things are tidy or current |
| **The emulator or the real 5160** — a bug, a hang, a device, timing, POST | **`inboard-hw-debug`** | Boot hang, black screen, POST code, device not detected, reset, timing mismatch. Also before proposing *any* new diagnostic approach — check the numbered techniques first |
| **A driver whose data is wrong** (a sub-case of the above, common enough to be its own skill) | **`win9x-dma-driver-audit`** | Distorted audio, garbled transfers, stalling reads — the device works but the bytes are wrong. Also before trusting any Windows 9x driver on this machine |

If a task spans two domains — fixing an emulator bug *and* then raising the PR for it — use each
skill at the point its domain starts, not both up front.

Two rules that apply to all of them:

- **Do not assert what the skill would have checked.** The last time tidiness was asserted rather
  than the pass run, it missed four stale claims and a self-corrupting command in a skill file.
- **Keep them current.** A technique that resolves *or rules out* a real bug gets written back,
  including retractions — several numbered techniques carry corrections to their own earlier
  conclusions, and those are the most valuable lines in the file.

**But don't burn tokens on them.** A skill is a once-per-topic load, not a per-step ritual:

- **Once per session per skill.** Already loaded this session? Follow it; don't reload it.
- **Not for mechanical work.** A one-line edit, a commit, a grep, answering a question from
  context — just do it. The trigger is *starting a new investigation* or *a real tidy pass*,
  not touching a file the skill happens to mention.
- **Not to confirm what you already know.** If the answer is in the conversation, use it.

**Never use `$1` or `$2` in a skill file.** They are substituted with the skill's invocation
arguments at load time and will silently corrupt the surrounding command.

## Working rules

- **Never `git add -A`.** Stage deliberately. This tree carries large untracked VM
  directories and emulator dumps by design.
- Verify a claim before committing it. `stat` the built exe against `git log` before
  trusting a test run; re-read GitHub state after any `gh` write.
- Patch scripts can silently no-op — always confirm `Patched: N` with N > 0.
- Never propose disabling `INBRDPC.SYS` as a test variable; it is required, not optional.
- Closing the loop with a contributor whose input was verified, shipped, or disproved is
  a deliverable, not a nicety. Ledger: `docs/contributor_input_ledger.md`.
  **This applies to people who engaged with this project** — raised an issue, answered a
  question, sent a config. It does **not** extend to every source cited: for a public
  write-up we merely relied on, credit by reference is sufficient and no reply is owed.
  Ask before treating a citation as a contact.
- **Always cite with the URL.** For a source we relied on, the link is the credit — and it points
  the next person at something useful. Name plus URL plus one line on what it actually gave us.
  Record what a source did **not** contain too: that saves the next reader a dead end.
