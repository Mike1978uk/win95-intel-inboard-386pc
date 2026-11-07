"""A copy of the SDK tunneldp sample that starts full screen on hardware: never asks for a window,
passes its startup gate, g_bWindowed starts FALSE. Usage: patch_exe.py in.exe out.exe"""
import re
import sys

b = bytearray(open(sys.argv[1], 'rb').read())
n = 0
for m in list(re.finditer(rb'\xF6[\x40-\x47]\x6E\x08', bytes(b))):
    b[m.start() + 3] = 0
    n += 1
assert n == 3
g = bytes.fromhex('399840010000' + '7511')
h = bytes(b).find(g)
assert h > 0 and bytes(b).count(g) == 1
b[h + 6] = 0xEB
n += 1
# cmp [g_bWindowed],ecx; sete: the flag at VA 0x100E044, file 0xCA44, starts TRUE.
assert bytes(b).count(bytes.fromhex('390D44E00001')) >= 1
assert b[0xCA44:0xCA48] == b'\x01\0\0\0'
b[0xCA44] = 0
n += 1
open(sys.argv[2], 'wb').write(b)
print('Patched:', n)
