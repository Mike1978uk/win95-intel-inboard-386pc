#!/usr/bin/env python3
"""
Make Windows 95's KEYBOARD.DRV's own "wait for port 64h Output-Buffer-Full" poll always succeed
immediately, instead of always timing out (2026-08-04).

BACKGROUND
----------
Companion fix to patch_vkd_kbdready.py. That patch fixed VKD.VXD's own internal wait-for-key
loop, but KEYBOARD.DRV (the 16-bit NE driver responsible for turning received scancodes into
actual Windows keyboard messages) does its OWN, completely independent raw port 64h status check
- it does not simply trust VKD.VXD's result. Since this hardware has no real 8042 (port 0x64 is
unclaimed, always reads back 0x00), KEYBOARD.DRV's own check also always sees "not ready" and
silently discards/times out, exactly like VKD.VXD did before its own fix - explaining why patching
VKD.VXD alone was not enough (matches the user's own recollection: Windows 3.11 needed BOTH
IBKBD.DRV and IBVKD.386 patched/replaced together for the same reason).

THE FOUND SITE
---------------
File offset 0xf14: `in al,64h` / `and al,1` / `loope -6` / `jcxz +8` - the classic bounded
"wait for Output Buffer Full" idiom, identical in shape to the VKD.VXD site. (Two OTHER, unrelated
`in al,64h` sites at 0xef8/0xf0a in the same file check bit 1 - Input Buffer Full, "ready to
accept a write" - not modified here: since this hardware's port 64h always reads 0x00, those
checks already pass instantly on their own and are not a problem.)

THE FIX
-------
Replace `IN AL,64h` (E4 64) with `MOV AL,1` (B0 01) at 0xf14 - same 2-byte length. `AND AL,1` then
always sees the bit set, `LOOPE` falls through immediately instead of burning its retry budget.

USAGE
-----
    python patch_keyboarddrv_kbdready.py [input-KEYBOARD.DRV] [output-path]

Defaults to KEYBOARD_stock.DRV -> KEYBOARD_INBOARD.DRV.
"""

import sys

OFFSET = 0xF14
EXPECTED = bytes.fromhex("e464")   # IN AL, 64h
NEW_VALUE = bytes.fromhex("b001")  # MOV AL, 1


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "osr1/KEYBOARD_stock.DRV"
    dst = sys.argv[2] if len(sys.argv) > 2 else "osr1/KEYBOARD_INBOARD.DRV"

    data = bytearray(open(src, "rb").read())

    actual = bytes(data[OFFSET:OFFSET + 2])
    if actual != EXPECTED:
        print(f"REFUSING TO PATCH: bytes at {OFFSET:#06x} don't match expected default.")
        print(f"  expected: {EXPECTED.hex()}")
        print(f"  actual:   {actual.hex()}")
        sys.exit(1)

    data[OFFSET:OFFSET + 2] = NEW_VALUE

    open(dst, "wb").write(data)
    print(f"Patched {OFFSET:#06x}: IN AL,64h ({EXPECTED.hex()}) -> MOV AL,1 ({NEW_VALUE.hex()})")
    print(f"Wrote {dst} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
