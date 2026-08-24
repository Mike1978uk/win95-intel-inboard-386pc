#!/usr/bin/env bash
# Deploy (or revert) the 20-bit DMA sound fix on a MOUNTED Windows 95 drive - the CF card
# in a reader, or any mounted copy of the image.
#
# What it changes: C:\WINDOWS\SYSTEM\MSSBLST.VXD, two bytes, maxPhys 0xFFF -> 0xFF. The XT's
# DMA page latch is 4 bits, so DMA reach is 20-bit; MSSBLST asks _PageAllocate for a buffer
# anywhere below 16 MB, gets one above 1 MB, and the latch truncates the address - the card
# then DMAs from whatever is at the truncated address and plays that. See
# docs/xt_dma_20bit_audit_2026_08_24.md.
#
# MSSBLST.VXD is dynamically loaded from WINDOWS\SYSTEM (confirmed in BOOTLOG.TXT:
# "Dynamic load success mssblst.vxd"), NOT bundled into VMM32.VXD, so unlike the VDMAD/VKD/VPICD
# patches this does NOT need a pre-monolith image - a plain file copy takes.
#
# Verified in the emulator 2026-08-24 on a copy of this same image: page 0x4E -> 0x09, no
# truncation, audio clean by ear.
#
# Usage: ./tools/deploy_sound_fix.sh <drive> [--revert]
set -euo pipefail
DST="${1:?usage: deploy_sound_fix.sh <mounted-drive-path, e.g. /d> [--revert]}"
MODE="${2:-deploy}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$DST/WINDOWS/SYSTEM/MSSBLST.VXD"
BAK="$DST/PREPATCH"

[ -f "$DEST" ] || { echo "ERROR: no $DEST - is $DST the right drive?"; exit 1; }
mkdir -p "$BAK"

if [ "$MODE" = "--revert" ]; then
    SRC="$BAK/MSSBLST.VXD"
    [ -f "$SRC" ] || SRC="$REPO/vxd-patches/sound/MSSBLST_stock.VXD"
    echo "Reverting from $SRC"
else
    SRC="$REPO/vxd-patches/sound/MSSBLST_INBOARD.VXD"
fi

STOCK=cc7e63aacb1f599fcd5b3fa1eb98169c
PATCHED=dcf32b4a7d8dbcc47e659847742417b6
NOW=$(md5sum < "$DEST" | cut -d' ' -f1)
echo "  before: $NOW  $(stat -c%s "$DEST") bytes"

# The patch is two bytes at fixed offsets in ONE known binary. If the card is carrying some
# other build of MSSBLST.VXD, those offsets mean something else there and copying this file
# over it would install code that was never audited against it. Refuse rather than guess -
# a patch script that cannot prove it matched is the failure mode of patch_vdmad.py.
if [ "$NOW" = "$STOCK" ]; then
    if [ "$MODE" = "--revert" ]; then echo "Already stock; nothing to do."; exit 0; fi
elif [ "$NOW" = "$PATCHED" ]; then
    if [ "$MODE" != "--revert" ]; then echo "Already patched; nothing to do."; exit 0; fi
else
     echo "REFUSING: $DEST is md5 $NOW, which is neither the stock binary this patch was"
     echo "          derived from ($STOCK) nor the patched one."
     echo "          Audit it first:  python tools/vxd_dma_audit.py \"$DEST\""
     exit 1
fi

if [ "$MODE" != "--revert" ]; then
    # Back up once, and never over an existing backup - a second run after a successful
    # deploy would otherwise save the PATCHED file as the "original".
    if [ -f "$BAK/MSSBLST.VXD" ]; then
        echo "  backup already exists at $BAK/MSSBLST.VXD, left alone"
    else
        cp -p "$DEST" "$BAK/MSSBLST.VXD"
        echo "  backed up -> $BAK/MSSBLST.VXD"
    fi
fi

cp "$SRC" "$DEST"
sync
echo "  after:  $(md5sum < "$DEST" | cut -d' ' -f1)  $(stat -c%s "$DEST") bytes"

if cmp -s "$SRC" "$DEST"; then
    echo "Verified: on-disk file matches $(basename "$SRC")."
else
    echo "VERIFY FAILED: the copy did not land. Do not boot this card."; exit 1
fi
echo
echo "Expected md5s:  stock   cc7e63aacb1f599fcd5b3fa1eb98169c"
echo "                patched dcf32b4a7d8dbcc47e659847742417b6"
echo "Eject cleanly, boot, and listen. Revert with: $0 $DST --revert"
