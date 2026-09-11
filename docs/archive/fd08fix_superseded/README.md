# FD08FIX.COM — route `INT 13h AH=08h` for floppies to `INT 40h`

**Status: SUPERSEDED 2026-09-07 - #25 was fixed in hardware instead. NEVER EXECUTED.**

The real fix turned out to be **one DIP switch**: moving the Trantor T130B option ROM from
`CA000` to `DA000`, so Sergey's floppy BIOS is scanned before any fixed-disk ROM and claims
`INT 13h` under its own documented install rule. Verified end to end on DOS 6.22 and Windows
95 - both drives report correct geometry and read real media. See
[`../docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md`](../docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md).

This driver was removed from the machine (binary deleted, `IOS.INI` `[SafeList]` entry taken
out) because carrying an unused resident `INT 13h` hook is exactly what misleads a later
debugging session. **It is kept here as the documented alternative** for anyone whose option
ROM addresses cannot be reordered - the code is correct and the reasoning behind it stands.

Original status below.

**BUILT AND VERIFIED, NEVER EXECUTED.** Not on hardware, not in emulation. Do not put
this in `FIXES.md` or `dist/` until it has actually run.

Fixes issue **#25** — `INT 13h AH=08h` reports drive type 3 (720K, 80 cyl / 9 sect) for *both*
floppies on a machine with a 1.44 MB 3.5" and a 1.2 MB 5.25".

## Why this, and not something else

The machine already knows the right answer. Measured on the real 5160, same call, same boot:

