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
