#!/usr/bin/env python3
"""Disassemble a DOS .COM by following its code from the entry point.

A .COM mixes code with messages and tables. A top-to-bottom sweep decodes the
text as instructions that look plausible (skill technique 16), so this follows
calls and jumps from 0100h instead and reports what it could not follow.

    python tools/comdis.py TEST.COM --out TEST.asm     full listing, routines labelled
    python tools/comdis.py TEST.COM --io               every port each routine touches
    python tools/comdis.py TEST.COM --refs 23F3        who loads this immediate

Coverage is printed: bytes reached as code, against the file. An indirect jump
or call (through a register or memory) is listed as unresolved, not guessed.
"""
import argparse
import collections
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_16

ORG = 0x100
STOP = {"ret", "retf", "iret", "jmp", "ljmp"}


def walk(data, extra=()):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    end = ORG + len(data)
    seen = {}
    calls = {ORG}
    todo = [ORG] + list(extra)
    unresolved = []
    while todo:
        pc = todo.pop()
        while ORG <= pc < end and pc not in seen:
            ins = next(md.disasm(data[pc - ORG:pc - ORG + 16], pc), None)
            if ins is None:
                break
            seen[pc] = ins
            m = ins.mnemonic
            tgt = None
            if m == "call" or m.startswith("j") or m.startswith("loop"):
                op = ins.op_str
                if op.startswith("0x") and ":" not in op:
                    tgt = int(op, 16)
                elif m in ("call", "jmp"):
                    unresolved.append((pc, m, op))
            if tgt is not None:
                if m == "call":
                    calls.add(tgt)
                todo.append(tgt)
            if m in STOP:
                break
            # INT 20h and INT 21h/4Ch end the program; neither is followed past
            if m == "int" and ins.op_str == "0x20":
                break
            pc += ins.size
    return seen, calls, unresolved


def routine_of(addr, calls):
    best = ORG
    for c in calls:
        if c <= addr and c > best:
            best = c
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--out")
    ap.add_argument("--io", action="store_true")
    ap.add_argument("--refs")
    ap.add_argument("--entry", action="append", default=[],
                    help="extra code entry (hex), e.g. a handler reached through a table")
    a = ap.parse_args()
    data = open(a.path, "rb").read()
    entries = [int(x, 16) for x in a.entry]
    # A trampoline (`call si` / `call bx` as a routine's first instruction) hides its
    # targets in the register load before each call to it. Follow those until closed.
    while True:
        seen, calls, unresolved = walk(data, entries)
        tramps = {c: seen[c].op_str for c in calls
                  if c in seen and seen[c].mnemonic == "call" and seen[c].op_str in ("si", "bx", "di")}
        new = []
        pcs = sorted(seen)
        for n, pc in enumerate(pcs[:-1]):
            i, j = seen[pc], seen[pcs[n + 1]]
            if j.mnemonic == "call" and j.op_str.startswith("0x") and int(j.op_str, 16) in tramps:
                reg = tramps[int(j.op_str, 16)]
                if i.mnemonic == "mov" and i.op_str.startswith(reg + ", 0x"):
                    t = int(i.op_str.split(",")[1], 16)
                    if t not in seen and t not in entries and ORG <= t < ORG + len(data):
                        new.append(t)
        if not new:
            break
        entries += new
    if tramps:
        print("trampolines:", ", ".join("%04x (call %s)" % kv for kv in tramps.items()), "-", len(entries), "routines found through them")
    covered = sum(i.size for i in seen.values())
    print("%s: %d bytes, %d instructions, %d bytes reached as code (%.0f%%), %d routines, %d unresolved jumps/calls"
          % (a.path, len(data), len(seen), covered, 100.0 * covered / len(data), len(calls), len(unresolved)))

    if a.refs:
        v = int(a.refs, 16)
        for pc in sorted(seen):
            i = seen[pc]
            if ("0x%x" % v) in i.op_str:
                print("%04x  %-6s %-30s  in routine %04x" % (pc, i.mnemonic, i.op_str, routine_of(pc, calls)))

    if a.io:
        # A port is whatever the nearest preceding `mov dx, imm` in the same routine loaded.
        byr = collections.defaultdict(list)
        dx = None
        for pc in sorted(seen):
            i = seen[pc]
            if pc in calls:
                dx = None
            if i.mnemonic == "mov" and i.op_str.startswith("dx, 0x"):
                dx = int(i.op_str.split(",")[1], 16)
            elif i.mnemonic in ("in", "out"):
                ops = i.op_str.replace(" ", "")
                if "dx" in ops:
                    port = "%04X" % dx if dx is not None else "DX?"
                else:
                    port = "%04X" % int(ops.split(",")[0 if i.mnemonic == "out" else 1], 16)
                byr[routine_of(pc, calls)].append("%s %s@%04x" % (i.mnemonic, port, pc))
        for r in sorted(byr):
            print("routine %04x: %s" % (r, "  ".join(byr[r])))

    if a.out:
        with open(a.out, "w") as f:
            f.write("; %s - followed from %04Xh. %d%% of the file reached as code.\n" % (a.path, ORG, 100 * covered // len(data)))
            f.write("; Unresolved (indirect) transfers: %s\n" % ", ".join("%04x %s %s" % u for u in unresolved))
            pc = ORG
            end = ORG + len(data)
            while pc < end:
                if pc in seen:
                    if pc in calls:
                        f.write("\nsub_%04x:\n" % pc)
                    i = seen[pc]
                    f.write("%04x  %-14s %-6s %s\n" % (pc, i.bytes.hex(), i.mnemonic, i.op_str))
                    pc += i.size
                else:
                    run = bytearray()
                    start = pc
                    while pc < end and pc not in seen and len(run) < 16:
                        run.append(data[pc - ORG]); pc += 1
                    txt = "".join(chr(c) if 32 <= c < 127 else "." for c in run)
                    f.write("%04x  db %-48s ; %s\n" % (start, " ".join("%02x" % c for c in run), txt))
        print("wrote", a.out)
    if unresolved:
        print("unresolved:", ", ".join("%04x %s %s" % u for u in unresolved[:40]))


if __name__ == "__main__":
    main()
