"""Remove the motor-stopped diagnostic from rdisk.c.

Line-based rather than exact-string: the block contains C escape sequences
whose representation through a shell heredoc is easy to get subtly wrong, and
a pattern that silently fails to match is worse than one that cannot.

Each removal is anchored on a unique marker line, bounded by an explicit end
condition, and printed. Nothing is written unless all three succeed.
"""
import os
import sys

P = r"C:\Users\lycet\AppData\Local\Temp\claude\86box_master\src\disk\rdisk.c"


def find(lines, needle, start=0):
    for i in range(start, len(lines)):
        if needle in lines[i]:
            return i
    sys.exit(f"FAILED: marker not found: {needle}")


def cut(lines, a, b, what):
    print(f"  -- {what}: lines {a+1}..{b} ({b-a} lines)")
    for ln in lines[a:b]:
        print(f"     {ln}")
    print()
    return lines[:a] + lines[b:]


def main():
    lines = open(P, encoding="utf-8").read().split("\n")

    # 1. the statics and the helper. From the comment opener above the marker,
    #    to the closing brace of rdisk_needs_start().
    m = find(lines, "DIAGNOSTIC, not for upstream.")
    a = m - 1
    if lines[a].strip() != "/*":
        sys.exit(f"FAILED: expected a comment opener above line {m+1}")
    b = find(lines, "return !rdisk_spun_up[dev->id];", m)
    while lines[b].strip() != "}":
        b += 1
    b += 1
    while b < len(lines) and lines[b].strip() == "":
        b += 1
    lines = cut(lines, a, b, "statics + rdisk_needs_start()")

    # 2. the CHECK_READY gate.
    m = find(lines, "DIAGNOSTIC: the motor has not been started")
    a = m - 1
    if lines[a].strip() != "/*":
        sys.exit("FAILED: expected a comment opener above the CHECK_READY gate")
    b = find(lines, "rdisk_needs_start(dev)) {", m)
    depth = 1
    b += 1
    while depth > 0:
        depth += lines[b].count("{") - lines[b].count("}")
        b += 1
    while b < len(lines) and lines[b].strip() == "":
        b += 1
    lines = cut(lines, a, b, "CHECK_READY gate")

    # 3. the spin-up flag in START STOP UNIT.
    m = find(lines, "DIAGNOSTIC: the motor is now running")
    b = m + 1
    while "rdisk_spun_up" not in lines[b]:
        b += 1
    b += 1
    lines = cut(lines, m, b, "spin-up flag")

    text = "\n".join(lines)
    for bad in ("rdisk_needs_start", "rdisk_spun_up", "rdisk_start_required",
                "RDISK_START_REQUIRED"):
        if bad in text:
            sys.exit(f"FAILED: {bad} still present after strip")

    tmp = P + ".tmp"
    open(tmp, "w", encoding="utf-8", newline="").write(text)
    os.replace(tmp, P)
    print("rdisk.c: 3 sites removed, no references remain")


if __name__ == "__main__":
    main()
