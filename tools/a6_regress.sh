#!/usr/bin/env bash
# A6: run every Mach8 probe in the AT bed vm_6695_w2 on one build and one card, and collect the results.
#
#   tools/a6_regress.sh <86Box.exe> <card: mach32_isa | 8514a> <label>
#
# Starts from vm_6695_w2/w2_pristine.img and 86box_pristine.cfg (made on first use), stages the probes
# in C:\M8SEQ, runs them from AUTOEXEC.BAT, waits for C:\M8SEQ\DONE.TXT, copies every .BIN to
# docs/captures/2026-10-10_a6/<label>/ and restores the bed. Diff two labels with cmp.
set -euo pipefail
cd "$(dirname "$0")/.."
EXE=$1; CARD=$2; LABEL=$3
BED=vm_6695_w2
OUT=docs/captures/2026-10-10_a6/$LABEL
SEQ=tools/m8seq
TMP=$(mktemp -d)
mkdir -p "$OUT"

[ -f $BED/w2_pristine.img ] || cp $BED/w2.img $BED/w2_pristine.img
[ -f $BED/86box_pristine.cfg ] || cp $BED/86box.cfg $BED/86box_pristine.cfg
cp $BED/w2_pristine.img $BED/w2.img
cp $BED/86box_pristine.cfg $BED/86box.cfg

PROBES="M8REGS M8BYTE M8BYTE2 M8BYTE3 M8BYTE4 M8TSX M8TS1 M8TS2 M8BLRD M8SEQ5 M8SEQ6 M8ROW5 M8SCMP M8ROW4
M8LINE M8SEQ M8ROW3 M8CMP M8EXP M8NIB M8FG6 M8FG7 M8FG8 M8FG9 M8POLY M8PL2 M8ROW M8SRC M8MONO M8TXT M8ROW6
M8ROW7 M8SRC4 M8SRC4B M8SRC4C M8SRC4D M8SRC4E M8TS2Q M8WRAP M8LOPT M8LEND M8MIX M8TILE M8PF M8CMPA M8CMPC
M8CMP8 M8STAT M8CONF"

python tools/fatcp.py $BED/w2.img --mkdir 'M8SEQ' >/dev/null
{
  printf '@ECHO OFF\r\nPROMPT $p$g\r\nPATH C:\\DOS\r\nCD \\M8SEQ\r\n'
  for p in $PROBES; do
    python tools/fatcp.py $BED/w2.img "M8SEQ\\$p.COM" $SEQ/$p.COM --yes >/dev/null
    printf '%s\r\n' "$p"
  done
  printf 'ECHO done > DONE.TXT\r\n'
} > "$TMP/AUTOEXEC.BAT"
python tools/fatcp.py $BED/w2.img 'M8SEQ\M8PRE.DAT' $SEQ/M8PRE.DAT --yes >/dev/null
python tools/fatcp.py $BED/w2.img 'M8SEQ\M8CONF.DAT' $SEQ/M8CONF_w311.DAT --yes >/dev/null
python tools/fatcp.py $BED/w2.img 'AUTOEXEC.BAT' "$TMP/AUTOEXEC.BAT" --yes >/dev/null

if [ "$CARD" = 8514a ]; then
  sed -i 's/^gfxcard = .*/gfxcard = vga\r\n8514a = 1/' $BED/86box.cfg
else
  sed -i "s/^gfxcard = .*/gfxcard = $CARD/" $BED/86box.cfg
fi
grep -n "gfxcard\|8514a" $BED/86box.cfg

powershell.exe -NoProfile -File tools/bed_launch.ps1 -Exe "$EXE" -VmPath ".\\$BED" -Log "$PWD/$OUT/86box.log" -Seconds 0 -AllowMouse | tail -1
for i in $(seq 1 180); do
  python -c "
import sys; sys.path.insert(0,'tools')
from fatls import Fat
sys.exit(0 if Fat('$BED/w2.img').resolve('M8SEQ\\\\DONE.TXT') else 1)" && break
  sleep 10
done
sleep 5
tasklist | grep -i 86box | awk '{print $2}' | while read pid; do taskkill //PID "$pid" //F >/dev/null; done
sleep 2
python - "$BED/w2.img" "$OUT" <<'EOF'
import sys; sys.path.insert(0, 'tools')
from fatls import Fat
img, out = sys.argv[1], sys.argv[2]
f = Fat(img); n = 0
d = f.resolve('M8SEQ')
for e in f.listdir(d['clus']):
    if e['name'].upper().endswith('.BIN') or e['name'].upper() == 'DONE.TXT':
        open(f"{out}/{e['name']}", 'wb').write(f.read(e)); n += 1
print(f"collected {n} files into {out}")
EOF
cp $BED/w2_pristine.img $BED/w2.img
cp $BED/86box_pristine.cfg $BED/86box.cfg
rm -rf "$TMP"
