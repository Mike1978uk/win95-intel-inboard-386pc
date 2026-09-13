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

## EVIDENCE CLASS — the owner's rule, and it demotes two of my numbers

> *"we also have the timings taken via COMrade so we build that in — real hardware is king on
> evidential status"*

Correct, and it reorders this whole section. Ranked:

| rank | instrument | what it is | used above |
|---|---|---|---|
| 1 | **COMrade + PIT channel 0** | our own code, our own timer, on the silicon. `RAMTIME2.OUT` latches PIT ch 0 either side of a `rep lodsw` | ❌ **not used — and it should have been** |
| 2 | SIV latency walk | third-party tool, no TSC, no CPUID, inferring from a timing loop | the Dhrystone and walk tables |
| 3 | **BOOTLOG span** | a side effect of an unrelated log, unknown unit, no control over the workload | **the 30% boot-slowdown claim** |

So the boot-span finding is the **weakest** evidence in this document and should be read as a
prompt to measure, not as a result. The SIV numbers sit above it, and both sit below a COMrade
run that has not been done.

## ⛔ BUT THE EXISTING COMrade SCRIPTS CANNOT SEE THIS CHANGE — check before running them

`RAMTIME2` and the parked `RMTMST` probe segments **`1000`, `8000`, `B800`, `F000`** — every one
of them real-mode, so **all below 1 MB**.

| register | set by `CPUSET.BAT` | covers | changed 2026-09-12? |
|---|---|---|---|
| `LMCR` `1001h:0/1` = `03FF` | since **2026-08-24** | low 640 KB | ❌ no |
| `CMLR` `1001h:4` = `F0` | **2026-09-12 14:40** | **1–16 MB** | ✅ **this is the fix** |

**The regions those scripts time were already cacheable before the fix and still are.** Re-running
them post-fix measures nothing about CMLR and would produce a confident null. `RMTMST.SCR` is
staged on the CF (`/d/RMTMST.SCR`, 2026-09-11 10:27) and is tempting precisely because it is ready
— **it is the wrong instrument for this question.**

### What a COMrade test of the cache fix actually needs

Time a **strided** read of a region **above 1 MB**, which real-mode `DEBUG` cannot address.
Options, cheapest first:

1. **The CMLR-off boot** (already specified above). No new code. Comments out one line, boots
   twice, and uses the same BOOTLOG instrument for both arms — which makes its weak evidence
   class much less of a problem, because the comparison is controlled.
2. **Unreal mode in a DEBUG script.** The Inboard is a 386: load a 4 GB-limit descriptor, drop
   back to real mode, keep the segment limit, then stride above 1 MB with the same PIT latch
   `RAMTIME2` already uses. This is the instrument that would settle it properly, and it reuses
   the timing harness verbatim — only the addressing changes.
3. Run the walk under Windows where the memory is already there, which is what SIV did — and we
   have already seen that answer.

**`gen_ramstride.py`'s own docstring has the right warning for this** and it applies to whichever
region is chosen: *"Pass 2 much faster than pass 1 means the stride is still smaller than the
cache line and the result is void."* The BL3 line is **16 bytes**, so the 32-byte stride it
already uses is correct.

## ⛔ CORRECTION — the driver DID change, so "only the CPU registers varied" is false

The owner asked directly whether the LS-120 driver changed between those boots. **It did, and I
should have said so when I first tabled the boot spans.**

| boot | date | driver on disk | CMLR | E1 switches |
|---|---|---|---|---|
| `BOOTLOG.OLD` | 09-09 14:04 | **old** `61fcab5d` | ❌ | ❌ |
| `BOOTLOG.PRV` | 09-12 16:27 | **new** `976e4114` | ✅ | ✅ |
| `BOOTLOG.TXT` | 09-12 17:41 | **new** `976e4114` | ✅ | ✅ |

