# Handoff — 2026-09-20g

Read this first. The BackPack request (#37, asked by @andrew-hoffman on #23 and
@JoshRodd on 86Box/86Box#8010) went from "notes" to "the vendor driver talks to
our model". One unknown remains before a drive letter.

---

## Where to pick up

**The EEPROM.** The driver reads a 93C46 to decide the pod is a BackPack
CD-ROM, and rejects us because ours reads as zeros. Three things, in order:

1. Which bit of register `0x00` carries the EEPROM DO line.
2. The word layout the driver parses, and the value meaning "CD-ROM".
3. Then hang the ATAPI engine from `lpt_epat.c` off the task file at `0x40`,
   bound to a CD-ROM. `CDROM_BUS_LPT` is in `cdrom.h` already, unused.

All three are answerable from `drivers/microsolutions_backpack/BPCDDRV_SYS.asm`
— **we hold the driver, so read it rather than iterate runs.** That steer came
from the owner mid-session and it was the right one.

The bed is `vm_bpck/`, config `tools/fixtures/86box.cfg.bpck`.
Test ISO for when there is something to mount:
`C:\Users\lycet\Downloads\Windows 98 Second Edition.iso`.

---

## What was measured

| | |
|---|---|
| wire protocol | ✅ knock, connect, ident complement pair, register addressing |
| chain scan | ✅ units 01..09+, two connects, 55,756 traced lines |
| task file | ⭐ **`0x40`** — `mov al,0x40` / `or al,[bp+8]` at `0x1573` |
| pod identity | 93C46 on `0x06`: CS `0x08`, DI `0x02`, CLK `0x01`; 1024 bits out through `0x00` |
| `0x0B` | bit 7 is a flag (`0x16D7`); `(val & 0x3F) >= 5` enables a capability (`0x18CA`) |
| primitives | `0xC05` address a register, `0xD5F` read it |

⛔ **The Ditto is a different product.** Only the Micro Solutions bridge is
shared, which is why layer 1 was transliterated from `lpt_ditto.c`. Do not
describe the work as "using the Ditto" — it is `lpt_bpck.c`, its own device.

⚠ `BPCDDRV.SYS` is **not XT-safe**: it masks the PICs at `0x28B8` with writes to
`0x20`/`0xA0`, and probes `0x22`/`0x23`, which alias on an XT bus.

---

## Bed gotchas that cost this session

- **86Box rewrites `86box.cfg` with its own defaults when a launch fails.** A
  missing ROM on the first launch left `machine = ibmxt`, `gfxcard = none`,
  no disk — and the next run was a black screen that looked like a bad machine
  choice. Check the config after any failed launch.
- **A VM directory needs its own `roms` junction**; the repo's `roms/` is not on
  86Box's search path (`<vm>/roms`, `<exe>/roms`, `%LOCALAPPDATA%\86Box\roms`).
- **`lpt_ditto.c` logging sits behind `#if 0`**, and its `.obj` can be months
  stale — prove a log string is in the exe (`grep -a -c`) before trusting an
  empty log. `strings` returns nothing for this binary; use `grep -a`.
- **86Box buffers its log until exit.** An empty log mid-run means nothing.
- **Re-run `tools/pe_subsystem_gui.py` after every build** or a console window
  opens behind the VM.
- `deskpro386` keeps disk geometry in CMOS and needs Compaq's setup disk.
  `hdc_1 = xtide_at` sidesteps BIOS disk setup entirely.

---

## Standing rules reaffirmed

- ⛔ **No reply to any GitHub comment without the owner approving the wording.**
  Submitting a PR is not permission to converse on it. Some detail now exists
  only as #8010 comments; check there before re-deriving.
- **The owner drives the VM.** No headless runs with kill timers.
- **Read the binary we hold before running another experiment.**
