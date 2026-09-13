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

## The instrument

`drivers/imation_ls120/tools/ecpprobe.py` emits `ECPPROBE.COM`, streams to the box
over COMrade's CRC-verified channel, runs in ~1.3 s, restores the ECR and control
port and disconnects. No DEBUG script, no keystrokes, no reboot.

Do **not** drive a bridge handshake with one `io_out` per port access - seven of
fifty timed out mid-frame on 2026-09-13, and a timeout does not say whether the
write landed.

## Next, in order

1. **Put the negotiation into `LS120TR.ASM`.** `LS_BringUp` currently performs no
   negotiation and no bridge-register configuration. Gate every ECP phase on the
   ECR with the vendor's budgets (`FFFFh` forward, `8000h` reverse), and keep the
   `0x2A21` recovery-and-retry.
2. ~~Re-read a register with a known non-zero value over ECP~~ **DONE** - BCLO
   over ECP returned `14h`, matching what nibble reports for the same register
   in the same run. ECP register reads carry real data. Nothing left to prove
   about the register path.
3. **Then the block path**, then SPP stays as the fallback.
4. 86Box's ECP FIFO still answers on demand and models no negotiation, so the bed
   cannot currently falsify any of this. Model it honestly before trusting a bed
   pass.

## Machine state

Vendor DOS driver loaded, drive at `D:`, media read (126 MB, 963 Cyl, 8 Heads,
32 Sec/Trk). Port restored by the probe. `ECPPROBE.COM`/`.BIN` left on `C:`.
Both repos clean. **Nothing pushed.**