`LS120MP.MPD` was deployed **2026-09-11 22:54:32**. So `OLD -> PRV` crosses **three** changes —
E1's switches, the driver restructure, and CMLR — not the two I listed. Every one of them lands
in the same gap, and none can be separated from the others by these logs.

**And the owner's own report contradicts the claim anyway:** *"boot didn't feel too bad on last
boot"*. That is the same class of evidence as "the desktop feels faster", which I accepted
without argument in the other direction.

**Net: the 30% boot-slowdown finding is withdrawn as a finding.** It survives only as a reason to
run the controlled CMLR-off test, which remains cheap. Do not carry the number forward.

⚠ The restructured driver does bias boot span slightly by design — it waits through up to
`LS_TICKS_READY` (8000) timer callbacks instead of blocking — but it inits at ~100% through the
boot and its measured init deltas (2 / 419 / 6 units) are three orders of magnitude smaller than
the 264,342-unit span difference. It is a confound, not the cause.

## THE HARDWARE SYMPTOM, as the owner describes it — this narrows it sharply

> *"nothing it just didn't read the drive"* … *"felt like the egg timer - i did eject the disk
> and it then complained about the drive not having a disk if i recall"*

Three facts, and together they localise the fault:

| observation | what it rules IN | what it rules OUT |
|---|---|---|
| No error dialog, just the hourglass | a **timeout**, not a refusal | a CHECK CONDITION that reached the OS; "media not formatted" |
| Ejecting produced *"no disk in drive"* | **the register path works** — the driver read the drive's status and reported media state correctly | a dead transport, a wrong port, a bridge that never connects |
| Init Success in 6 units | the bridge opens and the drive answers | the whole load/enumerate path |

**So the drive is talking. Registers are readable, media presence is correct, and only the
transfer hangs.** That is the profile of a failure confined to the **data phase**.

### Which fits the transport split exactly

`TRANSPORT_SPEC.md`: **registers are read in nibble mode; the data payload uses ECP `rep insb`.**
Two different mechanisms over the same cable. Everything reported working above is a register
read. The one thing reported broken is the only thing that uses the other mechanism.

**Hypothesis for the bench: the ECP data phase does not complete on the real bridge, so the
driver waits out `LS_TICKS_READY` (8 s) x `LS_MAX_RETRY` (5) and Explorer shows an hourglass for
~40 s.** The arithmetic matches the described symptom without needing anything else to be wrong.

⚠ **Not yet tested, and cheap to test wrongly.** The emulated bridge implements the data path
too, so if the bed reads the new pattern media successfully the difference is real and is in the
ECP handshake, not in our driver's logic. That is the comparison the bed now exists to make.

## Bed status 2026-09-13 — media is loaded and verifiable

`vm_ls120win/rd.img` was **formatted but empty**. A read of an empty volume returns zeros, and so
does a broken transfer, so it could not have failed. Now populated via `tools/fat16_put.py`:

| file | size | md5 | purpose |
|---|---|---|---|
| `HELLO.TXT` | 121 B | `02ccfe793c63` | one cluster, eyeballable in Notepad |
| `PATTERN.BIN` | 64 KB | `27425bfef648` | 32 clusters, offset-stamped every 16 bytes |
| `BIG.BIN` | 1 MB | `c2374fcb71c5` | 512 clusters — exercises sustained multi-sector transfer |

Every 16-byte block begins with its own file offset, so a misplaced read **reports where it
landed** instead of merely comparing unequal. Verified by walking the FAT chain off the image
independently of the writer. Empty original kept at `rd_empty.img.bak`.

Build check before the run (technique 74): `86Box.exe` 2026-09-12 15:25:03 is newer than
`src/device/lpt_epat.c` 15:24:39, tree clean, branch `lpt-epat-bridge`, head `92239ba`
*"Model the drive's busy time after a command"*.

## ⚠ "Are the latest CPU changes on the bed?" — NO, and they cannot be

The owner's question, 2026-09-13. Two separate reasons, either one sufficient.

