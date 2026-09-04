# Next session — 2026-09-04

> # ⚠ RETRACTED 2026-09-04, 11:40 — DO NOT ACT ON THIS DOCUMENT
>
> **The shutdown hang does not reproduce with any driver built from committed source.**
> Four shutdowns, three binaries, all clean: `71a2620c` (clamp + counters), `a0032294`
> (counters only) and `996f25b7` (neither). The binary that hung — md5 `70298a8f`, 3/3 —
> **cannot be rebuilt from any commit.** It was produced from an uncommitted working tree
> on 2026-09-03, deployed straight into the emulator image, and that source state is lost.
>
> So the hang was real, and it is now **unattributable and unbisectable**. Everything below
> about `Set_Global_Time_Out`, the polling contract, and the restructure is diagnosis of a
> defect in an artefact nobody can reproduce. **Do not spend a session restructuring the
> driver on the strength of it.**
>
> Checked and eliminated before concluding this:
> - the toolchain is deterministic (same source twice → same md5);
> - it was not a line-ending artefact (LF and CRLF builds are byte-identical);
> - it was not a bisect build (`-Release` refuses `-NoVolume`/`-NoCalldown`/`-NoDcb`/`-NoIo`,
>   and `70298a8f` is 20,619 bytes = the release size; bisect builds are 20,675);
> - `3b85848` + `-Release` gives the right size but `4e2dd46c`, not `70298a8f`.
>
> **The real 5160 was never running it.** The CF at `D:` holds `0fe2431a` — 2026-09-01,
> `dist/xtide_pdr/PORT_claim_master_stride2_rw.pdr`, tracked and reproducible. Nothing lost
> there, and its shutdown has never been tested. That is the test worth doing.
>
> Provenance is now enforced: `build.ps1` stamps the commit, shouts on a dirty tree, and
> appends md5 → commit/flags to `drivers/xtide_pdr/build_ledger.tsv`.
>
> What still stands from below: the measurements. The transport is exonerated (3,582 status
> polls against 400000 for one timeout; the last command transferred all 8 sectors), the
> freeze was a VMM-level wedge, and **`AEP_SYSTEM_SHUTDOWN` never reaches our handler** — so
> `XTIDE_VolDown` has never run. That last one is a real, standing bug worth its own look.

Written at the end of 2026-09-03. Everything below is committed and pushed; nothing depends on
that session still being alive.

---

## Start here

**The XT-IDE port driver's shutdown hang has a cause, and it is a restructure, not a patch.**

A Windows 9x port driver that polls its hardware — which ours must, because the Lo-tech XT-CF is
jumpered without an interrupt — is required to **start the I/O, arm `Set_Global_Time_Out`, and
return without completing**, then finish the request from its timeout handler. We block inline
instead. Microsoft's I/O Supervisor Guide states it directly; see technique 88 in the
`inboard-hw-debug` skill for the quote and the six-step contract.

**It is not proven.** It is a documented requirement we demonstrably do not meet, in exactly the
code five eliminations isolated. Two earlier fixes this session were equally confident and both
were negative, which is the reason for the hedge.

### The one-line summary of the evidence

| build | claims unit | fills DCB | inserts calldown | publishes volume | shutdown |
|---|---|---|---|---|---|
| driver renamed out | - | - | - | - | **clean** |
| `-ClaimMask 0` | no | no | no | no | **clean** |
| `-NoCalldown` | yes | yes | **no** | no | **clean** |
| `-NoVolume` | yes | yes | **yes** | no | **HANGS** |
| `-NoVolume -NoIo` | yes | yes | yes | no | **clean** |

Claim, DCB fields, published volume and the entire completion path are innocent. It is the
transport body, reached through the calldown.

---

## The job, in order

### 1. Restructure the request path to be non-blocking

This is the session's work and it is not small:

- `XTIDE_StartRequest` starts the command and **returns** without completing;
- transport state comes out of the globals it currently reuses (`XTIDE_ReqLba`, `ReqCount`,
  `ReqBuf`, `SgCur`, `SgLeft`) and into per-request state — the marker bug this session proved
  those globals are already shared unsafely;
- a timeout handler polls status, re-arms while busy, and on completion **CALLs** (never JMPs)
  `IOP_callback_ptr`, then `ILB_dequeue_iop`;
- the DDK sample in `BLOCK/SAMPLES/PORT` "demonstrates enqueueing and dequeueing of IOPs" per
  the Guide — read that before writing.

**Reference implementation:** `zikolas/cfu1-win9x` (checked out at
`C:\Users\lycet\RiderProjects\cfu1-win9x`) polls as well. It has twice found bugs that reading
the DDK sample never would. Diff against it before designing.

**Test:** boot `vm_xtide_inboard`, shut down by hand, expect *"It's now safe to turn off your
computer"*. The hang reproduces in the emulator now, so **this costs no real-hardware boots**.

### 2. Then the rest of the release blockers

`docs/xtide_pdr_cleanup_before_release.md` is current and honest. Items 3, 4, 5, 6 and 11 are
done and build-verified. Remaining: 2 (C: takeover), 7 (`IORF_LOGICAL_START_SECTOR` unexercised),
8 (stride autodetect), 9 and 10 (environmental).

