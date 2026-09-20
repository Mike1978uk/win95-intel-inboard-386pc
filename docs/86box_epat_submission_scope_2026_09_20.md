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
