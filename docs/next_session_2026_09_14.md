# Next session - 2026-09-14

Rewritten at the end of the 09-13 session. Earlier revisions layered corrections
on top of a wrong conclusion; this is the settled version.

## Start here

**Writes to the LS-120 truncate silently.** They do not corrupt. The drive is
probably fine. The next three things to do are all one word in `CONFIG.SYS`
followed by a copy, driven over COMrade - no builds, no hardware surgery.

## Machine state, physically

- CF is **in the 5160**. `CONFIG.SYS` has `/sf` restored and the two `SD120PPD`
  lines **active**.
- A **freshly formatted NOS LS-120 disk is in the drive**. It holds one file:
  `D:\FC.EXE`, 20,494 bytes, of which only the **first 6,144 are real** and the
  rest is unwritten zeros. **Keep it - it is the minimal reproducer.**
- `LS120MP.MPD` on the card is **`220be39e`, 8192 bytes, commit `d21d4a6`** - the
  first binary with both the ECR waits and the ECP negotiation. Previous binary
  is `LS120MP.B13` (`976e4114`, 09-11).
- Scratch to delete whenever: `C:\FC1.TXT`, `C:\FC2.TXT`, `C:\FC3.TXT`,
  `C:\TEMP\BACK.BIN`.
- Repo clean. **Nothing pushed.**

Reference CRC32s, verified host-side off the card:

| file | bytes | CRC32 |
|---|---|---|
| `C:\WINDOWS\COMMAND\FC.EXE` | 20,494 | `8fae6b02` |
| `C:\COMMAND.COM` | 92,870 | `879a379d` |

## CLOSED: why ECP failed, and it is now in the driver

**ECP mode was never negotiated.** Writing `74h` to the ECR configures only the
host side of the cable; the bridge stays in compatibility mode until asked, and
an unasked bridge never acknowledges the forward handshake - so the FIFO took one
byte and never drained. Every missing-ECR-wait theory was a symptom.

The step is `SD120PPD.SYS` `0x2993`, called from `0x2A21`/`0x2A46` - outside the
port open, which is why reading only the open path missed it:

```
out ctrl, 0Ch / 04h        ; enter 1284
w0(0); w2(1); w2(4)        ; 0x24D1 - IDLE INTO SPP.  REQUIRED.
out ctrl, 0Ch
out data, 10h              ; extensibility byte: ECP
out ctrl, 06h              ; request
poll status bit 6 (nAck) LOW, budget 100h
out ctrl, 07h / 04h        ; complete
```

Without the idle-into-SPP the request is ignored - status sits at `E0`, nAck
never asserts. With it, status reads `B8` and the forward cycle drains in one
iteration. Detail: `drivers/imation_ls120/TRANSPORT_SPEC.md` section 4g.

Shipped as `LS_Neg1284` / `LS_NegotiateEcp` in `LS120TR.ASM`, with the vendor's
`0x2A21` recovery-and-retry. Called from `LS_BringUp` (a refusal clears
`LS_HasEcp`, so the driver runs nibble rather than failing) and at the top of
both ECP block paths, before any pushes, so a refusal falls through to SPP for
that block. `LS_EcpNegStat` / `LS_EcpNegFail` hold the last status byte and a
failure count.

Also fixed: `LS120TR.ASM` had not assembled since `99ce277` (16:21 on 09-13) -
`LS_WaitDrq` and `LS_EcpWaitData` both defined `lwd_loop`/`lwd_ok`. Nothing
built between then and `b48a13a`.

## THE FINDING: silent truncation

Proven on a **freshly formatted** NOS disk, vendor stack, our driver not in the
path at all:

| | |
|---|---|
| `COPY C:\WINDOWS\COMMAND\FC.EXE D:\FC.EXE` | reported success |
| bytes 0 - 6,143 on `D:` | **byte-identical to source** |
| bytes 6,144 - end | **all zeros** - never written |

A formatted disk is zero-filled, so the tail is simply unwritten. On the
*previous*, used disk the same fault read back as high-entropy garbage, because
the medium still held older data past the stop point. **One mechanism, two
appearances** - and the second is what made it look like corruption.

This retro-explains everything from that day:

- The 427 KB file "diverged" at `0x4400` because the write **stopped** at 17,408.
- Small file edits round-tripped clean because they fit under the stop point.
- Same family as the recorded open item "a 30,720-byte write stalls on BOTH
  transports - size, not transport".

**The stop point VARIES**: 6,144 one run, 17,408 another - both multiples of
1,024 (two sectors). A timeout or FIFO back-pressure, not a fixed ceiling.

### The drive is NOT indicted

- A full `FORMAT` completed clean, no bad sectors - whole surface written and
  verified.
- The bytes that do land are byte-exact.

A degraded optical servo mis-positions or garbles; it does not stop cleanly on a
sector boundary leaving perfect data behind it. **Do not open or clean the drive
on this evidence.**

The fault is in the **write transfer path** - driver, bridge, FIFO or XT timing.

## Next, in order - all three are CONFIG.SYS edits and a copy

1. **Add `/di`** - "operate in polled mode". The current line omits it, so the
   vendor driver uses **IRQ 7**, which on this 8259 is also the spurious-interrupt
   vector, on a PIC measured to alias across `0x20-0x3F`. A missed or spurious
   interrupt mid-write gives exactly this signature: stops early, stop point
   varies, nobody is told. **Most XT-shaped of the three.**
