"""Read a bed's memory through 86Box's automation socket (tools/bed_launch.ps1 -Automation).

    python -I tools/bed_mem.py <bed dir> <port> <addr> <len> <out file>
    python -I tools/bed_mem.py <bed dir> <port> find <ascii marker> <len> <out file>

'find' scans conventional memory (0-A0000h) for the marker and saves len bytes from it.
"""
import json
import os
import socket
import sys

CHUNK = 65536


class Bed:
    def __init__(self, bed, port):
        token = open(os.path.join(bed, "automation.token")).read().strip()
        self.s = socket.create_connection(("127.0.0.1", port))
        self.f = self.s.makefile("rw", newline="\n")
        if not self.cmd(f"auth {token}").get("ok"):
            sys.exit("auth refused")

    def cmd(self, line):
        self.f.write(line + "\n")
        self.f.flush()
        return json.loads(self.f.readline())

    def read(self, addr, n):
        out = bytearray()
        while n > 0:
            k = min(n, CHUNK)
            j = self.cmd(f"memr {addr:#x} {k}")
            if not j.get("ok"):
                sys.exit(f"memr {addr:#x}: {j}")
            out += bytes.fromhex(j["data"])
            addr += k
            n -= k
        return bytes(out)


def main():
    bed, port = sys.argv[1], int(sys.argv[2])
    b = Bed(bed, port)
    if sys.argv[3] == "find":
        marker, n, out = sys.argv[4].encode(), int(sys.argv[5], 0), sys.argv[6]
        low = b.read(0, 0xA0000)
        at = low.find(marker)
        if at < 0:
            sys.exit(f"{sys.argv[4]} not found below A0000h")
        print(f"marker at {at:#x}")
        data = b.read(at, n)
    else:
        addr, n, out = int(sys.argv[3], 0), int(sys.argv[4], 0), sys.argv[5]
        data = b.read(addr, n)
    open(out, "wb").write(data)
    print(f"{len(data)} bytes -> {out}")


if __name__ == "__main__":
    main()
