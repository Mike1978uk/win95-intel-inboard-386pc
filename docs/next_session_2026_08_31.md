# Next session — 2026-08-31

Everything is staged and deployed. **One boot answers the question this session was built around.**

---

## Step 0 — the boot. Ten minutes, and it is the whole thing.

The CF is back in the 5160's reader as of the end of 2026-08-30, with phase 1 already in place:

| where | file | md5 |
|---|---|---|
| `WINDOWS\SYSTEM\IOSUBSYS\PORT.PDR` | phase 1 driver | `e3f01f8eb9f4a5e0f19e768a86a02e0b` |
| `C:\PORTPDR\PORT.PDR` + `PORT.INF` | install source | same |

The phase 0 device node has **no resources**, and `LogConfig` is only applied at install time, so it
must be removed and re-added rather than updated:

1. Device Manager → remove the existing *XT-IDE port driver* node
2. Add New Hardware → **No, I want to select from a list** → **Hard disk controllers** → Have Disk
3. Type `C:\PORTPDR` — do **not** Browse (issue #3)
4. Reboot with `F8` → **Logged**
5. `grep -i "port.pdr" /d/BOOTLOG.TXT`

### ⚠️ Read before booting

Phase 1 issues `IDENTIFY DEVICE` to **the live boot disk** while the real-mode BIOS is still driving
it. `IDENTIFY` does not touch media, and the code waits for BSY to clear before acting, so the drive
must be idle before anything happens — but a race with an in-flight real-mode transfer is not
impossible.

**The owner is imaging the CF before this boot** (2026-08-30, from the card reader). That covers
it. Record where the image landed, alongside the others.

This session could not take one itself - raw access to the physical device needs elevation
(Technique 14). What it did take is a configuration backup at
`OneDrive/Desktop/XT_project/cf_config_backup_2026_08_30/` - registry hives, `CONFIG.SYS`,
`AUTOEXEC.BAT`, `SYSTEM.INI`, `IOS.INI`, all of `IOSUBSYS`, and the logs. 1.1 MB. Handy as a
quick config restore, but not a substitute for the image: it protects the configuration, not
the filesystem.

## What the one line means

`AEP_INITIALIZE` returns success only if a drive answered **and** its IDENTIFY data validated, so
the result is one bit and it is unambiguous:

### `Init Success port.pdr` — the milestone

**We are talking to the CF from 32-bit protected mode.** Addressed the card at `0x300`, issued
`IDENTIFY`, and got back a 512-byte structure whose model string is readable ASCII. Nobody has done
this on an 8-bit XT-IDE card under Windows 95 before.

Next: **phase 2**, in this order.

1. **A readback channel, first.** Right now we cannot see *which* transport won or what the drive
   calls itself, and phase 2 cannot be debugged blind. Cheapest option that fits this machine:
   have the driver stash `XTIDE_ActiveTransport` and the 40-byte model string somewhere readable
   after the fact. Writing to a file from a VxD at init is awkward; a fixed low-memory scratch area
   read back over COMrade from DOS is likely cheaper. Decide this before writing transfer code.
2. **Sector reads** — `READ SECTORS` (`0x20`), one sector, LBA or CHS, through the same transport.
   Compare against the same sector read by the host from the CF in the reader. Byte-exact or it is
   not working.
3. **Writes.** `XTIDE_ReadData`'s comment already flags the trap: reads are low-then-high, writes
   are **high to `+8` first, then low to `+0` commits the word**. Getting that backwards looks like
   random corruption. Test on a scratch CF, never the boot disk.
4. **Slave.** The path is already parameterised — `XTIDE_TryUnit`, and the DEV read-back is written
   and unused. Needs a two-drive cable and a CF adapter jumpered as slave, which the owner does not
   have yet.

### `Init Failure port.pdr` — three candidates, in order of likelihood

Do **not** guess between them; each has a distinct check.

1. **No I/O resource on the node.** `XTIDE_Probe` bails immediately if `DDB_base_ioa` is zero.
   Check Device Manager → the node → Resources: it should show `0300-030F`. If it shows nothing, or
   still shows the phase-0 "not configured" warning, the `LogConfig` did not apply — which means the
   node was updated rather than removed and re-added. Redo step 0 properly.
2. **Both transports produced garbage.** The likeliest real failure, and the interesting one. It
   would mean the card is neither the `+8` latch nor plain 8-bit PIO — which points straight at the
   unresolved `bDevice = 0x0A` question below. Next move is `XTIDECFG.COM` (see Parked).
3. **The drive never went ready.** `XTIDE_WaitNotBusy` timed out at `XT_SPIN = 400000` polls. On a
   4.77 MHz bus that is a long time, so this is unlikely — but if the other two are excluded, raise
   the limit and re-test rather than assuming the card is dead.

**Whatever the result, it is real information.** A failure narrows the transport question to
something a single config screen answers.

---

## Parked, with a precise next action each

| item | state | next action |
|---|---|---|
| **`bDevice = 0x0A`** | The card's ROM says `0x0A` where every stock XT build says `0x06`. It names either XTIDE rev 2 or Lo-tech XT-CF — different transports. The v2.0.0-era enum was not retrievable | Run `XTIDECFG.COM` on the machine and read the **Device Type** line. One screen. Phase 1's autodetect may answer it first |
| **`0x306`/`0x307` both read `0x09`** | Unexplained. Two adjacent taskfile registers should not return the same value | Leave it. If phase 1 succeeds it is moot; if it fails, this is evidence about the decode |
| **`0x379` reads `0x00`** | Unexplained. An idle LPT status should not be all-zero | Low priority. Recorded in `measured_system_map_2026_08_30.md` |
| **#19 T130.MPD on real hardware** | Andrew has it booting in 86Box with 32-bit disk access. Staged at `C:\T130`, use `T130-XT.INF` | Owner's call: it comes **after** the XT-IDE driver. Andrew is owed the outcome once it runs |
| **#22 LS-120** | Parked, not closed. Real-mode driver works, keyboard stable | Nothing. Door left open to writing our own driver later |
| **#8 Mach8** | Accelerator confirmed alive and idle (`SUBSYS_STAT` = `0xAB`, GPIDLE set, INVALIDIO clear) | Free untried experiment: run the card at **512 KB** instead of 1 MB. If the self-test passes at one size, the defect is the 4/8-bit width handling (Technique 73) |

## Owed to people

- **@OBattler** — tagged on #21 with credit for the XT-IDE port and the broken-binary warning. No
  reply yet at end of session; none required.
- **@andrew-hoffman** — told about the CD driver. **Still owed** the T130B outcome once it runs on
  the real 5160. Ledger row is in the Outstanding section.

## Where the code is

- `drivers/xtide_pdr/` — the driver. `README.md` there is the full technical record; this file is
  only the running order.
- `drivers/xtide_pdr/src/XTIDETR.ASM` — our transport. The DDK sample is not redistributable, so
  `build.ps1` patches a working copy with asserted edits that fail loudly if an anchor moves.
- `docs/measured_system_map_2026_08_30.md` — what the machine actually answers, read-only.
- `docs/resources_and_sources.md` — every source and every hosted file, one page.

`git status` was clean at end of session; everything is pushed.
