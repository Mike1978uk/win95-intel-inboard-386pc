# XT-IDE port driver — what must be cleaned up before this ships

Written 2026-09-01, the night Windows 95 first read and wrote the real XT-CF through this
driver. The milestone is real; the driver is **not releasable**. This is the list, ordered by
what would hurt someone else's machine first.

Nothing here is speculative — each item is something observed or deliberately left undone.

**Status, 2026-09-03.** Items 3, 4, 5, 6 and 11 are done and build-verified. Item 1 is written
but not yet tested. Everything else is unchanged.

## Blocking — would damage or annoy a stranger's system

1. **Shutdown hangs.** ⏳ *fix written 2026-09-03; REPRODUCED IN THE EMULATOR, fix under test.*
   Observed on the 5160 on the claiming run: Windows would not complete a shutdown and the
   machine had to be powered off.

   **Reproduced on the Inboard bed, 2026-09-03**, with the unfixed driver
   (`1f0d8c30bd3c59c4298e60850c6c3eba`) and a shutdown driven by hand: the guest stalls on
   *"Please wait while your computer shuts down"* and never reaches *"It's now safe to turn off
   your computer"* - the owner confirms that is the exact real-hardware signature. **So this no
   longer needs machine time to iterate on.**

   `BOOTLOG.TXT` locates it (`docs/evidence_shutdown_control_bootlog_2026-09-03.txt`). Every
   stage Windows logs pairs cleanly, and the log simply stops:

   ```
   Terminate = User      Query Drivers / Unload Network / Reset Display   all paired
   Terminate = KERNEL    RIT / Win32                                      all paired
   EndTerminate = KERNEL                          <- last line in the file
   ```

   > **RETRACTED, same session.** This was written up as "the hang is located in the phase after
   > `KERNEL`, which is where `AEP_SYSTEM_SHUTDOWN` is broadcast". **That inference is wrong.**
   > A run that shuts down CLEANLY produces a byte-for-byte identical teardown ladder, ending at
   > the same `EndTerminate = KERNEL`. `BOOTLOG.TXT` simply stops logging there either way, so it
   > never distinguished the two and located nothing.
   >
   > **The general trap:** before reading meaning into where a log ENDS, check where it ends on a
   > SUCCESSFUL run. An end-of-file is only evidence if success looks different. Two theories were
   > built on this one and both cost a build.

   ### CONFIRMED OURS, by elimination

   `PORT.PDR` renamed to `PORT.PD_` in `IOSUBSYS`, nothing else changed: **Windows shuts down
   cleanly.** Put back: hangs. So the driver causes it, and the fault is in what we leave behind
   rather than anywhere else in this image. One-character rename, one boot - and it should have
   been the FIRST thing run, not the fifth (technique 77).

   **Also killed:** "we never remove our calldown from the physical DCB". There is no ISP
   function to remove one - `isp.inc` has only `ISP_INSERT_CALLDOWN` (5) - and `cfu1-win9x`
   never removes one either. Leaving it inserted is normal.

   ### LOCALISED, by bisection - it is the CALLDOWN

   Four builds, one variable each, manual shutdown, `-InGuest`/`WIN.INI run=` for the
   unattended ones:

   | build | claims unit | fills DCB | inserts calldown | publishes volume | shutdown |
   |---|---|---|---|---|---|
   | `PORT.PD_` (absent) | - | - | - | - | **clean** |
   | `-ClaimMask 0` | no | no | no | no | **clean** |
   | `-NoCalldown` | yes | yes | **no** | no | **clean** |
   | `-NoVolume` | yes | yes | **yes** | no | **HANGS** |
   | full build | yes | yes | yes | yes | **HANGS** |

   The only difference between the last clean row and the first hanging row is
   **`ISP_INSERT_CALLDOWN`**. So:

   - the claim, the inquiry and every DCB field we fill are **innocent**;
   - the published volume is **innocent** - which also means blocker 1 and blocker 2 are
     genuinely separate bugs, not one bug seen twice;
   - Windows sends I/O down our calldown during teardown, and something about how we answer
     it does not return.

   **The candidate, not yet tested:** `Port_r_not_io`, the DDK sample's path for a request our
   `XTIDE_WantIop` guard declines. It fails the request and completes it through

   ```asm
   mov eax, [ebx].IOP_callback_ptr
   sub eax, size IOP_CallBack_Entry
   call dword ptr [eax]
   ```

   which is the construct technique 82 already caught once: with a null or malformed
   `IOP_callback_ptr` it calls through a bogus address. And `XTIDE_WantIop` declines whenever
   `IOP_calldown_ptr` is **zero**, which a teardown-time quiesce plausibly is. A port driver is
   the bottom of the chain, so it cannot pass a request down - but failing one it should have
   completed, or completing one through a bad pointer, both fit.

   ### CAUSE FOUND, 2026-09-03: we block inside the request instead of returning

   From **Microsoft's I/O Supervisor Guide for Windows 9x/Me**, given by @andrew-hoffman on #21
   the same evening. This project did not have the document; the DDK's own `STORAGE.DOC` says
   nothing about it.

   > "Normally at this step, the port driver is going to have to wait for hardware to respond.
   > **If the hardware polling method is used** (instead of waking up when an interrupt arrives),
   > the driver then calls **`Set_Global_Time_Out` or `Set_Async_Time_Out`** so that the driver's
   > timeout (polling handler) routine gets called back later, for example in 10 milliseconds.
   > **Immediately after this `Set_Global_Time_Out` call, simply return (WITHOUT doing a JMP to
   > the `IOP_callback_ptr` routine). This releases the system from your driver, so the system
   > can run normally for a while.**"

   The required shape for a polling port driver:

   | step | | ours |
   |---|---|---|
   | 1 | start the I/O | yes |
   | 2 | `Set_Global_Time_Out` ~10 ms | **no** |
   | 3 | **return without completing** | **no - we block** |
   | 4 | poll from the timeout handler, re-arm if not done | **no** |
   | 5 | when done, **CALL** (not JMP) `IOP_callback_ptr` | yes |
   | 6 | `ILB_dequeue_iop`, start any queued IOP | **no** |

   **The XT-CF is jumpered with no interrupt, so we are a polling driver by construction** - this
   contract is ours, not an optional optimisation. Instead we spin inline inside `Port_request`
   (`XT_SPIN` = 400000 `in al,dx`) holding the whole system until the drive answers, then
   complete. On the 5160 that is roughly 0.6 s per wait on a 4.77 MHz bus, with nothing else able
   to run.

   **It fits every measurement**: it lives in the transport body (`-NoIo` shuts down clean), it
   needs the calldown (nothing else routes a request to us), and it survives normal operation -
   where the drive does answer - while wedging at `System_Exit`, where the scheduling and
   time-out context a blocking driver depends on is being torn down.

   **Not yet proven.** It is a documented requirement we demonstrably do not meet, in exactly the
   code the bisection isolated, which is much stronger than the two theories before it - but the
   fix has to shut down cleanly before this section says "fixed". The two negatives above are the
   reason for saying so.

   **Doing it properly is a restructure, not a patch:** the request routine returns early, state
   moves out of the globals the transport currently reuses, and a timeout handler drives the
   state machine. `zikolas/cfu1-win9x` polls too (`sup_stop`, `[irqhandle]` = 0 "hook gone
   (teardown): stay polled") and is the working reference for how.

   **Next run, already built:** `-NoVolume -NoIo` - same as the hanging build but the request
   handler completes everything as an error without touching hardware. Still hangs = the
   completion path. Clean = our transport blocks.

   ### TWO FIXES TRIED, BOTH NEGATIVE. Read before proposing either again.

   | build | change | result |
   |---|---|---|
   | `81810be1` | AER default `AEP_FAILURE` -> `AEP_SUCCESS`; `AEP_UNCONFIG_DCB` drops held DCB pointers | **hangs, identical** |
   | `70298a8f` | + `AEP_SYSTEM_SHUTDOWN` -> `XTIDE_VolDown`: `ISP_DESTROY_DCB` on the logical DCB, then unlink `DCB_next_logical_dcb` | **hangs, identical** |

   Both loaded (`Init Success port.pdr` in each log) and both stop at the same line, so these
   are real negatives, not unverified deployments. **Both changes are still correct on their own
   terms** and stay in: answering `AEP_FAILURE` to a notification is wrong per the DDK, and
   leaving a destroyed DCB linked into the physical chain is wrong per `cfu1-win9x`, whose
   `CFU1_VolDown` is working code doing exactly this. They just are not the cause.

   **The elimination that should have come first**, and did not: rename `PORT.PDR` out of
   `IOSUBSYS` and shut down. If it still hangs, the driver was never involved and every theory
   above is about the wrong component. That is technique 77's decisive move - the one-character
   rename that cracked #22 - and it was set up twice this session and spent on neither.

   **Next suspect if the driver is innocent:** `SD120PPD.SYS` / `ASPIHDRM.SYS`, the parallel-port
   LS-120 real-mode pair. Present on the bed **and** on the 5160, the last remaining real-mode
   unit (item 10), and already known from #22 to write to hardware it does not own. A real-mode
   ASPI driver is a plausible way to wedge the protected-to-real-mode transition at `System_Exit`.
   One-line REM to test - `tools/rem_config_line.py`.

   Cause found by reading the DDK rather than the machine. `Port_Async_Request` dispatches five
   AEP function codes and answers **`AEP_FAILURE` to every other one** (`PORTAER.ASM`, the line
   commented "set result code to indicate error"). IOS broadcasts at least four it has never
   heard of, all at `System_Exit`:

   | code | | code | |
   |---|---|---|---|
   | 1 | `AEP_SYSTEM_CRIT_SHUTDOWN` | 14 | `AEP_SYSTEM_SHUTDOWN` |
   | 4 | `AEP_UNCONFIG_DCB` | 21 | `AEP_PEND_UNCONFIG_DCB` |

   — plus `AEP_ASSOCIATE_DCB` (12), which **our own** `ISP_ASSOCIATE_DCB` makes IOS issue. These
   are notifications. "I refuse" is not an answer to one.

   Fix: the default becomes `AEP_SUCCESS` (already preset at the top of the routine), and
   `AEP_UNCONFIG_DCB` calls a new `XTIDE_ForgetDcb`, which drops the two pointers we hold into
   IOS-owned DCBs — the per-unit calldown record and `XTIDE_LogDcb` — before `ISP_DCB_DESTROY`
   frees them. `STORAGE.DOC` describes the message's contract as exactly that: "a layer typically
   just makes a note of the fact that the DCB is going away."

   The default is `SUCCESS` rather than an enumerated list of codes on purpose. Technique 51 in a
   new place: an exit condition gated on a hand-written list of values fails silently and badly
   the first time reality produces one the list does not have, and this project has paid for that
   four times already.

2. **Two volumes over one partition.** We publish our own logical volume from the boot disk
   while the Real Mode Mapper still owns C:, so the same filesystem is mounted twice with two
   independent caches. It survived a full boot on hardware and on a clone, but it is a
   corruption risk by construction and must not ship. The fix is the C: takeover - claim the
   DCB that already exists rather than creating a second one - not a warning in a README.

   @andrew-hoffman's warning of 2026-08-31 is load-bearing for this and should be answered
   before the work starts, not after: *"Once the driver loads successfully, the INT 13h BIOS
   will be disconnected and log files cannot be written to that device anymore. You have the
   controls of the airplane and no way to land it."*

3. ~~**`-NoWriteTest` is not the default.**~~ ✅ **Done 2026-09-03.** Inverted to `-WriteTest`,
   opt-in. A test that writes LBA 100 of whichever unit answers is no longer one forgotten flag
   away from every build. Defaults decide what happens when someone is tired.

4. ~~**The request marker must be impossible in a shipped build.**~~ ✅ **Done 2026-09-03.**
   Payload and code are both behind `XT_REQ_MARKER`; the release build's `.map` contains zero
   marker symbols.

   **What guarding it found is worth more than the guard.** It would not assemble, because
   **six variables the driver genuinely needs to run were living inside the 512-byte diagnostic
   buffer** — `XTIDE_OurDdb`, `XTIDE_VolBusy`, `XTIDE_OurUnit`, `XTIDE_PartStart`,
   `XTIDE_PartLen`, and `XTIDE_MkVolDrive`, the last being the drive-letter walk's *loop
   variable*. A release build could not have existed. Report and state are now separate;
   `tools/pdr_reqmarker.py` reads by fixed byte offset, so every slot that moved out left a
   placeholder behind.

   A `MARK` macro replaces the diagnostic stores, so a release build carries neither the store
   nor the field behind it — nineteen inline `ifdef`/`endif` pairs would have had one wrong.

## Correctness — known-wrong, not yet harmful

5. ~~**The device node reserves `0300-030F`, sixteen ports.**~~ ✅ **Done 2026-09-03.**
   `PORT.INF` declares `0300-031F`. Stride 2 puts alternate status at `0x31C`.

6. ~~**`DriverDesc` still says "phase 1 - IDENTIFY only".**~~ ✅ **Done 2026-09-03.** The device
   is now named `Lo-tech XT-CF / XT-IDE 8-bit disk controller`.

7. **`IORF_LOGICAL_START_SECTOR` handling is written but has never executed.** Every request
   measured, on both beds and on hardware, arrived absolute (`IOR_flags 0x401`). The bias code
   is there because a volume taken over from RMM may not be absolute - but it is unexercised,
   and unexercised code is where every expensive bug this project has had came from.
   **The C: takeover (item 2) is what exercises it.** The two are one piece of work.

8. **Stride autodetect has only ever chosen 1.** 86Box's XT-IDE is stride 1, so the branch that
   picks 2 has never run. Hardware builds pin `-Stride 2` and should keep doing so until
   autodetect gets a run of its own.

## Environmental — true of the test machine, not of the driver

9. **The real-mode SCSI/ASPI chain is REM'd out of `CONFIG.SYS` on the 5160.** That is what
   let IOS load the driver at all - `MODISK2.SYS` was flagged `Unsafe`/`Monolithic` and pinned
   six units in real mode. It is a workaround. The devices are meant to come back through the
   32-bit T130 miniport (issue #19), and until they do, this driver's success on that machine
   is conditional on hardware being switched off.

10. **One real-mode unit remains**, `SD120PPD`/`ASPIHDRM` - the parallel-port LS-120. Worth
    removing next to see whether IOS goes fully clean, and it is a one-line REM. Confirmed
    still present in the bed's `CONFIG.SYS` on 2026-09-03.

11. ~~**The boot-log delay channel ships in every build.**~~ ✅ **Done 2026-09-03.** Added after
    the list was first written. `PORT_Device_Init` carried an arrival marker plus a
    `DRP_reg_result` report — together up to ~12 s of spinning **with interrupts off**, which
    technique 86 already records as indistinguishable from a lockup to anyone watching. Measured
    at 134 boot-log ticks on the Inboard bed.

    `build.ps1 -Release` strips it, strips the marker, and **refuses to build** if any
    diagnostic or bisect switch is also set. "No path and no payload" is now one flag somebody
    can check rather than five they have to remember.

## Not defects, but state the reader needs

- **ATAPI/CD is permanently out of scope.** The Lo-tech XT-CF has D8-D15 unconnected.
- **Everything is 8-bit PIO.** A 16-bit XT-IDE variant needs a data-path change, not a switch.
- **The DDK sample this is built on ships four bugs of its own** - a request routine that
  destroys the registers its own header promises to preserve, a calldown spliced into every
  DCB the OS broadcasts, an `EnterProc` that emits no stack frame outside a DEBUG build, and
  an AER that answers `AEP_FAILURE` to every message it does not recognise (item 1).
  All four are fixed here; anyone starting from the same sample will meet them.

## Testing the shutdown at all — the harness had to change first

The teardown AEPs are broadcast at `System_Exit` and nowhere else, so testing item 1 needs a
real shutdown driven unattended.

**Host-side keystroke injection into 86Box could not be shown to reach the guest.** A
`Ctrl+Esc`/`U`/`Enter` sequence left the screen byte-identical across all three frames. Worse,
the harness's own evidence that keys *ever* landed turns out to be nothing: it taps Enter through
the DOS phase to clear `TSLCD`'s "Press [return]", and **that line has been REM'd out of
`CONFIG.SYS` since 2026-09-01**. Nothing in the DOS phase has needed a keystroke for days, so a
completed boot never evidenced delivery. Technique 71, in this project's own harness.

So the shutdown is triggered **from inside the guest**: `SHUT.BAT` in the StartUp folder settles
for 45 s via `CHOICE`, then runs `RUNDLL32.EXE user.exe,ExitWindows`. It is unattended,
repeatable, and takes no focus from whoever is using the host.

Two readback channels, both surviving a hang:

| channel | answers |
|---|---|
| `C:\SHUTLOG.TXT` — one line per batch stage | did the trigger fire at all |
| `BOOTLOG.TXT` `Terminate=`/`EndTerminate=` pairs | which teardown stage Windows stopped in |

A stage that starts and never ends **names the hang**. A screenshot of a frozen desktop does not.
`tools/pdr_inboard_run.ps1 -Shutdown -InGuest` runs it and prints both.

**The harness no longer steals focus.** It grabs it once, at launch, when the VM has it anyway;
after that a key is sent only if the VM *still* has focus, and skipped keys are counted and
reported so a run that missed its prompts is recognisable as VOID rather than as a result. The
previous version grabbed focus every 5 s for 300 s, which makes the host unusable for whoever is
sitting at it.

## The boot-order question, and the exact test that settles it

Owner's read, 2026-09-02, and it is worth proving rather than assuming: **the position of a
real-mode driver in `CONFIG.SYS` may matter as much as its presence.** IOS scans what exists when
it runs, so which units are claimed - and by whom - depends on load order.

Right now we cannot tell the two apart. We REM'd the whole SCSI/ASPI chain, so "MODISK2 absent"
and "MODISK2 loaded late" have never been separated.

**The single-variable test:** restore `MODISK2.SYS` exactly as it was, but move it to the **end**
of `CONFIG.SYS`, after every other `DEVICEHIGH` line. Nothing else changes.

| result | meaning |
|---|---|
| IOS still flags it `Unsafe`/`Monolithic` | it is **presence**. Order is a red herring and the chain has to go, or move to the 32-bit T130 miniport |
| flag gone, `Init Success` holds | it is **order**, and that is a far better answer - the devices can stay, and we ship a supported ordering |

The second outcome is the one worth hoping for, because it turns a caveat into an instruction.
Either way it belongs in whatever ships: **anyone running this on a machine with a real-mode ASPI
or MO/Zip stack will hit the same wall**, and `IOS.LOG` is how they will recognise it - look for
`Unsafe driver` and how many units are `going through real mode drivers`.

That diagnosis generalises even where `MODISK2` specifically does not, so it is the caveat to lead
with, not a footnote.

## Knock-on: `HSFLOP.PDR` now loads, so issue #18 is live again

Technique 74 recorded on 2026-08-25 that `HSFLOP.PDR` never loads on this machine, which is why
the floppy `maxPhys 0x1000 -> 0xFF` patch was inert and issue #3 was reframed.

**That is no longer true.** Every boot of the Inboard bed since the real-mode storage chain came
out ends:

```
Initing hsflop.pdr
Init Success hsflop.pdr
...
INITCOMPLETE = HSFLOP
INITCOMPLETESUCCESS = HSFLOP
```

So the floppy is on the 32-bit path, the DMA-reach patch is no longer dead code, and **issue #18
is testable for the first time**. Clearing the real-mode chain for the port driver's sake fixed
the precondition for a different issue entirely.
