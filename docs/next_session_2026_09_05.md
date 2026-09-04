# Next session — 2026-09-05

**Supersedes `next_session_2026_09_04.md`, which is retracted at the top of its own file.**

## Where the XT-IDE port driver (#21) actually stands

The driver works. It boots Windows, mounts the volume, reads and writes correctly through an
8-bit XT-CF on a stride-2 register map, and on the real 5160 it takes the disk in protected mode
with the real-mode mapper unloaded. **One defect remains: Windows will not finish shutting down.**

## The bug, closed to one register

```
C0003184:  8B 6B 08            mov ebp,[ebx+8]
           39 05 F4 E9 00 C0   cmp [C000E9F4], eax
C000318D:  75 ED               jne -19        ; loop while they differ; EAX never reloaded
```

| | EAX | EBX | outcome |
|---|---|---|---|
| clean (driver absent) | `C0FCE28C` — a VMM pointer | `C15200E8` | falls through; shutdown completes |
| hung (driver present) | **`00000000`** | `C15200E8` — **the same object** | spins forever |

`[C000E9F4]` is never written in **either** run, so the global is not the variable. The clean run
exits because EAX already matches. **With our driver loaded, VMM enters this wait holding a null
handle where a valid pointer belongs.**

Everything downstream follows: the state write at `C00032B4` never runs (0 writes with the driver,
213 without), so nothing advances and the shutdown waits forever.

### Do this first

EAX is set before `C000317F`. The caller path on a **clean** run is:

```
C0002657..C0002665 -> C0002795 -> C000352E..C0003559
-> C0001300..C0001354            (dispatcher; C0001300 is the return address)
-> C000317F .. C000318F          (the loop)
-> C0003190..C0003268 -> C0001359 C0001360 -> C000329A..C00032B4
```

Trace backwards from `C0001300`–`C0001354` and find what returns zero. It will be an object VMM
asks for at `System_Exit` that our calldown insert changes.

## The bed — this is the enabling piece, do not lose it

`vm_xtcf_faithful/` is the **first emulator bed that matches the real card**, and the hang only
reproduces there. It has your CF's own driver (`0fe2431a`) restored and ready.

86Box needed two fixes to model the card, both on `Mike1978uk/86Box` branch `xtcf-lotech-stride2`:

- **stride-2 decode** — the Lo-tech XT-CF rev 3 does not decode A0, so registers sit at `base+2N`
  over 32 ports. New `Register stride` option, default 1, so every existing config is unchanged.
- **8-bit PIO** — the card's BIOS sends `SET FEATURES 01h` and the data register becomes byte-wide
  with no high latch. 86Box was reading 16 bits per access and dropping every second byte.
- plus a BIOS entry for the XT+ r638 image read back off the card
  (`roms/hdd/xtide/ide_xtcf_lotech.bin`).

Without these the driver silently falls back to a stride-1 path, never properly claims the disk,
`RMM.PDR` serves it instead, and **the bug cannot appear**. That is why four emulator shutdowns
looked clean and told us nothing. See technique 90.

Diagnostics in that fork are marked `DIAGNOSTIC, not for upstream` and must be stripped before any
PR: the CS:EIP heartbeat and caller trace in `386_dynarec.c`, the `0E0h` debug port in
`inboard386.c`, the linear write watchpoint in `mem.c`, and the `ENABLE_XTIDE_LOG` define.
One genuine upstream fix is mixed in and should be kept: `xtide_init` never called `log_open`.

## Eliminated by measurement — do not re-run these

| candidate | how it died |
|---|---|
| the LS-120 real-mode chain (`SD120PPD.SYS`, `ASPIHDRM.SYS`) | REM'd on the CF, hardware still hangs. `IOS.LOG` disappeared entirely, so IOS's complaint and the hang are independent |
| the published logical volume | `-NoVolume` still hangs |
| claiming master+slave | `-ClaimMask 1` still hangs |
| the polling contract / `Set_Global_Time_Out` | the wait is ordinary VMM code a clean shutdown walks straight through |
| a timeout storm | 3,582 status polls where one expiry needs 400000 |
| the fast-fail clamp built 09-04 morning | never armed — `AEP_SYSTEM_SHUTDOWN` does not reach our handler |

## Two standing bugs found on the way, neither chased