**1. The bed image predates the fix.** `ls120win_master.img` was cut **2026-09-12 13:22**;
`CPUSET.BAT` gained the CMLR line at **14:40**. Verified rather than assumed — the string
`IBM486 /1001h:4` occurs **0** times in `ls120win.img` and **1** time in the CF's `CPUSET.BAT`.
(A naive grep for `1001h:4` *does* hit the image, but it lands in a documentation file at offset
`455645109`, not in a batch file. Match the command, not the register name.)

**2. Even if it were copied in, 86Box drops the writes.** Already proven in
`cpu_cache_cmlr_2026_09_12.md`: under 86Box `cpu_read()` returns `0xFF` for these indices and the
writes are discarded, while CTCHIP still cheerfully prints `92 / CE: Internal Cache: enabled`.
**86Box does not model the BL3 cache configuration registers at all.**

### What that means for the work in hand

- **The bed cannot test any cache hypothesis.** Do not try; the answer will be a confident null.
- It does *not* exonerate the cache for the LS-120 either — "the bed has no cache and works" and
  "hardware has cache and fails" is exactly the shape a cache problem would take. The bed simply
  cannot distinguish it.
- ⭐ **The decisive test is one boot on real hardware**: comment out the single CMLR line in
  `C:\CTCHIP\CPUSET.BAT`, boot, and try the drive. **One boot answers two open questions** — does
  boot time change, and does the LS-120 read. Single variable, no build, no deployment.

### And the provenance of "83.5 MHz" is now known

`vm_ls120win/86box.cfg` sets `cpu_speed = 83500000`, `cpu_multi = 3`. So the "actual 83.5 MHz"
figure in the forwarded analysis is **our own emulator configuration read back at us**, not a
measurement of the silicon. F1's verdict stands unchanged — the real part's clock has still never
been measured here — but the number was not invented, it was borrowed from this file.

## ⭐ ROOT CAUSE CANDIDATE — `LS_SPIN_BSY` is ~90 ms and the drive needs 500 ms+

Found by running the bed with media, 2026-09-13. **Evidence, not reasoning.**

The bed issued **five ATAPI commands and every one was INQUIRY** (`12 00 00 00 24 00`). Five is
exactly `LS_MAX_RETRY`. It never reached a single read.

Status polls counted between each command's data phase and its completion:

| attempt | nibble reads | status polls |
|---|---|---|
| 1 | 4116 | **2058** |
| 2 | 4116 | **2058** |
| 3 | 4120 | 2060 |
| 4 | 4118 | 2059 |
| 5 | 4000 | **2000** |

```
LS_SPIN_BSY   equ   2000        ; LS120TR.ASM:172 - comment claims "~90 ms"
```

`LS_WaitNotBusy` (`LS120TR.ASM:825`) spins that many times and gives up. **The constant is hit
dead on, five times.** The drive was measured on real hardware holding BSY **past 0.5 s**.

### It explains every hardware symptom, including the confusing ones

| symptom | explanation |
|---|---|
| hourglass, then nothing, **no error dialog** | five bounded spins time out; there is no device error to report, so no dialog |
| eject correctly says *"no disk in drive"* | media-presence checks are short register reads that finish well inside 90 ms |
| `Init Success` in 6 units | init never waits on a slow command |

### The fix is not to raise the constant

8 s of spinning would block the CPU and cost **2,000 nibble register reads per attempt** — by
`bus_optimisation_plan.md`'s own metric that is the worst possible shape. Last session's
restructure moved the **spin-up** wait onto the `RequestTimerCall` state machine but left the
**post-command BSY** wait as a bounded spin inside `LS_PacketCommand`, which runs inline from
`lt_issue`. Finishing that restructure is the fix: make the post-command BSY wait a timer state,
the way `LS_ST_READY` already is.

## ⛔ AND A FIDELITY BUG IN OUR OWN BRIDGE — the bed failed EARLIER than hardware

