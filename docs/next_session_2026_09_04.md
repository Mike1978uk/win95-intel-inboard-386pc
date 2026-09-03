# Next session — 2026-09-04

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

## Still unread

- The **retrocomputing.SE answer** on how IOS matches drivers to BIOS disks — bears on blocker 2,
  and neither it nor the Guide is fetchable from the dev environment.
- The **I/O Supervisor Guide** beyond the polling and calldown sections. It also documents the
  IOS data area and the `.IDUMP` / `.IDCB` / `.ILDCB` debugger commands, which may be worth more
  than any instrumentation we would write ourselves.