1. **`AEP_SYSTEM_SHUTDOWN` never reaches `Port_Async_Request`** — proven by zero output from the
   `0E0h` counters. So `XTIDE_VolDown` has never run. `AEP_UNCONFIG_DCB` is tested first and
   returns early; that is the likely explanation and it is untested.
2. **The duplicate `D:` is solvable.** With `-NoVolume` the machine shows only `C:`, and Windows'
   own `DiskTSD` assigns it (your bootlog shows `INITCOMPLETE = DiskTSD`). The driver does not need
   to be its own TSD. That closes the single-disk goal independently of the hang.

## Provenance — the reason 2026-09-04 cost a day

A shutdown hang was chased for a day in a binary (`70298a8f`) that could not be rebuilt from any
commit: built from an uncommitted tree on 09-03, deployed straight into the emulator image, source
lost. `build.ps1` now prints the commit, shouts on a dirty tree, and appends md5 → provenance to
`drivers/xtide_pdr/build_ledger.tsv`. **Commit before deploying anything you will draw a
conclusion from.** Technique 89.

---

## Update, later on 2026-09-04 — two corrections and the current lead

### ⚠ The "null handle" conclusion above is WRONG

Dumping the live bytes killed it. `C000318D`'s `jne -19` targets `C000317C`, which is
`dec eax / pop esi / ret` — **a failure return, not a loop head** — and at the wedge the compare is
*equal*, so the branch is not even taken. `C0003184` is a short leaf **called ~2,000,000 times**,
not a spin; every heartbeat landed there because it is hot. `EAX = 0` was that routine's own
`xor eax,eax` two instructions earlier. See technique 90d.

### What is actually cycling

A raw 256-entry ring, with both hot VMM regions excluded, gives the real loop — ~150 distinct
instructions across six routines, repeating:

```
C002D754 -> C002D48C -> C00039BC..C0003A81 -> C002D768 -> C002D796 -> C002BEC2 -> C002BED7
-> C002EA48 -> C002CA56 -> C0001430 -> C0001100 -> C00012C0 -> C0001370 -> C0001410..C000142A
-> C002BEF4 -> C002BEB6 -> C002D744 -> C002C8BC..C002C8E0 -> C002D74D -> C002D752 -> (repeat)
```

`C002xxxx` is a VxD loaded after VMM; `C0001xxx`/`C0003xxx` are VMM services it calls. So a VxD is
**polling** and never getting the answer it needs. It does real work each pass — this is not a
spinlock.

### The lead worth following, and why

**`AEP_SYSTEM_SHUTDOWN` never reaches `Port_Async_Request`** (proven: zero output from the `0E0h`
counters). If IOS broadcasts a shutdown event and waits for every registered driver to acknowledge,
and ours never receives it, IOS polls forever. That fits every observation and is testable
**driver-side** rather than by reverse-engineering Microsoft's code.

`XTIDE_DbgAep` now reports **every AEP the instant it arrives**, with ordering, through the debug
port (tag `10h`). Built as `d5297c78`. Read it with:

```
grep -a 'DBGPORT tag=10' <log> | awk '{print $3}' | uniq -c
```

- AEPs at shutdown but not `14` -> we mishandle one we do get; the list names it.
- **Nothing** at shutdown -> IOS is not reaching us; the target becomes how we registered
  (`DRP_FC_DYNALOAD`, technique 83).
- `14` does arrive -> this morning's zero-counter result was wrong and needs explaining first.

The owner's framing is worth keeping: **startup should be the mirror of shutdown**, and the same
capture shows both halves, so no extra run is needed to compare them.

### Dead end, recorded so nobody repeats it

Comparing our DRP against `HSFLOP`/`ESDI_506`/`SCSIPORT` by searching for the driver name in the
binary finds the **LE resident-name table**, not the DRP — and all four are byte-identical there.
The feature code lives in the data segment; a different approach is needed.

---

## THE LEAD, 2026-09-04 end of session — AEP census on the faithful bed

Two corrections first, both caused by the stride-1 bed:

- **`AEP_SYSTEM_SHUTDOWN` DOES reach us.** Our handler runs; `XTIDE_ShutdownArm` and
  `XTIDE_VolDown` both execute. The earlier "never arrives" was measured on the stride-1 bed where
  the driver never properly claimed the disk, so IOS never sent it the teardown.
- **The completion ladder is perfect:** offered 3527 / accepted 3527 / started 3527 / done 3527.
  Every IOP completed. Nothing is waiting on us for I/O.

