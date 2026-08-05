import sys
from pyfatfs.PyFat import PyFat
from pyfatfs.PyFatFS import PyFatFS

IMG = "../win95_XT_earlyVxD_prep.img"

fs = PyFatFS(filename=IMG, read_only=True, offset=32256)

def cat(path):
    try:
        data = fs.readbytes(path)
        print(f"--- {path} ({len(data)} bytes) ---")
        print(data.decode("latin1"))
    except Exception as e:
        print(f"--- {path}: ERROR {e}")

def ls(path):
    try:
        print(f"--- ls {path} ---")
        for name in fs.listdir(path):
            print(" ", name)
    except Exception as e:
        print(f"--- ls {path}: ERROR {e}")

cat("/CONFIG.SYS")
cat("/WINDOWS/SYSTEM.INI")
ls("/WINDOWS/SYSTEM/VMM32")
ls("/WINDOWS/SYSTEM")
fs.close()
