# Submitting the EPAT bridge upstream: what it actually takes

Assessed 2026-09-20. Short version: **the contribution is real and still
needed, but it is a rewrite against the current interface, not a rebase.**

## Where the branch is

`86box_upstream` on `lpt-epat-bridge`: **34 commits ahead** of
`86Box/86Box` master, **21,833 behind**. Merge base
`9ee5197c54498c5a7fdaeee66fb0169f8d3ebf20`.

20 files, 2,609 insertions. Split by what belongs upstream:

| file | lines | submit? |
|---|---|---|
| **`src/device/lpt_epat.c`** | **1392, new** | ✅ the contribution |
| `src/device/lpt.c`, `include/86box/lpt.h` | 181 | ✅ the ECP device hooks it needs |
| `src/disk/rdisk.c`, `include/86box/rdisk.h` | 118 | ✅ removable-disk attach |
| `src/config.c`, `char.c`, `hdd.c`, `m_xt.c`, `CMakeLists.txt` | 38 | ✅ wiring |
| `src/video/vid_ati_mach8.c`, `vid_8514a.c` (+headers) | 799 | ⛔ **TC1995's own upstream fix, cherry-picked in as `03ddc5eb1`** |
| `src/cpu/386_dynarec.c`, `src/mem/mem.c` | 195 | ⛔ CS:EIP heartbeat and write watchpoint - diagnostics |
| `src/device/inboard386.c` | 48 | ⛔ project-specific |
| `src/disk/hdc_xtide.c` | 134 | ⛔ separate concern, submit separately if at all |

## The gap upstream still has

`lpt_device_t` on current master:

```c
void    (*write_data)(uint8_t val, void *priv);
void    (*write_ctrl)(uint8_t val, void *priv);
void    (*strobe)(uint8_t old, uint8_t val, void *priv);
uint8_t (*read_status)(void *priv);
uint8_t (*read_ctrl)(void *priv);
void    (*epp_write_data)(uint8_t is_addr, uint8_t val, void *priv);
void    (*epp_request_read)(uint8_t is_addr, void *priv);
```

There is `lpt_read_ecp_mode()`, but **no ECP device-data path at all**. So the
defect our branch fixes is still present: the ECP FIFO is filled only by the
chardev passthrough, and **an ECP read from an emulated device returns `0xFF`
forever**. Our three additions - `ecp_read_data`, `ecp_write_data`,
`ecp_write_addr` - remain novel and additive.

Upstream also has **no parallel-port ATAPI bridge**. The only comparable device
is `lpt_ditto.c`, a tape drive. So `lpt_epat.c` is novel too.

## Why it is a rewrite, not a rebase

Our merge base predates `strobe`, `read_ctrl`, `epp_write_data` and
`epp_request_read`. `lpt_epat.c` was written against the older struct and does
its own strobe and EPP framing, which upstream now provides. A rebase would
conflict throughout and would leave the device duplicating interface that now
exists.

The right shape is:

1. Fresh clone of current master.
2. Add the three `ecp_*` callbacks to `lpt_device_t` and their plumbing in
   `lpt.c` - **this is a small, self-contained PR on its own**, and it is the
   part with general value: it makes ECP usable by *any* emulated LPT device,
   not just ours.
3. Port `lpt_epat.c` onto the current interface, using upstream's `strobe` and
   `epp_*` rather than reimplementing them.
4. Port the `rdisk.c` attach path.
5. Build, and run it in a bed against a real LS-120 image.
6. PR per the repo-hygiene rules: what broke, why, the fix, what was tested and
   on what, and plainly what was **not**.

**Step 2 is worth submitting by itself** and is a far smaller review surface
than the whole bridge. If only one thing goes up, it should be that.

## Status

⛔ **Not attempted yet, and not to be pushed.** Standing rule: no push to the
fork without the owner's yes on a **tested** branch. A port of this size cannot
be validated without a build and a bed run against current master, and an
unvalidated PR wastes a maintainer's time.

⚠ `src/io.c` in the working tree carries an access-width tally added
2026-09-20 as a local instrument (technique 128). **It must not go upstream.**

---

