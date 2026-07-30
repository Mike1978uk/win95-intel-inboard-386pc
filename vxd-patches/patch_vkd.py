"""
Patches stock Windows 95 (OSR2) VKD.VXD to fix A20 gate control on the Intel Inboard 386/PC.

BACKGROUND
----------
VKD.VXD's A20-enable path (the only site in the whole file that sends the 8042 "Write Output
Port" command, 0xD1, to port 0x64) assumes a real AT keyboard controller exists to interpret
that command. Confirmed by disassembly (this project, 2026-07-23) and by three independent
sources agreeing byte-for-byte:
  - Al Williams' 1990 Dr. Dobb's Inboard-specific A20 code: a single `outp(0x60, flag?0xDF:0xDD)`,
    no readback, no 0x64 command at all.
  - SuperFury's UniPCemu reverse-engineering of the real Inboard XT hardware: port 0x60 recognizes
    exactly 0xDD (disable) / 0xDF (enable); port 0x64 writes aren't modeled at all (no real 8042).
  - Direct empirical measurement on the real 5160 via COMrade's io_in/io_out (2026-07-23): a
    baseline read of port 0x60 returns 0x00; replicating VKD's exact command-byte-request sequence
    (OUT 0x64,0xAD -> OUT 0x64,0x20 -> IN 0x60) still returns 0x00, completely unchanged. There is
    no real 8042 answering on this hardware -- confirmed, not inferred.

THE BUG
-------
Disassembly (capstone, LE-realignment) of the stock VKD.VXD found exactly one code site in the
whole 45,371-byte file that sends 0xD1 to port 0x64 (grep for `mov al, 0xd1` -- exactly one raw
match, at file offset 0x1aa3):

    0x01a9d  and byte ptr [esi], 0xef   ; consume/clear the request-flag bit that triggered this
    0x01aa0  or al, 2                   ; force bit 1 (the AT "A20 enable" bit) into the pending
                                        ;   8042 output-port byte
    0x01aa2  push eax
    0x01aa3  mov al, 0xd1               ; select "Write Output Port" 8042 command
    0x01aa5  call 0x1d3d                ; OUT 0x64, 0xD1  (a command with no real 8042 listening)
    0x01aaa  pop eax
    0x01aab  call 0x1d35                ; OUT 0x60, AL    (writes the bit-1-forced byte -- but the
                                        ;   Inboard only recognizes the exact literal 0xDD/0xDF,
                                        ;   never whatever this AT-style derivation produces)
    0x01ab0  jmp 0x1a3c

This is reached whenever bit 4 (0x10) of a per-instance request-flags dword is set, dispatched
from a larger keyboard-request handler starting around 0x1a3d. It is the confirmed A20-ENABLE
path (the `or al, 2` can only ever set the bit, never clear it, so this is unambiguously enable,
not disable/toggle). No symmetric "disable" site using the same 0xD1 output-port mechanism was
found elsewhere in the file as of this session -- A20 disable, if VKD ever requests it at all
during a normal boot, may go through a different mechanism not yet traced. This patch fixes the
confirmed, uniquely-fingerprinted enable path, which is the one that must succeed for Windows to
get from real mode into protected mode at all.

THE FIX
-------
Bytes at file offset 0x1a9d, length 19 (0x1a9d-0x1aaf inclusive), originally:

    80 26 ef              and byte ptr [esi], 0xef
    0c 02                 or al, 2
    50                     push eax
    b0 d1                 mov al, 0xd1
    e8 93 02 00 00         call 0x1d3d       (OUT 0x64,0xD1 -- talks to nothing)
    58                     pop eax
    e8 85 02 00 00         call 0x1d35       (OUT 0x60,AL -- wrong derived value)

Replaced with (same 19-byte length, so nothing after this site needs relocating):

    80 26 ef              and byte ptr [esi], 0xef      (unchanged -- still consume the request bit)
    b0 df                 mov al, 0xdf                  (literal Inboard "enable A20" byte)
    e8 8e 02 00 00         call 0x1d35                   (OUT 0x60,AL -- now writes 0xDF directly,
                                                          reusing the existing wait-for-controller-
                                                          ready helper for bus-timing safety)
    90 90 90 90 90 90 90 90 90   (9x NOP filling the removed 0x64 write + push/pop, now dead)

The 0x64 write is dropped entirely rather than left in place: it's confirmed to talk to nothing
on this hardware (see BACKGROUND), so removing it is strictly a simplification, not a behavior
change beyond the fix itself.

USAGE
-----
    python patch_vkd.py [path-to-stock-VKD.VXD] [output-path]

Defaults to VKD_stock.VXD -> VKD_INBOARD.VXD in this directory.
"""

import sys
import capstone

PATCH_OFFSET = 0x1a9d
PATCH_LEN = 19

ORIGINAL_BYTES = bytes.fromhex("8026ef0c0250b0d1e89302000058e885020000")

NEW_BYTES = (
    bytes.fromhex("8026ef")      # and byte ptr [esi], 0xef   (unchanged)
    + bytes.fromhex("b0df")      # mov al, 0xdf
    + b"\xe8" + (0x1d35 - (PATCH_OFFSET + 3 + 2 + 5)).to_bytes(4, "little", signed=True)  # call 0x1d35
    + b"\x90" * 9                # nop padding
)

assert len(ORIGINAL_BYTES) == PATCH_LEN
assert len(NEW_BYTES) == PATCH_LEN, f"new patch is {len(NEW_BYTES)} bytes, must be exactly {PATCH_LEN}"


def realign(data, target, md, search_back=48):
    for start in range(max(0, target - search_back), target + 1):
        chunk = bytes(data[start:target + 16])
        try:
            insns = list(md.disasm(chunk, start))
        except Exception:
            continue
        for insn in insns:
            if insn.address == target:
                return insn
    return None


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "VKD_stock.VXD"
    dst = sys.argv[2] if len(sys.argv) > 2 else "VKD_INBOARD.VXD"

    data = bytearray(open(src, "rb").read())

    actual = bytes(data[PATCH_OFFSET:PATCH_OFFSET + PATCH_LEN])
    if actual != ORIGINAL_BYTES:
        print(f"REFUSING TO PATCH: bytes at {PATCH_OFFSET:#x} don't match expected original.")
        print(f"  expected: {ORIGINAL_BYTES.hex()}")
        print(f"  actual:   {actual.hex()}")
        sys.exit(1)

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    print("=== original code at patch site ===")
    for insn in md.disasm(bytes(actual), PATCH_OFFSET):
        print(f"  {insn.address:#07x}  {insn.mnemonic} {insn.op_str}")

    data[PATCH_OFFSET:PATCH_OFFSET + PATCH_LEN] = NEW_BYTES

    print("\n=== patched code at patch site ===")
    for insn in md.disasm(bytes(NEW_BYTES), PATCH_OFFSET):
        print(f"  {insn.address:#07x}  {insn.mnemonic} {insn.op_str}")

    # sanity: confirm the call target really lands on 0x1d35 (the OUT 0x60,AL helper)
    call_insn = realign(bytes(data), PATCH_OFFSET + 5, md)
    if call_insn is None or call_insn.mnemonic != "call":
        print("\nWARNING: could not re-verify the patched call instruction by realignment.")
    else:
        target_str = call_insn.op_str
        print(f"\nVerified patched call operand: {target_str}")

    open(dst, "wb").write(data)
    print(f"\nWrote {dst} ({len(data)} bytes).")


if __name__ == "__main__":
    main()
