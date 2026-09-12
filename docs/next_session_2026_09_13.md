# Where everything stands — end of 2026-09-12

Supersedes the earlier version of this file, whose "ONE THING TO DO FIRST" is now answered.

---

## CLOSED TODAY

### The LS-120 driver is correct. The failure is timing, on hardware only.

Same driver (`md5 976e4114`), same image, measured both sides:

| driver | real 5160 | the bed |
|---|---|---|
| `xtidemp.mpd` | 23 | 21 |
| `t130.mpd` | 93 | 73 |
| **`ls120mp.mpd`** | **1520 — `Init Failure`** | **6 — `Init Success`** |

The other two agree closely, so the bed is faithful and ours is the outlier by 250x. In emulation
the driver connects, pulses SRST, reads the ATAPI signature `14 EB`, issues PACKET commands and
moves data — **325 EPAT protocol lines, 11 commands, 11 block writes, 6 block reads** — then
returns `Init Success` and Windows reaches a desktop.

`docs/epat_emulation_result_2026_09_11.md` predicted exactly this: *"the bridge completes
immediately by design (it models no drive latency), so emulation will not reproduce that
timeout."* It does not, and **that non-reproduction is the diagnosis**: the real drive is slow to
come ready, the state machine spends `LS_TICKS_COLD` (3000) then `LS_TICKS_READY` (8000), the SRB
fails and init fails with it. 1520 log units is that budget being spent.

**Next on this: the spin-up timing, on the bench.** The bed cannot measure it.

Two hypotheses raised and **killed** today, both by static reasoning, recorded so nobody retries:

- `NumberOfPhysicalBreaks = 1` vs `LS_MAX_XFER = 65024` "inconsistent" — **wrong**. The DDK's own
  parallel-port sample `PC2X.H` defines `MAX_TRANSFER_LENGTH = 64 * 1024` with
  `NumberOfPhysicalBreaks = 1`. Same pairing.
- `BufferAccessScsiPortControlled = TRUE` — **not it**. SCSIPORT accepts it; the bed reaches
  `Init Success` with it set.

### The L1 cache covers nothing Windows uses — measured, with the fix value

Full write-up: `docs/cpu_cache_cmlr_2026_09_12.md`. Read off the CPU over COMrade:

`1000h:0 = 92` (CE **enabled**), `1001h:0/1 = FF/03` (LMCR = low 640 KB), **`1001h:4` (CMLR) =
`00`**, ECMLR `00`, `1002h:3 = 03` (2:1 clock live).

Cache on; nothing between 1 MB and 16 MB cacheable; Windows lives entirely above 1 MB. **The
owner's clock double and red-ray's "cache is off" measurements are both correct.**

Reference value from REVTO486's own dump on the 3.11 machine: **CMLR = `F0`**. One line to add to
`CPUSET.BAT`, before the cache is switched on:

```
CTCHIP34.EXE IBM486 /1001h:4=&11110000
```

**NOT YET APPLIED.** Verify with `CPUSHOW.BAT` (`1001h:4` should read `F0`), then benchmark —
the size of the win is unmeasured. Reversible: registers reset on power cycle.

### A working emulation bed exists — `vm_ls120win/`

```
86box_upstream/build/src/86Box.exe   (NOT 86box_full - see technique 117)
vm_ls120win/86box.cfg.master          proven vm_xtide_mpd config + lpt_epat + rdisk type 4
vm_ls120win/nvr/mach8.nvr             128 bytes, REQUIRED - without it nothing boots
vm_ls120win/ls120win_clean.img        the working master: network removed, clean shutdown
vm_ls120win/ls120win_master.img       byte-exact copy of the CF, never booted
vm_ls120win/rd.img                    120 MB SuperDisk medium (963*256 sectors)
```

Restore `ls120win.img` from `ls120win_clean.img` before every run. Clean shutdown verified —
7 `Terminate` stages, 7 `EndTerminate`, none unpaired.

---

## What cost time today, so it does not again

Eight boots went into bisecting disk images, the LS-120 driver, the EPAT bridge and the ATI
display VxD against a `Windows protection error`. **None was the cause.** Written up as
technique 117. In short:

1. **A fresh bed has no `nvr/`**, so the Mach8 gets blank NVRAM and Win95 dies in display init —
   on *every* image, including known-good ones. Copy `nvr/mach8.nvr` from a configured bed.
2. **The wrong emulator build.** The previous handoff said `86box_upstream` cannot boot Win95 and
   the fixes live in `86box_full`. **The opposite is true**, and the evidence was already on disk:
   86Box writes its own exe path into its logfile header, and `vm_xtide_mpd/stdout_wordxfer_stride2.txt`
   from a working run names `86box_upstream`. One `head -12` would have replaced eight boots.
