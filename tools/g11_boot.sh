#!/usr/bin/env bash
# G11 smoke boot: start a bed on one build with one card, take full-frame shots at 25/45/70 s,
# record whether 86Box is still up, stop only that 86Box, restore the bed's config.
#
#   tools/g11_boot.sh <86Box.exe> <bed dir> <gfxcard> <8514a: 0|1> <label>
set -euo pipefail
cd "$(dirname "$0")/.."
EXE=$1; BED=$2; CARD=$3; IBM=$4; LABEL=$5
OUT=docs/captures/2026-11-18_x22_g11/$LABEL
PORT=9897
mkdir -p "$OUT"
cp "$BED/86box.cfg" "$BED/86box.cfg.g11save"
trap 'cp "$BED/86box.cfg.g11save" "$BED/86box.cfg"; rm -f "$BED/86box.cfg.g11save"' EXIT
sed -i "s/^gfxcard = .*/gfxcard = $CARD/" "$BED/86box.cfg"
if [ "$IBM" = 1 ]; then grep -q '^8514a = 1' "$BED/86box.cfg" || sed -i "s/^gfxcard = .*/&\r\n8514a = 1/" "$BED/86box.cfg"
else sed -i '/^8514a = /d' "$BED/86box.cfg"; fi
grep -E "^(machine|gfxcard|8514a)" "$BED/86box.cfg" > "$OUT/config.txt"
PID=$(powershell.exe -NoProfile -File tools/bed_launch.ps1 -Exe "$EXE" -VmPath "$BED" -Log "$PWD/$OUT/86box.log" -Seconds 0 -AllowMouse -Automation $PORT | tr -d '\r' | awk '$1=="pid"{print $2} /REFUSED/{print > "/dev/stderr"}')
[ -n "$PID" ] || { echo "no pid"; exit 1; }
t=0
for s in 25 45 70; do
  sleep $((s - t)); t=$s
  if ! tasklist //FI "PID eq $PID" | grep -q "$PID"; then echo "$LABEL: 86Box DIED before ${s}s"; exit 0; fi
  powershell.exe -NoProfile -File tools/bed_shot.ps1 -VmPath "$BED" -Port $PORT -Out "$OUT/s$s.png" >/dev/null 2>&1 || echo "shot $s failed"
done
echo "$LABEL: 86Box up at 70 s"
taskkill //PID "$PID" //F >/dev/null 2>&1 || true
