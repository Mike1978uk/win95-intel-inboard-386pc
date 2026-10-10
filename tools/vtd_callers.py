#!/usr/bin/env python3
"""Which VxDs ask the timer device (VTD) for a faster tick, or read it?

A Win9x VxD calls a service as INT 20h followed by a 4-byte ID: service word, device word
(technique 60). VTD is device 0005h; its table (DDK INC32/VTD.INC) is below. A raw scan finds
CD 20 ss ss 05 00, so each hit is checked by disassembling a short window before it and
requiring the CD 20 to sit on an instruction boundary.

    python tools/vtd_callers.py <dir or files...>
"""

import os
import sys

VTD = ["Get_Version", "Update_System_Clock", "Get_Interrupt_Period", "Begin_Min_Int_Period",
       "End_Min_Int_Period", "Disable_Trapping", "Enable_Trapping", "Get_Real_Time",
       "Get_Date_And_Time", "Adjust_VM_Count", "Delay"]
EXTS = (".VXD", ".386", ".PDR", ".MPD", ".DRV", ".SYS")


def on_boundary(data, i):
    """True if some decode starting up to 24 bytes back lands on i."""
    try:
        import capstone
    except ImportError:
        return None
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for back in range(1, 25):
        start = i - back
        if start < 0:
            break
        pos = start
        for ins in md.disasm(data[start:i + 6], start):
            if ins.address == i:
                return True
            if ins.address > i:
                break
            pos = ins.address + ins.size
            if ins.mnemonic == "int" and ins.op_str == "0x20":
                pos += 4
                break
    return False


def scan(path):
    data = open(path, "rb").read()
    hits = []
    i = data.find(b"\xcd\x20")
    while i >= 0:
        svc = int.from_bytes(data[i + 2:i + 4], "little")
        dev = int.from_bytes(data[i + 4:i + 6], "little")
        if dev == 5 and (svc & 0x7FFF) < len(VTD):
            hits.append((i, VTD[svc & 0x7FFF], on_boundary(data, i)))
        i = data.find(b"\xcd\x20", i + 1)
    return hits


def main():
    files = []
    for a in sys.argv[1:]:
        if os.path.isdir(a):
            for r, _, fs in os.walk(a):
                files += [os.path.join(r, f) for f in fs if f.upper().endswith(EXTS)]
        else:
            files.append(a)
    for f in sorted(files):
        hits = scan(f)
        if hits:
            names = ", ".join(f"{n}@{o:#x}{'' if ok else ' (unconfirmed)'}" for o, n, ok in hits)
            print(f"{os.path.basename(f):<16} {names}")


if __name__ == "__main__":
    main()