3. **The control was run fourth, not first.** A new bed invalidates every control you think you
   have. Until it has booted something known-good, its negatives are void.
4. **`BOOTLOG.TXT` cannot locate this crash.** Three failing runs produced logs of byte-identical
   length (8361) ending on the same line — a flush boundary, not a death point. An early reading
   of "it died at `ati.vxd`" was unsound.

Consolation: by the time the bed was fixed, the LS-120 driver, the EPAT bridge, the ATI VxD, the
SCSI chain and four disk images were all cleared **by measurement**, so none needs revisiting.

## Corrections to standing claims

- **CTCHIP's writes DO reach the CPU.** The previous handoff suspected they did not, because
  `22h`/`23h` alias onto the 8259 (technique 75). Every value read back correctly in a separate,
  write-free invocation. Implication: the BL3 claims those I/O cycles internally rather than
  driving them onto the bus. **Technique 75 applies to devices probing chipset ports, not to the
  CPU's own configuration registers.**
- **CTCHIP's post-write display is not a read-back.** Proven by running the same batch under
  86Box, where those indices are unimplemented (`cpu_read()` returns `0xFF`, writes dropped) and
  CTCHIP still prints `92 / CE: Internal Cache: enabled`. Use `CPUSHOW.BAT` — it takes no write
  argument, so its display is a genuine read.
- **The cache half of #9 was never finished.** `CPUSET.BAT`'s own comments say so:
  *"NOT replicated on purpose: 1001h bytes 2/3/4 … read those off CPUSHOW.BAT on the 3.11 machine"*.
  The issue was closed on the clock result. The capture has now been taken.

## Open / next

1. **Apply CMLR = `F0`** on the Win95 side, verify with `CPUSHOW.BAT`, benchmark. Owner runs it.
2. **LS-120 spin-up timing on the bench** — the one thing the bed cannot measure. `pf.c` allows
   8 s; we allow 8000 ticks, and 1520 log units says the budget is being spent and lost.
3. **Unopened instrument:** the DDK ships `DEBUG/SCSIPORT.PDR` + `SCSIPORT.SYM` — a debug SCSIPORT
   with symbols. If a driver question needs SCSIPORT's own reasoning, that is the tool, and the
   bed is where to risk it.
4. Writes are still untested everywhere — `PHASE_DATA_OUT` and `epat_pio_request(out=1)` have
   never executed on either side.
