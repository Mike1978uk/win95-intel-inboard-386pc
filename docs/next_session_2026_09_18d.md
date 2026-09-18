# 2026-09-18d — the hardware characterisation session

A long session on the real 5160 over DOS COMrade. The drive now **enumerates,
mounts and reads on hardware**, and the device is characterised end to end.
ECP is **not** fixed; the next step for it is identified and scoped below.

## What changed on the machine

| | |
|---|---|
| LS-120 | **enumerates and reads on the 5160**, mounted at `J:` |
| what unblocked it | **disabling the ECP Printer Port (LPT1) node** (owner also moved our node to `0278`; two variables, LPT1 is the one with a mechanism) |
| driver shipped | `d8154f1d` / code `5084a71c` — SPP, non-blocking init |
| Windows with the drive **disconnected** | **boots to desktop, no block** — the old regression is gone |
| writes | **fail, "write protected"** — cause identified, not yet fixed |

## The write-protect cause is identified

`docs/ls120_media_state_responses.md` has the measurements. In short:

Our miniport sets `AutoRequestSense = FALSE` and never writes `ScsiStatus`,
`SenseInfoBuffer` or `SRB_STATUS_AUTOSENSE_VALID` — `grep` finds no reference
to any of them. `LS_RequestSense` exists, is called once internally to clear
the condition, and the sense bytes are **discarded**.

So the class driver gets a featureless `SRB_STATUS_ERROR` and cannot separate
"medium changed, retry" (`6/28h` — routine, queued two-deep after any reset)
from "write protected" (`7/27h`). For removable media the conservative
fallback is read-only.

**The fix, next session's first job:** on ERR, set `ScsiStatus = 02h`
(CHECK CONDITION), copy the sense already fetched into `SenseInfoBuffer`
bounded by `SenseInfoBufferLength`, and OR `SRB_STATUS_AUTOSENSE_VALID` into
the status.

## ECP: where it actually stands

`docs/ls120_ecp_vs_spp_timing_2026_09_18.md` has the detail.

| | result |
|---|---|
| ECP **register** reads | ✅ work, with negotiate + terminate (`ECPTERM2`, 09-17) |
| ECP **bulk** | ❌ **delivers nothing** |

`ECPBULK.SCR` was built this session — the first script ever to do bulk over
ECP *with* a correct terminate (`ECP2`'s bulk path + `ECPTERM2`'s routine
`1600` on both block tails). Result, reproduced twice:

- terminate works — no `F5` flood, port left usable
- **every data buffer still `EE` poison** while SPP fills them with the boot
  sector
- 21.25 / 21.75 s against SPP's 15.66 / 16.26 s — longer, delivering nothing

⚠ **Scope:** this says *this implementation* of ECP bulk fails. `ECP2`'s bulk
path predates the 1284 work, so its **negotiate** is as suspect as its
terminate was. It does **not** say ECP bulk is impossible.

### The next step for ECP, and where it stalled

Transliterate the **vendor's own** ECP block path rather than patching
`ECP2`'s guess. Located and partly read:

- `SD120PPD.SYS` **`0x43F5`** — the ECP block **read** routine
  (`pushf / push bx / push si / push dx / cld`, then a mode branch on
  `[0xBC7]`, ECR manipulation, `rep insb` at `0x4465`)
- the nibble sibling is at `0x43AB`
- `[0xBFA]` is the LPT base (`cmp word ptr [0xbfa], 0x3bc`)

⛔ **Blocked on one thing:** `0x43F5` has **no direct caller** in the
disassembly — it is reached through the driver's dispatch table, so the `DX`
it is entered with is unknown. Every reading of the `add dx,3` / `sub dx,2`
walk tried so far is inconsistent with a real port address. **Find the
dispatch table entry that points at `0x43F5` and what loads `DX` before it.**
Do not transliterate until that is resolved.

⚠ **And audit before adopting:** the vendor's DMA-assisted block variant
writes `out 23h` / `out 22h`, which alias onto the 8259 on this XT
(technique 75). That is in the *block transfer* path, not just chipset init,
so `/ni` does not protect against it.

## Everything characterised, with captures

`docs/ls120_media_state_responses.md`, captures in
`docs/captures/2026-09-18_media/`.

- **Media states** — writable `31h`/WP=0, protected `31h`/WP=1, absent
  `00h`/WP=1. ⛔ **An empty drive reports write-protected**; medium type is the
  discriminator, not the WP bit.
- **Drive states** — ready `50h`; settling `80h` then clears; **powered off
  `80h` forever**; **disconnected `77h`**. ⛔ `FFh` never occurs, so the
  driver's only absence test is dead code. ⛔ The ATAPI signature `14 EB` comes
  back from a **switched-off** drive, so it is not a presence test.
- **Unit attention** is queued **two deep** after SRST (`29h` then `28h`); one
  sense read does not drain it. INQUIRY is exempt, which is why the old probe
  worked.
- **Geometry** C=963 H=8 S=32 × 512 = 246,528 blocks = 120.4 MB, and READ
  CAPACITY agrees exactly.
- **Mode pages** 01, 03, 05, 08, 0B. Write cache **disabled**. **No page 2** and
  **READ BUFFER mode 3 is ILLEGAL REQUEST**, so the drive declares no preferred
  transfer size — the 3584-byte bridge burst ceiling is the only constraint.
- **START STOP UNIT** — eject works and is **fast** (physically confirmed);
  load is ILLEGAL REQUEST (manual-insert drive, no loader); spin-up is accepted
  and leaves BSY set.

## Still outstanding

1. **The sense fix** (above) — unblocks writes.
2. **The 9 MB bulk round trip.** `MKBIG.BAT` is written
   (`docs/captures/2026-09-18_media/`) and builds ~9.1 MB on `C:` by doubling a
   real file. Needs Windows, since DOS has no drive letter for the drive with
   the vendor `.SYS` REM'd out. ⚠ At the measured ~39 us/byte that is roughly
   **6 minutes each way** over SPP. The owner specifically wants this because
   the medium was in a garbage state after the vendor driver and a DOS format.
3. **ECP**, per the blocked step above.
4. The driver still runs commands **inline** once ready — only the settle moved
   to the timer. Step 4 of `docs/ls120_enumeration_from_working_drivers.md`.
5. `LS_SettleStep`'s absence test should key on `77h` and on BSY never
   clearing, not on `FFh`.

## Method written back

`inboard-hw-debug` **technique 126** — characterising a storage device before
writing its driver: deriving every probe from one proven capture, dodging the
harness's own unit attention, mapping all media and drive states, poisoning
destinations so "no data" is evidence, timing an A/B with the guest's clock and
interleaved arms, and checking the A/B scripts are comparable before believing
any number.

Two wrong results were caught before publication this session and both are
recorded there: a shell `printf` turning `\E` into ESC (which produced a
fake 43x ECP win), and a failed READ BUFFER whose stale buffer looked like a
buffer capacity.
