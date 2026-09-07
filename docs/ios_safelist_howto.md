# Whitelisting a real-mode driver in `IOS.INI` — how and why

A manual edit, made by hand on the guest, that **no installer performs and no patch in this repo
applies for you**. Every 32-bit storage driver this project ships depends on it. Written up
2026-09-07 because it had been referenced from six places and explained in none.

Original find: **@andrew-hoffman**, 2026-08-26 on
[issue #3](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/3), from
[this Dynabook support note](https://support.dynabook.com/support/viewContentDetail?contentId=108303).
He measured it in 86Box before it was ever tried here.

---

## The problem it solves

Windows 95's I/O Supervisor will not hand a disk to a 32-bit driver if something it does not
recognise has hooked `INT 13h` first. It has no way to tell a benign hook from one that is actively
moving data, so it assumes the worst and falls back to the real-mode BIOS for that unit — or, in
the ASPI case, for **every** unit on the machine.

On this project that unrecognised hooker is `INBRDPC.SYS`, the Intel Inboard 386/PC's own driver
from 1987, which is **required and not optional** — the machine does not run without it.

`WINDOWS\IOS.LOG` names the problem in as many words:

```
unrecognized hooker of INT 13h
```

and, in the worse variant that refuses miniports as a class:

```
Punting miniports because of unknown ASPI driver SD120PPD
```

`IOS.LOG` is rewritten every boot and does not exist at all when IOS is happy. Its **absence is the
pass condition**; do not go looking for a success message in it.

## What `INBRDPC.SYS` actually does to `INT 13h`

Worth knowing before you whitelist something, because "trust me" is not a reason.

Disassembled 2026-09-04 from the entry point `IOS.LOG` itself reports. The handler picks a
wait-state value depending on whether `DL` says hard disk or floppy, writes it to port `0x670` —
the Inboard's own speed/wait-state register — **only if it is slower than what is already
programmed**, chains to the original vector, and restores the normal speed on the way out.

It never touches `ES:BX`, never rewrites the request registers, never bounces or stages a buffer,
never remaps memory. **It is not in the data path.** Whitelisting it tells IOS something true.

The honest residual: it exists to slow the machine down for disk access, so a 32-bit driver doing
its own I/O bypasses that slowdown. That would show as timing flakiness under load, not silent
corruption. `386MAX.SYS` is on Microsoft's own stock `[SafeList]` for exactly the same reason —
a memory/speed manager that adjusts machine state around the call rather than touching the transfer.

---

## Doing it

On the guest, with any text editor that will not reformat the file — `EDIT.COM` is fine.

1. Open `C:\WINDOWS\IOS.INI`.
2. Find the `[SafeList]` section. It is a list of real-mode driver filenames, one per line, and it
   already has Microsoft's own entries in it (`386max.sys` and friends).
3. Add the driver's filename on its own line:

   ```
   [SafeList]
   ...
   inbrdpc.sys
   ```

   On this machine's card that line ended up at line 290. The position does not matter; being
   inside `[SafeList]` does.
4. Save and **reboot**. IOS reads this once at startup.

Use the name as it appears in `CONFIG.SYS`. Only the filename — no path, no `DEVICE=`.

### Verifying it took

Do not trust the edit; read what IOS did with it.

```
type C:\WINDOWS\IOS.LOG
```

- **File does not exist** → IOS has no complaints. This is the pass.
- **File exists** → read it. `unrecognized hooker of INT 13h` still naming your driver means the
  entry did not take: check the spelling, check it is under `[SafeList]` and not a neighbouring
  section, and check you rebooted.

Then confirm the thing you actually wanted, which is a driver *running*:

```
find "Init Success" C:\BOOTLOG.TXT
```

Delete `BOOTLOG.TXT` before the boot you intend to read. A stale one names a driver from two images
ago and reads exactly like a result — this has cost this project whole sessions (technique 74).

---

## The ASPI variant, which is a different mechanism

`Punting miniports because of unknown ASPI driver <NAME>` is **class-wide**: IOS declines every
miniport on the machine, not just the affected unit. On this machine it was `SD120PPD.SYS`, an
Imation parallel-port LS-120 driver with nothing to do with the disk or the SCSI chain, and while
it was loaded no 32-bit storage driver could ever have loaded — any test run before that point
failed for a reason unrelated to itself.

Two ways out, and this project has used both:

- **Whitelist it** in `[SafeList]`, as above. Keeps the real-mode driver working.
- **Remove it from `CONFIG.SYS`** if you do not need it. This is the current state on the real
  5160: `MA13B`, `TSLCDR`, `MODISK2` and `NASPIBUF` are all `REM`'d out, having been replaced by
  `T130.MPD` in protected mode.

Prefer whitelisting when the real-mode driver is still doing a job, and removal when a 32-bit
driver has taken that job over. Do **not** apply either to `INBRDPC.SYS` — it is required.

---

## Scope

This step is **not** Inboard-specific in general. Any machine whose `CONFIG.SYS` loads a real-mode
driver that hooks `INT 13h` and is not on Microsoft's stock list hits the same wall, which is why
it is a prerequisite in the `XTIDEMP.MPD` install notes for people who do not have an Inboard at
all. What *is* Inboard-specific is the particular entry: on a machine without `INBRDPC.SYS`, that
line is neither needed nor meaningful — but you may need a line naming whatever your machine loads
instead.

Measured consequences on this machine, and the caveat that goes with them, are on
[issue #17](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/17): units on real-mode
drivers went from six to one, but two variables changed in one boot and the whitelist's effect
alone was never isolated.
