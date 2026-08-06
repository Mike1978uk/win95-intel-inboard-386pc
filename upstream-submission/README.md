# Upstream 86Box submission

This is a copy of exactly what was submitted to the official 86Box project:

**[86Box/86Box#7626 — Add the Intel Inboard 386/PC accelerator card for the IBM XT](https://github.com/86Box/86Box/pull/7626)**

It's a deliberately minimal, clean-room subset of this project's full fork
(`86box_full/`) — just the Inboard 386/PC hardware device model and the small
number of core-file timing/PIC/DMA fixes it depends on. It does **not**
include the Mach8 video work, T130B SCSI, 3C509B stub, or any of the
debug/tracing hooks that live in `86box_full/` — those are out of scope for
what 86Box's own maintainers need to review.

The **emulator build linked from the main README and GitHub Release** is
built from `86box_full/`, not from this folder — that's the fully-featured
fork with everything needed to actually run Windows 95 on this hardware, and
is the one to use if you just want to try it. This folder exists purely as a
public record of what was proposed upstream, in case that PR is ever amended,
rebased, or lost to history on GitHub's side.

## Contents

- `src/device/inboard386.c`, `src/include/86box/inboard386.h` — the new
  device model (as submitted)
- `core-files.patch` — a unified diff of the small changes to existing 86Box
  files (`cpu.c`/`cpu.h`, `x86_ops_io.h`, `x86_ops_jump.h`, `pic.c`,
  `dma.c`, `m_xt.c`, `machine_table.c`, `device/CMakeLists.txt`, and the
  corresponding headers) that the device model depends on

Apply the patch against a clean 86Box checkout with:

```
git apply core-files.patch
```

then drop the two files above into their listed paths.
