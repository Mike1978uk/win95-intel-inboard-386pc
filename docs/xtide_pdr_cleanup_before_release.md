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

   The phase after `KERNEL` is the ring-0 VxD teardown - `System_Exit` and
   `Sys_Critical_Exit` - which is exactly where IOS broadcasts `AEP_SYSTEM_SHUTDOWN` and
   `AEP_SYSTEM_CRIT_SHUTDOWN`.

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
