# ECP for the LS-120 — the plan, rewritten 2026-09-13

ECP is the deliverable. SPP stays as the fallback, because that is what the vendor ships and
what a machine without an ECP port needs. Both, not one.

## Why the previous approach failed

It was built in the wrong order: implement → test in the bed → infer. That produced an ECP
transport which passes in emulation and **fails on the real machine** (`Init Failure`, 1598 log
units, against `Init Success` in 225 for the pre-ECP build).

Two root causes, both now understood:

1. **Our ECP read waits for nothing.** The vendor gates every phase on the ECR. A bare
   `rep insb` reads before the bridge has data.
2. **The bed cannot tell us that.** Its bridge answers `ecp_read_data` on demand, so the FIFO is
   never legitimately empty and an unwaited read always succeeds. The model was shaped around
   our driver instead of the protocol, so it validated our own mistake back to us.

There is also a wrong claim in our own spec — §4f said per-register ECP "times out by design".
The mode tables say otherwise: `ECP Read` is READ selector 13 and `ECP Write` is WRITE selector
6, and **both take a register number**. Per-register ECP is the vendor's normal mode here.
Corrected in `drivers/imation_ls120/TRANSPORT_SPEC.md`.

## The order, and nothing starts before the step above it finishes

### 1. MEASURE — one ECP probe on the real machine

**We have never run an ECP sequence on the hardware.** Every COMrade probe to date
(2026-09-08, 2026-09-11: connect, INQUIRY, READ(10)) was nibble/SPP. So the ECR's behaviour on
the owner's actual card is assumed, not known.

Replay the vendor's ECP register read (`SD120PPD.SYS` rva `3CCEh`) for one known register — ATA
status, `cont 18h + 7 = 1Fh`, which reads `50h` on a healthy idle drive — and record the ECR at
each phase:

```
out ctrl(37Ah), 04h            ; forward
out ECR(77Ah),  74h            ; ECP mode
read ECR  -> expect bit0 = 1   (FIFO empty)
out data(378h), 1Fh            ; register number, ECP address cycle
read ECR  -> expect bit0 = 1
out ECR,  34h ; out ctrl, 20h ; out ECR, 74h     ; reverse
read ECR  -> expect bit0 = 0   (DATA AVAILABLE)  <-- the bit the whole fix rests on
in  al, FIFO(778h)  -> expect 50h
out ctrl, 04h ; out ECR, 34h   ; restore
```

The bridge must be connected first, so extend an existing probe rather than writing a new one.
**Restore the ECR and control register before deciding anything** (technique 118).

What it settles, none of which we currently know:
- does ECR bit 0 clear when the bridge has a byte ready, on this card
- does an address cycle to `base+0` actually select a register
- is one byte returned, and is it the right byte
- roughly how long the reverse wait takes, which sizes the budgets

### 2. MODEL — make 86Box's ECP FIFO honest

Only after step 1, and built to match that capture.

Today the EPAT model answers `ecp_read_data` on demand and the LPT FIFO is bypassed. It must
instead **put bytes into the FIFO**, so that ECR bit 0 (empty), bit 1 (full) and bit 2
(serviceIntr) mean what the hardware means.

This is worth doing for 86Box on its own account, independently of this driver: it is shared
parallel-port infrastructure, and at present it cannot represent a device that is not instantly
ready. Any driver tested against it inherits that lie. The owner has other parallel devices.

### 3. IMPLEMENT — the driver, to the vendor's shape

- Gate every ECP phase on the ECR: empty before an address cycle, **not-empty before a read**.
  Budgets `FFFFh` forward, `8000h` reverse, as the vendor uses.
- Block reads: wait for FIFO-full and take a whole chunk; fall back to one byte per serviceIntr
  for the tail — `SD120PPD.SYS` `47C7h` and `47AEh`.
- Block writes already chunk correctly (16 bytes per ECR-empty wait, the vendor's `[0C18h]`).
- Keep SPP as the fallback and keep `MODE=SPP` / `MODE=ECP` selectable.

### 4. VALIDATE — bed first, then hardware, in that order

The bed is only a valid gate once step 2 is done **and** the vendor's own DOS driver can drive
it end to end. Until then a bed pass means nothing.

## Standing rules for this work

- **Never ship `-Mode auto` to the CF.** The owner's port is ECP-capable, so auto silently
  selects the transport under development. Pin the mode deliberately.
- **Read `BOOTLOG` off the card before claiming anything about a hardware run**, and compare
  against `BOOTLOG.PRV` rather than against a figure in a doc. A stale number in this repo sent
  a whole day in the wrong direction on 2026-09-13.
- **The vendor binaries are the authority on the protocol.** `SD120PPD.SYS` full sweep is
  26,095 instructions over all 56,198 bytes; `SD120PPD.MPD` covers `.text` `400h`-`FE12h`.
  Both are complete — read them, do not sample them.
