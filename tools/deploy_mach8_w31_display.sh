#!/usr/bin/env bash
# Swap Windows 95's display driver for the ATI mach8 Windows 3.1 driver that is known to
# work on THIS card, taken from the user's own working Windows 3.11 build.
#
# Why this is a different class of change from the VxD work: MACHW3V.386 is loaded by a
# SYSTEM.INI `display=` line, NOT baked into VMM32.VXD. So it needs no pre-monolith image
# and is reversible by editing two lines back. See deploy_premonolith.sh for the changes
# that DO require a pre-monolith image.
#
# Windows 95 runs 16-bit Windows 3.1 display drivers. Expect no PnP and no display-property
# resolution switching - resolution comes from the [display] section written here.
#
# Motivation (2026-08-23): with display.drv=vga.drv - completely unaccelerated - Sound Blaster
# Pro playback on real hardware is distorted. The user saw the same symptom under Windows 3.11
# and it went away when the display driver was corrected. Plausible mechanism: SB Pro playback
# is DMA with an interrupt per buffer boundary, and a slow non-preemptible display driver
# starves the refill.
#
# Usage: ./tools/deploy_mach8_w31_display.sh <drive> [WIDTHxHEIGHT]
#   default 640x480 - fewer pixels per blit is the friendlier case for the audio test.
#   The 3.11 build itself ran 1024x768.
#   Revert: ./tools/deploy_mach8_w31_display.sh <drive> --revert
set -euo pipefail
DST="${1:?usage: deploy_mach8_w31_display.sh <drive> [WxH|--revert]}"
MODE="${2:-640x480}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
INI="$DST/WINDOWS/SYSTEM.INI"
BAK="$DST/PREPATCH"
[ -f "$INI" ] || { echo "ERROR: no $INI"; exit 1; }
mkdir -p "$BAK"
cp -p "$INI" "$BAK/SYSTEM.INI.pre-mach8"

if [ "$MODE" = "--revert" ]; then
python - "$INI" <<'PY'
import sys,re
p=sys.argv[1]; s=open(p,'rb').read().decode('cp437')
head,sep,rest=s.partition('[keyboard]')
head=re.sub(r'(?im)^display\.drv=.*$','display.drv=vga.drv',head,count=1)
head=re.sub(r'(?im)^386Grabber=.*$','386Grabber=vgafull.3gr',head,count=1)
rest=re.sub(r'(?im)^display=.*$','display=*vdd',rest,count=1)
open(p,'wb').write((head+sep+rest).encode('cp437'))
print("  reverted to vga.drv / vgafull.3gr / *vdd")
PY
    echo "Done. Reboot."; exit 0
fi

W="${MODE%x*}"; H="${MODE#*x}"
for f in MACHW3.DRV MACHW3.3GR MACHW3V.386; do
    cp "$REPO/mach8_w31_display/$f" "$DST/WINDOWS/SYSTEM/$f"
    printf '  WINDOWS\SYSTEM\%-14s <- mach8_w31_display/%s\n' "$f" "$f"
done

python - "$INI" "$W" "$H" <<'PY'
import sys,re
p,W,H=sys.argv[1],sys.argv[2],sys.argv[3]
s=open(p,'rb').read().decode('cp437')
head,sep,rest=s.partition('[keyboard]')
# [boot] - the authoritative copy. [boot.description] is cosmetic, leave it alone.
head=re.sub(r'(?im)^display\.drv=.*$','display.drv=MACHW3.DRV',head,count=1)
head=re.sub(r'(?im)^386Grabber=.*$','386Grabber=MACHW3.3GR',head,count=1)
# [386Enh] - replace the bundled *vdd with the mach8 VDD
if re.search(r'(?im)^display=',rest):
    rest=re.sub(r'(?im)^display=.*$','display=MACHW3V.386',rest,count=1)
else:
    rest=re.sub(r'(?im)^\[386Enh\]\s*$','[386Enh]\ndisplay=MACHW3V.386',rest,count=1)
out=head+sep+rest
# [display] - values from the working 3.11 build, resolution overridden
disp=(f"[display]\nbpp=8\nx_resolution={W}\ny_resolution={H}\ndpi=96\nvad=0\n"
      "EngineOnly=1\ndib2devengine=1\nDevBmp=1\nMach8=1\nAutoEnvironment=on\n")
if re.search(r'(?im)^\[display\]',out):
    out=re.sub(r'(?ims)^\[display\].*?(?=^\[|\Z)',disp+"\n",out,count=1)
else:
    out=out.rstrip("\r\n")+"\r\n\r\n"+disp
open(p,'wb').write(out.encode('cp437'))
print(f"  SYSTEM.INI: display.drv=MACHW3.DRV, 386Grabber=MACHW3.3GR, display=MACHW3V.386, {W}x{H}x8")
PY
echo "Done. Reboot. If it black-screens, boot to a command prompt (F8) and run this with --revert."