| vector | A: | B: |
|---|---|---|
| `INT 13h` (what Windows asks) | `BL=03` 720K, `CX=4F09` | `BL=03` 720K, `CX=4F09` |
| `INT 40h` (Sergey's ROM, `D000:10D6`) | `BL=04` **1.44M**, `CX=4F12` | `BL=02` **1.2M**, `CX=4F0F` |

So this **forwards to measured-correct data**. It does not synthesise geometry, which is what
makes it different from the shim proposed in earlier plans.

It is also the *documented* arrangement rather than a workaround. AMIBIOS 98 Technical
Reference p.138, *INT 40h Revector for Floppy Functions*: with a hard disk present the floppy
service resides at `INT 40h` and **all** BIOS floppy functions are revectored there. Something in
the `INT 13h` chain answers `AH=08h` itself instead of forwarding; this restores the forwarding
for that one function.

Full evidence: [`../docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md`](../docs/evidence/int13_ah08_ROOT_CAUSE_2026-09-07.md).

## Scope — deliberately one function

| request | behaviour |
|---|---|
| `AH=08h`, `DL < 80h` | reissued on `INT 40h`, its result and flags returned |
| everything else | chained untouched — **all** hard-disk I/O included |

Reads and writes are not touched, and must not be: DOS already reads 1.44 MB disks, which a 1986
XT BIOS cannot do, so `AH=02h`/`03h` demonstrably reach Sergey's ROM already.

## Deploy

Last line of `AUTOEXEC.BAT`, after `IVT68FIX.COM`. Technique 38 — that timing is load-bearing for
`IVT68FIX` and this is the same class of fix.

## ⚠ Before the first boot: the IOS SafeList

A resident `INT 13h` hook makes the Windows 95 I/O Supervisor refuse **every** miniport unless it
is listed — see [`../docs/ios_safelist_howto.md`](../docs/ios_safelist_howto.md). **`XTIDEMP.MPD`
(#21) is in the blast radius**, and it is currently what serves `C:` in protected mode.

On the first boot after deploying, confirm in `BOOTLOG.TXT`:

```
Init Success xtidemp.mpd
rmm.pdr   Dynamic load success ... and NEVER reaches INITCOMPLETE
```

If `RMM` reaches `INITCOMPLETE`, IOS has punted and `C:` is back on the real-mode mapper. Remove
this and fix the SafeList entry before going further.

## Verification done so far

Assembled with **NASM 2.16.03** (`-f bin`), from <https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/>.
The built `.COM` was disassembled and checked against the source's intent instruction by
instruction — the resident handler, the chain-out pointer, and the 18-paragraph TSR size that
covers the resident region and discards the installer.

That verifies the binary matches the design. **It does not verify the design.** The remaining
assumption, stated plainly and unmeasured: that Windows takes the drive type from `AH=08h` at all.
It fits every symptom and it has not been proven.

`build_ledger.tsv` records md5, size, commit and toolchain, so a binary found later can be
identified — technique 89.

## v1 HUNG THE MACHINE AT BOOT, 2026-09-07 - cause, fix, and the deployment lesson

`FD08FIX.COM` v1 (md5 `3fa1523d...`) was put in `AUTOEXEC.BAT` and the machine **hung during
boot**. `AUTOEXEC.BAT` was restored from its backup and the machine boots normally again.

### The bug: `retf 2` returns with interrupts disabled

An `INT` clears IF. **`IRET` is what restores it**, from the FLAGS the `INT` pushed. v1 ended the
`AH=08h` path with `retf 2`, which *discards* that stacked FLAGS word - so the handler returned
with **IF still clear**. The first `AH=08h` during boot therefore killed the timer and the
keyboard, and everything stopped.

`retf 2` is a real idiom for interrupt handlers, which is why it looked right, but it is only safe
when the handler explicitly restores IF. It does not do so for free.

### The fix (v2, md5 `bfbc1b79...`)

Return via `IRET`, patching only CF into the caller's stacked FLAGS so IF and everything else
survive untouched:

```asm
        int     40h
        push    bp
        mov     bp, sp          ; [bp+6] = the caller's stacked FLAGS
        jc      .setcf
        and     word [bp+6], 0FFFEh
        jmp     short .done
.setcf: or      word [bp+6], 1
.done:  pop     bp
        iret
```

Registers are untouched between `int 40h` and the return, so `AH`/`CX`/`DX`/`ES:DI` reach the
caller as the diskette handler left them.

### The deployment lesson, which matters more than the bug

**Do not install an untested `INT 13h` hook from `AUTOEXEC.BAT`.** The failure mode is an
unbootable machine that needs the card pulled and put in a reader to recover.

The equivalent test costs a single reboot: leave `AUTOEXEC.BAT` alone, boot normally, start
COMrade, and **run the `.COM` by hand from the prompt**. A hang then costs a power cycle and
nothing else, because the next boot is clean by construction.

Only wire it into `AUTOEXEC.BAT` once it has survived that. The `[SafeList]` entry can be added
early - it is inert unless the driver actually loads.

**Current state: `C:\FD08FIX.COM` on the card is v2. `AUTOEXEC.BAT` is clean and does NOT call
it. The `IOS.INI` `[SafeList]` entry is in place and harmless.**

## CORRECTION: the boot hang was NOT the `retf 2` bug

The section above diagnoses the v1 boot hang as `retf 2` leaving IF clear, and states it as
settled. **That was wrong**, and it was asserted without ever being measured.

v2 (with the IRET fix) was run by hand from the prompt on 2026-09-07 and **wedged the shell
immediately, printing nothing**. COMrade stayed responsive at 25 ms RTT, so interrupts were fine -
which by itself disproves the IF theory.

### The real defect: no entry-point jump

**A `.COM` begins executing at `0100h`.** v1 and v2 both put the resident *handler* there, so DOS
ran the interrupt handler as the program's entry point. `AH` was not `08h`, so it took the chain
path - `jmp far [cs:old13]` - while `old13` still held the file's zero bytes. A far jump to
`0000:0000`, executing the interrupt vector table as code.

The machine died **before the handler was ever installed**, which is why no banner appeared and
why the `retf 2` flaw could not have been reached. Fixed in v3 (md5 `fedab2c2cfade59beba1d2880f736c41`) with a
`jmp install` as the first instruction; the handler now sits at `0102h` and the installer points
the vector there.

The `retf 2` -> `IRET` change is still correct and stays in - it was a real latent bug that would
have surfaced on the first `AH=08h` once the driver actually loaded. It simply was not this one.

### What this cost, and the rule it earns

Two hangs, both from a defect that **disassembling the binary could not catch**, because the
binary was a faithful rendering of a wrong layout. Every verification run was of the code, and
none of them asked *what does DOS do with this file when it loads it*.

**For any new `.COM`, check the first instruction at `0100h` before anything else.** If it is not
a jump to the setup code, nothing after it matters. That check takes one line of disassembly and
would have caught this before it ever reached the machine.

And the diagnosis rule, which this project already has as technique 81 and which I broke:
**a fix credited without the failure being reproduced is a guess.** The IF explanation fitted the
symptom, so it was written up as the cause. Reproducing it once would have shown the banner never
printed, and that single observation rules out every theory downstream of "the handler ran".