The owner drove the bed to the desktop: **the drive was not in My Computer at all.** On the real
5160 it *is* visible and gets a letter. So the bed was overstating the fault.

Cause: last session's `busy_ms` applied the drive latency at `PHASE_COMPLETE` for **every**
command, INQUIRY included. A real LS-120 answers INQUIRY, REQUEST SENSE and MODE SENSE out of
firmware; only commands that read, write or position the medium pay the spin-up. Delaying
INQUIRY stalls enumeration, which the real drive does not do.

Fixed in `lpt_epat.c` with `epat_cdb_touches_media()` — the latency now applies only to
READ/WRITE(6|10), READ CAPACITY, START STOP UNIT, VERIFY and SYNCHRONIZE CACHE.

**Expected after the fix:** the drive enumerates and gets a letter, exactly as on hardware, and
then hangs on the first read of `HELLO.TXT`. That is the hardware symptom reproduced in
emulation, which is what the bed is for.

⚠ Lesson for the model: *"hardware goes BSY after a command"* was measured with a probe script
that issued INQUIRY on a **spun-down** drive. Generalising it to every command was mine, and the
bed contradicted it within one boot.

## ⛔ THE LS-120 HAS NEVER READ ON HARDWARE — so nothing on 2026-09-12 broke it

Owner, 2026-09-13: *"my hardware has yet to display drive contents"*.

This retires a line of inquiry that ran for several exchanges. Earlier wording — *"the drive was
visible again but I couldn't access it"* — read as a regression. It is not one. **The drive has
never displayed contents on the real 5160 at any point.**

Consequences, all of them useful:

| hypothesis | status |
|---|---|
| The CMLR cache fix altered LS-120 behaviour | ❌ **dead.** The read did not work before 2026-09-12 either |
| The driver restructure (deployed 09-11 22:54) broke reads | ❌ dead, same reason |
| The E1 switch change affected it | ❌ dead, same reason |
| `LS_SPIN_BSY` is ~90 ms against a drive needing 500 ms+ | ✅ **consistent** — a constant that has been wrong since it was written produces a fault that has never worked, which is exactly what is reported |

**This is the stronger form of the evidence.** A defect that has never worked points at something
structural in our own code rather than at any of the three changes that landed this week, and the
spin constant is structural: it has been 2000 since the transport was written.

⚠ Note for the record: I spent several exchanges treating a never-worked fault as a regression,
on the strength of the word *"again"*. **Ask "did this ever work?" before building a timeline.**

## ⭐⭐ THE VENDOR'S OWN NUMBER — it waits 10 SECONDS, we wait 90 ms

Prompted by the owner, 2026-09-13: *"are you using the sources that we disassembled — the sys,
the vendor windows driver, the linux driver, the ddk examples — are we on top today?"*

**I was not, and he was right to ask.** The whole session to that point ran on our own source and
the bed. `SD120PPD.MPD` — the vendor driver that **works on this exact hardware** — was sitting
fully disassembled in `SD120PPD_MPD.asm` (22,503 lines) and unopened. I was about to pick a new
value for `LS_SPIN_BSY` by judgement, which is how the wrong one got there in the first place.

### The vendor's wait-for-ready loop, `SD120PPD_MPD.asm` rva `0x001d35`

```asm
0x001d3e  mov  edi, 0xf4240      ; 1,000,000 iterations
0x001d47  push esi
0x001d48  call 0x550e            ; read ATA status
0x001d50  test al, 0x80          ; BSY?
0x001d52  jne  0x1d58            ;   yes -> stall and retry
0x001d54  test al, 8             ; DRQ?
0x001d56  jne  0x1d74            ;   yes -> done
0x001d58  push 0xa               ; 10
0x001d5a  call 0x5201            ; ScsiPortStallExecution(10 us)
0x001d61  dec  edi
0x001d64  jne  0x1d47
0x001d66  mov  byte ptr [0x203e4], 2   ; timeout
```