### 3. Cheap wins that are now unblocked, one run each

- **Item 10** — REM `SD120PPD.SYS` / `ASPIHDRM.SYS` out of `CONFIG.SYS` and see whether IOS goes
  fully clean. `tools/rem_config_line.py` does it safely (CRLF-forced, refuses a no-op).
- **The boot-order test** — restore `MODISK2.SYS` at the *end* of `CONFIG.SYS` and see whether
  the `Unsafe`/`Monolithic` flag is about presence or order. Written up at the foot of the
  cleanup doc.
- **Issue #18 is live again.** `HSFLOP.PDR` now reaches `Init Success` on this bed — technique
  74 recorded it never loading, which is why the floppy `maxPhys` patch was inert. Clearing the
  real-mode chain for the port driver's sake fixed the precondition for a different issue.

---

## Read this before touching the bed

- **The bed is `vm_xtide_inboard/`** — the real card's own image on the real machine profile. It
  currently holds the release build `70298a8f…` (both teardown fixes in, no diagnostics).
- **`mouse_type = msserial` is load-bearing.** With `none`, Windows puts up a modal at shell
  start, Explorer never finishes, and **nothing in the StartUp folder ever runs**. Every
  shell-dependent test on this bed was silently impossible before 2026-09-03. Technique 87.
