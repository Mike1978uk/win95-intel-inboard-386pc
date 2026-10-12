#!/usr/bin/env python3
"""List what a bed's Windows 95 will run unattended at boot: the StartUp folder and WINSTART.BAT.

  python tools/bed_startup_hooks.py <disk image>     prints one line per hook; exit 2 if any, 3 if unreadable

A test fixture left in StartUp runs on every later boot and can shut the machine down
under the owner. Reads the first partition only, read-only.
"""
import struct, sys, warnings
warnings.filterwarnings('ignore')
from pyfatfs.PyFatFS import PyFatFS

img = sys.argv[1]
mbr = open(img, 'rb').read(512)
start = struct.unpack_from('<I', mbr, 446 + 8)[0] if mbr[510:512] == b'\x55\xaa' and mbr[446 + 4] else 0
try:
    fs = PyFatFS(img, offset=start * 512, read_only=True)
except Exception as e:
    print(f'cannot read {img} as FAT: {e}')
    sys.exit(3)
hooks = []
for d in ('/WINDOWS/Start Menu/Programs/StartUp',):
    if fs.exists(d):
        hooks += [f'{d}/{n}' for n in fs.listdir(d) if n.lower() != 'desktop.ini']
for f in ('/WINDOWS/WINSTART.BAT', '/WINSTART.BAT'):
    if fs.exists(f):
        hooks.append(f)
fs.close()
for h in hooks:
    print(h)
sys.exit(2 if hooks else 0)