### What IOS actually sends us

Startup: `PEND_UNCONFIG(21) UNCONFIG(4) ASSOCIATE_DCB(12) CREATE_VRP(18) MOUNT_NOTIFY(17)
PEND_UNCONFIG(21) UNCONFIG(4) CREATE_VRP(18) x2 DESTROY_VRP(19) CREATE_VRP(18) MOUNT_NOTIFY(17)`

Shutdown: `PEND_UNCONFIG(21) DCB_LOCK(16) DESTROY_VRP(19) UNCONFIG(4)` **twice**, then
`UNINITIALIZE(15)`, then `SYSTEM_SHUTDOWN(14)`.

### The bug

`Port_Async_Request` dispatches only `AEP_INITIALIZE`, `AEP_DEVICE_INQUIRY`, `AEP_CONFIG_DCB`,
`AEP_IOP_TIMEOUT`, `AEP_BOOT_COMPLETE`, `AEP_UNCONFIG_DCB` and `AEP_SYSTEM_SHUTDOWN`. Everything
else falls to `pa_note_only` and is answered **`AEP_SUCCESS`** — "done" — having done nothing.

At shutdown that means we answer these blind:

| code | AEP | why it matters |
|---|---|---|
| **15** | **`AEP_UNINITIALIZE`** | counterpart of `AEP_INITIALIZE`. We create a DDB at init with `ISP_CREATE_DDB` and **nothing ever destroys it**. We claim to have released everything while still holding the DDB and the calldown. **Prime suspect.** |
| 16 | `AEP_DCB_LOCK` | unhandled |
| 21 | `AEP_PEND_UNCONFIG_DCB` | a *query* — "may I unconfigure this?" — answered yes, blind |

A driver that reports it has uninitialised and has not is exactly what would leave a VxD polling
forever for all drivers to release. That matches the observed cycle: a VxD at `C002xxxx` calling
VMM in a loop, our driver idle, all our I/O finished.

### Do this first

Handle `AEP_UNINITIALIZE`: destroy the DDB (`ISP_DEALLOC_DDB`, the sample already does this on the
init-failure path in `Port_initialize`), drop the calldown, clear our own state. Then re-run the
faithful bed. `AEP_DCB_LOCK` and `AEP_PEND_UNCONFIG_DCB` are the next two if that is not enough.

Note the sample's `pa_note_only` comment claims answering `AEP_SUCCESS` "is the right answer to all
of them". That is true for notifications and **false for `AEP_UNINITIALIZE`**, which is a command.

### ❌ Tested and negative: handling `AEP_UNINITIALIZE` does not fix it

Built `4c124470` (`16ce19b`): on `AEP_UNINITIALIZE` call `ISP_DEALLOC_DDB` on the DDB created at
init and clear our reference, instead of answering `AEP_SUCCESS` blind. **Still hangs**, same VMM
cycle, same AEP sequence. So holding the DDB was not the cause. The handler is correct on its own
terms and has been kept.

**Next two, in order:** `AEP_DCB_LOCK` (16) and `AEP_PEND_UNCONFIG_DCB` (21) — the remaining
shutdown codes we answer `AEP_SUCCESS` blind. `PEND_UNCONFIG` is a *query*, so answering it wrongly
is the more interesting of the two.

Also still unexplained and worth a look before more guessing: at startup we receive
`PEND_UNCONFIG(21)` and `UNCONFIG(4)` **twice**, and our `AEP_UNCONFIG_DCB` handler calls
`XTIDE_ForgetDcb`, which clears the `XTIDE_InsertedDcb` slot. If the DCB is later reconfigured we
never re-record it, so `XTIDE_VolDown`'s unlink scan at shutdown may be looking at an empty table.
That is a state-tracking bug regardless of whether it causes the hang.

### The census asymmetry — 4 `CREATE_VRP`, 3 `DESTROY_VRP`

Counting the whole session: `CREATE_VRP(18)` x4 (all at startup), `DESTROY_VRP(19)` x1 at startup
and x2 at shutdown. **One volume record is created and never destroyed.** That is the only genuine
imbalance in the census, and "a VxD polling until every volume is gone" fits the captured cycle.

The duplication is separately explainable: `PEND_UNCONFIG`/`DCB_LOCK`/`DESTROY_VRP`/`UNCONFIG` all
arrive **twice** at shutdown, and `PEND_UNCONFIG`/`UNCONFIG`/`MOUNT_NOTIFY` twice at startup -
consistent with **two DCBs**, the physical one and the logical volume we publish.

