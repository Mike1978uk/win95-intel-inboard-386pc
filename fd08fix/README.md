# FD08FIX.COM — route `INT 13h AH=08h` for floppies to `INT 40h`

**Status: BUILT AND VERIFIED, NEVER EXECUTED.** Not on hardware, not in emulation. Do not put
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
