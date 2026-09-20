"""Remove each statement introduced by a DIAGNOSTIC marker comment.

The remaining sites are multi-line `epat_log(...)` calls, some behind a
one-line `if`. Transcribing them exactly is error-prone, so this finds the
marker comment and deletes through the end of the statement it introduces -
tracking parenthesis depth so a multi-line call is removed whole.

It prints every line it removes. Read that output; a strip nobody looked at is
how a working line gets deleted with the dead one.
"""
import os
import sys

ROOT = r"C:\Users\lycet\AppData\Local\Temp\claude\86box_master"
MARKER = "DIAGNOSTIC 2026-09-13"

FILES = ["src/device/lpt_epat.c"]


def strip(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    out = []
    removed = 0
    i = 0
    while i < len(lines):
        if MARKER not in lines[i]:
            out.append(lines[i])
            i += 1
            continue

        # Marker found. Drop it, then drop the statement that follows.
        print(f"  -- {path}:{i+1}")
        print(f"     {lines[i].strip()}")
        removed += 1
        i += 1

        depth = 0
        started = False
        while i < len(lines):
            ln = lines[i]
            print(f"     {ln.strip()}")
            depth += ln.count("(") - ln.count(")")
            if "(" in ln:
                started = True
            i += 1
            # End of statement: balanced, and this line closes it.
            if started and depth <= 0 and ln.rstrip().endswith(";"):
                break
            if not started and ln.rstrip().endswith(";"):
                break
        print()
    return "\n".join(out), removed


def main():
    total = 0
    for rel in FILES:
        p = os.path.join(ROOT, rel)
        text, n = strip(p)
        if n == 0:
            continue
        tmp = p + ".tmp"
        open(tmp, "w", encoding="utf-8", newline="").write(text)
        os.replace(tmp, p)
        print(f"{rel}: {n} marked statements removed")
        total += n
    if total == 0:
        sys.exit("Stripped: 0 - refusing a no-op.")
    print(f"Stripped: {total}")


if __name__ == "__main__":
    main()
