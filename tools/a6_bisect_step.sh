#!/usr/bin/env bash
# git bisect run step for the A6 Mach32 exit: build this commit, run M8FG6 on a Mach32 in vm_6695_w2.
# Exit 0 = probe completed (good), 1 = 86Box exited first (bad), 125 = does not build (skip).
#   cd 86box_a6base && git bisect run ../tools/a6_bisect_step.sh
ROOT=/c/Users/lycet/RiderProjects/86Box-Inboard
export PATH=/c/msys64/mingw64/bin:$PATH
cmake --build build -j 8 > build_bisect.log 2>&1 || exit 125
H=$(git rev-parse --short HEAD)
cd "$ROOT"
A6_PROBES="M8FG6" tools/a6_regress.sh "$ROOT/86box_a6base/build/src/86Box.exe" mach32_isa "bisect_$H" > /dev/null 2>&1
[ -f "docs/captures/2026-10-10_a6/bisect_$H/DONE.TXT" ] && exit 0 || exit 1
