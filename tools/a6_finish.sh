#!/usr/bin/env bash
# A6 tail, 10-10: wait for the running HEAD 8514/A run, rebuild HEAD with the diagnostic reverted (3667abf59),
# then rerun the branch-point 8514/A set and the Mach32 set on the fixed HEAD. Results only; compare next session.
cd "$(dirname "$0")/.."
until [ -f docs/captures/2026-10-10_a6/head_8514a/DONE.TXT ] && ! tasklist | grep -qi 86box; do sleep 20; done
sleep 10
(cd 86box_3c509b && PATH=/c/msys64/mingw64/bin:$PATH cmake --build build_log -j 8 > build_log.txt 2>&1)
tools/a6_regress.sh "$PWD/86box_a6base/build/src/86Box.exe" 8514a base_8514a
tools/a6_regress.sh "$PWD/86box_3c509b/build_log/src/86Box.exe" mach32_isa head_mach32_fixed