`0x5201` thunks to the IAT entry for **`ScsiPortStallExecution`** (imports list, `0x01187c`).
Confirmed by its sibling at `0x520d`, which builds a millisecond delay by calling `0x5201` with
`0x3e8` = 1000 — so the argument is microseconds.

**1,000,000 x 10 us = 10 seconds.**

### Side by side

| | vendor `SD120PPD.MPD` | Linux `pf.c` | **ours** |
|---|---|---|---|
| ready timeout | **10 s** | 8 s (`PF_SPIN`) | **~90 ms** (`LS_SPIN_BSY` 2000) |
| between polls | **`ScsiPortStallExecution(10)`** | scheduler | **nothing — spins flat out** |

**Two independent working implementations agree on ~10 s. Ours is the outlier by a factor of
~110.** That is not a tuning difference, it is a wrong constant.

### The pacing is the second finding, and it is ours to have thought of

The vendor stalls 10 us between status reads. We spin. By `bus_optimisation_plan.md`'s own
metric a status read costs the same ~3.90 us of bus as a byte of real data, so an unpaced poll
loop is pure contention — the exact anti-pattern that document is built around, in our own code,
on the hot path. The vendor already does the right thing.

### What this changes about the fix

The earlier plan — *move the post-command BSY wait into the `RequestTimerCall` state machine* —
is still right and is now better supported: `LS_ST_READY` already polls once per 1 ms tick, which
is the vendor's shape with an even cheaper pacing. The correction is to route the **post-command**
wait there too, with a budget of **10 s**, not to raise a spin count.

⚠ Do not simply set `LS_SPIN_BSY = 220000`. Ten seconds of unpaced spinning is 220,000 nibble
register reads and a blocked CPU — worse than the bug.

---

# LATE 2026-09-13 — SPP PROVEN END TO END. ECP builds, selects, and stops after INQUIRY.

## ⭐ The LS-120 reads AND writes, verified from outside the guest

The owner opened the drive in the bed, listed it, opened `HELLO.TXT`, **edited it and saved**.
Checked host-side afterwards, which is the only proof that counts:

| file | before | after |
|---|---|---|
| `HELLO.TXT` | 121 bytes, md5 `02ccfe793c63` | **168 bytes, md5 `02b26be69183`** - his text appended, original intact |
| `PATTERN.BIN` | `27425bfef648` | **unchanged** |
| `BIG.BIN` | `c2374fcb71c5` | **unchanged** |

Both pattern files byte-identical is the half that could have failed: a broken transport would
have scrambled them. **Mount, directory, read, modify, write - all correct.**

Measured cost of the SPP/nibble transport over that session: **8.4 nibble half-reads per payload
byte**, 219,776 EPAT operations for 25,088 bytes. Slow, and correctly so.

## The three driver bugs that got it there

1. **`FINISH` state** - a BSY drive with no ERR means the command is still running, so wait it
   out on pf.c's 8 s budget instead of re-issuing. Re-issuing burned all five retries 60 ms into
   a 750 ms wait.
2. **The write path did not exist.** The ICR gate required data-IN, so every write sent its CDB
   and no payload. Now decodes all four ATAPI phases.
3. **Bridge model swallowed payload bytes equal to `0x22`** - the unlock recogniser ran on every
   data-port write and the early return ate the byte. Reads were immune; only a write pushes
   arbitrary payload that way.

## ECP: everything is in place except the last handshake

Built and committed, both transports selectable (`MODE=SPP` / `MODE=ECP`, or `-Mode` at build
time for testing without touching a live registry):

