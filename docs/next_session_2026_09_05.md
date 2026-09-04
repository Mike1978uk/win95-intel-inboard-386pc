# ⭐ THE GOLDEN PATH — start here, in this order

Written at the close of 2026-09-04 after twelve emulator boots. **The shutdown hang is not fixed**,
but for the first time there is a route rather than a list of guesses. Follow it in order; each step
is cheaper than the one after it, and steps 0 and 1 cost no machine time at all.

## 🛑 THE RULE THIS SESSION PAID FOR — read every source you are given

**When someone hands us a source, read it and decide whether it is useful. Each one, at intake.**

@andrew-hoffman posted the *I/O Supervisor Guide* on 2026-09-03. It was downloaded to
`C:\IOSGuide\IOS_Guide.doc` the same day. On 2026-09-04 twelve emulator boots and most of a
session went into reverse-engineering, from binaries, the polling contract and the serialisation
shape **that document states in plain English and supplies as assembly source.** It was on disk
the whole time. I opened it once, grepped four tokens, got no hits, and wrote it off.

- A **keyword miss is not a document review.** Extract the text, print how much came out, and grep
  for the *concepts* before concluding anything.
- **Andrew has not been wrong yet.** The IOS punt, the `.INF` class, the vendor README, the
  RAM-granularity bug, the published XT I/O map, and now this. When he names a document, read it
  before writing code.
- The project already had this rule for vendor READMEs (technique 75, which cost two patch rounds
  on the LS-120). **It applies to every source, not just READMEs.** Add it at intake, with a note
  saying whether it was read and what it contained - including "nothing relevant", which is a
  result worth recording.

---

### Step 0 — read the Guide. FREE, and it has the code we need.

`C:\IOSGuide\IOS_Guide.doc` (Microsoft's *I/O Supervisor Guide*, from @andrew-hoffman, issue #21 —
NOT in the Win95 DDK). Extract with a printable-run scan in **latin1**, not UTF-16:

```python
import re; d=open('C:/IOSGuide/IOS_Guide.doc','rb').read()
s=re.sub(r'\s+',' ',b' '.join(re.findall(rb'[\x20-\x7e]{4,}',d)).decode('latin1'))
```

194,000 characters come out. Two places to read:

| char offset | what is there |
|---|---|
| **~56,500** | the polling procedure step by step, quoting **ESDI_506's own source** for `ILB_enqueue_iop` |
| **~59,900** | **full assembly source of `IOS_serialize` and `IOS_serialize_callback`** |

I dismissed this file earlier on four keyword misses. Do not repeat that: a keyword miss is not a
document review.

### Step 1 — `DCB_max_sg_elements`. One byte, one build, one boot.

`ESDI_506` sets it to `11h` (17) at `AEP_CONFIG_DCB`; **we never write it, so it stays 0** — while
scatter/gather requests demonstrably reach us (technique 85, the bug that wrote a descriptor list
into the root directory). Set it in `XTIDE_ConfigDcb` beside the `DCB_max_xfer_len` write that is
already there. Cheapest real change available.

### Step 2 — the one missing step in the polling contract.

**Do not restructure anything.** `PORTREQ.ASM` already enqueues with `ILB_enqueue_iop`, takes
`DDB_BF_ACTIVE_BIT` with `bts`/`jc`, returns without completing when busy, and drains with
`ILB_dequeue_iop` — the same shape `ESDI_506` uses. The gap is only *inside the wait*:
`XTIDE_WaitNotBusy` / `XTIDE_WaitDrq` busy-spin `XT_SPIN` = 400,000 `in al,dx` **while holding the
queue's lock**, where the Guide requires `Set_Global_Time_Out` then a return, with the polling
handler completing later and calling `ILB_dequeue_iop`.

`Port_iop_timeout` (`AEP_IOP_TIMEOUT`) is an empty stub answering `AEP_SUCCESS` — the natural home
for that handler. Use the Guide's `IOS_serialize` source; do not invent the shape.

**Bisect it.** First build proves IOS tolerates ONE deferred completion. Only restructure if it does.

### Do NOT re-derive these — six theories, all dead

| | how it died |
|---|---|
| VMM waits on thread list `C001080C` | walked at the wedge: **empty** |
| VMM waits on list `C0010C98` | walked at the wedge: **empty** |
| circular chain at `TCB+6Ch` | walked: terminates at NULL after one node |
| the published volume / appy-time event | `-NoVolume`: still wedges |
| the own-TSD work | **stripped from the image**, verified absent: still wedges |
| "we answer the teardown AEPs wrongly" | `ESDI_506` answers `AEP_FAILURE` to 21 and 16 and shuts down cleanly |

