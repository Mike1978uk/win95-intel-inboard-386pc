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