- **The network modal still appears** (issue #20 — 86Box has no 3C509B). Disable the adapter in
  the guest's hardware profile if it becomes annoying; do not *remove* the node, the real machine
  has a working card.
- **Never deploy into the image while 86Box is running.** Check `Get-Process 86Box` first.
- **Stopping the harness task kills the VM with it** — `Start-Process` without `-Wait` is not
  detached here.

## Driving the guest — what works and what does not

- **Host-side plain keys work** (Enter, letters) while the VM has focus.
- **Host-side chords do NOT reach the guest.** `Alt+F4` closes 86Box itself; `Ctrl+Esc` opens the
  *host's* Start menu. This cost a whole evening being misread as "keystroke injection doesn't
  work".
- **A batch in the StartUp folder cannot shut Windows down** — Win95 refuses while an MS-DOS box
  is open, so the batch triggering the shutdown is the thing blocking it.
- **`WIN.INI [windows] run=`** launches a *Windows* program with no DOS box. `USER.EXE` is
  16-bit, so it needs `RUNDLL.EXE`, not `RUNDLL32.EXE`. Left cleared in the image.
- **If the owner is at the machine, just ask them to click.** Two clicks costs them seconds and
  the budget nothing; automating it cost most of an evening. Standing preference, in memory.

---

## Owed to people

- **@andrew-hoffman** — replied 2026-09-03 on #21 with the bisect table and both dead ends. His
  I/O Supervisor Guide link is what produced the cause; the local copy is at
  `C:\IOSGuide\IOS_Guide.doc`. His third link, `sdz-mods/O2-OZ777_W98`, is a second working
  miniport in C and has **not** been read yet.
- **@zikolas** — already credited; his driver was load-bearing again this session (`CFU1_VolDown`).
- Ledger: `docs/contributor_input_ledger.md`, current.

## Update 2026-09-04: the contract is now measured, and the fix is smaller than described

The cause above was read out of a document. It has since been **confirmed from the binaries**,
with no machine time. Every Microsoft IOS port driver on the CF arms a timeout; ours arms
nothing at all:

| driver | timeout / event services |
|---|---|
| `ESDI_506.PDR` | `Set_Global_Time_Out` x1, `Set_Async_Time_Out` x2 |
| `SCSIPORT.PDR` | `Set_Global_Time_Out` x1, `Cancel_Time_Out` x3, `Set_Async_Time_Out` x1, `Schedule_Global_Event` x3 |
| `HSFLOP.PDR` | `Set_Global_Time_Out` x5, `Cancel_Time_Out` x1, `Create_Semaphore` x2, `Wait_Semaphore` x4, `Signal_Semaphore_No_Switch` x1 |
| **our `PORT.PDR`** | **nothing** |

Reproduce with `python3 tools/pdr_vxd_services.py --only time_out,semaphore roms/xtcf_card/*_reference.PDR drivers/xtide_pdr/build/PORT.pdr`.
References copied to `roms/xtcf_card/{ESDI_506,SCSIPORT}_reference.PDR` (`HSFLOP` was already
there). Technique 88 carries the method.

**This changes the plan.** The handoff said the fix was a restructure into a state machine driven
from a timeout handler. `HSFLOP.PDR` shows a cheaper legal shape, and it is the right model for
us because floppy is the only one of the three that must survive a device that may not answer:

- `0x1853`/`0x1865` `Create_Semaphore` at init;
- `0x2D95` `Set_Global_Time_Out` then `0x2DBE` `Wait_Semaphore`, 43 bytes apart;
- one `Signal_Semaphore_No_Switch` — the timeout handler frees the waiter.

So **blocking is permitted, provided something asynchronous can break the block**. Try that
before the restructure: keep the inline wait, but wait on a semaphore with an armed
`Set_Global_Time_Out` instead of spinning on `in al,dx`. `0x2DE0` (`Set_Global_Time_Out`, then
`pop esi; jmp [edi+0Ch]`) shows the full return-without-completing form if the cheap shape fails.

⚠ Those offsets come from a byte scan, **not a disassembler**. Confirm before building on them.
And `ESDI_506.PDR` is the weaker reference despite being the obvious one — IDE has IRQ 14, so it
completes from an interrupt, not from a timeout handler.

## Update 2026-09-04, second pass: is it even a hang?

Reading the transport turned up something the handoff had assumed away.

**The poll loops are already bounded.** `XT_SPIN = 400000` with `dec ecx / jnz`, in both
`XTIDE_WaitNotBusy` (`XTIDETR.ASM:582`) and `XTIDE_WaitDrq` (`:606`); each returns `CF=1` on
expiry, and the driver does not retry — one shot per IOP, then `IORS_DEVICE_ERROR`. **The driver
cannot spin forever.** At roughly 0.6 s per expired wait, several waits per sector and a whole
cache flush queued at shutdown, "hangs" and "takes eleven minutes" look identical from a chair.
Nobody has recorded how long the machine was left.

`XT_SPIN` is also explicitly uncalibrated — *"status poll limit; no timer at init"*. Its sibling
`XT_TIMEBASE` carries a written account of this exact bug biting once already: a constant
calibrated on 86Box's Deskpro that was meaningless on the 5160.

### Do this first — it is free and decisive

**Watch 86Box's XT-IDE access log during the freeze.** The emulator logs every access with the
guest EIP, and the hang reproduces in the emulator, so this costs no 5160 boots and no build:

| what the log shows during the freeze | what it means | fix |
|---|---|---|
| accesses still arriving, from the `WaitNotBusy` / `WaitDrq` EIPs | timeout storm, not a wedge | shorten the shutdown timeout; the contract fix then stops it freezing the UI |
| accesses arriving, request completing, another starting | it finishes eventually — leave it running and time it | as above |
| nothing at all | genuinely wedged, and **not** in the spin | the contract fix, and re-open where |

### Then: enforce a shutdown timeout — the safe version of "just stop"

`XTIDE_VolDown` already runs on `AEP_SYSTEM_SHUTDOWN`. Have it set a flag, and have both wait
loops load a much smaller count when the flag is set (`XT_SPIN_SHUT`, ~4000 = ~6 ms) instead of
400000. Six lines in each loop.

**This cannot lose data, and that is the point.** When the drive answers, a wait completes in
microseconds and never reaches either limit; the short count only bites where the long one would
also have failed, 100x slower. It changes *how long we wait before failing*, not *whether we
fail* — unlike completing requests with fake success, which would silently drop the final cache
flush and corrupt the CF.

If shutdown then completes: the freeze was the spin, and we have both the diagnosis and a
stopgap. If it still freezes: either requests arrive before the shutdown AEP, or the freeze is
not in the spin — and that is worth knowing before writing the semaphore restructure.

⚠ Run it in the emulator on a **copy** of the image. Do not put a first-cut shutdown path on the
CF; and note that powering off a genuinely hung shutdown is itself destructive on the real
machine — Win95 freezes before it has flushed and said "safe to turn off".

### Already answered, do not re-run

"Can we just not do I/O at shutdown?" — the bisect above already ran the stronger form: **stubbed
request handler, shutdown clean**. Not doing I/O is known to fix it. A shutdown-only stub would
add exactly one fact — whether requests arrive *after* `AEP_SYSTEM_SHUTDOWN` — and the timeout
experiment above yields that fact without risking a write.

### `Port_iop_timeout` is an empty stub

`PORTAER.ASM` dispatches `AEP_IOP_TIMEOUT` to a routine that returns the preset `AEP_SUCCESS`.
That tells IOS we own the timed-out IOP and have dealt with it, having done nothing. A second
missed contract, independent of the polling one. It only matters once we stop blocking — while
we block, IOS cannot run its timeout either.

### What real mode still has to teach

The XT-IDE Universal BIOS polls too, and shuts down fine — because in real mode nothing else
needs the CPU. Two things follow:

- **It is evidence the transport is right and the fault is structural**, consistent with the
  bisect. The same register and latch sequence works from DOS, verified: IDENTIFY returns
  `TRANSCEND`.
- **XUB times against the BIOS tick at `0040:006C`, not a naked loop count.** That is the
  calibration `XT_SPIN` does not have, and XUB's source states real numbers for how long an
  XT-CF may legitimately stay busy. Worth reading for that alone.
- It is also a **control**: if the drive genuinely stops answering at shutdown, a real-mode
  access at the same moment would see it too.

## Still unread

- The **retrocomputing.SE answer** on how IOS matches drivers to BIOS disks — bears on blocker 2,
  and neither it nor the Guide is fetchable from the dev environment.
- The **I/O Supervisor Guide** beyond the polling and calldown sections. It also documents the
  IOS data area and the `.IDUMP` / `.IDCB` / `.ILDCB` debugger commands, which may be worth more
  than any instrumentation we would write ourselves.