2. **Add `/w0`** - slowest write timing rung. The Inboard is a fast CPU on a slow
   8 MHz bus, and a delay loop calibrated against CPU speed comes out too short.
   The fault is write-only, so `/w0` before `/r0`.
3. **Copy files of several known sizes** and record where each stops. Stop point
   tracking BYTES = FIFO/back-pressure; tracking ELAPSED TIME = a timeout.

`ECP=0` - which suppresses the DMA transfer path that writes `0x22`/`0x23` - is
the fallback if none of the three moves it.

Method for each: edit, reboot, `COPY`, `file_hash` both sides, and if it differs
find the stop point with `file_read` at a few offsets. Do **not** use `FC /B` -
it takes an hour on a mismatch and floods the console. COMrade goes deaf during
LS-120 transfers, so expect timeouts while one is in flight; out-of-band ops
sneak through between them.

## Then: profile the machine's limits INTO the emulator

The bed completes instantly, never goes BSY and models no negotiation, so it
**passes every case the hardware fails** and cannot falsify anything. Once the
stop point is characterised, model it in `lpt_epat.c`: bus pacing, drive busy
time, whatever actually gates the transfer. The bed then reproduces the bug,
which makes it a regression test and the upstream model honest. An emulator that
cannot reproduce a known hardware failure is a bug in its own right.

## DESIGN RULE: calibrate, do not hardcode an XT profile

Tempting to bake this machine's numbers in. Do not - it narrows the driver to our
own remit and repeats the vendor's mistake in a new place. The vendor's answer
was a hand-picked switch table (`/rx` 0-11, `/wy` 0-4, `/di`), and it fails here
because nobody chose a rung for a 386 accelerator in a 1983 chassis.

**Measure at init instead.** Write a known pattern, read it back, settle on the
largest chunk that survives on whatever machine the driver loaded on. An XT lands
somewhere small and paced; a later machine lands on the fast path. One binary, no
switches - and strictly better than the table the vendor shipped.

Two properties worth keeping whatever the bus: **poll rather than trust IRQ 7**,
and **read back what we wrote** while the transport is unproven. Silent
truncation is data loss on any machine; it is only rarer elsewhere.

## Then, the driver work

4. **REM the two `SD120PPD` lines and boot Windows** with `220be39e`. Both 09-13
   boots logged `Init Failure` at **1598 / 1585** units against `Init Success` in
   **2** units on 09-09 - a driver spinning and timing out, not declining to
   start. The vendor driver owning `0x378` is the leading hypothesis and REMing it
   is the single-variable test.
5. Read `LS_EcpNegStat` / `LS_EcpNegFail` from that boot. `E0` against `B8`
   distinguishes "the bridge ignored the request" from "it refused".
6. `LS120MP.NEW` and `LS120MP.B13` sit in `IOSUBSYS` and are both `976e4114`.
   Confirm IOS filters strictly on `.MPD` before trusting a boot, or move them.

## The bed, and why it is worth more now

`SD120PPD.SYS` is **retired as a reference for this hardware**. The repo calls it
"the one implementation known to work on the owner's machine" in several places;
it enumerates, reads geometry, lists directories and reports successful copies
while silently truncating writes. Diffing our driver against it can only
reproduce its bug.

That raises the value of the bed rather than lowering it. `lpt-epat-bridge` is
~24 commits and ~1,900 lines - `lpt_epat.c` (1,271), `lpt.c`, `rdisk.c`, the
headers - and is a parallel-port LS-120 for 86Box. Nothing upstream models
parallel-port ATAPI, and EPAT is one of the Linux `paride` family, so CD-ROM and
Zip sit behind the same bridge. It is a device class, not one drive.

Before it is submittable:

- **No 1284 negotiation modelled.** We now have the sequence byte-for-byte, so
  this is ready to write rather than research.
- **The vendor driver does not survive it** - `[0117:0000B929] Illegal
  instruction` after the CPP chain scan. That run used a build predating the last
  two branch commits *and* the fixture with the wrong switch line (fixed in
  `f4daf17`). **Owed one re-run** with the current exe before any conclusion.
- **Writes never exercised** - `PHASE_DATA_OUT` has never run in the bed.
- **Never goes BSY** - hardware returns `80` after a command, the model `40`.

For a PR, cut out `386_dynarec.c`, `mem.c`, `inboard386.c` and `hdc_xtide.c` -
diagnostics and unrelated features. Upstream is `lpt_epat.c`, `lpt.c`, `rdisk.c`,
the two headers, `config.c`, `CMakeLists.txt`.

## Corrections made on 09-13, so they do not resurface

- **"The LS-120 writes corrupt data"** - no, they truncate. Corrected same night.
- **"17,408 is a track boundary"** - no, it is where the write stopped.
- **"No `Init Failure` on hardware"** - wrong. Both 09-13 boots failed. The memory
  index carried that retraction; it has been re-retracted.
- **"`LS120TR.ASM` never assembled since the ECR waits"** - too strong. It built
  fine until `99ce277` at 16:21.
- **The bed fixture's switch line** was not the owner's - `/sf` was missing, so
  every `dos_vendor_*` log predates `f4daf17` and cannot be compared to hardware.