**Never yet run: `-NoVolume` WITH the AEP reporter.** `83c3bca1` predates `XTIDE_DbgAep`. If the
duplication collapses to one pass, the pairs are confirmed per-DCB and any leftover VRP is
definitely not ours. One build, one boot, and it splits the two theories above.

Worth correlating against `HSFLOP.PDR` / `ESDI_506.PDR`, which shut down cleanly on this machine:
which of these codes do they dispatch, and do their VRP counts balance? Copies are in
`roms/xtcf_card/`.

### ⭐ THE CORRELATION — CFU1 answers `AEP_FAILURE` where we answer `AEP_SUCCESS`

`zikolas/cfu1-win9x`, `win/vxd/CFU1.ASM` ~line 3029 - a **working** Win9x IOS port driver that
shuts down cleanly. Its entire AEP dispatch:

```
0  AEP_INITIALIZE      handled
6  AEP_DEVICE_INQUIRY  handled
2  AEP_BOOT_COMPLETE   AEP_SUCCESS
3  AEP_CONFIG_DCB      handled
16..19  DCB_LOCK / MOUNT_NOTIFY / CREATE_VRP / DESTROY_VRP  -> AEP_SUCCESS as a block
everything else  ->  mov word ptr [ebx+2], 0FFFFh    ; AEP_FAILURE
```

**So a driver that shuts down cleanly returns `AEP_FAILURE` for `UNCONFIG_DCB`(4),
`SYSTEM_SHUTDOWN`(14), `UNINITIALIZE`(15) and `PEND_UNCONFIG_DCB`(21).** It does not implement
them; it tells IOS so, and IOS handles it.

We do the opposite. On 2026-09-03 the sample's default was changed from `AEP_FAILURE` to
`AEP_SUCCESS` on the reasoning quoted in `PORTAER.ASM` - *"AEP_SUCCESS ... is the right answer to
all of them"*. **CFU1 says the sample was right and that change was wrong.** We are acknowledging
four teardown commands we do not implement.

**Do this first, it is one build:** restore `AEP_FAILURE` as the default for undispatched codes,
and acknowledge only `16..19` as CFU1 does. Note this also means *removing* our `UNCONFIG_DCB` and
`SYSTEM_SHUTDOWN` handlers from the dispatch, or at least making them answer FAILURE - which is a
bigger behavioural change than it looks, so bisect it: default-FAILURE first, keeping our two
handlers, then FAILURE for those two as well.

CFU1 also keeps an `aep_counts[32]` census array - independent validation of the method used here.
It does **not** remove its calldown at shutdown either, which weakens (does not kill) the
never-removed-calldown theory above.

---

## ⭐⭐ THE CLOSING FINDING, end of 2026-09-04 — VMM waits for a list we are still on

### First, one more negative

`c5b5b7f0` / commit `4ba1995` — answering `AEP_FAILURE` for undispatched codes and acknowledging
only `16..19`, exactly as CFU1 does. **Still hangs.** So the AEP default is not the cause. The CFU1
comparison was still the right method; this particular difference just was not it. Boot was
unaffected (C: and D: both present), so failing `ASSOCIATE_DCB` at startup is harmless.

### The real loop, and this one IS a loop

```
C0008F8A:  81 3D 10 08 01 C0  0C 08 01 C0   cmp dword [C0010810], C001080C
           0F 85 43 FF FF FF                jne -0BDh   -> C0008ED3, backwards, verified
```

Comparing a list head against the **sentinel address of the list itself** - the standard
`while (head != &list)` idiom. **VMM is waiting for a queue to drain.**

### Who puts things on it, and what is left

A linear write watchpoint on `C0010808-C001081F`:

| | |
|---|---|
| writers | **exactly one**: `[0028:C000921F]`, dword, always to `C0010810` |
| writes during the run | 107 |
| **writes during shutdown** | **zero** |
| last value pushed | **`C1032D1C`** |

`C103xxxx` is **our driver's own address range** (our code executes at `C1031B30`, `C1031EBE`,
`C1031AED`...). So VMM is holding a pointer to an object inside our driver on a list it later spins
waiting to see empty, and **nothing removes it at teardown**.

