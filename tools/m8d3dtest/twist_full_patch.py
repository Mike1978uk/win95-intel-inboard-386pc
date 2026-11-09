"""A copy of the DX5 SDK twist sample that starts full screen: D3DAppIVerifyDriverAndMode (tri3
misc.c) prefers a window when the display is the primary and the driver draws at its depth; the
window's surface does not fit beside an 800x600 desktop in the Mach8's 1 MB. The primary test is
made to fail, so a full-screen mode is picked as for a non-primary device.
Usage: twist_full_patch.py twist.exe out.exe"""
import sys

b = bytearray(open(sys.argv[1], 'rb').read())
# cmp [bIsPrimary],0; je pick_mode  ->  cmp [bIsPrimary],0; jmp pick_mode; nop (VA 0x40A0F4)
old = bytes.fromhex('833D56174200000F8434000000')
new = bytes.fromhex('833D5617420000E93500000090')
assert bytes(b).count(old) == 1, 'not the DX5 SDK twist.exe this was made for'
i = bytes(b).find(old)
b[i:i + len(old)] = new
open(sys.argv[2], 'wb').write(b)
print('Patched:', 1)
