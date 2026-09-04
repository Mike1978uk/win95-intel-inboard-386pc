#!/usr/bin/env python3
"""List the VMM services a Win9x VxD/PDR calls, resolved to names.

A VxD service call is `CD 20` followed by `dw service, dw device_id`. Keeping
device_id == 0001 (VMM) and resolving the ordinal against the DDK's own
INC32/VMM.INC gives a readable picture of what a driver actually does, without
a disassembler.

Used to establish that every Microsoft IOS port driver arms Set_Global_Time_Out
while ours armed nothing. See technique 88 in the inboard-hw-debug skill.

    python3 tools/pdr_vxd_services.py roms/xtcf_card/*.PDR
    python3 tools/pdr_vxd_services.py --offsets roms/xtcf_card/HSFLOP_reference.PDR

VMM.INC is looked up at VMM_INC below, or via --vmm-inc.
"""

import argparse
import os
import re
import sys

VMM_INC = r"C:/Users/lycet/OneDrive/Desktop/XT_project/Windows95_ddk/INC32/VMM.INC"

# Anchors that must hold if the ordinal count is being read correctly.
ANCHORS = {"Get_VMM_Version": 0x000, "Get_Cur_VM_Handle": 0x001}


def load_vmm_table(path):
    """Ordinal -> name, by counting VMM_Service declarations in order."""
    names = []
    with open(path, errors="ignore") as fh:
        for line in fh:
            m = re.match(r"\s*VMM_Service\s+(\w+)", line)
            if m:
                names.append(m.group(1))
    table = {i: n for i, n in enumerate(names)}
    by_name = {n: i for i, n in table.items()}
    for name, want in ANCHORS.items():
        got = by_name.get(name)
        if got != want:
            sys.exit(f"{path}: anchor {name} resolved to {got}, expected {want:#05x}")
    return table


def scan(path):
    """Yield (file_offset, service_ordinal) for every VMM service call."""
    data = open(path, "rb").read()
    for m in re.finditer(b"\xcd\x20", data):
        i = m.end()
        if i + 4 > len(data):
            continue
        service = data[i] | (data[i + 1] << 8)
        device = data[i + 2] | (data[i + 3] << 8)
        if device == 0x0001:
            yield m.start(), service, data[i + 4 : i + 12]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--vmm-inc", default=VMM_INC)
    ap.add_argument("--offsets", action="store_true",
                    help="list every call site rather than the set of services")
    ap.add_argument("--only", default="",
                    help="comma-separated substring filter on service names")
    args = ap.parse_args()

    if not os.path.exists(args.vmm_inc):
        sys.exit(f"VMM.INC not found at {args.vmm_inc} - pass --vmm-inc")
    table = load_vmm_table(args.vmm_inc)
    only = [s.lower() for s in args.only.split(",") if s]

    for path in args.files:
        print(f"== {path}")
        hits = list(scan(path))
        if not hits:
            print("   no VMM service calls at all")
            continue
        if args.offsets:
            for off, svc, tail in hits:
                name = table.get(svc, "?")
                if only and not any(s in name.lower() for s in only):
                    continue
                print(f"   file {off:#07x}  {svc:#05x}  {name:<24} next: {tail.hex(' ')}")
        else:
            seen = sorted({svc for _, svc, _ in hits})
            for svc in seen:
                name = table.get(svc, "?")
                if only and not any(s in name.lower() for s in only):
                    continue
                n = sum(1 for _, s, _ in hits if s == svc)
                print(f"   {svc:#05x}  {name:<28} x{n}")


if __name__ == "__main__":
    main()
