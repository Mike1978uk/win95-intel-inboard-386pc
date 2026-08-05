#!/usr/bin/env python3
"""
Skip INBRDPC.SYS's own redundant per-call AH=87h self-test (2026-08-04).

BACKGROUND
----------
Full recursive-descent disassembly (seeded from the DOS-driver strategy/interrupt entry points
AND the INT 15h AH=87h/88h dispatcher, zero unresolved branches) of stock INBRDPC.SYS found that
its AH=87h (Extended Memory Block Move) handler, on every single invocation, checks a word flag
at address 0x2B7:

    cmp word ptr [0x2B7], -1
    jne <run the self-test>     ; only skipped if flag == -1
    jmp <skip to success>

If not -1, it unconditionally re-runs a full 32KB block-move-and-verify self-test (INT 15h AH=87h
against itself, comparing 3 bytes, reporting to port 0x670) before doing the real work. Exhaustive
search of the whole file found this flag is NEVER written anywhere - it stays at its on-disk
default forever, meaning this self-test redundantly re-runs on EVERY AH=87h call from HIMEM.SYS/
Windows during boot, not just once. Under this project's heavily-waitstated CPU emulation (and
plausibly on the real underlying hardware timing too), that adds up to what looks like a stall
but may just be extremely wasteful repeated work.

We've already independently proven (Phase 1 fidelity test, 2026-08-04) that this hardware's
extended-memory-move functionality works correctly via the direct 0xDD/0xDF A20 path this same
handler uses - the self-test's own actual verification isn't in question, just its needless
repetition. Skipping it entirely (treating it as "already passed" from driver load) is safe.

THE FIX
-------
The flag's on-disk default at file offset 0x6BA (runtime data offset 0x2B7, DELTA=0x403) is the
word 0x003C. Change it to 0xFFFF (-1) so the "already tested" branch is taken from the very first
call onward.

USAGE
-----
    python patch_inbrdpc_selftest_skip.py [input-INBRDPC.SYS] [output-path]

Defaults to INBRDPC_stock.SYS -> INBRDPC_INBOARD.SYS.
"""

import sys

FLAG_OFFSET = 0x6BA
EXPECTED = bytes.fromhex("3c00")
NEW_VALUE = bytes.fromhex("ffff")


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "INBRDPC_stock.SYS"
    dst = sys.argv[2] if len(sys.argv) > 2 else "INBRDPC_INBOARD.SYS"

    data = bytearray(open(src, "rb").read())

    actual = bytes(data[FLAG_OFFSET:FLAG_OFFSET + 2])
    if actual != EXPECTED:
        print(f"REFUSING TO PATCH: bytes at {FLAG_OFFSET:#06x} don't match expected default.")
        print(f"  expected: {EXPECTED.hex()}")
        print(f"  actual:   {actual.hex()}")
        sys.exit(1)

    data[FLAG_OFFSET:FLAG_OFFSET + 2] = NEW_VALUE

    open(dst, "wb").write(data)
    print(f"Patched flag at {FLAG_OFFSET:#06x}: {EXPECTED.hex()} -> {NEW_VALUE.hex()}")
    print(f"Wrote {dst} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
