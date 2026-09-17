# LS-120 on the real 5160 over COMrade — 2026-09-17

The exact working sequence, recorded because it was re-derived twice. **DOS COMrade**
(`io_in`/`mem_read` exist; the Win95 build has neither).

## The procedure that works

```
# 1. generate (host).  --spin FFFF is the proven reset settle; 2000 aborts every command.
python drivers/imation_ls120/tools/gen_rw_probe.py --spin FFFF --mediaread --lba 0 > out.b64
python -c "import base64;open('X.SCR','wb').write(base64.b64decode(open('out.b64','rb').read().strip()))"

# 2. push (bytes bypass the model context, CRC-32 verified end to end)
file_write  path=C:\X.SCR  src_path=<repo>\docs\captures\2026-09-17_ls120\X.SCR

# 3. run
run_command  "DEBUG < C:\X.SCR > C:\X.OUT"

# 4. pull
file_read   path=C:\X.OUT  dest_path=<repo>\docs\captures\2026-09-17_ls120\X.OUT
```

### Traps that cost time, all hit today

- **`DEBUG` is the right command. `C:\WINDOWS\COMMAND\DEBUG.EXE` does NOT exist here** and
  gives `Bad command or file name`.
- **`run_command` returns `idle` in ~1 s while DEBUG is still running.** The redirect leaves
  nothing on the console to poll. **Reading the .OUT immediately returns 0 bytes** — that is
  *read too early*, **not** an empty result and **not** the DOS-7 stderr trap. Wait, then read.
- **`dos_status` RTT is the load meter**: ~30 ms idle, **177 ms while DEBUG grinds**. Use it
  to tell "still working" from "wedged" without touching the console.
- **`file_write`/`screen_read` time out at 8 s while DEBUG holds the console.** File and
  console I/O fail independently; a timeout is not a result.
- **`src_path` is confined to the repo root** (`COMRADE_HOST_FS_ROOT`). Staging a probe in the
  scratchpad is refused — put it in `docs/captures/`.
- **Python here cannot resolve MSYS paths** (`/tmp`, `/d/`). bash can. Stage with `cp`, then
  let Python use a relative path.

## Measured on the hardware, 2026-09-17

All three from `INQNOW.OUT` / `RWNOW.OUT`, SPP/nibble transport.

**Hardware ID — the drive identifies itself:**

```
0700  00 80 00 01 7B 00 00 00 - 4D 41 54 53 48 49 54 41   ....{...MATSHITA
0710  4C 53 2D 31 32 30 20 43 - 4F 53 4D 20 20 20 30 34   LS-120 COSM   04
0720  30 32 37 30                                          0270
```

byte 0 `00` direct-access, byte 1 `80` removable, byte 4 `7B` additional length 123.

**Media sense — REQUEST SENSE works and the unit attentions drain in order:**

```
2040  70 00 06 ... 29 00     key 6 UNIT ATTENTION, ASC 29  POWER ON / RESET OCCURRED
2060  70 00 06 ... 28 00     key 6 UNIT ATTENTION, ASC 28  MEDIUM MAY HAVE CHANGED
2080  70 00 00 ... 00 00     key 0 NO SENSE - clean
20A0  70 00 00 ... 00 00     clean
```

**Media access — `READ(10)` at LBA 0 returns a real FAT boot sector:**

```
2200  EB 3E 90 4D 53 57 49 4E 34 2E 30    ".>.MSWIN4.0"
23F0  ... 57 49 4E 42 4F 4F 54 20 53 59 53 ... 55 AA   "WINBOOT SYS" + signature
```

Stage marker `1F00 = 95`, the final rung of the ladder.

⚠ The other dumped regions hold Windows help text (*"You can set Windows Explorer to show
o…"*). That is **stale low memory in unwritten slots**, not probe output. Read the marker and
the named regions, not whatever is lying around.

## Conclusion

**The transport, the drive and the medium are all working on the 5160 today.** Identify,
sense, and a verified media read, over the EPAT bridge, via nibble/SPP. Nothing below the
miniport is broken, so the Windows-side failure is above the transport.
