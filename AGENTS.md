# AGENTS.md

Rules for any AI agent working in this repository. `CLAUDE.md` is the long version and is
loaded automatically by Claude Code; this file is the short one, for any other tool, and for
anyone who wants to see what we actually hold agents to.

Most of it exists because it was breached first. Where a rule has an origin, it is named — a
rule with a story attached gets followed.

---

## 1. Comments are a canary

@JoshRodd, [86Box/86Box#8010](https://github.com/86Box/86Box/pull/8010), 2026-09-20:

> *"double-check comments it writes to make sure they are not non-sensical ... Comments going
> off the rails could mean code is off the rails, too."*

- A comment explains **why** the code is the way it is — the constraint. Not what the code
  used to do, and not the story of how we found out. The git history is for that.
- **Re-read every comment in a file you generated or ported**, before it is committed.
- A comment describing a different device, a function the file never calls, or a bug fixed
  elsewhere is **evidence the code was moved without being understood**.

This caught a real defect within an hour of being written down: `lpt_bpck.c` was submitted
still describing the LS-120 it was ported from, calling `rdisk_*` functions it does not use,
and carrying a dated `WRITE(10)` diagnostic — in a read-only CD-ROM.

## 2. Alert, do not fix

Same source:

> *"don't tell it to just 'fix it'; tell it to alert you but NOT make any changes so you can
> manually review what went wrong."*

When something looks wrong in work already committed, pushed or submitted: **say what is
wrong and stop.** Do not open an editor. A silent fix destroys the evidence of how it went
wrong, and the owner decides whether it is fixed, reverted or left.

This is stronger than "ask before outward-facing actions" — it applies to the local tree too.

## 3. PRs and replies to people are hand-written

> *"would you be comfortable making sure to hand-write PRs? ... it can be frustrating when an
> AI responds with a comment to a human who typed a comment out."*

Draft on request. **Never post unprompted.** The owner approves the wording and, by default,
sends it. This was asked for directly by an 86Box maintainer after it was breached.

## 4. Audit generated code, especially a whole new file

@JoshRodd's phrase for the failure mode is **"vibe rot"**: a codebase several agents have
contributed to without anyone reading the result. A 1,000-line file that compiles is not a
file that has been reviewed.

## 5. Evidence rules

The ones that cost the most time here:

- **Real hardware outranks inference.** Read the machine before theorising. On 2026-08-25
  every theory derived from source was wrong and all three answers came from the hardware.
- **A negative result from an unverified run is not a result.** Prove the patched file
  actually loaded before explaining why the patch did nothing.
- **Prove the binary you tested is the binary you built** — `stat` it against `git log`.
- **A patch script that reports nothing patched has not patched anything.** Confirm `Patched: N`
  with N > 0.
- **A self-test must be able to fail.** Poison the buffer; get the evidence from outside the
  code under test.
- **Evidence in a public claim must not be a property of one machine.** A drive letter, an IRQ,
  a slot number: would a stranger who reproduced this correctly still see the number you
  quoted? If not, quote what they *would* see.

## 6. Writing

- Commit subjects **50 characters**, bodies **5 lines or fewer**, imperative mood. Say *what*
  and *why*; the diff says *how*. Longer reasoning goes in `docs/`, and the commit links to it.
- One logical change per commit.
- **Never `git add -A`.** This tree carries large untracked VM directories and emulator dumps
  by design. Stage deliberately.
- Fewest words that carry the meaning. No superlatives, no "comprehensive" / "robust" /
  "seamless". Negative results are worth recording, but in one line.

## 7. Cite with the URL, and say what a source did *not* have

Name plus URL plus one line on what it actually gave us. **Record the dead ends too** — that
saves the next reader the fetch. See `docs/resources_and_sources.md`, which marks misses with ❌.

## 8. Close the loop with contributors

People who give good technical input give more of it when they learn it landed. When a lead is
verified, shipped or disproved, **telling them is a deliverable**, not a nicety —
`docs/contributor_input_ledger.md` tracks who is owed what.

And thank them. A reply that is all findings and no thanks reads as extraction.

## 9. Spend the session on the named thing

> *"You might need to save tokens by only actively prompting instead of letting Claude Code run
> in a loop and burn all it wants to and not always on the highest priority action items."*
> — @andrew-hoffman, [issue #3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3)

Do the named thing, report, stop. An unattended loop picks its own priorities and they are not
the owner's. **Cheapest test first** — a config change and one boot beats a build; state the
cost of a step before taking it.

---

Full detail, and the project-specific methodology, in [`CLAUDE.md`](CLAUDE.md) and
`.claude/skills/`.
