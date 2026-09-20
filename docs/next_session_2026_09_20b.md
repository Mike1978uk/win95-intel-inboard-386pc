# Handoff 2026-09-20b — EPP validated. Dword patch staged, never booted.

The LS-120 works over EPP under Windows and the bytes are proven. What is
open is speed, and there is a two-byte patch staged for it.

---

## 1. What is now established

| | |
|---|---|
| **EPP write is byte-correct** | 36,735,152 bytes, `FC /B` → `no differences encountered` |
| **EPP throughput** | **75-99 KiB/s** - 36,735,152 B in 7 min by WALL CLOCK, +/-59 s. NOT 85.4; that figure treated a coarse reading as exact |
| **ECP read is byte-correct** | the same file, read back by the DOS driver over `ECP Read` |
| **ECP bulk works on this hardware** | vendor DOS driver, `Read Mode : ECP Read` — see below |

The verification is strong because the write and the read used **different
drivers over different transports**, so a symmetric error cannot cancel
(technique 79). Recorded in `drivers/imation_ls120/MEASURED_FACTS.md` §2x.

### ⛔ "ECP is closed on this hardware" was wrong, and is retracted

Every ECP elimination this project made was measured on the **Windows** path —
our miniport, and the vendor miniport with `ECP=1` (`3a318bf`, three configs).
The vendor **DOS** driver's ECP path had never been tested, and it works.
`/de` disables the *Epp* check and `/db` the *Eppbios* check; **nothing in the
owner's switch line suppresses ECP**, so autodetection lands on it.

So the bridge and the bus do ECP bulk. Our empty buffers are an implementation
defect. `MEASURED_FACTS.md` §2y and the skill's ECP section are corrected.

⚠ The owner stated this repeatedly across sessions while the written record
contradicted him. Technique 124: repetition is a signal to open the document,
not to re-argue.

---

## 2. The open question is speed, and ECP may not be the answer

**Inference, not measurement.** `FC` ran 00:26 → 00:47:54, ~22 min, of which the
`C:` side is ~70 s. That bounds the ECP read at **~31 KiB/s or better**, with
FC's compare overhead inside the figure. Against EPP's 75-99 KiB/s it is
*suggestive that ECP is slower here* — and that is all it is.

If it holds, it reframes everything: EPP already beats ECP on this machine, and
the lever is EPP **width**, not transport.

---

## 3. The lever: EPP dword, staged and never booted

`drivers/imation_ls120/eppfast_deploy/` — copy with the CF in a reader.

| file | md5 |
|---|---|
| `SD120PPD.SYS` patched DOS driver | `96018ce66d2312a26fa16193b5273a37` |
| `SD120PPD.SYK` stock DOS driver | `cbb42e8eb7847869e274e45f258cf718` |
| `SD120PPD.MPD` patched miniport | `4f1fb59cb7dda09d1002c34bfe32ef0f` |
| `SD120PPD.STK` stock miniport | `08104ffb559ae4b47b84377daee473bc` |
| `BOUNDARY.BIN` 4,000,000-byte test file | `a2ea9a7af4c73214840b2988d334a353` |

### What the patch does

Both drivers contain a dword bulk path — `ScsiPortRead/WritePortBufferUlong` in
the miniport, read mode **11** / write mode **4**, against byte-wide mode 10/3.
**One I/O access carries four bytes instead of one.** On this machine technique
109e measured 3.90 us of fixed per-access synchronisation against 1.87 us per
byte inside it, so dword should take 5.77 us/byte to **2.85** — up to 2x.

It is unreachable because the upgrade is gated at rva `0x80AB` (MPD) /
`0x87D5` (SYS) on a byte set **only** as a by-product of a successful chipset
detection — and detection is suppressed by `/de /db /ni`, because it writes
`0x22`/`0x23`, which alias onto the 8259 on an XT (technique 75).

`patch_sd120ppd_eppfast.py` nops the `jne`. Two bytes. It adds **no port
access**, leaves the CPU-class test below it intact, and with `/fe` the transfer
routines stay on the `[0x20BD9]` branch that never reaches `0x22`/`0x23`.
Round-trip verified byte-identical both files; refuses a no-op.

⛔ `/ded` and `/fed` are **dead switches in the miniport** — parsed, stored,
read by nothing. Do not use either as an A/B; it would look like a clean
negative and mean nothing.

### Do the DOS driver FIRST

Only it **prints the mode it chose**:

```
    Read  Mode : EPP Fast      <- dword, the gate opened
    Read  Mode : EPP Normal    <- byte-wide, it did not
```

Patch it, add `/fe` to its `CONFIG.SYS` line, boot to DOS, read the banner.
A printed string instead of a stopwatch inference, on a driver that cannot
damage the Windows install. **`/fe` there is for that test only** — the
verification copy wants the DOS side on a different transport from the write.

### Safety

If dword fails, the driver's own ladder at `0xAEEC` calls `0xABC6`, latches
`[0x20C7C]=1` and drops to byte-wide for the rest of the boot. Worst case is
today's behaviour.

⚠ That latch is **one-way and never cleared**, so a transient error costs the
width until reboot. **Always time the first copy after a fresh boot**, and use
two consecutive copies as a free detector: equal → dword held; second slower →
the latch fired.