That is consistent with every measurement of the day: all our I/O completes (ladder exact at
3527/3527/3527/3527), the driver goes idle, our AEP handlers all run, and a VxD polls forever.

### Do this first next session

1. **Identify what lives at `C1032D1C`** in the loaded driver. Our load base is derivable from a
   known code address (e.g. a heartbeat sample inside `XTIDE_ReadData`) minus its file offset; then
   `C1032D1C - base` gives the offset into our image, which the `.map` and listing will name.
2. **Find what registered it.** One writer (`C000921F`) means one VMM service; a caller trace armed
   on `C000921F` (the ring tooling already exists) names it in one run.
3. Then the fix is to deregister it - at `AEP_UNINITIALIZE` or `AEP_SYSTEM_SHUTDOWN`, both of which
   we now demonstrably receive and handle.

Note this sits oddly with the fact that our driver makes **zero VMM service calls** (technique 88's
table) - so whatever put us on that list, we did not call VMM to do it directly. IOS may have done
it on our behalf at `ISP_INSERT_CALLDOWN`, which would finally explain the original bisect: *no
calldown -> clean, calldown inserted -> hangs*, the one signal that has survived every test today.

---

## CORRECTION 2026-09-04, later — `C1032D1C` is NOT inside our driver

Commit `8fe1609` says the queued node is *"inside our driver's own address range"*. That was
inferred from "our code runs at `C1031xxx`" and never from a computed base. It is wrong.

### The load base, established three independent ways

The binary in `vm_xtcf_faithful/prextide.img` is `c5b5b7f0`, which is exactly the build
`drivers/xtide_pdr/build/PORT.map` describes — so the map is the right one for this run.

| evidence | map offset | address |
|---|---|---|
| `XTIDE_WaitNotBusy` `in al,dx` | obj1+0xDD5 | `C1031B16` |
| `XTIDE_WaitDrq` `in al,dx` | obj1+0xDEC | `C1031B2D` |
| `XTIDE_ReadData`, 8-bit path | obj1+0xE1C | `C1031B5D` |
| `XTIDE_WriteData` `out dx,al` | obj1+0x11A0 | `C1031EE1` |
| heartbeat's 16 live bytes at `C103274A` | matches the file at obj1+0x1A0A byte for byte | |
| same heartbeat: `ESI`=`C1030E14`, `EDI`=`C1031268` | = `XTIDE_IdBuf` (0xD4), `XTIDE_PartType` (0x528) | |

**Object 1 loads at linear `C1030D40`.** (The XT-IDE log prints `pc` *after* the one-byte
`in`/`out`; the heartbeat prints it at the instruction start. That one-byte difference is why
the two families of anchor disagree by 1, and reconciling them is what pins the base exactly.)

### Where the node actually sits

```
C1032D1C - C1030D40 = 0x1FDC
object 1 vsize      = 0x1B88   ends C10328C8
whole module packed = 0x1E21   ends C1032B61
```

**0x454 past the end of object 1, 0x1BB past the end of the module.** Not `port_ilb`, not
`PORT_DDB`, not any symbol in the `.map`.

And `C1030C54` — 18 of the same 107 writes — is `base - 0xEC`, i.e. just *before* our image.
Nodes on both sides of our module, with the rest at `C0FD*`, `C0FF*` and `C159F068`: that is the
signature of **heap blocks**, one allocated just before our module and one just after, not of our
own statics. Consistent with something allocated on our behalf while we were being loaded — which
is the `ISP_INSERT_CALLDOWN` suspicion above — but that is inference, not measurement.

### Free result: it is a wedge, not slow progress

Technique 88 carries an open question — *"nobody has recorded how long the machine was left"* —
with a correction warning that our spins are bounded, so minutes of timeouts would look identical
to a hang. The session that produced `faithful.log` was left running and answers it by accident:

| | |
|---|---|
| last write to the list | byte 440,158,692 |
| file size | 446,313,242 |
| after the last write | **6.15 MB of heartbeat and nothing else**, ~1 hour of wall clock |

Zero device I/O, zero list activity, one hour. That is a wedge.

### And the second cluster is a list WALK, not a spin

The live instruction bytes at the freeze:

```
C000323C:  8D 50 04     lea  edx,[eax+4]
           8B 42 FC     mov  eax,[edx-4]      ; node = node->next
           85 C0        test eax,eax
           74 09        jz   ...
           85 48 08     test [eax+8],ecx      ; ecx = 80000082
C000324B:  8B D0        mov  edx,eax
C000324D:  EB F0        jmp  -10h             ; back into the walk
```

