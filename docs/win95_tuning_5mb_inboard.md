# Tuning Windows 95 OSR1 on the Inboard 386/PC (5 MB RAM, CF disk)

Scope: **Windows-side settings only.** `CONFIG.SYS` and `AUTOEXEC.BAT` are deliberately out of
scope - `INBRDPC.SYS`, `NODIAGS` and the CPU-control line are load-bearing on this machine and
are not tuning knobs (see `memory/feedback_inbrdpc_required_not_optional.md`).

Machine as tested: IBM 5160 + Intel Inboard 386/PC, IBM 486BL3 with cache enabled from
`CTCHIP34` in `AUTOEXEC.BAT` (issue #9), 5 MB total (1 MB base + 4 MB piggyback), CF card via
the T130B, ~1480 MB free.

---

## The two settings already changed

### 1. Fixed 32 MB swap file - **keep it**

Correct call, and for a better reason than the one usually given.

The common argument is "a fixed file protects the CF card from write wear". That part is
mostly wrong: with `min = max` the file is allocated **once**, and after that the number of
bytes written to it is set by paging pressure, not by the file's size. A 32 MB file and a
16 MB file that both hold the working set incur the same write volume.

What a fixed size actually buys you is worth having anyway:

- **Contiguity.** Windows allocates the whole extent at creation. Defragment first, then set
  it, and `WIN386.SWP` is one run - which matters on a CF card only for seek-free sequential
  reads, but matters a lot on the FAT chain walk.
- **No grow/shrink storms.** A dynamic swapfile on a 5 MB machine resizes constantly, and each
  resize is a metadata write plus a stall.

On sizing: Google's suggestion of 15-20 MB is too small, not too large. Windows 95 OSR1 with
5 MB of RAM will page essentially every non-resident code path; the swapfile has to hold the
whole working set of the shell plus one application. 32 MB is the right order of magnitude and
costs 2% of the free space. **No change needed.**

### 2. "Typical role of this machine" = *Network server* - **revert to Desktop computer**

This one is a 1990s magazine tip aimed at 16-32 MB machines, and it does not do what it is
usually claimed to do. In Windows 95 the role setting changes exactly two things - the path
cache and the name cache:

| Role | PathCache entries | NameCache |
|---|---|---|
| Mobile or docking system | 16 | 4 KB |
| **Desktop computer** (default) | 32 | 8 KB |
| Network server | 64 | 16 KB |

It does **not** enlarge VCACHE, which is what people think they are buying. What it does buy
is 8 KB more locked, non-pageable memory and a larger structure to manage, on a machine with
5 MB. That is a small loss rather than a disaster - so this is a tidy-up, not an emergency -
but it is a loss, and there is no offsetting gain on a single-user machine with one disk.

**Set it back to "Desktop computer".**

---

## The setting that is actually worth making: cap VCACHE

This is the big lever on a 5 MB machine and it is the one the usual advice misses.

VCACHE sizes itself from installed RAM and will take a share that a 5 MB system cannot spare.
It then competes with the working set: pages get evicted to the swapfile to make room for a
disk cache that is caching the swapfile. Capping it is the single highest-value change here.

In `SYSTEM.INI`:

```ini
[vcache]
MinFileCache=512
MaxFileCache=1024
```

(Values in KB.) Start at 1024; if the machine still thrashes, try 512/512.

## Second: tell the pager to exhaust RAM before touching the disk

`ConservativeSwapfileUsage=1` is correct advice and Google is right about it. It stops Windows
proactively writing idle pages out while physical RAM is still free - which on a normal machine
is a latency win and on this one is pure waste, because the disk is a CF card behind a polling
SCSI controller.

In `SYSTEM.INI`:

```ini
[386Enh]
ConservativeSwapfileUsage=1
```

---

## Check before tuning anything else: is the disk in MS-DOS Compatibility Mode?

**Control Panel → System → Performance**, top of the page.

If it says *"Drive C: is using MS-DOS compatibility mode file system"*, stop tuning and fix that
first - it dwarfs every setting above. In compatibility mode every disk access, **including every
page fault against the swapfile**, leaves protected mode, runs the real-mode INT 13h handler in
V86, and comes back. On a 16 MHz 386-class part that is the whole performance budget.

The T130B is configured here with no IRQ (polling) and `bios_addr = 0`, which is exactly the
shape of a controller Windows may decline to drive in protected mode. Worth confirming which
mode it lands in; that answer changes what is worth doing next.

## Free wins that cost nothing

- Wallpaper **None** (a 640x480x16 bitmap is ~150 KB resident), pattern None.
- Screen saver **None**.
- Trim Startup group and tray applets - each is a full process.
- Defragment **before** creating the fixed swapfile, not after.

## One thing to look at but not change yet

If `SMARTDRV` is loaded in `AUTOEXEC.BAT` it is double-caching against VCACHE and holding XMS
that Windows wants. That is a real cost - but `AUTOEXEC.BAT` is out of scope on this machine by
policy, so record it, do not change it.
