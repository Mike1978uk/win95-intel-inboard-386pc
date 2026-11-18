"""Build a logging CUSTOM.GRA for Flight Simulator 3.0 from the owner's PS21.GRA.

    python -I tools/fs3gra/build.py <PS21.GRA> <out CUSTOM.GRA>

The output holds Microsoft's driver code, so it is built for the owner's own copy and is
never committed or published. See loggra.asm for what the tail does.
"""
import os
import struct
import subprocess
import sys
import tempfile

NASM = r"C:\msys64\mingw64\bin\nasm.exe"


def main():
    src, out = sys.argv[1], sys.argv[2]
    gra = bytearray(open(src, "rb").read())
    if struct.unpack("<H", gra[0:2])[0] != len(gra) - 2:
        sys.exit("length word does not match the file: not a GRA as read in the renderer doc")
    if gra[2] != 0xE9 or gra[5] != 0xCB:
        sys.exit("header is not JMP near + RETF")
    oinit = 5 + struct.unpack("<h", gra[3:5])[0]
    base = (len(gra) + 15) & ~15            # the tail starts on a paragraph
    gra += b"\0" * (base - len(gra))
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as t:
        tail = os.path.join(t, "tail.bin")
        subprocess.run([NASM, "-f", "bin", "-o", tail, f"-DBASE={base}", f"-DOINIT={oinit}",
                        os.path.join(here, "loggra.asm")], check=True)
        gra += open(tail, "rb").read()
    struct.pack_into("<h", gra, 3, base - 5)        # header JMP -> log_init
    struct.pack_into("<H", gra, 0, len(gra) - 2)
    open(out, "wb").write(gra)
    print(f"original init {oinit:#x}, tail at {base:#x}, {len(gra)} bytes -> {out}")


if __name__ == "__main__":
    main()