5. Replies still owed: @TC1995 (#8, two comments), @andrew-hoffman (#23), disruptor (#26).

## Machine state

CF was imaged to `~/OneDrive/Desktop/win95_postsiv.img` (2,038,063,104 bytes) and is the basis of
the bed. The 5160 was last booted to the DOS/3.11 image with COMrade on COM1; `C:\CTCHIP\` was
created there and holds `IBM486.CFG` only (a partial `CTCHIP34.EXE` push was deleted — a 61 KB
serial transfer exceeds COMrade's 8 s op-timeout and the retry truncated the file).

---

# LATER THE SAME DAY — 2026-09-12, second half

Supersedes parts of the above. Read this section first.

## ✅ THE CACHE IS FIXED AND VERIFIED ON HARDWARE

`CMLR` (`1001h:4`) read **`00`** — cache enabled, but **nothing between 1 MB and 16 MB cacheable**,
and Windows 95 lives entirely above 1 MB. It was running completely uncached.

Fixed to **`F0`** (REVTO486's value on the known-good 3.11 machine), added to `CPUSET.BAT`
(backup `CPUSET.BAK`), **verified by read-back**: `1001h:4 = F0`.

⏳ **BENCHMARK NOT TAKEN.** Owed to red-ray, and required before any further CPU tuning or nothing
is interpretable. Full write-up: `docs/cpu_cache_cmlr_2026_09_12.md`.

**Every BL3 register that exists now matches revto486.** Remaining tuning levers (known-good is not
optimal) are in `docs/bus_optimisation_plan.md` under **CPU LEVERS**; only **XTOUT** has reported
evidence behind it.

**Three corrections:** CTCHIP's writes *do* reach the CPU (the 22h/23h aliasing suspicion is
wrong — technique 75 applies to devices, not the CPU's own registers); CTCHIP's post-write display
is *not* a read-back (use `CPUSHOW.BAT`); and **there is no further register gap** — `1002h`/
`1004h` bytes 4/5 are undefined reserved bits, proven by a write test that left them `00`.

## ✅ A WORKING LS-120 BED EXISTS — and the driver is CORRECT

| driver | real 5160 | bed |
|---|---|---|
| `xtidemp.mpd` | 23 | 21 |
| `t130.mpd` | 93 | 73 |
| **`ls120mp.mpd`** | **1520 — `Init Failure`** | **6 — `Init Success`** |

325 EPAT protocol lines, full ATAPI sequence, Windows to a desktop. **The restructure is sound;
the hardware failure is drive timing.**

**Measured on hardware:** after a command the real drive returns status **`80` (BSY)** with a clean
error register, still set after ~0.5 s, settled to `50` by the next command. **The emulated bridge
returns `40` and never goes BSY.** That is the whole divergence.

### The bed — `vm_ls120win/`

```
86box_upstream/build/src/86Box.exe     <- NOT 86box_full
vm_ls120win/nvr/mach8.nvr              <- 128 bytes, REQUIRED or NOTHING boots
vm_ls120win/ls120win_clean.img         <- working master (network removed, clean shutdown)
vm_ls120win/ls120win_master.img        <- byte-exact CF copy, never booted
vm_ls120win/rd.img                     <- 120 MB FAT16 superfloppy
```

Restore BOTH the image and `86box.cfg` from their masters every run — 86Box normalises
`rdisk_01_image_path` away into `image_history`, and then no medium is loaded.

⚠ **The earlier claim in this file that `86box_upstream` cannot boot Win95 is WRONG.** Every
successful Win95 run in this project used it. Eight boots were lost to that plus the missing NVRAM.
Written up as **technique 117**.

### Emulator change, built but NOT yet confirmed

`lpt_epat.c` gained a **`busy_ms`** option (0 = old instant behaviour, "750 ms (measured)") holding
BSY after a command via a `pc_timer_t`, following `lpt_ditto.c`. Purpose: make the bed reproduce the
hardware failure. Bed config carries `busy_ms = 750`. **The run confirming it was still in flight
when the session ended — check `grep "drive busy" vm_ls120win/86box.log` first.**

## Open, in order

1. **Benchmark the cache change.** Owed to red-ray; gates all CPU tuning.
2. **Confirm `busy_ms` reproduces `Init Failure` in the bed.** If it does, the LS-120 is fixable
   entirely in emulation.
3. **The x10 ratio test** on the driver's tick budgets — scales = real timeout, doesn't move =
   the timer is not advancing during `ScsiPortInitialize` and the fix is architectural.
   ⚠ **Two ConfigInfo hypotheses are already dead** (`NumberOfPhysicalBreaks`,
   `BufferAccessScsiPortControlled`) — do not retry them.
4. Writes still unproven everywhere.
5. @andrew-hoffman: the ECP port claims **DMA channel 3**, so the one DMA-capable storage device
   here is the LS-120. Gated behind PIO working; the vendor's DMA path writes `0x22`/`0x23`.

## Machine state

CF is in the 5160. `CPUSET.BAT` patched and verified. `C:\CTCHIP\IBM486X.CFG` added (an extended
probe config; harmless). COMrade DOS build was on COM1. `INQ9B.OUT`/`RDST2.OUT` are fresh probe
captures on `C:`.

---

# 2026-09-13 — THE #1 ITEM IS ANSWERED, AND IT RETIRES THIS DOC'S PREMISE

Read off the CF in the host reader; no machine time, no boot.

## `Initing ls120mp.mpd` is there — and so is `Init Success`, in all three logs

| log | timestamp | init took | result |
|---|---|---|---|
| `BOOTLOG.TXT` 2026-09-12 17:41 | `00115D79` | **6 units** | ✅ `Init Success` |
| `BOOTLOG.PRV` 2026-09-12 16:27 | `000F7943` | 419 units | ✅ `Init Success` |
| `BOOTLOG.OLD` 2026-09-09 14:04 | `000D546F` | 2 units | ✅ `Init Success` |

The only `LoadFailed` lines are `ndis2sup`, `ebios`, `vshare`, `EBIOS` — all stock Win95
noise, unrelated.

**Provenance of the binary that ran** (technique 74): `/d/WINDOWS/SYSTEM/IOSUBSYS/LS120MP.MPD`
is md5 `976e4114d215…`, **byte-identical** to `drivers/imation_ls120_mpd/build/LS120MP.MPD`.
The restructured driver is what executed. Backup `LS120MP.MP0` is the older `61fcab5d…`.

## ⛔ RETRACTION — "Init Failure in 1520 on the 5160" is dead

The 2026-09-12 framing was: *`Init Success` in 6 units in the bed vs `Init Failure` in 1520 on
the 5160, therefore the bed is faithful and the hardware is the outlier.*

**The hardware does not fail, and its best boot took 6 units — the bed's exact number.** The bed
and the hardware now agree at init. Anything reasoned from "hardware fails where emulation
succeeds" must be re-derived, including the motivation for last session's `busy_ms` latency model
(which is still a fidelity improvement, just not the explanation for an init failure).

⚠ Note the spread: 2, 6 and 419 units across three boots of the same binary. **Init duration is
not stable**, and 419 vs 6 is 70x. That variability is itself unexplained and is probably the
more interesting signal now.

## Two open items from memory were already closed in code — check before re-proposing

| item | memory says | `src/LS120MP.ASM` actually says |
|---|---|---|
| spin-up timeout | *"we allow ~1s, `pf.c` allows 8"* | `LS_TICKS_READY equ 8000 ; 8 s, pf.c's PF_SPIN` — **already 8 s** |
| retries | — | `LS_MAX_RETRY equ 5 ; pf.c's PF_MAX_RETRIES` — matches |
| async completion | proposed this session as a new lever (B1a) | `RequestTimerCall` already used at `LS120MP.ASM:711` and `:852`. **This driver already does it**; `XTIDEMP.MPD` does not |

## Where the failure actually is now

Init succeeds. The owner reports the drive **visible but not accessible** on the last boot. So
the fault has moved from load/init to the **data path**, which is where `LS_DrainSense` and the
media/sense handling live.

**Blocked on one fact, and it is the owner's to give:** what *exactly* happened on access —
the literal error text, and whether the drive had a letter in My Computer or was only present in
Device Manager. Those point at different causes (sense/media handling vs the known
`UserDriveLetterAssignment="II"` collision with the Nakamichi on `I:`) and guessing between them
costs a boot.

## ⚠ THE OWNER'S LEAD: did the cache change alter this? — boot got LONGER

His question, mid-session: *"perhaps the cpu caching altered behaviour?"* Worth taking
seriously, because `CPUSET.BAT` is dated **2026-09-12 14:40**, which splits the three logs.

| log | when | CMLR fix | LS-120 init | **total boot span** |
|---|---|---|---|---|
| `BOOTLOG.OLD` | 09-09 14:04 | ❌ off | 2 units | **873,495** |
| `BOOTLOG.PRV` | 09-12 16:27 | ✅ on | 419 units | **1,014,346** |
| `BOOTLOG.TXT` | 09-12 17:41 | ✅ on | 6 units | **1,137,837** |

**Boot got ~30% longer after the cache was fixed, and it is monotonic.** If the units are ms
that is 14.6 -> 16.9 -> 19.0 minutes. The unit is unverified; the *ratios* are what matter.

### This is coherent with the SIV regression, which is why it is worth chasing

`cpu_cache_cmlr_2026_09_12.md` measured working sets **above the 16 KB L1 running ~2.3x slower**
after the fix, probably line fills. A Windows 95 boot is the archetypal large, scattered,
low-locality workload — registry, VxD loading, INF parsing. Dhrystone (L1-resident) got 6.5x
faster; boot may be paying the other side of exactly that trade.

That would also explain the owner's report honestly: **the desktop genuinely feels faster**
(small hot loops, redraw) **while boot got slower**. Both can be true.

### ⛔ But do NOT conclude it yet — two confounds and a noise floor

1. **E1 lands in the middle.** SW1-3/4 were changed 2026-09-11, moving 384 KB of conventional
   RAM off the planar onto the Inboard. `BOOTLOG.OLD` predates that; both others follow it. So
   `OLD -> PRV` crosses **two** changes, not one.
2. **`PRV -> TXT` is 12% apart with no known change between them.** That is the run-to-run noise
   floor, and the headline 30% is only ~2.5x it. n=1 pre-fix.
3. The 09-09 boot also had the conflicting DOS driver still present.

### The cheap single-variable test — one boot, one edit

Comment out **only** the CMLR line in `C:\CTCHIP\CPUSET.BAT`:

```
REM CTCHIP34.EXE IBM486 /1001h:4=&11110000
```

Boot, keep the log, restore the line, boot again. Two boots, nothing else touched, and it
separates the cache from E1 and from noise. **Cheapest test that can answer it** — no build, no
driver work, and it settles whether the project's own cache fix has a cost nobody costed.

⚠ If it confirms, the fix is **not** simply reverted — Dhrystone nearly halved and the desktop is
better. It becomes a question of which CMLR mask is right, and C2 (`LMROR`, the 1 MB read-only
mask) in the CPU-levers table is suddenly relevant rather than exotic.

### What it does NOT explain: the LS-120

Our driver has **no software delay loops** — no `loop`, no `jmp $+2`, no cycle-counted waits.
Every wait is a 1000 us `RequestTimerCall` tick, so it is wall-clock and CPU-speed immune. And
on this machine consecutive I/O accesses are already ~3.90 us apart because the **Inboard
synchronises to the 4.77 MHz bus** — a cost the cache cannot touch. So the EPAT bit-bang
handshake timing is cache-immune by construction.

**Init duration 2 / 419 / 6 is therefore not a CPU-speed story**, and the 70x spread between two
boots with identical config and identical binary remains the unexplained signal.
