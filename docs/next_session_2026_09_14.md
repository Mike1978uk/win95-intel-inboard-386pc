# Next session - 2026-09-14

## CLOSED: why ECP failed on the hardware

**ECP mode was never negotiated.** Writing `74h` to the ECR configures only the
host side of the cable; the bridge stays in compatibility mode until asked, and
an unasked bridge never acknowledges the forward handshake - so the FIFO took one
byte and never drained. The missing ECR waits we chased all week were a symptom.

The step is `SD120PPD.SYS` `0x2993`, called from `0x2A21`/`0x2A46` - outside the
port open (`0x2C42`), which is why reading only the open path missed it:

```
out ctrl, 0Ch / 04h        ; enter 1284
w0(0); w2(1); w2(4)        ; 0x24D1 - IDLE INTO SPP.  REQUIRED.
out ctrl, 0Ch
out data, 10h              ; extensibility byte: ECP
out ctrl, 06h              ; request
poll status bit 6 (nAck) LOW, budget 100h
out ctrl, 07h / 04h        ; complete
```

**Proven on the owner's 5160**: negotiation succeeded (status `B8`), the forward
address cycle drained in one iteration, the reverse wait was satisfied, and a
real byte came back agreeing with nibble. Without the idle-into-SPP the request
is ignored - status sits at `E0`, nAck never asserts.

Full detail and the before/after table: `drivers/imation_ls120/TRANSPORT_SPEC.md`
section 4g.

## Negotiation is now IN THE DRIVER, and deployed

`LS_Neg1284` / `LS_NegotiateEcp` in `LS120TR.ASM`, with the vendor's `0x2A21`
recovery-and-retry. Called from `LS_BringUp` (a refusal clears `LS_HasEcp`, so
the driver runs nibble rather than failing) and at the top of both ECP block
paths, before any pushes, so a refusal falls through to SPP for that block.
`LS_EcpNegStat` / `LS_EcpNegFail` record the last status byte and a failure
count - `E0` against `B8` is the whole of the evidence when it goes wrong.

**On the CF now**: `LS120MP.MPD` md5 `220be39e`, 8192 bytes, commit `d21d4a6`.
Previous binary remains as `LS120MP.B13` (`976e4114`, 09-11).

`LS120TR.ASM` had not assembled since `99ce277` (16:21 on 09-13) - `LS_WaitDrq`
and `LS_EcpWaitData` both defined `lwd_loop`/`lwd_ok`. Last good build before
that was 15:47. Fixed in `b48a13a`.

## CORRECTED, 2026-09-14: it is SILENT TRUNCATION, not corruption

The write stops early and DOS is never told. Everything below this heading was
written before that was understood - the "corruption" is stale medium content
read back from beyond the point the write stopped.

Proven on a **freshly formatted** NOS disk, vendor stack, our driver not in the
path:

| | |
|---|---|
| `COPY C:\WINDOWS\COMMAND\FC.EXE D:\FC.EXE` (20,494 bytes) | reported success |
| bytes 0 - 6,143 on `D:` | **byte-identical to source** |
| bytes 6,144 - end | **all zeros** - never written |

A formatted disk is zero-filled, so the tail is simply unwritten. On the
*previous*, used disk the same fault read back as high-entropy garbage because
the medium still held older data there. One mechanism, two appearances.

This retro-explains everything:

- The 427 KB file "diverged" at `0x4400` because the write **stopped** at 17,408.
- The owner's small file edits round-tripped clean because they fit under the
  stop point.
- It is the same family as the recorded open item *"a 30,720-byte write stalls
  on BOTH transports - size, not transport"*.

**The stop point VARIES**: 6,144 one run, 17,408 another - both multiples of
1,024 (two sectors). A timeout or FIFO back-pressure, not a fixed ceiling.

### The drive is probably NOT at fault

- A full `FORMAT` completed clean, no bad sectors - the whole surface written
  and verified.
- The bytes that do land are byte-exact.

A degraded optical servo mis-positions or garbles; it does not stop cleanly on a
sector boundary leaving perfect data behind it. **Do not open or clean the
drive on this evidence.** The track-boundary hypothesis below is superseded:
17,408 was where the write stopped, not where a seek went wrong.

