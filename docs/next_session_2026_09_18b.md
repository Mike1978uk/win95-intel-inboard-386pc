# LS-120 — handoff, 2026-09-18 (afternoon)

Machine state: CF **in the 5160**. `SYSTEM.INI` is **stock** (`shell=Explorer.exe`,
sha256 `491f48ee…`, byte-identical to the card's own `SYSTEM.B26`). Card holds
`LS120MP.MPD` = `9cf2da38` (SPP+trace); previous build kept as `LS120MP.B26`.
Two scratch files left on the card: `C:\LSPROBE.TXT` (5 bytes) and
`C:\LSTEST.BIN` (2,048 bytes, a partial transfer) — delete when convenient.

## ⭐ THE FINDING: every ECP path in the driver was unreachable

`LS_HasEcp` is set **only** in `LS_DetectEcp`. `LS_DetectEcp` was called **only**
from `LS_PortOpen`. `LS_PortOpen` is called **only** from `LS_BridgeProbe` and
`LS_ColdStart` — and **neither has a single call site anywhere in the driver**.

So the flag was initialised to `0` and nothing could ever change it, while every
ECP path sat behind it:

```
LS_BlockRead:   cmp LS_HasEcp, 0 / je LS_BlockReadSpp
LS_BlockWrite:  cmp LS_HasEcp, 0 / je LS_BlockWriteSpp
LS_BringUp:     cmp LS_HasEcp, 0 / je lbu_inq        (skips LS_NegotiateEcp)
```

**In every build mode, including `-Mode ecp`.** The 1284 negotiate/terminate work
proven on hardware on 09-18 was compiled in and structurally dead. That is why an
`auto` build's trace came back byte-identical to an SPP build's.

Fixed in `4414ea8`: `LS_BringUp` now calls `LS_DetectEcp` before testing the flag.
**Built but never booted** — `1c8cfb4f`, code `f93851ec`, commit `1f79095`, clean.

## ⛔ Two retractions in MEASURED_FACTS — the owner was right, every session

1. **"ECP block streaming — the bridge does not do it"** is **withdrawn** (§2z).
   `ECPNOW.OUT` was captured 23:45 on 09-17; the four-run proof that 1284
   negotiation is mandatory is 00:08–00:14 on 09-18 — **23 minutes later**, and
   `ECPNOW.SCR` contains no negotiation at all. A run missing its prerequisite is
   not a negative result. **ECP bulk is UNTESTED, not refuted.**
2. **"An ECP register read is SLOWER than nibble"** is an instruction **count**,
   not a timing. ECP and nibble have never been timed against each other here.

The file now carries a standing rule: any row not measured must say so.
Technique 124 records how to spot the next one — check a capture's timestamp
against the discovery it depends on.

## ⭐ NEXT ACTION: model the drive in the bed to match hardware

The bed's unfaithfulness is what makes its results untransferable — it enumerates
while the 5160 does not. Ranked, all sourced from `MEASURED_FACTS`:

1. **Latency. This is the one.** The model completes immediately, which is exactly
   why inline blocking cannot reproduce there. Add: nibble **38.9 µs/byte**
   (19.9 ms per 512-byte sector, §2); **reset settle in seconds** (`--spin 2000`
   ≈0.29 s aborts every command, `0FFFFh` ≈3 s works, §3); **BSY = `80h` after a
   command** (the model returns `40h` and never goes busy). There is already a
   `busy_ms = 750` knob in `vm_ls120win/86box.cfg.master`.
2. **The 1284 state machine** — refuse ECP until negotiated, stay in ECP until
   terminated, return `F5` to nibble reads in between (§2a). Without this the bed
   "passes ECP code the real bridge refuses" and no ECP result from it is worth
   anything.
3. **The 3,584 burst ceiling** (§3a) — 8 sectors offers `0E00` not `1000`, strands
   the last 512 bytes, completion never reaches a clean `50`, and the **next**
   command returns ABORTED `B4`.
4. **Unit attentions** — ASC `29` then `28` after SRST, one per REQUEST SENSE,
   **INQUIRY exempt** (§4). Possibly already partly modelled; check.
5. **Writes** — `PHASE_DATA_OUT` has never run in the bed.

Do 1 alone first. If the bed then fails the way the 5160 does, the bug is
reproducible at emulator cost and everything after it gets cheap.

## The trace channel: works in the bed, DEAD on hardware

`-Trace` writes 16-byte ASCII records to a ring at `0B9000h`; `tools/ls120_trace.py`
parses it. **Proven in the bed** — captured a full working enumeration.

**It does not survive on the 5160.** Measured, not assumed: poison `55 AA` at
`B9000`, boot Windows, come back — the page reads `20 07 20 07` (text VRAM
cleared by Windows' exit mode-set). Four further candidates (`0x9F000`,
`0x9E000`, `0x9FC00`, `0xBF000`) were **all** overwritten. Nothing in the first
megabyte survives.

The poison test is what made that conclusive rather than "the driver never ran".
**Next channel to try is the owner's idea: the drive's own 512-byte buffer**
(`WRITE BUFFER`/`READ BUFFER`, proven byte-exact and media-untouched, §4). Catch:
it needs the transport working — but that inverts usefully, because the channel
succeeding *is* the result. Unknown whether our own SRST clears it; check first.

## The structural gap against four working references

| | PC2X (MS, parallel port) | SD120PPD (this drive) | T130 (polled) | **ours** |
|---|---|---|---|---|
| `RequestTimerCall` | ✓ 40 ms, self-re-arming | ✓ 1/10 ms ×9 | ✓ 2 ms ×2 | **none** |
| completion | deferred | deferred | deferred | **inline** |
| `SpecificLuExtensionSize` | non-zero | `0x7a` | — | **0** |
| `MapBuffers` | FALSE | FALSE | — | TRUE |
| SRB functions answered | — | 4 | — | **1** |

PC2X is the closest reference — parallel port, deliberately no IRQ: *"Keep
interrupts disabled and poll with timer. I see no reason to tie up an IRQ
resource for this card."* Its ECP register path and ours both pass INQUIRY
through; none of them synthesise it the way `XTIDEMP` does.

⚠ Do **not** treat deferral as proven-necessary: the bed enumerates without it.
The hypothesis is that real drive latency blows SCSIPORT's SRB timeout, which is
precisely what item 1 above would let us test.

## Emulator

`86box_upstream` (`lpt-epat-bridge`) now carries **TC1995's Mach8 fix**,
cherry-picked as `03ddc5eb1` — *"fixed the Graphics Ultra RAM addressing and
16-bit databus POST test"*. The owner confirms the ATI card now works properly
in the bed. That very likely closes issue #8 / technique 73's open hypothesis —
**worth checking and closing on the issue tracker.**

Also added, `DIAGNOSTIC not for upstream`: `ls120_trace_dump()` in `src/86box.c`,
env-gated on `LS120_TRACE_DUMP`, dumps 4 KB from `0B9000h` once a second and
**only when the ring's magic is present** — it overwrote a good capture with a
blank before that guard existed.

## Traps paid for today

- **`file_write` with `offset` TRUNCATES the file.** It is not a patch-in-place.
  This reduced `SYSTEM.INI` to 17 bytes on the card; recovered from the card's own
  `SYSTEM.B26` and verified byte-identical. Always `file_stat` after a write.
- `file_write` **does** support `src_path`, but an 8 KB transfer exceeds the 8 s
  op timeout — it wrote 2,048 bytes and stopped. Raise the timeout.
- COMrade `mem_read`/`mem_write` take the address as **hex** (`0xB9000`), and the
  host side is confined to the repo root.
- **COMR95 cannot do `mem_read`/`mem_write`/`io_in`/`io_out`** — DOS COMrade is a
  strict superset. There is no way to read guest memory while Windows runs.
- Ctrl+Alt+Del does **not** recover the Explorer hang; ending Explorer does not
  either. Only the power switch — which erases any RAM-based trace.
- `shell=Notepad.exe` does not end the Windows session when closed, so that exit
  route does not exist. `PROGMAN.EXE` is on the card if a shell with *File → Exit
  Windows* is ever wanted — but do not test with it, it masks the symptom.

## Port state, before and after a full Windows session

`0x77A` ECR `0x15` → `0x15` (SPP), `0x37A` control `0x0C` → `0x0C`, `0x379`
status `0x00` → `0x00`. **Byte-identical.** Windows and the driver leave the
parallel port exactly as they find it, so the "non-SPP ECR poisons nibble reads"
concern does not apply to this build.
