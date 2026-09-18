# Handoff 2026-09-18e — the vendor's ECP paths, decoded and applied

## Staged on the card, never booted

`LS120MP.MPD` = **`6924eeaf` / code `1616e877` / commit `e5c2fc4`, clean tree**,
built `-Phase 2 -Mode ecp`. Written to **both** `C:\LS120MP\` and
`C:\WINDOWS\SYSTEM\IOSUBSYS\` (CRC-verified, identical).

**Revert if it misbehaves:** `C:\LS120MP\LS120MP.SPP` is `d8154f1d`, the SPP
build that enumerates and mounts at `J:`. Copy it over `LS120MP.MPD` in both
places.

## The one thing that changed everything

**ECP needs the peripheral negotiated into 1284 ECP mode before the
descriptor.** CPP-connected is not enough. Without the negotiate the first
address cycle never drains the FIFO, the routine spins to its `0x8000` budget
and branches to teardown having emitted nothing — which is what every earlier
ECP attempt was doing. Proven by two runs one variable apart:
`VC1_vendor_descriptor_connected.OUT` (no negotiate, spins) and
`VC2_vendor_descriptor_negotiated.OUT` (negotiated, every wait passes first
poll). Negotiation acknowledged reads status `B8h`.

## The vendor's ECP paths, in full

Bounds came from the dispatch tables at file `0x4E9D` (read) and `0x4F15`
(write). The ECP entries hold **start and end**, not start and length:

| | register handler | block handler |
|---|---|---|
| ECP Read, selector 13 | `0x3CCE`–`0x3D59` | **`0x4627`–`0x4833`** |
| ECP Write, selector 6 | `0x4932`–`0x498B` | **`0x4CA3`–`0x4E4C`** |

Add `0x100` for DEBUG addresses. Disassembled with
`scratchpad/dis16.py` (capstone, 16-bit); data references inside the image are
**not** shifted, which is why `[0BFA]` is the port base in both views.

### Read, dbg `4727`–`4933`

```
park 14h ; ctrl 04 (fwd)
if [0C1A] == CX -> skip the descriptor entirely     ; length cache
ECP 74h ; [0C1A] = CX
  base+0 = 0Eh  / FIFO = 0Bh
  base+0 = 0Fh  / FIFO = count-high
  if [0C5C] > C3h:  base+0 = 0Bh / FIFO = count-low ; JBE, so strictly >
park 34h ; ctrl 04 (fwd) ; ECP 74h
  base+0 = 80h                                      ; block read, FORWARD
park 34h ; ctrl 20h (REVERSE) ; ECP 74h
  stream: aligned -> wait ECR bit1 FULL, rep insb 16
          else    -> wait ECR bit2 serviceIntr, one byte
