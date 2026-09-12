#!/usr/bin/env python3
"""MOVED 2026-09-13 to tools/pedis.py, and generalised to any flat i386 PE.

This copy was hardcoded to SD120PPD.MPD: it ignored argv[1] as a filename and
filtered code sections by the names that one file happened to have. Pointed at
any other binary it produced an empty dump, which reads exactly like a clean
result. The owner's call: "we need a tool not just for this one driver."

    python tools/pedis.py <file> sections|imports|io|str|all|dis <rva> [n]
"""
import os
import sys

NEW = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "..", "..", "tools", "pedis.py")
print(__doc__, file=sys.stderr)
print(f"  -> {os.path.normpath(NEW)}", file=sys.stderr)
raise SystemExit(2)
