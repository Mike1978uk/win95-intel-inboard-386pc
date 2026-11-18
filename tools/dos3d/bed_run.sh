#!/usr/bin/env bash
# Run DOS programs in the AT bed vm_6695_w2 (486, Graphics Ultra) and collect what they leave.
#
#   tools/dos3d/bed_run.sh <86Box.exe> <out dir> <shot seconds, comma list> <file>... -- <command>...
#
# Starts from the bed's pristine image and config, copies each file into C:\RUN, writes an
# AUTOEXEC.BAT running the commands from there and then making DONE.TXT, launches through
# bed_launch.ps1 with automation, takes full-frame shots at the given seconds, waits for
# DONE.TXT (gives up after RUN_STALL seconds, default 180), copies every file in C:\RUN back
# to <out dir>/run/, stops only this 86Box and restores the bed.
set -euo pipefail
cd "$(dirname "$0")/../.."
EXE=$1; OUT=$2; SHOTS=$3; shift 3
FILES=(); while [ "$1" != -- ]; do FILES+=("$1"); shift; done; shift
BED=vm_6695_w2
PORT=9896
mkdir -p "$OUT/run"
TMP=$(mktemp -d)

if powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='86Box.exe'\" | Where-Object { \$_.CommandLine -match '$BED' } | Select-Object -First 1" | grep -q .; then
  echo "an 86Box already has $BED open"; exit 1
fi
FATW=--while-running
cp $BED/w2_pristine.img $BED/w2.img
cp $BED/86box_pristine.cfg $BED/86box.cfg
trap 'cp $BED/w2_pristine.img $BED/w2.img; cp $BED/86box_pristine.cfg $BED/86box.cfg' EXIT

python tools/fatcp.py $BED/w2.img --mkdir RUN $FATW >/dev/null
for f in "${FILES[@]}"; do
  python tools/fatcp.py $BED/w2.img 'RUN\'"$(basename "$f")" "$f" --yes $FATW >/dev/null
done
{
  printf '@ECHO OFF\r\nPATH C:\\DOS\r\nCD \\RUN\r\n'
  for c in "$@"; do printf '%s\r\n' "$c"; done
  printf 'ECHO done > DONE.TXT\r\n'
} > "$TMP/AUTOEXEC.BAT"
python tools/fatcp.py $BED/w2.img AUTOEXEC.BAT "$TMP/AUTOEXEC.BAT" --yes $FATW >/dev/null

PID=$(powershell.exe -NoProfile -File tools/bed_launch.ps1 -Exe "$EXE" -VmPath ".\\$BED" -Log "$PWD/$OUT/86box.log" -Seconds 0 -AllowMouse -Automation $PORT | tr -d '\r' | awk '$1=="pid"{print $2} /REFUSED/{print > "/dev/stderr"}')
[ -n "$PID" ] || { echo "bed_launch printed no pid"; exit 1; }
echo "pid $PID"
done_txt() {
  python -c "
import sys; sys.path.insert(0,'tools')
from fatls import Fat
print('done' if Fat('$BED/w2.img').resolve('RUN\\\\DONE.TXT') else '')"
}
t=0
for s in ${SHOTS//,/ }; do
  sleep $((s - t)); t=$s
  tasklist //FI "PID eq $PID" | grep -q "$PID" || { echo "86Box exited at ${s}s"; break; }
  powershell.exe -NoProfile -File tools/bed_shot.ps1 -VmPath "$BED" -Port $PORT -Out "$OUT/s$s.png" >/dev/null 2>&1 || echo "shot $s failed"
done
while [ -z "$(done_txt)" ]; do
  [ $t -ge ${RUN_STALL:-180} ] && { echo "no DONE.TXT after ${t}s"; break; }
  tasklist //FI "PID eq $PID" | grep -q "$PID" || { echo "86Box exited before DONE.TXT"; break; }
  sleep 5; t=$((t + 5))
done
taskkill //PID "$PID" //F >/dev/null 2>&1 || true
sleep 2
python - "$BED/w2.img" "$OUT/run" <<'EOF'
import os, sys
sys.path.insert(0, 'tools')
from fatls import Fat
f = Fat(sys.argv[1])
for e in f.listdir(f.resolve('RUN')['clus']):
    if not e['dir'] and not e['name'].upper().endswith(('.EXE', '.COM')):
        open(os.path.join(sys.argv[2], e['name']), 'wb').write(f.read(f.resolve('RUN\\' + e['name'])))
        print('got', e['name'])
EOF
echo "after ${t}s"
