#!/usr/bin/env python3
"""
Neuter the 4 remaining AT-only keyboard-controller command sends in stock VKD.VXD, found via a
full comprehensive scan (2026-08-04) of all standard AT keyboard command bytes after the A20 fix
(patch_vkd.py) was already applied to the one previously-known 0xD1/port-64 site.

BACKGROUND
----------
This hardware (Intel Inboard 386/PC on a real IBM 5160 XT) has no real 8042 keyboard controller -
port 0x60 recognizes exactly two literal bytes (0xDD disable A20, 0xDF enable A20) and nothing
else; port 0x64 has no real responder at all. Intel's own Win-for-Inboard reference driver
(IBKBD.DRV) avoids sending AT-only keyboard commands like "Set Typematic Rate" (0xF3) except
behind a bounded-timeout guard, and never implements "Set LED" (0xED) at all - consistent with
the documented fact that this simpler XT-style keyboard interface has no LEDs or typematic-rate
hardware to configure.

THE FOUND SITES
----------------
All 4 use the same shared low-level helpers as the already-patched A20 site
(call 0x1d35 = "out 0x60,al", call 0x1d3d = "out 0x64,al" - raw, non-blocking port writes, not a
wait-for-ACK loop, so not a hang risk by themselves - but still AT-only protocol noise this
hardware doesn't implement and shouldn't be sent):
  0x1de5: mov al, 0xED (Set LED)              -> call 0x1d35 (out 60h,al)
  0x1e26: mov al, 0xF3 (Set typematic rate)   -> call 0x1d35 (out 60h,al)
  0x1e6f: mov al, 0xF4 (Enable)               -> call 0x1d35 (out 60h,al)
  0x1e9b: mov al, 0xFE (Resend)               -> call 0x1d3d (out 64h,al)

THE FIX
-------
NOP out each 7-byte "mov al,imm8 ; call rel32" pair (2 + 5 bytes) at each site, matching the
project's existing philosophy of neutralizing phantom-hardware access
(patch_vpicd.py/patch_vdmad.py) rather than guessing at alternate behavior. Same shared helpers
remain untouched (still used by the legitimate, already-fixed A20 site's own direct 0xDD/0xDF
writes).

USAGE
-----
    python patch_vkd_kbdcmds.py [input-VKD.VXD] [output-path]

Defaults to VKD_INBOARD.VXD -> VKD_INBOARD_kbdcmds.VXD (chain on top of the existing A20 patch).
"""

import sys

SITES = {
    0x1de5: (bytes.fromhex("b0ed"), "Set LED (0xED)"),
    0x1e26: (bytes.fromhex("b0f3"), "Set typematic rate (0xF3)"),
    0x1e6f: (bytes.fromhex("b0f4"), "Enable (0xF4)"),
    0x1e9b: (bytes.fromhex("b0fe"), "Resend (0xFE)"),
}
SITE_LEN = 7  # 2-byte mov al,imm8 + 5-byte call rel32


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "VKD_INBOARD.VXD"
    dst = sys.argv[2] if len(sys.argv) > 2 else "VKD_INBOARD_kbdcmds.VXD"

    data = bytearray(open(src, "rb").read())

    patched = 0
    for offset, (expected_mov, label) in SITES.items():
        actual = bytes(data[offset:offset + 2])
        if actual != expected_mov:
            print(f"REFUSING at {offset:#06x} ({label}): expected {expected_mov.hex()}, "
                  f"found {actual.hex()} - byte layout mismatch, do not trust this patch")
            continue
        # sanity: byte 2 must be E8 (call rel32) to one of the known shared helpers
        if data[offset + 2] != 0xE8:
            print(f"REFUSING at {offset:#06x} ({label}): expected call (0xE8) at +2, "
                  f"found {data[offset+2]:#04x}")
            continue
        data[offset:offset + SITE_LEN] = b"\x90" * SITE_LEN
        patched += 1
        print(f"patched {offset:#06x}: {label} neutralized")

    if patched == 0:
        print("\nREFUSING TO WRITE OUTPUT: found 0 real sites to patch - re-derive site offsets "
              "for whatever build was just fed to this script before trusting its output.")
        raise SystemExit(1)

    open(dst, "wb").write(data)
    print(f"\nWrote {dst}, {len(data)} bytes ({patched}/{len(SITES)} sites patched)")


if __name__ == "__main__":
    main()