`EAX` cycles `C0FCEB50 -> C0FCF120 -> C10CD860 -> C0FCF288 -> C0FCF120 ...` — it revisits nodes,
so the walk does not terminate. `EDI` is pinned at `C159F068` throughout, the most-written value
on the watched list (32 of 107), and `EBX` at `C15200E8`, the object named on 2026-09-04.

### What has no control yet

`run_control_clean.log` and `run_ctrl_calltrace.log` armed the watch on `C000E9F0-C000E9F7`, not
on this list. **So nothing yet shows what a clean shutdown does to `C0010810`** — whether it
drains, and who pops it. That is the comparative to run next if the instrumented run does not
name the popper on its own.

---

# SESSION CLOSE 2026-09-04 evening — read this before anything above it

Eleven emulator boots, each with a real shutdown driven by hand at the desktop. **The hang is not
fixed.** What follows is what is now known, what was falsified, and the one measurement left
running when the session ended.

## Retracted this session — do not act on these

| claim | where it came from | why it is dead |
|---|---|---|
| `C1032D1C` is an object inside our driver | commit `8fe1609` | our object 1 loads at `C1030D40`; that address is `base+0x1FDC`, past object 1's `0x1B88` and past the whole packed module. It is a heap neighbour |
| VMM waits for thread list `C001080C` to drain | commit `8fe1609` | walked it at the wedge: **0 nodes, `next==prev==sentinel`**. The `cmp/jne` at `C0008F8A` passes |
| VMM waits for list `C0010C98` | this session | walked it at the wedge: **also empty**. `C0008F40`'s `jz` exit is available |
| the published volume / appy-time event is the cause | this session | `-NoVolume` (`2f7c8f4e`) removes the publish, the logical DCB, the drive-letter association and the only `_SHELL_CallAtAppyTime` call. **Still wedges, identically** |

## Established, and worth not re-deriving

- **Every queued node carries `THCB` at +0Ch** — they are VMM Thread Control Blocks, and `+14h` is
  the System VM handle `C15200E8` on all of them (the value pinned in `EBX` through every wedge).
- **The orphan is named.** `CREATE_VRP` for VRP `C0FD65C8`, **drive 2 = `C:`**, issued right after
  `AEP_ASSOCIATE_DCB` on our DCB `C0FD6490`, and never destroyed. `A:` (`C0FD2C6C` → `C0FD2C38`) is
  created and destroyed on the same machine seconds apart. **Destroys report the VRP 0x34 below the
  create's pointer — pair them on that offset.**
- **IOS abandons our teardown after `AEP_DCB_LOCK`.** `A:` runs `21·16·19·4` all on one DCB. Ours
  runs `21` on `C0FD6490`, then `16` on a *different* object, then nothing. `SYSTEM_SHUTDOWN`
  follows and the machine wedges. Identical across all four driver fixes.
- **Windows is genuinely 32-bit on this disk.** After Windows starts: **5,039,676 XT-IDE accesses,
  every one from selector `0028`; zero from `D000`.** All real-mode INT 13h traffic is DOS boot.
  `RMM.PDR` loads and never reaches `INITCOMPLETE`; `PORT` does.
- **No I/O reaches us after the teardown starts** — zero accesses between the first teardown AEP and
  the wedge. IFSMGR never even attempts the unmount, which is why driver-side I/O fixes cannot help.
- **Thread `C159F068` has inconsistent links**: its `+4`/`+8` both point at sentinel `C0010C98`
  while that sentinel points at itself. A thread that believes it is on a list the list does not
  contain. First anomaly found in a VMM structure rather than in our answers.

## The four driver fixes made tonight — all correct, none causal, all kept

Each is a DDK contract we were breaking. Each changed the teardown by **not one AEP**.

1. **`AEP_PEND_UNCONFIG_DCB` (21) was answered `AEP_FAILURE`** by the catch-all. `STORAGE.DOC`: it
   is the *first* AEP sent when a DCB is destroyed and layers "are expected to stop and prevent all
   further input and output". Now dispatched and answered. (`eabb9be`)
2. **Everything but READ/WRITE/VERIFY returned `IORS_INVALID_COMMAND`**, `IOR_FLUSH_DRIVE`
   included. Flush, media-check, lock/unlock, cancel, clear/abort/restart queue, spin up/down and
   DOS reset are honest no-ops on an XT-CF, so they are answered. (`de58591`)