⚠ It catches *detected failures*, not **silent wrong data** (technique 62). So
`FC /B` stays on the list after the patch, not just before it.

---

## 4. Next session, in order

1. **Confirm both destination paths on the card** — not verified, FC had the
   disk locked. Believed `\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD` and the vendor
   install source. Deploy to **both**: a refresh from a stale install source
   silently reverts, which cost a week in September.
2. **DOS driver, read the banner.** Settles whether the gate opens.
3. **Time `BOUNDARY.BIN` properly** — `TIME` stamps either side in a batch on
   the machine so the serial round trip is outside the measurement, arms
   interleaved not back-to-back (technique 126e). This gives the ECP number we
   are missing and the byte-wide EPP baseline in the same session.
4. **Then the miniport**, and `FC /B` after it.

`BOUNDARY.BIN` is 4 MB so the loop is ~3 minutes, not an hour. Every dword
holds its own address, so a byte-lane swap — the classic failure of a 32-bit
transport on an 8-bit bus, and exactly what this patch risks — is visible at a
glance rather than inferred.

---

## 5. Process notes worth keeping

- **Read the guest's clock, not your own.** I reported FC as "past an hour" and
  recommended breaking it; `FC.TXT` is stamped 00:47:54 and it had taken 22
  minutes, inside my own prediction. The timestamp was available throughout.
- **A long run needs a progress signal.** Redirecting to a file left nothing on
  the console to watch. Put `TIME` stamps around it instead.
- **File I/O and console I/O block independently.** `dir_list` timed out
  repeatedly while FC held the disk, though `dos_status` answered at 41 ms.

---

## 6. DEPLOYED to the CF, 2026-09-20 — never booted

Card in the host reader. All three live copies hashed to stock first
(`08104ffb…` / `cbb42e8e…`), so the baseline was clean.

| on the card | md5 | was |
|---|---|---|
| `\WINDOWS\SYSTEM\IOSUBSYS\SD120PPD.MPD` | `4f1fb59c…` | `.B4F` |
| `\LS120VEN\sd120ppd.mpd` | `4f1fb59c…` | `.B4F` |
| `\SD120PPD\SD120PPD.SYS` | `96018ce6…` | `.B4F` |
| `\BOUNDARY.BIN` | `a2ea9a7a…` | new |

All verified **at the destination**, not from the staging copy.

`CONFIG.SYS` is now the `/fe` variant (401 bytes, `CONFIG.EPF`), so the next
boot is the DOS banner test. `CONFIG.W95` is the route back to Windows;
`CONFIG.B4E` is the pre-`/fe` DOS line. `EPPFAST.TXT` on the card root has the
steps, the revert commands and the 46-second baseline.

**Nothing has been booted.** The banner is the first evidence.

### Two self-inflicted traps hit while deploying, both already in the skill

- **Native Windows Python cannot see MSYS paths.** `/d/CONFIG.SYS` threw
  `FileNotFoundError`; it needs `D:\CONFIG.SYS`. Assertions-before-write meant
  nothing was damaged.
- **Backslashes through a bash heredoc, twice** (technique 84 says not to).
  `"D:\r\n"` wrote `COPY C:\BOUNDARY.BIN D:\r` with a bare LF instead of
  `D:\` + CRLF — a wrong command in the file the owner would have typed from.
  Rewritten with the Write tool and verified byte by byte.

---

## 7. FIRST PATCHED RUN, 2026-09-20 — bytes perfect, speed unmoved

`TBW.BAT` under Windows, patched miniport, 4,000,000 bytes to `J:`:

| | elapsed | rate |
|---|---|---|
| copy 1 | **53.17 s** | 73.5 KiB/s |
| copy 2 | **44.49 s** | 87.8 KiB/s |
| verify | `FC: no differences encountered` | |

- **Nothing is broken.** Byte-identical, including the partial final sector and
  the short final ATAPI burst. If dword engaged, it is byte-correct.
- **No downgrade.** Copy 2 was *faster* than copy 1, so the `[0x20C7C]` latch
  never tripped.
- **No measurable gain.** 73.5-87.8 KiB/s sits inside the pre-patch 75-99.

⛔ **We cannot say whether dword engaged**, because no Windows baseline was ever
taken with this instrument. The only comparison available is the coarse +/-59 s
wall-clock figure, whose range swallows the result. **That is a control failure
and it was mine** — this very document says establish a control first.

**Drivers reverted to stock on the card**; the patched trio kept as `.EPF`,
stock as `.B4F`, swap commands in `\ABTEST.TXT`. Next action is the stock
baseline with the same batch, then a like-for-like comparison.

### The lever is smaller than advertised — arithmetic, not opinion

| | |
|---|---|
| bus time for 4 MB byte-wide (5.77 us/byte) | **23 s** |
| measured | **44-53 s** |
| everything that is not bus | **21-30 s** |

Even byte-wide, **the bus is only about half the budget** — the rest is bridge
nWAIT, drive write latency, FAT updates and Windows. So a perfect dword win is
worth **~20-25% overall, not 2x**. Earlier notes in this document say "up to
2x"; that was always a claim about the *bus* and it was allowed to read as a
claim about the copy. Corrected here.
