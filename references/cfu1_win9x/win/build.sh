#!/bin/sh
# Build CFU1.VXD + CFUDIAG.EXE for Windows 95/98 on macOS
# using Open Watcom v2 (native arm64) + JWasm.
#
# Prereqs: ~/tools/ow2 (OW2 snapshot with armo64/), jwasm in armo64/,
# and the patched Win98 DDK includes in ./ddkinc (see ddkinc/vmm.inc,
# patched for JWasm -- the original is vmm.inc.orig).
set -e
cd "$(dirname "$0")"

export WATCOM="$HOME/tools/ow2"
export PATH="$WATCOM/armo64:$PATH"
export INCLUDE="$WATCOM/h:$WATCOM/h/nt"

echo "=== CFU1.VXD ==="
jwasm -q -coff -D BLD_COFF -D IS_32 -D MASM6 -D DEBLEVEL=0 \
      -Iddkinc -Fo=vxd/CFU1.obj vxd/CFU1.ASM
# NB: must be DYNAMIC or Win98 CreateFile("\\\\.\\...") fails with err 2
wlink format windows vxd dynamic option quiet, map=vxd/CFU1.map \
      name vxd/CFU1.VXD file vxd/CFU1.obj export CFU1_DDB.1

echo "=== CFUDIAG.EXE ==="
wcc386 -bt=nt -zq -w4 -ox -fo=diag/CFUDIAG.obj diag/CFUDIAG.C
wlink system nt option quiet name diag/CFUDIAG.EXE \
      file diag/CFUDIAG.obj library advapi32.lib

mkdir -p dist
cp vxd/CFU1.VXD diag/CFUDIAG.EXE dist/
[ -f CFU1.INF ] && cp CFU1.INF dist/
ls -la dist/
