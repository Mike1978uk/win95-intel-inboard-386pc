"""Decode M8BYTE.BIN: per test, the timeouts, GP_STAT, and the runs found in the row."""
import sys
names = ["baseline", "86E8=80 alone", "86E9=01 alone", "96E8=07 alone", "A6E8=AA alone",
         "9AE8=B3 alone (no word cmd)", "9AE9=40 alone (no word cmd)", "86E8=80 then 86E9=01",
         "64 x IN E2E8 byte", "64 x IN E2E9 byte", "32 x IN E2E8 word"]
names2 = ["CUR_X 40 len4, 86E9=01", "CUR_X 40 len8, 86E9=01", "CUR_X 50 len4, 86E9=01",
          "A6E8=20, 86E9=01", "AAE8=B3, 9AE9=40 (no word cmd)", "BAEE=01 alone",
          "BAEE=01, BAEF=00", "9AE8=B3, 9AE9=40 (no word cmd)"]
d = open(sys.argv[1], "rb").read()
assert d[:4] in (b"M8B1", b"M8B2")
if d[:4] == b"M8B2":
    names = names2
n = int.from_bytes(d[4:6], "little")
for t in range(n):
    r = d[6 + t * 516: 6 + (t + 1) * 516]
    gs = int.from_bytes(r[2:4], "little")
    px = r[4:]
    head = f"{t:2} {names[t]:30} idle_to={r[0]} data_to={r[1]} GP_STAT={gs:04X}  "
    if t < 8:
        runs, x = [], 0
        while x < 512:
            c = px[x]; s = x
            while x < 512 and px[x] == c: x += 1
            if c != 0x3C: runs.append(f"{s:03X}+{x-s} col {c:02X}")
        print(head + ("; ".join(runs) if runs else "row all 3C (no draw)"))
    else:
        print(head + " ".join(f"{b:02X}" for b in px[:64]))
