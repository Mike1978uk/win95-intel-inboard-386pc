#!/usr/bin/env bash
# Build an 86Box VM that is a like-for-like twin of the real 5160+Inboard, running a
# COPY of the same golden pre-monolith CF image that is on the real machine.
#
# The point is a matched pair: identical disk contents, identical BIOS revision,
# identical peripherals, so any behavioural difference between emulator and hardware
# is a genuine fidelity gap rather than a configuration difference. This project has
# already lost days to the other kind (a machine sharing ibmxt_config silently ignored
# `bios =` and booted a 1982 ROM the Inboard cannot work with).
#
# Usage: ./tools/setup_golden_vm.sh <golden.img> <vm-dir> [--sbpro]
set -euo pipefail
SRC="${1:?usage: setup_golden_vm.sh <golden.img> <vm-dir> [--sbpro]}"
VM="${2:?usage: setup_golden_vm.sh <golden.img> <vm-dir> [--sbpro]}"
SBPRO="${3:-}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$REPO/86box_full/build/phase1_mingw/src/86Box.exe"

[ -f "$SRC" ] || { echo "ERROR: no such image: $SRC"; exit 1; }
[ -f "$EXE" ] || { echo "ERROR: no 86Box.exe at $EXE - build it first"; exit 1; }

mkdir -p "$VM"
IMG="$VM/$(basename "$SRC")"
echo "Copying image (this takes a while)..."
cp "$SRC" "$IMG"

# ROM tree: a junction, not a copy - the tree is large and must stay in step with the repo.
if [ ! -e "$VM/roms" ]; then
    cmd //c mklink //J "$(cygpath -w "$VM/roms")" "$(cygpath -w "$REPO/roms")" >/dev/null
    echo "  roms/ -> junction to $REPO/roms"
fi

# Geometry, derived from the image itself. Getting this wrong is a silent failure mode -
# the machine boots and then behaves oddly (see vm_test_canonical's *_wronggeom logs).
read -r HEADS SECTORS CYLS <<EOF
$(python - "$IMG" <<'PY'
import struct,sys,os
p=sys.argv[1]; size=os.path.getsize(p)
d=open(p,'rb').read(512)
assert struct.unpack_from('<H',d,510)[0]==0xAA55, "no MBR signature - not a partitioned image"
H=S=0
for i in range(4):
    e=0x1BE+i*16
    if d[e+4]==0: continue
    endH=d[e+5]; endS=d[e+6]&0x3F
    H=max(H,endH+1); S=max(S,endS)
assert H and S, "no partitions found"
C=size//(512*H*S)
print(H,S,C)
PY
)
EOF
echo "  geometry from MBR: $SECTORS sectors x $HEADS heads x $CYLS cylinders"
echo "  ($(stat -c%s "$IMG") bytes; $((SECTORS*HEADS*CYLS*512)) addressable)"

cat > "$VM/86box.cfg" <<CFG
[General]
vid_renderer = qt_software
update_icons = 0

[Machine]
machine = ibmxt_inboard386
cpu_family = ibm486bl3
cpu_multi = 3
cpu_speed = 83500000
cpu_use_dynarec = 0
cpu_waitstates = 31
mem_size = 5120

[IBM XT (Inboard 386/PC)]
bios = ibm5160_050986
enable_5161 = 0

[Video]
gfxcard = mach8_vga_isa

[ATI Mach8 (ATI Graphics Ultra) (ISA) #1]
bus_width = 8

[Input devices]
keyboard_type = internal
mouse_type = none

[Hard disks]
hdc_1 = xtide
hdd_01_fn = $(basename "$IMG")
hdd_01_ide_channel = 0:0
hdd_01_parameters = $SECTORS, $HEADS, $CYLS, 0, ide
hdd_01_speed = ramdisk
scsicard_1 = t130b

[Trantor T130B #1]
bios_addr = 0
base = 340
irq = 00
zero_wait = 1

[Floppy and CD-ROM drives]
fdd_01_type = 35_2hd
fdd_02_type = none

[Named Pipe (COM) #1]
path = \\.\pipe\comrade86box_com1
mode = 1
reconnect = 1
CFG

if [ "$SBPRO" = "--sbpro" ]; then
cat >> "$VM/86box.cfg" <<'CFG'

[Sound]
sndcard = sbprov2

[Sound Blaster Pro v2 #1]
base = 220
irq = 5
dma = 1
CFG
echo "  Sound Blaster Pro FITTED (0x220 / IRQ5 / DMA1)"
fi

echo
echo "Ready. Launch with:"
echo "  cd $VM && \"$EXE\" --vmpath ."
echo "  (NOT --fullscreen 0: --fullscreen takes no argument in this build, so the 0 is"
echo "   swallowed as a positional CONFIG FILENAME. 86Box then boots an empty default"
echo "   config - no hard disk, straight to ROM BASIC - and goes fullscreen. Seen 2026-08-24.)"