The fault is in the **write transfer path** - driver, bridge, FIFO or XT timing.

## THE FINDING: the vendor stack does not round-trip data on this machine

Tested on **brand-new NOS media**, with the owner's correct switch line
(`/de /db /ni /sf /dpc /dp /fp`), our miniport not in the path at all.

| | |
|---|---|
| `COPY C:\COMMAND.COM D:\CMDTEST.BIN` | reported success |
| source CRC32, verified host-side off the CF | `879a379d` |
| what comes back off `D:` | `72d7d02c` |

Four independent routes return the same wrong bytes: `file_hash` on `D:`, DOS
`FC /B`, COMrade `file_read`, and `COPY` back to `C:` then hashed locally - the
last of which removes the LS-120 from the hashing step.

Also measured:

- **It tracks the MEDIUM, not the file.** A 427 KB file diverged at `0x4400` =
  17,408 = one full track (32 sec x 512 = 16,384) plus two sectors, on a drive
  reporting 963 Cyl / 8 Heads / **32 Sec/Trk**. `COMMAND.COM` is byte-identical
  at that same *file* offset and breaks elsewhere - because the two files start
  at different places on the disk. **Leading hypothesis: writes that cross a
  track boundary go wrong; writes inside one track do not.** That matches the
  owner's experience exactly - a small file edit round-tripped clean, everything
  past ~16 KB fails. A track crossing is a seek, and LS-120 positioning is an
  **optical servo**, so a degraded or dirty pickup produces this signature.
  Test: note the divergence LBA, not the file offset, and check whether it is a
  multiple of 32 sectors.
- **Not `/sf`.** The first corruption happened with `/sf` absent; restoring it
  changed nothing.
- **Not the recovered media.** New disk, same result. The recovered disk failed
  *loudly* (`Disk Write Error`); the new one fails *silently*.
- **Reads are self-consistent** across all four routes. Repeatable is not the
  same as correct, and every route goes through the vendor driver - so
  write-vs-read is **still open**. The only independent check is that disk in
  another machine's LS-120.

**This retires `SD120PPD.SYS` as a trustworthy reference for this hardware.**
The repo has called it "the one implementation known to work on the owner's
machine" in several places. It enumerates, reads geometry, lists directories and
reports successful copies while returning wrong bytes. Diffing our driver
against it can only reproduce its bug.

## Next, in order

1. **REM the two `SD120PPD` lines and boot Windows** with `220be39e`. Both
   09-13 boots logged `Init Failure` at **1598 / 1585** units against `Init
   Success` in **2** units on 09-09 - a driver spinning and timing out, not
   declining to start. The vendor driver owning `0x378` is the leading
   hypothesis and REMing it is the single-variable test.
2. Read `LS_EcpNegStat` / `LS_EcpNegFail` from that boot.
3. Format the NOS disk (a full format is a whole-surface write-and-verify) and
   repeat the copy. The corrupt `WC2P9XUP.EXE` and `CMDTEST.BIN` are evidence
   until then.
4. **Read the corrupt NOS disk in the owner's OTHER LS-120 machine.** The only
   independent path available, and it settles write-vs-read: corrupt there too
   = the 5160 wrote it wrong; clean there = the 5160's read path is the fault.
   No code, no build.
5. `LS120MP.NEW` and `LS120MP.B13` sit in `IOSUBSYS` and are both `976e4114`.
   Confirm IOS filters on `.MPD` before trusting a boot, or move them out.
6. 86Box models no 1284 negotiation, so the bed still cannot falsify any of
   this. `tools/fixtures/dosctrl/CONFIG.SYS` was missing `/sf` - fixed in
   `f4daf17`, but every `dos_vendor_*` log predates that fix.

## Machine state

DOS prompt, vendor stack loaded, NOS disk in `D:` holding two known-corrupt
files. `FC3` was aborted mid-run; `C:\FC1.TXT`, `C:\FC2.TXT`, `C:\FC3.TXT` and
`C:\TEMP\BACK.BIN` are scratch and can go. Repo clean. **Nothing pushed.**
