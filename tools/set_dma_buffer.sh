#!/usr/bin/env bash
# Add (or remove) the [386Enh] DMA buffer settings on a mounted card.
#
# Andrew Hoffman's reasoning (issue #5, 2026-08-20): the XT's DMA page register is only
# 4 bits, so DMA reach is 20-bit and the buffer must live under 1 MB. A buffer above 1 MB
# does not crash - the page register silently truncates and the 8237 fetches from the wrong
# physical address, so audio PLAYS but is WRONG. That is the distortion symptom.
#
# This was "tested" on 2026-08-20 and recorded as VOID rather than negative, because it ran
# while the corrupted VDMAD was crashing the machine regardless. With that fixed, the test
# finally means something.
#
# Values follow the user's own working Windows 3.11 build on this hardware, which carries
# DMABufferIn1MB=True / DMABufferSize=64. (Per the user those lines were AI-suggested tweaks
# that merely did not break anything, so this shows the setting is TOLERATED here, not that
# it is required - see docs/external_clues_and_correspondence_2026_08.md.)
#
# Usage: ./tools/set_dma_buffer.sh <drive> [--revert]
set -euo pipefail
DST="${1:?usage: set_dma_buffer.sh <drive> [--revert]}"
INI="$DST/WINDOWS/SYSTEM.INI"
[ -f "$INI" ] || { echo "ERROR: no $INI"; exit 1; }
mkdir -p "$DST/PREPATCH"
cp -p "$INI" "$DST/PREPATCH/SYSTEM.INI.pre-dmabuf"
python - "$INI" "${2:-add}" <<'PY'
import sys,re
p,mode=sys.argv[1],sys.argv[2]
s=open(p,'rb').read().decode('cp437')
s=re.sub(r'(?im)^\s*(DMABufferIn1MB|DMABufferSize|HardDiskDMABuffer)\s*=.*\r?\n','',s)
if mode!='--revert':
    block="DMABufferIn1MB=Yes\r\nDMABufferSize=64\r\n"
    assert re.search(r'(?im)^\[386Enh\]',s), "no [386Enh] section"
    s=re.sub(r'(?im)^\[386Enh\]\s*\r?\n', lambda m: m.group(0)+block, s, count=1)
    print("  added: DMABufferIn1MB=Yes, DMABufferSize=64")
else:
    print("  removed the DMA buffer lines")
open(p,'wb').write(s.encode('cp437'))
PY
echo "Done. Reboot."
