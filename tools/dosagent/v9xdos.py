#!/usr/bin/env python3
"""Probe client for the V9x DOS agent (memory, ports, text screen, keys, reboot).

Speaks the V9XR frame protocol on TCP 9869 (the DOS agent's own messages 0x00B0-0x00B6,
src/guest-dos/hw_dos.c). Files, exec and the detached handoff stay with v9xctl.ps1.

  v9xdos.py [--host H] [--port N] mem <addr> <len> [--out FILE]
  v9xdos.py ... memw <addr> <hex bytes>
  v9xdos.py ... in <port> [--word] [--count N]
  v9xdos.py ... out <port> <value> [--word]
  v9xdos.py ... screen
  v9xdos.py ... keys <text> [--enter]
  v9xdos.py ... reboot [--cold]

Addresses take 0x hex or seg:off (B800:0000). The probes run only while the agent is in the
foreground, not while a handed-off program runs.
"""
import argparse
import random
import socket
import struct
import sys

MAGIC = b"V9XR"
VERSION = 0x0100
HELLO, ERROR = 0x0001, 0x8FFF
MEM_READ, MEM_WRITE, IO_IN, IO_OUT, SCREEN, KEYS, REBOOT = range(0x00B0, 0x00B7)

# US layout: character -> (scan code, character). Enough to type DOS command lines.
_ROWS = [("1234567890-=", 0x02), ("qwertyuiop[]", 0x10), ("asdfghjkl;'`", 0x1E),
         ("\\zxcvbnm,./", 0x2B)]
_SHIFT = {"!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
          "(": "9", ")": "0", "_": "-", "+": "=", "{": "[", "}": "]", ":": ";", '"': "'",
          "~": "`", "|": "\\", "<": ",", ">": ".", "?": "/"}
SCAN = {}
for chars, first in _ROWS:
    for i, c in enumerate(chars):
        SCAN[c] = first + i
SCAN[" "] = 0x39


def key_word(c):
    base = _SHIFT.get(c, c.lower())
    if base not in SCAN:
        raise SystemExit(f"no scan code for {c!r}")
    return (SCAN[base] << 8) | ord(c)


class Agent:
    def __init__(self, host, port):
        self.s = socket.create_connection((host, port), timeout=15)
        self.rid = random.randint(1, 0x7FFFFFFF)
        label = b"v9xdos.py"
        self.call(HELLO, struct.pack("<HHH", VERSION, VERSION, len(label)) + label)

    def _recv(self, n):
        buf = b""
        while len(buf) < n:
            part = self.s.recv(n - len(buf))
            if not part:
                raise SystemExit("connection closed by the agent")
            buf += part
        return buf

    def call(self, msg, payload=b""):
        self.rid += 1
        self.s.sendall(MAGIC + struct.pack("<HHIIII", VERSION, msg, self.rid, 0,
                                            len(payload), 0) + payload)
        head = self._recv(24)
        if head[:4] != MAGIC:
            raise SystemExit("bad frame from the agent")
        _, mtype, _, _, length, _ = struct.unpack("<HHIIII", head[4:])
        body = self._recv(length)
        if mtype == ERROR:
            raise SystemExit(f"agent error: {body[4:].decode('latin1', 'replace')} "
                             f"(status {struct.unpack('<I', body[:4])[0]})")
        return body


def addr(text):
    if ":" in text:
        seg, off = text.split(":")
        return (int(seg, 16) << 4) + int(off, 16)
    return int(text, 0)


def hexdump(base, data):
    for i in range(0, len(data), 16):
        row = data[i:i + 16]
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        print(f"{base + i:06X}  {row.hex(' '):<47}  {text}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9869)
    sub = ap.add_subparsers(dest="verb", required=True)
    p = sub.add_parser("mem"); p.add_argument("addr"); p.add_argument("length", type=lambda t: int(t, 0)); p.add_argument("--out")
    p = sub.add_parser("memw"); p.add_argument("addr"); p.add_argument("data")
    p = sub.add_parser("in"); p.add_argument("ioport", type=lambda t: int(t, 0)); p.add_argument("--word", action="store_true"); p.add_argument("--count", type=int, default=1)
    p = sub.add_parser("out"); p.add_argument("ioport", type=lambda t: int(t, 0)); p.add_argument("value", type=lambda t: int(t, 0)); p.add_argument("--word", action="store_true")
    sub.add_parser("screen")
    p = sub.add_parser("keys"); p.add_argument("text"); p.add_argument("--enter", action="store_true")
    p = sub.add_parser("reboot"); p.add_argument("--cold", action="store_true")
    a = ap.parse_args()
    ag = Agent(a.host, a.port)

    if a.verb == "mem":
        base, data = addr(a.addr), b""
        while len(data) < a.length:
            n = min(32768, a.length - len(data))
            data += ag.call(MEM_READ, struct.pack("<II", base + len(data), n))
        if a.out:
            open(a.out, "wb").write(data)
            print(f"{len(data)} bytes -> {a.out}")
        else:
            hexdump(base, data)
    elif a.verb == "memw":
        n = struct.unpack("<I", ag.call(MEM_WRITE, struct.pack("<I", addr(a.addr)) + bytes.fromhex(a.data)))[0]
        print(f"wrote {n} bytes")
    elif a.verb == "in":
        w = 2 if a.word else 1
        data = ag.call(IO_IN, struct.pack("<HBH", a.ioport, w, a.count))
        vals = struct.unpack(f"<{a.count}{'H' if w == 2 else 'B'}", data)
        print(" ".join(f"{v:0{w * 2}X}" for v in vals))
    elif a.verb == "out":
        w = 2 if a.word else 1
        ag.call(IO_OUT, struct.pack("<HBH", a.ioport, w, 1) + struct.pack("<H" if w == 2 else "<B", a.value))
        print("ok")
    elif a.verb == "screen":
        d = ag.call(SCREEN)
        mode, cols, rows, cx, cy, page = d[:6]
        cells = d[6:]
        print(f"mode {mode:02X}h {cols}x{rows} page {page} cursor {cx},{cy}")
        for r in range(rows):
            print(cells[r * cols * 2:(r + 1) * cols * 2:2].decode("cp437").rstrip())
    elif a.verb == "keys":
        words = [key_word(c) for c in a.text] + ([0x1C0D] if a.enter else [])
        n = struct.unpack("<I", ag.call(KEYS, b"".join(struct.pack("<H", w) for w in words)))[0]
        print(f"stuffed {n} of {len(words)} keys")
    elif a.verb == "reboot":
        ag.call(REBOOT, bytes([0 if a.cold else 1]))
        print("rebooting")


if __name__ == "__main__":
    main()