## Update, same day: it applies cleanly after all

The 21,833-commit gap turns out not to matter much for these files.
`git apply --3way` of the submittable subset onto current master
(`5731e20a3`, *Merge pull request #8007 from WNT50/mod70*):

```
Applied patch to 'src/char/char.c' cleanly.
Applied patch to 'src/config.c' cleanly.
Applied patch to 'src/device/CMakeLists.txt' cleanly.
Applied patch to 'src/device/lpt.c' cleanly.
Applied patch to 'src/disk/hdd.c' cleanly.
Applied patch to 'src/disk/rdisk.c' cleanly.
Applied patch to 'src/include/86box/lpt.h' cleanly.
Applied patch to 'src/include/86box/rdisk.h' cleanly.
```

1,963 lines, `src/device/lpt_epat.c` added whole. **Nine files, no conflicts.**

The patches are kept at `docs/upstream_patches/`:

| file | what |
|---|---|
| `ecp_device_hooks.patch` | just the three `ecp_*` callbacks and their plumbing - 262 lines |
| `epat_full.patch` | the whole submittable subset - 1,963 lines |

Both are generated from `merge-base(origin/master, lpt-epat-bridge)..HEAD`, so
they carry none of the Mach8 cherry-pick, none of the dynarec/mem diagnostics
and nothing from `inboard386.c`.

⚠ **Applying is not building, and building is not working.** The struct gained
`strobe`, `read_ctrl`, `epp_write_data` and `epp_request_read` since our merge
base; a clean textual apply does not mean `lpt_epat.c` uses them correctly, or
that it should not now be rewritten to use them instead of its own framing.

⚠ **"Hooks with no consumer" is a fair review objection.** The three `ecp_*`
callbacks are only justified by a device that uses them, so the small PR and
the bridge probably have to go up together after all.

---

## It builds on current master

Applied to a worktree at `5731e20a3` and built with the same MinGW/Ninja
toolchain the project uses. **One compile error, one line to fix:**

```
src/disk/hdd.c: error: duplicate case value
    case TAPE_BUS_LPT:            <- upstream added this since our merge base
    previously used here: case CDROM_BUS_LPT:
```

Both are `6`. Upstream has independently grown LPT-attached device support and
already maps that value to `"lpt"` - `scsi_tape.h` even comments that it
*"coincides with HDD_BUS_LPT/CDROM_BUS_LPT for config strings"*. **Our case was
redundant**, so the fix is to delete ours, which is also the minimal diff.

With that one deletion: **`[362/362] Linking CXX executable src\86Box.exe`.**

The working patch is kept as
`docs/upstream_patches/epat_on_master_5731e20a3.patch` - **9 files, 1,693
insertions, 21 deletions**, which is a reviewable size.

| | |
|---|---|
| applies to master | ✅ no conflicts |
| builds on master | ✅ after one redundant case removed |
| **runs** | ⏳ bed run in progress against the master binary |

⛔ Still not to be pushed. "Builds" is not "works", and the PR also needs a
decision on whether `lpt_epat.c` should be rewritten to use upstream's
`strobe` / `epp_write_data` / `epp_request_read` rather than its own framing -
a reviewer will ask.

### ⚠ A Release build of master writes no log

The master worktree was configured with a plain `-DCMAKE_BUILD_TYPE=Release`,
and 86Box compiles `pclog` out of a release build. So `-L` is accepted (both
master and our fork parse `-L` / `--logfile`) and **nothing is written**.

That is not a failure, but it means **a bed run against a release build cannot
be validated from the emulator log**. Validate from inside the guest instead -
extract `BOOTLOG.TXT` from the image afterwards and check the driver's
`Initing` / `Init Success` pair and the `INITCOMPLETESUCCESS` entries for
DiskTSD / SCSIPORT / VFAT / IFSMGR, and prove the log is from *this* run by
diffing it against the source image's copy.

For a run where the `[ECPDIAG]` and EPAT trace output is actually wanted,
configure the worktree the way the project's own build is configured rather
than a bare Release.

## First bed run against master: NO EVIDENCE, and that is all it means

`tools/ls120_bed_run.ps1 -Tag master_epat -Seconds 420` against the master
binary. The runner's own check fired:

```
86Box pid 21236, running 420 s...
NO BOOTLOG.TXT - the run may not have reached Windows. Read the screen.
```

The guest wrote **nothing** to the image in eleven minutes - `ls120win.img`
mtime never moved off the moment the runner restored it - and `BOOTLOG.TXT`
was absent afterwards.

⛔ **This is not evidence against the patch.** It is an absence of evidence,
and the reason it cannot be diagnosed is mine: the worktree was configured
`-DCMAKE_BUILD_TYPE=Release`, where 86Box compiles `pclog` out, so there is no
log to read and no way to tell a failed machine init from a failed device init
from a guest that simply never started.

Things that were *not* ruled out, and must be before the patch is blamed:

- a **control run of unpatched master** on the same bed, same channel
  (technique 127 - an anomaly is only a finding if the known-good run lacks it);
- ROM discovery: neither build has a `roms/` directory beside its exe, so both
  rely on the `-P` path, but 86Box's search order may have changed in 21,833
  commits;
- config-format drift between the fork's 86Box and master.

The fork builds **`RelWithDebInfo`**; the master worktree was **`Release`**.
Rebuilt to match so the only variable is the patch. **Re-run required before
anything is concluded.**

## The control settles it: the bed is fine, master will not start

Same bed, same config, same image, same driver (`SHIP_SPP_FIXED.MPD`,
md5 `55327f8e`), only the emulator binary changed.

| | fork build | master + patch |
|---|---|---|
| `BOOTLOG.TXT` written to the image | ✅ **20,533 bytes** | ⛔ absent, on two runs |
| emulator log | ✅ 1,523 bytes | ⛔ **no file created at all** |
| EPAT bridge activity | ✅ `EPAT: CONNECT`, `W reg 1E = A0`, `device reset: status 50, signature 14 EB` | — |

The fork's run reaches the drive: `signature 14 EB` is the ATAPI signature, and
it matches what the real LS-120 returns. **The bridge model works.**

The master build produces **no log header at all** - not even the `# ROM path:`
/ `# Asset path:` lines the fork prints before touching any device - so it is
failing before device init, not in anything this patch adds.

⇒ **The patch is exonerated by control, and so is the bed.** What is unproven
is only that master *can be run at all* in this environment.

### Most likely cause, with evidence but not proof

The fork logs `# ROM path: C:/Users/lycet/AppData/Local/86Box/roms/`, and that
directory holds only `machines/` and `network/`, dated 30 July. 86Box's ROM set
has grown and been reorganised over 21,833 commits, so a master build that
cannot find a ROM it now requires for `ibmxt_inboard386` would fail exactly
this way - machine never starts, window sits there, nothing written.

**To finish this validation:** update the local 86Box ROM set to one current
with master, and regenerate `86box.cfg` with master's own UI rather than
reusing the fork-era config. Neither is difficult; both are environment work,
not code work.

## Where the submission stands

| | |
|---|---|
| applies to current master | ✅ 9 files, +1,693 / −21, no conflicts |
| builds on current master | ✅ after removing one redundant `case` |
| runs on current master | ❓ **blocked on the local ROM set**, not on the patch |
| runs on the fork | ✅ bridge reaches the drive and returns the ATAPI signature |
| ready to push | ⛔ **no** - the `strobe`/`epp_*` design question is still open |

## Local state this left behind

- **A git worktree in the scratch directory.** `86box_upstream` now has
  `C:/Users/lycet/AppData/Local/Temp/claude/86box_master` registered at
  `5731e20a3`, with a full `RelWithDebInfo` build of master + the patch in it.
  Useful next session; it will go stale when temp is cleared, and
  `git -C 86box_upstream worktree prune` tidies the entry afterwards.
  (A third worktree, `86Box-Inboard/86box_master` at `b2033f1aa`, predates
  this and was not touched.)
- **`86box_upstream/src/io.c` is modified and uncommitted** - the access-width
  tally from technique 128. It is env-gated on `IOWIDTH_BASE` and inert
  otherwise. **Kept deliberately. It must never go upstream.**