### The bed, and how to reproduce in ~15 minutes

`vm_xtcf_faithful/` — the real card's own image on `ibmxt_inboard386`, with 86Box modelling the
Lo-tech XT-CF (stride-2 decode, 8-bit PIO, the card's ROM). Restore `86box.cfg` from
`86box.cfg.master` every run, clear `C:\BOOTLOG.TXT`, deploy with `tools/fatcp.py` and **verify the
md5 at the destination**. Then boot, wait for the desktop, and shut down **by hand** — Start → Shut
Down → OK. Host chords never reach the guest.

### What a fixed run looks like

| signal | hangs | fixed |
|---|---|---|
| teardown of our DCB | `21 · 16 · <stop>` | `21 · 16 · 19 · 4` |
| `DESTROY_VRP` for `C0FD65C8` (drive 2 = `C:`) | absent | present |
| VRP create/destroy | 2 / 1 | 2 / 2 |
| 86Box | spins at `C0003xxx`/`C0008Exx`, `EDI` pinned to a THCB | reaches "safe to turn off" |

### Two rules this session paid for

- **Check the built artefact, never the source you wrote.** Two runs were wasted on changes that
  never reached the binary — an unreachable dispatch block, and a regex that rewrote a comment.
- **Dump bytes before naming a structure.** Every wrong turn came from proposing a data structure;
  the results that stuck (`THCB` signature, both lists empty) came from dumping memory.

---

# Next session â 2026-09-05

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
| clean (driver absent) | `C0FCE28C` â a VMM pointer | `C15200E8` | falls through; shutdown completes |
| hung (driver present) | **`00000000`** | `C15200E8` â **the same object** | spins forever |

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

Trace backwards from `C0001300`â`C0001354` and find what returns zero. It will be an object VMM
asks for at `System_Exit` that our calldown insert changes.

## The bed â this is the enabling piece, do not lose it

`vm_xtcf_faithful/` is the **first emulator bed that matches the real card**, and the hang only
reproduces there. It has your CF's own driver (`0fe2431a`) restored and ready.

86Box needed two fixes to model the card, both on `Mike1978uk/86Box` branch `xtcf-lotech-stride2`:

- **stride-2 decode** â the Lo-tech XT-CF rev 3 does not decode A0, so registers sit at `base+2N`
  over 32 ports. New `Register stride` option, default 1, so every existing config is unchanged.
- **8-bit PIO** â the card's BIOS sends `SET FEATURES 01h` and the data register becomes byte-wide
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

## Eliminated by measurement â do not re-run these

| candidate | how it died |
|---|---|
| the LS-120 real-mode chain (`SD120PPD.SYS`, `ASPIHDRM.SYS`) | REM'd on the CF, hardware still hangs. `IOS.LOG` disappeared entirely, so IOS's complaint and the hang are independent |
| the published logical volume | `-NoVolume` still hangs |
| claiming master+slave | `-ClaimMask 1` still hangs |
| the polling contract / `Set_Global_Time_Out` | the wait is ordinary VMM code a clean shutdown walks straight through |
| a timeout storm | 3,582 status polls where one expiry needs 400000 |
| the fast-fail clamp built 09-04 morning | never armed â `AEP_SYSTEM_SHUTDOWN` does not reach our handler |

## Two standing bugs found on the way, neither chased

1. **`AEP_SYSTEM_SHUTDOWN` never reaches `Port_Async_Request`** â proven by zero output from the
   `0E0h` counters. So `XTIDE_VolDown` has never run. `AEP_UNCONFIG_DCB` is tested first and
   returns early; that is the likely explanation and it is untested.
2. **The duplicate `D:` is solvable.** With `-NoVolume` the machine shows only `C:`, and Windows'
   own `DiskTSD` assigns it (your bootlog shows `INITCOMPLETE = DiskTSD`). The driver does not need
   to be its own TSD. That closes the single-disk goal independently of the hang.

## Provenance â the reason 2026-09-04 cost a day

A shutdown hang was chased for a day in a binary (`70298a8f`) that could not be rebuilt from any
commit: built from an uncommitted tree on 09-03, deployed straight into the emulator image, source
lost. `build.ps1` now prints the commit, shouts on a dirty tree, and appends md5 â provenance to
`drivers/xtide_pdr/build_ledger.tsv`. **Commit before deploying anything you will draw a
conclusion from.** Technique 89.

---

## Update, later on 2026-09-04 â two corrections and the current lead

### â  The "null handle" conclusion above is WRONG

Dumping the live bytes killed it. `C000318D`'s `jne -19` targets `C000317C`, which is
`dec eax / pop esi / ret` â **a failure return, not a loop head** â and at the wedge the compare is
*equal*, so the branch is not even taken. `C0003184` is a short leaf **called ~2,000,000 times**,
not a spin; every heartbeat landed there because it is hot. `EAX = 0` was that routine's own
`xor eax,eax` two instructions earlier. See technique 90d.

### What is actually cycling

A raw 256-entry ring, with both hot VMM regions excluded, gives the real loop â ~150 distinct
instructions across six routines, repeating:

```
C002D754 -> C002D48C -> C00039BC..C0003A81 -> C002D768 -> C002D796 -> C002BEC2 -> C002BED7
-> C002EA48 -> C002CA56 -> C0001430 -> C0001100 -> C00012C0 -> C0001370 -> C0001410..C000142A
-> C002BEF4 -> C002BEB6 -> C002D744 -> C002C8BC..C002C8E0 -> C002D74D -> C002D752 -> (repeat)
```

`C002xxxx` is a VxD loaded after VMM; `C0001xxx`/`C0003xxx` are VMM services it calls. So a VxD is
**polling** and never getting the answer it needs. It does real work each pass â this is not a
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
binary finds the **LE resident-name table**, not the DRP â and all four are byte-identical there.
The feature code lives in the data segment; a different approach is needed.

---

## THE LEAD, 2026-09-04 end of session â AEP census on the faithful bed

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
else falls to `pa_note_only` and is answered **`AEP_SUCCESS`** â "done" â having done nothing.

At shutdown that means we answer these blind:

| code | AEP | why it matters |
|---|---|---|
| **15** | **`AEP_UNINITIALIZE`** | counterpart of `AEP_INITIALIZE`. We create a DDB at init with `ISP_CREATE_DDB` and **nothing ever destroys it**. We claim to have released everything while still holding the DDB and the calldown. **Prime suspect.** |
| 16 | `AEP_DCB_LOCK` | unhandled |
| 21 | `AEP_PEND_UNCONFIG_DCB` | a *query* â "may I unconfigure this?" â answered yes, blind |

A driver that reports it has uninitialised and has not is exactly what would leave a VxD polling
forever for all drivers to release. That matches the observed cycle: a VxD at `C002xxxx` calling
VMM in a loop, our driver idle, all our I/O finished.

### Do this first

Handle `AEP_UNINITIALIZE`: destroy the DDB (`ISP_DEALLOC_DDB`, the sample already does this on the
init-failure path in `Port_initialize`), drop the calldown, clear our own state. Then re-run the
faithful bed. `AEP_DCB_LOCK` and `AEP_PEND_UNCONFIG_DCB` are the next two if that is not enough.

Note the sample's `pa_note_only` comment claims answering `AEP_SUCCESS` "is the right answer to all
of them". That is true for notifications and **false for `AEP_UNINITIALIZE`**, which is a command.

### â Tested and negative: handling `AEP_UNINITIALIZE` does not fix it

Built `4c124470` (`16ce19b`): on `AEP_UNINITIALIZE` call `ISP_DEALLOC_DDB` on the DDB created at
init and clear our reference, instead of answering `AEP_SUCCESS` blind. **Still hangs**, same VMM
cycle, same AEP sequence. So holding the DDB was not the cause. The handler is correct on its own
terms and has been kept.

**Next two, in order:** `AEP_DCB_LOCK` (16) and `AEP_PEND_UNCONFIG_DCB` (21) â the remaining
shutdown codes we answer `AEP_SUCCESS` blind. `PEND_UNCONFIG` is a *query*, so answering it wrongly
is the more interesting of the two.

Also still unexplained and worth a look before more guessing: at startup we receive
`PEND_UNCONFIG(21)` and `UNCONFIG(4)` **twice**, and our `AEP_UNCONFIG_DCB` handler calls
`XTIDE_ForgetDcb`, which clears the `XTIDE_InsertedDcb` slot. If the DCB is later reconfigured we
never re-record it, so `XTIDE_VolDown`'s unlink scan at shutdown may be looking at an empty table.
That is a state-tracking bug regardless of whether it causes the hang.

### The census asymmetry â 4 `CREATE_VRP`, 3 `DESTROY_VRP`

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

### â­ THE CORRELATION â CFU1 answers `AEP_FAILURE` where we answer `AEP_SUCCESS`

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

## â­â­ THE CLOSING FINDING, end of 2026-09-04 â VMM waits for a list we are still on

### First, one more negative

`c5b5b7f0` / commit `4ba1995` â answering `AEP_FAILURE` for undispatched codes and acknowledging
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

## CORRECTION 2026-09-04, later â `C1032D1C` is NOT inside our driver

Commit `8fe1609` says the queued node is *"inside our driver's own address range"*. That was
inferred from "our code runs at `C1031xxx`" and never from a computed base. It is wrong.

### The load base, established three independent ways

The binary in `vm_xtcf_faithful/prextide.img` is `c5b5b7f0`, which is exactly the build
`drivers/xtide_pdr/build/PORT.map` describes â so the map is the right one for this run.

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

And `C1030C54` â 18 of the same 107 writes â is `base - 0xEC`, i.e. just *before* our image.
Nodes on both sides of our module, with the rest at `C0FD*`, `C0FF*` and `C159F068`: that is the
signature of **heap blocks**, one allocated just before our module and one just after, not of our
own statics. Consistent with something allocated on our behalf while we were being loaded â which
is the `ISP_INSERT_CALLDOWN` suspicion above â but that is inference, not measurement.

### Free result: it is a wedge, not slow progress

Technique 88 carries an open question â *"nobody has recorded how long the machine was left"* â
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

`EAX` cycles `C0FCEB50 -> C0FCF120 -> C10CD860 -> C0FCF288 -> C0FCF120 ...` â it revisits nodes,
so the walk does not terminate. `EDI` is pinned at `C159F068` throughout, the most-written value
on the watched list (32 of 107), and `EBX` at `C15200E8`, the object named on 2026-09-04.

### What has no control yet

`run_control_clean.log` and `run_ctrl_calltrace.log` armed the watch on `C000E9F0-C000E9F7`, not
on this list. **So nothing yet shows what a clean shutdown does to `C0010810`** â whether it
drains, and who pops it. That is the comparative to run next if the instrumented run does not
name the popper on its own.

---

# SESSION CLOSE 2026-09-04 evening â read this before anything above it

Eleven emulator boots, each with a real shutdown driven by hand at the desktop. **The hang is not
fixed.** What follows is what is now known, what was falsified, and the one measurement left
running when the session ended.

## Retracted this session â do not act on these

| claim | where it came from | why it is dead |
|---|---|---|
| `C1032D1C` is an object inside our driver | commit `8fe1609` | our object 1 loads at `C1030D40`; that address is `base+0x1FDC`, past object 1's `0x1B88` and past the whole packed module. It is a heap neighbour |
| VMM waits for thread list `C001080C` to drain | commit `8fe1609` | walked it at the wedge: **0 nodes, `next==prev==sentinel`**. The `cmp/jne` at `C0008F8A` passes |
| VMM waits for list `C0010C98` | this session | walked it at the wedge: **also empty**. `C0008F40`'s `jz` exit is available |
| the published volume / appy-time event is the cause | this session | `-NoVolume` (`2f7c8f4e`) removes the publish, the logical DCB, the drive-letter association and the only `_SHELL_CallAtAppyTime` call. **Still wedges, identically** |

## Established, and worth not re-deriving

- **Every queued node carries `THCB` at +0Ch** â they are VMM Thread Control Blocks, and `+14h` is
  the System VM handle `C15200E8` on all of them (the value pinned in `EBX` through every wedge).
- **The orphan is named.** `CREATE_VRP` for VRP `C0FD65C8`, **drive 2 = `C:`**, issued right after
  `AEP_ASSOCIATE_DCB` on our DCB `C0FD6490`, and never destroyed. `A:` (`C0FD2C6C` â `C0FD2C38`) is
  created and destroyed on the same machine seconds apart. **Destroys report the VRP 0x34 below the
  create's pointer â pair them on that offset.**
- **IOS abandons our teardown after `AEP_DCB_LOCK`.** `A:` runs `21Â·16Â·19Â·4` all on one DCB. Ours
  runs `21` on `C0FD6490`, then `16` on a *different* object, then nothing. `SYSTEM_SHUTDOWN`
  follows and the machine wedges. Identical across all four driver fixes.
- **Windows is genuinely 32-bit on this disk.** After Windows starts: **5,039,676 XT-IDE accesses,
  every one from selector `0028`; zero from `D000`.** All real-mode INT 13h traffic is DOS boot.
  `RMM.PDR` loads and never reaches `INITCOMPLETE`; `PORT` does.
- **No I/O reaches us after the teardown starts** â zero accesses between the first teardown AEP and
  the wedge. IFSMGR never even attempts the unmount, which is why driver-side I/O fixes cannot help.
- **Thread `C159F068` has inconsistent links**: its `+4`/`+8` both point at sentinel `C0010C98`
  while that sentinel points at itself. A thread that believes it is on a list the list does not
  contain. First anomaly found in a VMM structure rather than in our answers.

## The four driver fixes made tonight â all correct, none causal, all kept

Each is a DDK contract we were breaking. Each changed the teardown by **not one AEP**.

1. **`AEP_PEND_UNCONFIG_DCB` (21) was answered `AEP_FAILURE`** by the catch-all. `STORAGE.DOC`: it
   is the *first* AEP sent when a DCB is destroyed and layers "are expected to stop and prevent all
   further input and output". Now dispatched and answered. (`eabb9be`)
2. **Everything but READ/WRITE/VERIFY returned `IORS_INVALID_COMMAND`**, `IOR_FLUSH_DRIVE`
   included. Flush, media-check, lock/unlock, cancel, clear/abort/restart queue, spin up/down and
   DOS reset are honest no-ops on an XT-CF, so they are answered. (`de58591`)
3. **Quiesce on code 21**, gating data movement only â quiescing the housekeeping commands would
   refuse the very flush that lets a volume go. (`19f9668`)
4. **Load group was the sample's `DRP_MISC_PD`, `'Generic Port Drv'`.** Read off the card's own
   binaries: `ESDI_506` = `ESDI_PD`, `SCSIPORT` = `NT_PD`, `HSFLOP` = `NEC_FLOPPY`. Ours was the
   only one in a non-disk band; our `DRP_BT_ESDI` bus type already matched `ESDI_506`. (`c5632a6`)

## Also done

- **Stride autodetect is now the default** (`405f5e4`). One binary covers a classic XTIDE at
  `base+N` and the XT-CF at `base+2N`. The probe reads Status and Alternate Status under each
  candidate and keeps the map where they agree; it writes nothing, so it cannot repeat the SRST
  accident a write-probe would cause. **â  It has NEVER executed on hardware** â the ledger has one
  `XT_STRIDE=0` build ever, `e8edf217`, built today and never deployed. Every 5160 binary is pinned
  stride 2. Its first boot is a test.
- **Upstream PR [#7858](https://github.com/86Box/86Box/pull/7858)** raised: XT-IDE logging was inert
  on the plain card because only `jride_init()` opened a log handle. Four lines. README updated.
- The XT-CF stride-2 / 8-bit PIO model **stays local** by decision â revisit upstreaming once the
  driver lands, when it can be submitted as "validated by a working 32-bit port driver".

## â The fifth theory, raised and killed in the same run â kept only as a worked example

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
`C0FCEB50`, `C0FCF120`, `C10CD860` â **so the chain is circular and a NULL-terminated walk over it
can never end.** A `CHAINWALK` hook was left in `386_dynarec.c` that follows `[tcb+6Ch]` with
repeat detection and prints where the cycle closes.

## â­ START HERE â the chain terminates, so that theory is dead too

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

---

## â­ Mining Microsoft's own port drivers, 2026-09-04 late â what a clean driver has that we don't

The owner's call, and the right one: *"we have something for a regular IDE disk and other drivers
that can be used as almost a template - all the answers of what we need has to be in them."*
`ESDI_506.PDR` does **exactly our job** and tears down cleanly on this machine seconds before we
fail to. No boots were spent on any of what follows.

Tooling now in the repo, because a scratchpad does not survive:
`tools/vxd_disasm.py` (handles `CD 20` + inline 4-byte service id, which desyncs every naive
disassembler), `tools/vxd_drp.py`, `tools/vxd_aep_audit.py`, `tools/vxd_isp_audit.py`.
Requires `capstone` (5.0.7 present).

### ESDI_506's entire AER â disassembled, not inferred

```asm
0611  mov  word ptr [ebx+2], 0        ; preset AEP_SUCCESS
0617  mov  ax, word ptr [ebx]         ; AEP_func
061A  cmp ax,2  je BOOT_COMPLETE      0624  cmp ax,0   je INITIALIZE
062E  cmp ax,0Fh je UNINITIALIZE      0638  cmp ax,6   je DEVICE_INQUIRY
0642  cmp ax,3  je CONFIG_DCB         064C  cmp ax,5   je IOP_TIMEOUT
0656  mov  word ptr [ebx+2], 0FFFFh   ; AEP_FAILURE to everything else
065C  ret
```

**Six codes. `AEP_FAILURE` for the rest â including `AEP_PEND_UNCONFIG_DCB` (21) and
`AEP_DCB_LOCK` (16).** `HSFLOP` dispatches six too, and also ignores 21. We dispatch fifteen.

**This retires the whole "we answer the teardown wrongly" family.** Windows' own IDE port driver
refuses exactly the codes tonight's fixes were written to answer, and shuts down cleanly. Four
builds went into that theory and every one changed the teardown by not a single AEP - which is
consistent, and now explained.

### The verified ISP service comparison

Counts below are **disassembly-verified**, not scanned - see the trap below.

| ISP service | ours | ESDI_506 | HSFLOP | SCSIPORT |
|---|---|---|---|---|
| `CREATE_DDB` | 1 | 1 | 1 | â |
| `INSERT_CALLDOWN` | 2 | 1 (+1 conditional, same packet reused) | 1 | â |
| **`CREATE_DCB`** | **1** | â | â | â |
| **`ASSOCIATE_DCB`** | **1** | â | â | â |
| **`DESTROY_DCB`** | **1** | â | â | â |
| `DEALLOC_DDB` | 2 | 1 | â | â |
| `CREATE_IOP` / `ALLOC_MEM` / `DEALLOC_MEM` | â | 2 / 1 / 1 | â | 1 / 1 / 3 |
| `GET_DCB` | â | â | 2 | â |
| `DEVICE_REMOVED` / `DEVICE_ARRIVED` | â | â | 1 / 1 | â |

**No Microsoft port driver creates, associates or destroys a DCB.** That is TSD work. We are the
only one doing it - the "be our own TSD" edifice from technique 83, whose premise (a dynamically
registered driver gets no TSD engagement) is already known to be false on the faithful bed, since
DiskTSD assigns `C:` on its own.

**Caveat that stops this being the hang:** in the `-NoVolume` configuration none of that code
executes, and it still wedges. Our *executed* ISP set there is `CREATE_DDB` + one
`INSERT_CALLDOWN` + `DEALLOC_DDB` - near-identical to ESDI_506's.

### The calldown packet, field by field

`ISP_insert_calldown` is `hdr(4), dcb, req, ddb, expan_len(w), flags(dd), lgn(b)`.

| field | ESDI_506 | ours |
|---|---|---|
| dcb / req / ddb / lgn | set, `lgn` from `AEP_lgn` | same |
| **expan_len** | **`24h`** - 36 bytes of per-IOP scratch | **`0`** |
| **flags** | `0`, or `DCB_dmd_phys_sgd` (`800h`) on a capability test | `DCB_dmd_small_memory` (`10h`), unconditional |

Both packets are well-formed, so an earlier worry that the sample left fields as stack garbage was
unfounded. `ISP_i_cd_flags` legitimately takes `DCB_dmd_*` values - `STORAGE.DOC`: *"a layer driver
that satisfies a specific demand stipulated in the DCB dmd flags must turn off the demand bit"*.

**The one difference that is at least coherent with a known contract gap:** per-IOP scratch is what
a driver needs to carry state across a **deferred** completion. ESDI_506 asks for 36 bytes; we ask
for none, because we complete inline. That is self-consistent with our design and different in kind
from how every Microsoft port driver is built - see technique 88's polling contract. **Not a proven
cause. Recorded as a design difference.**

### â  The trap that produced a false lead, and the constraint that kills it

A byte scan for `mov word ptr [reg+disp], imm16` reported `ISP_DISASSOCIATE_DCB` in both working
drivers and none in ours - a beautiful "start four, close three" story that was **completely
wrong**. Disassembling the site:

```asm
0000075F  mov word ptr [edi + 0x68], 0xf     ; a DCB field store. Not an ISP packet.
```

`0Fh` is `ISP_DISASSOCIATE_DCB` *and* an ordinary field value. Worse, the "verify it by checking an
indirect CALL follows" heuristic **also passed it**, because an unrelated call happened to be 44
bytes later.

**`ISP_func` is at offset 0 of the packet, so only a ZERO-displacement store can be one.** With
that constraint the false positive disappears and the table above is stable. `tools/vxd_isp_audit.py`
enforces it.

Generalise: technique 75 already says a raw byte scan is not evidence and each hit must be confirmed
by its idiom. **Add: confirm it against the STRUCTURE's own layout.** A field's offset is a hard
constraint that a plausibility heuristic is not.

---

## â­â­ THE GAP, 2026-09-04 â ESDI_506 returns without completing. We never do.

Read out of `ESDI_506.PDR`'s own request routine (the calldown target named in its
`ISP_insert_calldown` packet, `+8 = 284h`, i.e. object 1 + 284h = file 884h). No boots.

```asm
0884  mov  ebx,[esp+4]              ; the IOP
0888  mov  edi,[ebx+10h]            ; IOP -> DCB
088B  mov  edi,[edi+8]              ; -> the controller object
0891  cli
...                                 ; link this IOP into the controller's queue
08DD  bts  dword ptr [edi+2Eh], 9   ; TEST-AND-SET a busy bit
08E2  jb   98Ah                     ; already owned? -> branch away
08E8  xor  ebx,ebx
08EA  xchg dword ptr [edi+6Ah],ebx  ; atomically take the queue head

098A  sti
098B  ret                           ; <-- RETURNS WITHOUT COMPLETING
```

**A working Win9x port driver serialises the controller with a lock bit, queues the request, and
returns without touching `IOP_callback_ptr`.** The owner of the lock completes it later.

`XTIDE_StartRequest` has exactly one exit. Every path - success, invalid command, quiesced,
bad sector, I/O error - falls through to `xsr_complete`, which calls `IOP_callback_ptr` inline
before returning. **We have no queue, no lock, and no deferred path whatsoever.**

This is the I/O Supervisor Guide's contract, quoted in technique 88: *"Immediately after this
Set_Global_Time_Out call, simply return (WITHOUT doing a JMP to the IOP_callback_ptr routine).
This releases the system from your driver, so the system can run normally for a while."*

### Why this is different from the retracted version of the same idea

Technique 88 reached the polling contract from a binary that could not be rebuilt, and that
diagnosis was correctly retracted - the evidence was void, not the contract. It is back now for a
different reason: **it is what remains** after eliminating, by measurement or by construction:

- both VMM list-waits (`C001080C`, `C0010C98` - walked, both empty at the wedge)
- the per-thread chain at `TCB+6Ch` (walked, terminates at NULL after one node)
- the published volume and the appy-time event (`-NoVolume`, still wedges)
- **the entire own-TSD path** (stripped from the image, verified absent, still wedges)
- every AEP answer (ESDI_506 answers `AEP_FAILURE` to 21 and 16 and shuts down cleanly)
- the load group, the IOR command set, the DDB retention

Our ISP profile now matches ESDI_506's exactly: `CREATE_DDB`, `INSERT_CALLDOWN`, `DEALLOC_DDB`.
The remaining measured differences are all facets of one thing:

| | ESDI_506 | ours |
|---|---|---|
| VMM service calls | **48**, incl. `Set_Global_Time_Out` / `Set_Async_Time_Out` | **0** |
| calldown `expan_len` | `24h` - 36 bytes of per-IOP scratch | `0` |
| completion | queue + lock bit, return without completing | inline, always |

Per-IOP scratch is what a driver needs to carry state across a deferred completion. We request
none because we never defer. All three are the same design decision seen from three angles.

### It is still NOT proven, and here is what would prove it

A driver that holds the system inline works perfectly while the drive answers - which is why
3527/3527 IOPs complete and the desktop is fine. `System_Exit` is precisely where the scheduling
context an inline driver leans on is torn down. That fits, and "fits" has been wrong six times.

**Bisect it, do not build it.** The full restructure is large. The cheap first step is to prove
IOS tolerates a deferred completion at all: on ONE request, arm `Set_Global_Time_Out` and return
without completing, then complete from the timeout handler. If that boots, the model is viable and
the restructure is worth doing. If it hangs the boot, we have learned that for one build.

`HSFLOP.PDR` is the better template than `ESDI_506` for this machine: it is the driver that must
survive a device which may simply not answer, and technique 88 already records its idiom -
`Set_Global_Time_Out` at `2D95h` then `Wait_Semaphore` 43 bytes later at `2DBEh`, with a single
`Signal_Semaphore_No_Switch` elsewhere. Arm a timeout, block on a semaphore, let the timeout
handler signal it. **A port driver MAY block, as long as something asynchronous can free it** -
and that is a far smaller change than a full state machine.

### The DCB declaration gap â ESDI_506's CONFIG_DCB vs ours

Its handler starts at file `3617h` (`AEPHDR` is 12 bytes, so `AEP_d_c_dcb` is `[ebx+0Ch]`):

```asm
361A  mov  ecx,[ebx+0Ch]                   ; the DCB
3620  cmp  byte ptr [ecx+0BAh], 1          ; DCB_unit_on_ctl - master or slave
362C  mov  [edx], ecx                      ; link the DCB into the DDB
362E  or   dword ptr [ecx+20h], 0C0000000h ; DCB_device_flags, top two bits
3635  mov  dword ptr [ecx+50h], 0FFFFFFFFh ; DCB_max_xfer_len   = unlimited
3649  mov  byte ptr [ecx+77h], 11h         ; DCB_max_sg_elements = 17
```

Offsets resolved from `BLOCK/INC/DCB.INC`. **Careful with that file**: `DCB` embeds
`DCB_COMMON DB SIZE DCB_COMMON DUP (?)`, so a naive offset walk is short by `4Fh` for every
field after it - `DCB_COMMON` is `50h` bytes, not one.

| field | ESDI_506 | ours |
|---|---|---|
| `DCB_device_flags` | `or C0000000h` | set (`PHYSICAL`/`WRITEABLE`) |
| `DCB_max_xfer_len` | `FFFFFFFFh` | set |
| **`DCB_max_sg_elements`** | **`11h` (17)** | **never written - stays 0** |

**We declare support for zero scatter/gather elements and are handed scatter/gather lists anyway.**
Technique 85 is the record of that: `IORF_SCATTER_GATHER` requests do arrive, and misreading them is
what wrote a descriptor list into the volume's root directory. The read path never sees them because
VFAT's mount-time reads are single-buffer; its writes are scattered.

Not claimed as the shutdown cause. It is a verified declaration gap of exactly the class worth
hunting, it is one byte to fix, and it is testable in one build.

### â  CORRECTION, and the lead is BETTER for it â read Andrew's IOS Guide, it has the code

**I dismissed `C:\IOSGuide\IOS_Guide.doc` earlier this session as "a dead end for these codes".
That was wrong** - based on four exact tokens (`DCB_LOCK`, `CREATE_VRP`, `DESTROY_VRP`,
`PEND_UNCONFIG`) not appearing. A plain printable-run scan in latin1 extracts **194,000
characters**, including 488 mentions of `DCB`, 21 of `calldown`, 12 of `semaphore` and 8 of
`IOP_callback_ptr`. It contains what we spent the evening reverse-engineering from binaries.

What is actually in it:

1. **The polling procedure, step by step**, naming `ILB_enqueue_iop` / `ILB_dequeue_iop`, and
   quoting **ESDI_506's own source**: `cli` / `push esi` / `push ebx` /
   `call [esdi_ilb].ILB_enqueue_iop`. That matches the `bts`/queue/`ret` shape disassembled out of
   `ESDI_506.PDR` at `08DDh`/`098Ah`, so document and binary agree.
2. **Full assembly source of `IOS_serialize` and `IOS_serialize_callback`** - a working
   serialisation implementation, including the callback-stack insertion
   (`mov [edx.IOP_cb_address], offset32 IOS_serialize_callback` /
   `add [eax.IOP_callback_ptr], size IOP_callBack_entry`).
3. A semaphore worker-thread variant, described as a *"reportedly successful implementation of an
   IOS port driver"*: `CreateSemaphore` / `VWIN32_CreateRing0Thread` / `Wait_Semaphore` / dequeue /
   callback, with the IO handler doing enqueue-signal-return.

**And a correction to the gap itself.** I wrote that we "complete inline, always - no queue, no
lock, no deferred path". False. `PORTREQ.ASM` in our own build already does:

```asm
call XTIDE_WantIop                 ; ours?
call [port_ilb].ILB_enqueue_iop    ; queue it
bts  [edi].DDB_port_flags, DDB_BF_ACTIVE_BIT
jc   Port_rq_ret                   ; already active -> return, no completion
call Port_Start_Request            ; else drain the queue
...
call [Port_ilb].ILB_dequeue_iop
```

**The queue, the lock and the return-without-completing are all present, from the sample.** They
are the same shape ESDI_506 uses. What is missing is only the step *inside the wait*: the Guide
requires `Set_Global_Time_Out` and a return, with the polling handler finishing later. We instead
busy-wait `XT_SPIN` = 400,000 `in al,dx` **while holding `DDB_BF_ACTIVE_BIT`**, so the queue's lock
is held across the whole wait and the system is not released.

That is a far smaller change than the restructure feared in technique 88, and the Guide gives the
shape for it. `Port_iop_timeout` (`AEP_IOP_TIMEOUT`) is also still an empty stub that answers
`AEP_SUCCESS` having done nothing - it is the natural home for the polling handler.

**Method note worth keeping:** a keyword miss is not a document review. Extract the text, measure
how much came out, and grep for the *concepts* (`polling`, `calldown`, `semaphore`) before
concluding a primary source has nothing. This one had the answer for a day and a half.
