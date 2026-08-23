#!/usr/bin/env bash
# Apply the full Inboard patch set to a PRE-MONOLITH Windows 95 OSR1 install.
#
# "Pre-monolith" means Setup has not yet run its VMM32.VXD combine step: the
# individual VxDs still exist as files in WINDOWS\SYSTEM\VMM32\ and VMM32.VXD is
# still the stock 411,132-byte copy from the CAB. Patched VxDs dropped into that
# folder are baked into the combined VMM32.VXD by Setup itself, which is the only
# route that reliably takes - see the 2026-08-23 finding that a VxD replaced after
# the combine was never loaded at all (BOOTLOG showed "Loading Vxd = VDMAD", the
# bundled form, and VMM32.VXD is W4-compressed so it could not be patched in place).
#
# Originals are backed up to <drive>\PREPATCH\ before anything is overwritten.
#
# Usage: ./deploy_premonolith.sh /d
set -euo pipefail
DST="${1:?usage: deploy_premonolith.sh <mounted-drive-path, e.g. /d>}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BAK="$DST/PREPATCH"

[ -d "$DST/WINDOWS/SYSTEM/VMM32" ] || { echo "ERROR: $DST is not a Windows 95 install"; exit 1; }
[ -f "$DST/WINDOWS/SYSTEM/VMM32/VDMAD.VXD" ] || {
    echo "ERROR: no staged VxDs in WINDOWS\SYSTEM\VMM32 - this image is POST-monolith."
    echo "       Patching it would not take. Use a pre-monolith image."; exit 1; }
mkdir -p "$BAK"

put () {  # put <source> <dest-relative-path>
    local src="$1" rel="$2" dest="$DST/$2"
    if [ -f "$dest" ]; then
        local b="$BAK/$(basename "$rel")"
        [ -f "$b" ] || cp -p "$dest" "$b"
    fi
    cp "$src" "$dest"
    printf '  %-34s <- %s\n' "$rel" "${src#$REPO/}"
}

echo "Deploying to $DST (originals -> $BAK)"

# 1. INBRDPC.SYS self-test skip
put "$REPO/vxd-patches/osr1/INBRDPC_selftest_skip.SYS"        "INBRDPC.SYS"
# 2. VPICD - phantom slave 8259 at 0xA0/0xA1 neutered (36 sites, all verified)
put "$REPO/vxd-patches/osr1/VPICD_INBOARD.VXD"                "WINDOWS/SYSTEM/VMM32/VPICD.VXD"
# 3. VDMAD - phantom DMA controller 2 neutered. THE FIXED BUILD: the older
#    VDMAD_INBOARD.VXD corrupted OBJ1:0x1660 and is the cause of the issue #5 BSOD.
put "$REPO/vxd-patches/VDMAD_INBOARD_FIXED.VXD"               "WINDOWS/SYSTEM/VMM32/VDMAD.VXD"
# 4. Custom VKD built from the Win95 DDK source (port-64h discard removed, port-61h XT ack)
put "$REPO/custom_vkd/build/VKD_CUSTOM_INT09FIX_v2.VXD"       "WINDOWS/SYSTEM/VMM32/VKD.VXD"
# 5. KEYBOARD.DRV - not part of the combine, but staged here for one atomic deploy
put "$REPO/vxd-patches/osr1/KEYBOARD_INBOARD.DRV"             "WINDOWS/SYSTEM/KEYBOARD.DRV"
# 6. INT 68h vector fix - the F000:FF53 build (Necasek), NOT the older 0x3C0 stub
put "$REPO/ivt68fix/IVT68FIX.COM"                             "IVT68FIX.COM"
# 7. COMrade, for live introspection. Copied but deliberately NOT auto-started:
#    WIN.INI's [windows] run= is left empty so the COM port can be set up by hand first.
put "/c/Users/lycet/RiderProjects/Open-Source-PC110/Software/COMrade/dist/COMR95.EXE" "COMR95.EXE"
put "/c/Users/lycet/RiderProjects/Open-Source-PC110/Software/COMrade/dist/COMRADE.EXE" "COMRADE.EXE"

# 8. SYSTEM.INI [boot] display.drv - Setup's pnpdrvr.drv placeholder never gets
#    finalised on this machine and boots to a black screen. vga.drv is the driver
#    Setup itself used successfully throughout. Only the [boot] copy matters; the
#    [boot.description] one is cosmetic.
cp -p "$DST/WINDOWS/SYSTEM.INI" "$BAK/SYSTEM.INI"
python - "$DST/WINDOWS/SYSTEM.INI" <<'PY'
import sys,re
p=sys.argv[1]; s=open(p,'rb').read().decode('cp437')
head,sep,rest=s.partition('[keyboard]')
new=re.sub(r'(?im)^display\.drv=.*$','display.drv=vga.drv',head,count=1)
assert 'display.drv=vga.drv' in new, "no display.drv line in [boot]"
open(p,'wb').write((new+sep+rest).encode('cp437'))
print("  SYSTEM.INI [boot]                  <- display.drv=vga.drv")
PY

# 9. IVT68FIX must run as the VERY LAST line of AUTOEXEC.BAT - firing it earlier is
#    proven to get clobbered by DOS's own low-memory init before INT 68h is ever used.
cp -p "$DST/AUTOEXEC.BAT" "$BAK/AUTOEXEC.BAT"
if ! grep -qi "IVT68FIX" "$DST/AUTOEXEC.BAT"; then
    printf 'C:\IVT68FIX.COM\r\n' >> "$DST/AUTOEXEC.BAT"
    echo "  AUTOEXEC.BAT                       <- C:\IVT68FIX.COM appended as last line"
fi

echo "Done."
