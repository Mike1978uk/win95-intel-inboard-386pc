#!/usr/bin/env python3
"""
Make VKD.VXD's protected-mode "wait for port 64h Output-Buffer-Full" poll always succeed
immediately, instead of always timing out (2026-08-04).

BACKGROUND
----------
Live-traced this session ([kbdporttrace2]/[vkdlivedump] in 386_dynarec.c/io.c, see memory
osr1_pivot_and_fidelity_pass_2026_08_04.md): once Windows 95 reaches protected mode, VKD.VXD's
keyboard-receive path waits for a scancode via the classic AT-8042 idiom:

    mov ecx, 0x10000     ; bounded retry count
wait:
    in  al, 64h          ; read 8042 status byte
    test al, 1           ; bit 0 = Output Buffer Full (byte ready at port 60h)
    loope wait           ; keep trying while ZF=1 (bit clear) and ecx!=0
    ret                  ; falls through here either way (success OR timeout)

This hardware (Intel Inboard 386/PC on a real IBM 5160 XT) has no real 8042 - port 0x64 is
completely unclaimed and always reads back 0x00, so `test al,1` always leaves ZF=1 and the loop
always burns its full 65536-iteration budget before giving up. Confirmed via full live tracing
this session (see memory): the scancode DOES correctly reach the emulator's own key_queue[] and
IRQ1 DOES correctly fire (both verified with dedicated trace hooks) - the guest simply never looks
at it, because it's waiting on a status bit this hardware can never set. This is a faithful
reproduction of a real hardware limitation (matches the user's own recollection of Windows 3.11
needing Intel's own IBKBD.DRV/IBVKD.386 for the exact same reason - the stock Microsoft keyboard
driver assumes a real AT 8042 exists), not a bug in this project's own code.

THE FIX
-------
File offset 0x3e11 is the exact 2-byte `IN AL,64h` instruction. Real-mode BIOS/DOS on this exact
hardware already works correctly without ever checking port 64h at all - it just trusts that a
byte is ready once its IRQ1 handler runs. Mirror that here: replace `IN AL,64h` (E4 64) with
`MOV AL,1` (B0 01) - same 2-byte length, no other instruction needs to move. The immediately-
following `TEST AL,1` now always sees bit 0 set (ZF=0), so `LOOPE` falls through on its very first
iteration with the "byte ready" outcome, and RET returns immediately instead of burning the full
retry budget for nothing.

USAGE
-----
    python patch_vkd_kbdready.py [input-VKD.VXD] [output-path]

Defaults to VKD_INBOARD_v2.VXD -> VKD_INBOARD_v3.VXD (chains on top of the existing A20 + AT-
keyboard-command patches already applied to that file).
"""

import sys

OFFSET = 0x3E11
EXPECTED = bytes.fromhex("e464")   # IN AL, 64h
NEW_VALUE = bytes.fromhex("b001")  # MOV AL, 1


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "osr1/VKD_INBOARD_v2.VXD"
    dst = sys.argv[2] if len(sys.argv) > 2 else "osr1/VKD_INBOARD_v3.VXD"

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