| piece | state |
|---|---|
| Inboard XT's LPT modelled as the ECP card it is (`lpt_set_ecp`) | ✅ `0x378`, IRQ 7, ECR at `0x77A` |
| `lpt_device_t.ecp_read_data` + `lpt_set_ecp_read_data()` | ✅ the framework's ECP FIFO was chardev-only |
| `LS_BlockReadEcp` / `LS_BlockWriteEcp`, SPP fallback | ✅ audited, zero fixed-port writes |
| driver selects ECP | ✅ **24 nibble half-reads against SPP's 195,064** |
| CDB stays SPP, payload uses ECP | ✅ |
| **the data phase itself** | ❌ **stops here** |

### Exactly where it stops

```
CDB 12 00 00 00 24 00 ...        <- INQUIRY, sent via SPP, correct
data phase in, 36 bytes           <- the drive has data
R reg 1A -> 02                    <- ireason 2 = data IN, correct
R reg 1C/1D -> 24 00              <- byte count 36, correct
DISCONNECT                        <- and nothing more, ever
```

Everything up to the transfer is right. The ECP read returns nothing useful, the driver gives up,
SCSIPORT sees no device and the drive does not appear.

### What has been checked and is NOT the cause

- `epat_data_read()` is self-contained - advances `pos`, calls `epat_pio_request()` - so it works
  for ECP as well as nibble.
- `epat_write_ctrl()` does store the direction bit in `dev->ctrl`, and `lpt_get_ctrl_raw()`'s
  `& 0xef` keeps bit 5. So `case 0x0400`'s `ctrl & 0x20` test should pass.
- The ECR write lands in range: `0x378 + 0x402 = 0x77A`, and `lpt.c` claims `port+0x400` for 3
  ports when `dev->ecp` is set.

### Next step, and it is cheap

**Log it rather than reason about it.** Add one `lpt_log` in `lpt.c` `case 0x0400` printing
`ecr`, `lpt_get_ctrl_raw()` and whether `dev->dt->ecp_read_data` was called. One boot answers
whether the read is reaching the device at all, which splits the remaining space in half. That
is the thing I should have done instead of a third reasoning pass.

## Two bugs of mine worth not repeating

- **A probe must not change state.** `LS_DetectEcp` left the ECR in byte mode; any ECR mode with
  bits 7:5 set makes the port bidirectional, and nibble status reads go through exactly that -
  so the probe broke the fallback it was probing for. Boot stalled with 12,588 nibble reads and
  no commands. `LS_EcpLeave` had the same flaw.
- **An absent setting must not clobber a default.** `LsParseMode` returns 0 for "not stated" and
  `LsFindAdapter` stored it unconditionally, zeroing the build-time pin - so a `-Mode ecp` build
  silently ran nibble. Caught only by measuring the transport rather than trusting the flag.

## Bed state

Restored to the **proven SPP build** (`38069bfd`, md5 `d9512bd4`). Media intact and carrying the
owner's edit. Emulator is current with the ECP work, which is inert unless a driver selects it.

## Open, unrelated, and the owner raised it twice

**The long pause after `smwclock` in `AUTOEXEC.BAT`, on real hardware as well as in the bed.**
`c:\clock\smwclock s` is the last line before `IVT68FIX.COM`, and the owner notes the following
step never appears to write to the screen. His suggestion - if something is waiting unanswered,
let it pass faster - is worth testing. Not touched tonight; it predates all of this work.

---

# END OF 2026-09-13 — SPP SHIPS. ECP READS. ECP WRITES CORRUPT THE BOOT SECTOR.

## ⛔ START HERE TOMORROW: ECP WRITES ARE NOT SAFE

The last ECP run left `rd.img`'s boot sector with its OEM field overwritten -
`!#k{]IHC` instead of `MSWIN4.1` - which is why the drive showed empty. Signature,
BPB, FAT, root directory and all file data were untouched, and both pattern files
verified byte-exact afterwards. **Repaired in place; media snapshotted as
`vm_ls120win/rd_verified_good.img`.**

**Do not run an ECP-pinned build against media you care about.** Something in the ECP
write path puts bytes at LBA 0. Reads are clean; only writes are implicated.