ctrl 04 ; ECR 34h
```

Every write is bracketed by a wait on ECR bit 0, budget `0x8000`. **Count is
big-endian.** There is **no `A0h` last-byte command anywhere in the ECP read.**

### Write, dbg `4DA3`–`4F4C`

Same descriptor, entry park `14h`, version gate uses `JB` (`>= C3h`) where the
read path uses `JBE` (`> C3h`). Never leaves forward. Before the `C0h` block
command it requires **status `base+1` bit 3 (nFault) SET**, looping on ECR
bit 0. Streams `rep outsb` from `DS:SI` with `DS=ES`. Teardown is `ECR=34h`.

### `[0C5C]` is bridge register `0Bh`

Loaded at dbg `9F8D` (`MOV DX,000B`, then the register-read dispatcher at
`254C`). `epat.c` line 297 calls the same register "the version code".
**Measured on this bridge: `0Bh` = `C6h`**, so count-low is emitted here.

## The bridge's register space, `00h`–`17h`

Destination poisoned `EE` first, so an unread register cannot read as zero.

```
00: 00 99 99 99 99 99 99 00   08: 85 10 00 C6 00 00 00 00
10: F0 FF 00 00 00 00 80 02
```

⚠ Taken **without** the EPAT init writes (`WR(8,10) WR(0C,14) WR(0A,38)
WR(12,10)`, epat.c:241). `0Ah` selects what `0Bh` returns, so this is the
unselected view. `REGDUMP2.SCR` adds the init and has **not been run** — do
that first, and expect `0Bh` to be the authoritative version only then.

⛔ **There is no firmware dump path.** No download/upload switch in the vendor
driver (`/dpc /rx /wy /de /ded /db /dp /di /fe /fev /fp /ni /ix /IRQ /PORT /P
/sf /pd` is the complete list), no firmware blob in it, and `epat.c` knows only
two values for `0Ah` — `0x38` connected, `0x11` disconnected. The EPAT is a
mask-ROM part; `0Bh` is an 8-bit version code, not a window into code space.

## Three defects fixed in our driver (`e5c2fc4`)

1. **The `A0h` last-byte tail on the ECP read.** Ours flipped to forward, sent
   `A0h`, flipped back to reverse and read one byte. That is `epat.c`'s
   **nibble** shape grafted onto ECP, and the flip abandons the reverse channel
   the data arrives on. Removed; `ECP_CMD_LAST` deleted with it.
2. **The ECR park.** Commit `3e6bcf0` made it `14h` everywhere, off the write
   descriptor's *entry* park. Wrong — before a **direction change** the vendor
   parks at `34h`, because ECR mode 000 is not bidirectional and cannot carry a
   reverse request. Reverted; `3e6bcf0` is superseded, do not reapply it.
3. **The descriptor** — necessary, and now measured rather than inferred.

## Not done, and why

**The corrected read is untested.** The DOS harness hung **three times, always
at the same point** - reproducible, not intermittent. The first hang
was my bug — the per-byte gate at `10E0` sets `CX = 0x8000` as its own spin
budget, so calling it inside a `LOOP` destroys the counter. `ECPDESC4.SCR`
fixes that with `PUSH`/`POP CX` and a `JC` timeout exit, and **still** hangs.
Each attempt needed a 5160 reset; `Ctrl+C` cannot reach a loop that polls no
DOS service.

The remaining suspect is **cumulative**, not a single infinite loop: the only
change from `ECPDESC.SCR` (which completes in ~4 s) is that the read path now
gates every byte, and the script drives that path repeatedly. At `0x8000` spins
per byte worst case, a read that mostly times out costs seconds *per call*.
Before debugging it further, bound the gate far lower - `0x0800` is ample at
39 us/byte - or drop the DOS harness for ECP entirely and test through the
driver, which is the only consumer that matters.

## Next, in order

1. **Boot the staged build.** It is the whole point of the card write. If the
   drive enumerates over ECP, the loop is closed.
2. If not, **single-step the vendor's `4727` on live hardware** — same recipe
   as `VCONN2.SCR`: relocate the CPP connect and the 1284 negotiate above the
   loaded image (it ends at `DC85`), `g=E000`, then set `IP`. That recipe
   worked for the descriptor and will work here.
3. Find why `ECPDESC4.SCR` hangs, or abandon the DOS harness for ECP and test
   only through the driver.
4. Still open from earlier: **the sense fix** — `ScsiStatus = 02h`,
   `SenseInfoBuffer`, `SRB_STATUS_AUTOSENSE_VALID`. That is what unblocks
   writes, and it is independent of all of the above.
5. Run `REGDUMP2.SCR` for the selected register view.

## Method notes

- ⛔ **`file_stat` can serve a stale directory entry.** It reported 0 bytes for
  three capture files that were 33 KB. Use `DIR` to confirm a size. I called a
  real measurement a harness failure on the strength of it.
- The dispatch tables mix two entry layouts: start/length for most selectors,
  **start/end** for the ECP ones. Read the pair before trusting either.
