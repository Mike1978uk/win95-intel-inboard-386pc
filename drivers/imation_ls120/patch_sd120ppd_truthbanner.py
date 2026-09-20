#!/usr/bin/env python3
r"""Make the vendor DOS driver's banner report the mode it SELECTED.

It currently reports the mode that was *requested*. `[0xBDF]` - the flag that
picks "EPP Fast" over the mode table's "EPP Normal" - is set at driver start
from the incoming `dh`, before any of the gates that decide whether dword is
actually allowed run, and it is never updated afterwards. So the banner can
print "EPP Fast" while the driver is transferring byte-wide, and it has no way
to print "Fast" for a mode that was upgraded on the way through.

That makes the banner useless as a readout of the dword decision, which is
what three sessions have been trying to obtain. This patch repoints the four
tests at the registers that hold the **selected** mode at that moment:

    read  name block  0x8827, 0x8833   cmp byte [0xBDF],1  ->  cmp dh, 0Bh
    write name block  0x887C, 0x8888   cmp byte [0xBDF],1  ->  cmp dl, 04h

`dh` and `dl` are live at all four sites - the driver stores them to the
selected-mode variables at `0x884E` / `0x8852`, inside the same routine, and
nothing touches `edx` in between.

Each original is 5 bytes (`80 3E DF 0B 01`) and each replacement is 3
(`80 FE 0B` / `80 FA 04`) plus two `nop`, so nothing moves and no branch
target changes. **It adds no port access**, changes no transfer path, and
alters only which string pointer is stored.

⚠ This is a diagnostic. It changes what the driver PRINTS, not what it does.

    python patch_sd120ppd_truthbanner.py SD120PPD.SYS out.SYS
    python patch_sd120ppd_truthbanner.py --revert out.SYS back.SYS
"""
import argparse
import hashlib
import sys

ORIG = bytes.fromhex("803edf0b01")          # cmp byte ptr [0xBDF], 1
READ_NEW = bytes.fromhex("80fe0b9090")      # cmp dh, 0Bh ; nop ; nop
WRITE_NEW = bytes.fromhex("80fa049090")     # cmp dl, 04h ; nop ; nop

SITES = [
    (0x8827, READ_NEW, "read name, EPP-BIOS branch"),
    (0x8833, READ_NEW, "read name, normal branch"),
    (0x887C, WRITE_NEW, "write name, EPP-BIOS branch"),
    (0x8888, WRITE_NEW, "write name, normal branch"),
]

STOCK_MD5 = "cbb42e8eb7847869e274e45f258cf718"


def apply(data, revert):
    out = bytearray(data)
    n = 0
    for off, new, what in SITES:
        want, become = (new, ORIG) if revert else (ORIG, new)
        have = bytes(out[off:off + 5])
        if have == become:
            print(f"  {off:#07x}  already {'stock' if revert else 'patched'} - {what}")
            continue
        if have != want:
            sys.exit(f"FAILED at {off:#07x}: expected {want.hex()}, found {have.hex()}\n"
                     f"  This is not the build this patch was written for.")
        out[off:off + 5] = become
        print(f"  {off:#07x}  {want.hex()} -> {become.hex()}  {what}")
        n += 1
    return bytes(out), n


def verify(data):
    """Disassemble each site and prove the replacement decodes as intended."""
    try:
        import capstone
    except ImportError:
        print("capstone not installed - NOT verified", file=sys.stderr)
        return
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    for off, _new, what in SITES:
        ins = list(md.disasm(data[off:off + 5], off))
        text = " ; ".join(f"{i.mnemonic} {i.op_str}".strip() for i in ins)
        total = sum(i.size for i in ins)
        flag = "OK " if total == 5 else "LEN!"
        print(f"  {flag} {off:#07x}  {text}   ({total} bytes)  {what}")
        if total != 5:
            sys.exit("FAILED: replacement does not occupy exactly 5 bytes - "
                     "a branch target would move.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--revert", action="store_true")
    args = ap.parse_args()

    data = open(args.src, "rb").read()
    md5 = hashlib.md5(data).hexdigest()
    print(f"in : {args.src}  {len(data)} bytes  md5 {md5}"
          + ("  (stock)" if md5 == STOCK_MD5 else ""))

    out, n = apply(data, args.revert)
    if n == 0:
        sys.exit("Patched: 0 - nothing changed. Refusing to write a no-op.")

    print(f"Patched: {n}")
    print("verify:")
    verify(out)

    open(args.dst, "wb").write(out)
    print(f"out: {args.dst}  {len(out)} bytes  md5 {hashlib.md5(out).hexdigest()}")


if __name__ == "__main__":
    main()