## What is PROVEN

**SPP end to end, on the owner's own hands.** He opened the drive, listed it, opened
`HELLO.TXT`, edited it and saved. Verified host-side: 121 -> **168 bytes**, his text
appended, original intact, and `PATTERN.BIN` / `BIG.BIN` **byte-identical** - the half
that could have failed. Bed now carries the proven SPP build (`38069bfd`, md5
`d9512bd4`).

**ECP reads, exactly.** Per-command byte counts from the emulated bridge:
INQUIRY 36, REQUEST SENSE 14, READ CAPACITY 8, READ(10) 512 each - every one correct.
READ CAPACITY returns last LBA 246,527 / 512-byte blocks = 126,222,336 bytes, matching
the medium byte for byte.

## Emulator work, all committed to `mike/lpt-epat-bridge`

| commit | what |
|---|---|
| `96a7bc7` | the Inboard XT's LPT is modelled as the ECP card it is - `0x378`, IRQ 7, ECR at `0x77A` |
| `85a2946` | `lpt_device_t.ecp_read_data` - the ECP FIFO was chardev-only, an emulated device could never supply bytes |
| `05630e2` | **`lpt_set_ecp()` must precede `lpt_port_setup()`** - the flag only matters at registration, and `port+0x400` is claimed only if it is already set |
| `20c76af` | spindle model - spin-up once, then ~50x quicker, instead of a flat delay on every command |
| `8ab2203` | `ecp_write_data` - the FIFO drain fed `write_data()`, whose EPAT path is framed for SPP blocks, so ECP payload was dropped |
| `8d529e4` | SuperDisk 120/240 types un-`#if 0`'d - the bed had no valid type for a 246,528-sector image |

⚠ **`[ECPDIAG]` tracing is still in `lpt.c`** (three `pclog` points). Remove once ECP is
settled.

## Driver work, committed

`FINISH` state; the write path (the ICR gate required data-IN, so writes sent a CDB and
no payload); phase decode for all four ATAPI phases; `MODE=SPP` / `MODE=ECP` in
`AdapterSettings` plus `-Mode` at build time; ECP transport with SPP fallback; the CDB
pinned to SPP; a FIFO drain before leaving ECP mode.

## The ECP write, precisely where it stands

```
CDB 2A ... / data phase out, 512 bytes / block write done
ireason 00 (data out), byte count 0200 - all correct
[ECPDIAG] ECR write 74 (mode 3)
[ECPDIAG] ECR write 15 (mode 0)     <- nothing in between
```

`rep outsb` is not reaching the FIFO. `lpt.c` `case 0x0400` mode 3 writes only when the
direction bit is CLEAR, which `LS_EcpEnter` does arrange for a send - so the next step is
**one diagnostic in `lpt_write_fifo()` / the write-side `case 0x0400`**, not another
reasoning pass. Tonight proved that twice: the "zero ECR writes" line found the
registration-order bug in a single boot after three failed arguments.

And the corruption says the bytes are not merely lost - some reach LBA 0. Worth checking
whether `LS_BlockWriteEcp` leaves `ESI`/`ECX` such that a later SPP write runs with the
wrong pointer.

## Measured, and it reframes the optimisation case

With the spindle model honest, **status polls per command fell 3,419 -> 1,968** - and
1,968 is `LS_SPIN_BSY` = 2,000. The drive answers in 15 ms and the driver burns a full
unpaced 2,000-iteration spin (~60 ms of nibble reads) waiting for it. **We are now slower
than the device.** Pacing the poll loop is worth more than the transport, exactly as the
constant sweep predicted.

## Open, untouched

- The pause after `smwclock`. **`IVT68FIX.COM` is exonerated**: 20 bytes, four memory
  writes and `INT 20h`, no loop and no port access, and it prints nothing by design. The
  gap is either `smwclock` itself or `WIN.COM` starting. Needs a timestamped capture.
