import hashlib
from pyfatfs.PyFat import PyFat
from pyfatfs.PyFatFS import PyFatFS

IMG = "../win95_XT_earlyVxD_prep.img"
OFFSET = 32256

VXD_SRC = {
    "/WINDOWS/SYSTEM/VMM32/VPICD.VXD": "../../vxd-patches/VPICD_stage1_patched.VXD",
    "/WINDOWS/SYSTEM/VMM32/VDMAD.VXD": "../../vxd-patches/VDMAD_stage1_patched.VXD",
    "/WINDOWS/SYSTEM/VMM32/VKD.VXD": "../../vxd-patches/VKD_INBOARD.VXD",
    "/WINDOWS/SYSTEM/IBKBD.DRV": "../IBKBD.DRV",
    "/WINDOWS/SYSTEM/IBVKD.386": "../IBVKD.386",
}

INBRDPC_SRC = "../inbrdpc_fixed_v4_int15shim.bin"


def md5(data):
    return hashlib.md5(data).hexdigest()


fs = PyFatFS(filename=IMG, read_only=False, offset=OFFSET)

# 1. SYSTEM.INI keyboard lines
sysini = fs.readbytes("/WINDOWS/SYSTEM.INI").decode("latin1")
before = sysini
sysini = sysini.replace("keyboard.drv=keyboard.drv", "keyboard.drv=ibkbd.drv")
sysini = sysini.replace("keyboard=*vkd", "keyboard=ibvkd.386")
assert sysini != before, "SYSTEM.INI keyboard lines not found / not replaced"
fs.remove("/WINDOWS/SYSTEM.INI")
fs.writebytes("/WINDOWS/SYSTEM.INI", sysini.encode("latin1"))
print("[deploy] SYSTEM.INI keyboard lines updated (ibkbd.drv / ibvkd.386)")

# 2. VxDs + keyboard driver files
for dest, src in VXD_SRC.items():
    with open(src, "rb") as f:
        data = f.read()
    try:
        fs.remove(dest)
        print(f"[deploy] removed existing {dest}")
    except Exception:
        pass
    fs.writebytes(dest, data)
    readback = fs.readbytes(dest)
    assert md5(readback) == md5(data), f"readback mismatch for {dest}"
    print(f"[deploy] wrote {dest} <- {src} ({len(data)} bytes, md5 {md5(data)}, verified)")

# 3. Confirm INBRDPC.SYS matches v4 (deploy if not)
with open(INBRDPC_SRC, "rb") as f:
    v4_data = f.read()
current = fs.readbytes("/INBRDPC.SYS")
if md5(current) == md5(v4_data):
    print(f"[deploy] /INBRDPC.SYS already matches v4 int15shim (md5 {md5(v4_data)}) - no change needed")
else:
    fs.remove("/INBRDPC.SYS")
    fs.writebytes("/INBRDPC.SYS", v4_data)
    readback = fs.readbytes("/INBRDPC.SYS")
    assert md5(readback) == md5(v4_data)
    print(f"[deploy] /INBRDPC.SYS replaced with v4 int15shim (md5 {md5(v4_data)}, verified)")

# 4. VMM32.VXD left untouched (Technique 15)
vmm32 = fs.readbytes("/WINDOWS/SYSTEM/VMM32.VXD")
print(f"[deploy] /WINDOWS/SYSTEM/VMM32.VXD left untouched, size={len(vmm32)} bytes")

fs.close()
print("[deploy] done, filesystem closed cleanly")
