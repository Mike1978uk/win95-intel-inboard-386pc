# Next session - 2026-09-15

`next_session_2026_09_14.md` is still current for the **truncation** work — the
reproducer, the CRC32s, the `/di` / `/w0` / size-sweep order. Nothing here
replaces it. This is the driver-design session that ran alongside it.

## Start here

**A disconnected drive used to stop Windows reaching the desktop. It was ours,
it is found, and the fix is built but has never run.** One boot answers it, and
the drive is already unplugged for the test that matters most.

## Machine state

- CF in the 5160, COMrade up at the Win95 DOS prompt.
- **The NOS disk formatted by the vendor DOS driver is in the drive**, holding
  the truncation reproducer `D:\FC.EXE` — 20,494 bytes, first 6,144 real. Keep it.
- `LS120MP.MPD` on the card is still **`220be39e`** (commit `d21d4a6`). The new
  binary has **not** been deployed.
- New build: **`9d89bcc3`**, 8192 bytes, commit `560e9fd`, **tree clean**,
  `-Phase 2`. Sitting in `drivers/imation_ls120_mpd/build/`.
- Repo clean, nothing pushed.

## What the owner found, and what it was

Booting with the drive unplugged sat forever and released the moment the drive
was plugged back in. Three faults compounding, all ours:

1. **`LS_RegRead` ended in an unconditional `clc`.** It could not report a failed
   read, so three `jc` guards that looked like they handled a dead bus were dead
   code. With nothing on the cable the SPP status port floats high, both nibbles
   read `0Fh`, and the routine returned **`FFh` reported as a good read**. `FFh`
   has BSY set — so an empty cable looked exactly like a busy drive.
2. **The SRST wait was `mov ecx, 0FFFFh`** — the only loop in the transport with
   no derived bound. At 46 µs per nibble read (8 accesses x 5.77 µs, technique
   109e) that is **3.0 s inside one timer callback**.
3. **`LS_TICKS_COLD = 3000` assumed a tick was a millisecond.** 3000 x 3.0 s is
   **~2.5 hours per SRB**, and SCSIPORT issues one per target.

The comment claimed "3 s to open a bridge that is present". It was ~276 s even
with a sane step, and unbounded in practice.

## What changed

Built and listing-verified (branch targets checked in the emitted bytes,
technique 82) — **not run anywhere**.

- `LS_StatusRead`: reads ATA status, CF=1 on `00h`/`FFh`. The judgement is here
  and not in `LS_RegRead` because `00h` is a legitimate byte count and a
  legitimate error register; for *status* both values are impossible.
- All five status waits go through it. `LS_PfWait` gained the bail-out `pf.c`
  has no need for — its bus is a cable, not a bridge that may be absent.
- `LS_SPIN_RESET = 2000` (~92 ms) replaces `0FFFFh`; `LS_TICKS_COLD` 3000 → 30,
  so the cold start is the ~2.8 s it always claimed to be.
- `LS_PortOpen` split out of `LS_ColdStart`, and **`LS_BridgeProbe`** reads the
  EPAT's own version register (write `38h` to bridge reg `0Ah`, read `0Bh`).
- `LsFindAdapter` returns **`SP_RETURN_NOT_FOUND`** when three attempts get no
  answer. Under 1 ms if nothing is there.

`DESIGN.md` is new and holds the invariants. `IMPLEMENTATION.md` stays the build
spec and now says which is which.

## Next, in order

1. **Deploy `9d89bcc3` and boot with the drive still unplugged.** This is the
   whole test, and the machine is already in the right state for it. Expect a
   desktop at normal speed and no LS-120 in Device Manager.
2. **Plug the drive in and boot again.** The regression that matters: the probe
   must not decline a bridge that is actually there. If it does, the driver will
   not load at all — worse than the bug it fixes. `LS_BridgeProbe` uses the full
   `LS_PortOpen`, not a bare CPP connect, precisely to avoid this, but that
   reasoning has not met hardware.
3. **Read `dxBridgeVer`** once it loads. Linux prints this value and never
   compares it, so we have no constant for our EPAT. Capture it and write it
   down — presence only needs "not `00h`, not `FFh`", but a known value turns a
   liveness check into an identity check.
4. Then back to the truncation order in `next_session_2026_09_14.md`.

## The thing worth taking to the bed

`epat.c:274-281` — **the EPAT has a built-in test-pattern generator.** Write
`13h`=1, `13h`=0, `0Ah`=`11h` to the *bridge's own* registers, then read 512
bytes: they come back `k, 0FFh-k`.

It is in the bridge, so it verifies a block read **with no drive attached and no
media**. That is the first test we have ever had that separates the transport
from the device, it discharges technique 111b item 3 off the bench, and it is a
much smaller first milestone for `lpt-epat-bridge` than a full ATAPI model — the
bed could reproduce a transport fault instead of passing every case.

**Unverified on our bridge.** And the drive is unplugged right now, which is
exactly the configuration it wants.

## Not done

- The SRST wait is bounded but still lives inside `LS_ColdStart`. Per `DESIGN.md`
  I3 it should be a state in `LsTimer`; the budget accounts for it meanwhile.
- `MaximumTransferLength` is still a build constant, not taken from the
  negotiated transport (`DESIGN.md` change 6). At 4096 bytes the nibble fallback
  is ~94 ms for one transfer.
- The phase-0 build (`build.ps1` with no `-Phase`) does not link: `LS_ModeReq` is
  `extrn`'d outside the `LS_PHASE2` guard. Pre-existing, and phase 0 is retired,
  but it means the default invocation of `build.ps1` fails. **Use `-Phase 2`.**