3. **Quiesce on code 21**, gating data movement only — quiescing the housekeeping commands would
   refuse the very flush that lets a volume go. (`19f9668`)
4. **Load group was the sample's `DRP_MISC_PD`, `'Generic Port Drv'`.** Read off the card's own
   binaries: `ESDI_506` = `ESDI_PD`, `SCSIPORT` = `NT_PD`, `HSFLOP` = `NEC_FLOPPY`. Ours was the
   only one in a non-disk band; our `DRP_BT_ESDI` bus type already matched `ESDI_506`. (`c5632a6`)

## Also done

- **Stride autodetect is now the default** (`405f5e4`). One binary covers a classic XTIDE at
  `base+N` and the XT-CF at `base+2N`. The probe reads Status and Alternate Status under each
  candidate and keeps the map where they agree; it writes nothing, so it cannot repeat the SRST
  accident a write-probe would cause. **⚠ It has NEVER executed on hardware** — the ledger has one
  `XT_STRIDE=0` build ever, `e8edf217`, built today and never deployed. Every 5160 binary is pinned
  stride 2. Its first boot is a test.
- **Upstream PR [#7858](https://github.com/86Box/86Box/pull/7858)** raised: XT-IDE logging was inert
  on the plain card because only `jride_init()` opened a log handle. Four lines. README updated.
- The XT-CF stride-2 / 8-bit PIO model **stays local** by decision — revisit upstreaming once the
  driver lands, when it can be submitted as "validated by a working 32-bit port driver".

## ❌ The fifth theory, raised and killed in the same run — kept only as a worked example

**This was wrong. Read the subsection after it.** The reasoning below looked airtight: it came from
the loop's own instructions rather than from a guess at a structure, which is exactly what the
earlier failures lacked. It was still wrong, because it assumed the code that appears most in a
heartbeat is the code that is stuck.

```asm
C0003221:  8B 47 6C     mov  eax,[edi+6Ch]    ; EDI = C159F068, a THCB
C000323C:  8D 50 04     lea  edx,[eax+4]
           8B 42 FC     mov  eax,[edx-4]      ; next at +0
           85 C0        test eax,eax
           74 09        jz   <exit>           ; exits ONLY on NULL
           85 48 08     test [eax+8],ecx      ; ecx = 80000082
C000324D:  EB F0        jmp  back
```

`C159F068`'s `+6Ch` is `C10CD89C`. `EAX` was observed revisiting `C0FCF51C`, `C0FCF0F8`,
`C0FCEB50`, `C0FCF120`, `C10CD860` — **so the chain is circular and a NULL-terminated walk over it
can never end.** A `CHAINWALK` hook was left in `386_dynarec.c` that follows `[tcb+6Ch]` with
repeat detection and prints where the cycle closes.

## ⭐ START HERE — the chain terminates, so that theory is dead too

```
CHAINWALK head [tcb+6Ch] = C10CD89C
CHAINWALK #00 C10CD89C next=00000000 +8=00000001
CHAINWALK terminated cleanly at NULL after 1 nodes
```

One node, `next = NULL`. The walk ends immediately, so `C0003xxx` is **not** the non-terminating
loop. **This is technique 90d's warning, repeated:** a heartbeat concentrating in one address range
means that code is HOT, not STUCK. `C0003221`-`C000324D` is a short leaf called over and over by an
outer loop; the outer loop is what spins.

**Start here next session, and do not propose a fifth structure first.** 90d already prescribes the
method and names the approach chain from the ring buffer:

```
C002BED7 -> C002EA48 -> C002CA56 -> C0001430..C0001444
```

1. Arm a raw undeduped ring (technique 49) that **excludes** `C0003100-C0003300` and
   `C0008E00-C0009100`, so it holds what runs BETWEEN visits to the hot leaf rather than the leaf
   itself. The first attempt at this failed because the counter was reset on leaving the range and
   never accumulated - the wedge cycles through *two* regions, so gate on "outside both".
2. That gives the outer loop's own addresses. Dump the code there live (technique 44) before
   reading anything into them.
3. Only then ask what it is iterating.

**Four theories were falsified tonight and every one came from naming a data structure that ought
to matter. The two results that stuck - the `THCB` signature and the empty lists - both came from
dumping bytes.** Dump first.
