# SD120PPD.SYS on the EPAT model - the load path, read in full (#44)

2026-09-25. What the vendor LS-120 DOS driver does from load to "drive installed", what each
step requires of the bridge, and whether 86Box's EPAT model (`src/device/lpt_epat.c`) does it.
Written because the same facts were being re-derived one bed run at a time.

Addresses are file offsets in `SD120PPD.SYS` (md5 `cbb42e8e`), which equal memory offsets in
its load segment. Disassembly: `tools/pedis.py`-style capstone passes; the full sweep is
`drivers/imation_ls120/SD120PPD_SYS.asm`. Companion: `docs/sd120ppd_sys_full_read_2026_09_13.md`
(transfer handlers, ASPI surface) - this document covers what that one did not.

## Read first: the AT bed and the real 5160 take different paths

Read out of the resident driver on the real 5160 over DOS COMrade (driver at `0575:0000`,
loaded with `/port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp /fe`):

| variable | meaning | real 5160 |
|---|---|---|
| `[0BD7]` | port type | `0C` - ECP |
| `[0BD9]` | read mode (table `0x4E9D`) | `0D` - **ECP Read** |
| `[0BDB]` | write mode (table `0x4F15`) | `06` - **ECP Write** |
| `[0BFA]` | port base | `0378` |
| `[0BFC]` | IRQ | `07` |
| `[0C00]`/`[0C01]` | chip open / connected | `01` / `00` |
| `[0C07]`/`[0C08]` | saved bridge reg 8 values | `01` / `02` |
| `[0C0D]` | skip interrupt arming at disconnect | `01` - **`CPP(0x48)` is not sent** |
| `[0C13]` | armed | `01` |
| `[0C15]`/`[0C16]`/`[0C17]` | slow mode / alt status / reg 12 bit 7 | `00` / `00` / `00` |
| `[0BE9]` | | `01` |
| `[09E9]` | transfer option flags | `00C0` |
| `[0946]` | chip revision flag (see step 4) | `01` (`0xE2`) |
| `[09BE]` | chip test passed | `01` |

The bed was staged by `tools/bed_stage_at.py` with the line **without `/fe`**, and the model is
not ECP-detected the way the real card is, so the driver there chose a nibble read mode that
reads status twice per strobe, a write mode that sends every byte twice, and the interrupt
completion path. **None of those is the real machine's path.** The model fixes below are real
defects, but the target for #44 is the real line and the ECP path.

## The load path, in order

| # | step | where | requires of the bridge | model |
|---|---|---|---|---|
| 1 | open: probe, preamble `CPP 30 40 50 00` | `0x2C42`, `0x2DBB` | checkpoint statuses `B?`,`5?`,`B?`/`F?` | ✅ |
| 2 | chain scan `CPP(0x10\|u)` | `0x2D0B` | id `FFAA` for unit 0, first nibble readable **before** any strobe | ✅ fixed |
| 3 | register writes, slow modes | write table `0x4F15` | each byte written twice before the strobe = one byte | ✅ fixed |
| 4 | chip revision test | `0x6FD1` | internal window: `WR(0E,0C) WR(0F,21) WR(0F,01)`, read `0x1F` = `00` or **`E2` (measured)** | ✅ fixed |
| 5 | interrupt self-test (with `/IRQ`) | `0x9B6E`, handler `0x9C13` | reg 8 bit 6 + `0x80` on data pulses nACK; handler counts 20 with PE set and nACK **released** | ✅ fixed |
| 6 | per-unit select, signature | `0x6FFD`, `0x70F3` | task file at `0x18`, signature `14 EB` | ✅ |
| 7 | vendor command `F0` | `0x7169` | any completion; result ignored | ✅ (aborts + INTRQ) |
| 8 | NOP `00` | `0x7351` | **completion with INTRQ**, even though aborted; timeout = unit absent | ✅ fixed |
| 9 | IDENTIFY PACKET DEVICE `A1` | `0x7233`, `0x739C` | INTRQ, DRQ, 512 bytes; word 0 `(w & C003) == 8000`, type 0 | ✅ fixed (captured block) |
| 10 | block read of the 512 bytes | read table `0x4E9D` | strobe-driven nibbles; repeated status reads return the same nibble | ✅ fixed |
| 11 | INQUIRY `PACKET 12` | `0x719F` | CDB via block write (doubled bytes in slow modes), 36 bytes back | ✅ fixed - works in B12 |
| 12 | READ CAPACITY etc. from `ASPIHDRM` | ASPI path | CHECK CONDITION then REQUEST SENSE; completion **polled or by interrupt** | ✅ polled (`/di`): drive `D:` reads (B15). ❌ interrupt mode needs the transfer engine |
| 13 | slave probe | `0x6FFD` loop | an absent slave: task file reads `00`, commands ignored | ✅ fixed - was a second letter `E:` |

### Mechanisms the model gained, each tied to the code that needs it

- **INTRQ in bridge register `0x12` bit 5** (`0x3619`, `0x16CD`): raised on data phase, completion
  and abort; cleared by a status read or a new command; masked by nIEN.
- **SRST only on the 1->0 edge**, and an ATA reset **does not** clear bridge registers `00-17`.
- **Internal window `0x0E`/`0x0F`**: writes stored per index; `0x0F` reads return the selected
  index; with internal `0C` bit 0 set, `0x18-0x1F` are the chip's view (`0x1F` = `E2`).
- **Block checksum**: internal register 2 = XOR of every byte a block moved (`0x31F0`/`0x329E`
  check it when the caller's flag bit 0 is set).

### Step 12 in the bed - the interrupt completion path

Only reached because the bed's line has no `/fe` and `[0C0D]` ends up clear. The chain:
disconnect `0x3041` sends `CPP(0x48)` and sets control bit 4; the drive's INTRQ must then pulse
nACK -> IRQ 7; the dispatcher `0x2116` checks the PIC in-service bit (`0x2323`) and calls
`0x1E84`, whose check `0x1DF3` runs `0x356A` (non-ECP: nACK high and control bit 4; ECP: ECR bit 6
then status bit 3) and asks `CPP(0x08|unit)`, reading status once: bit 7 set, bit 3 clear,
bits 6-4 = unit -> ours. Modelled from this read (arm on `CPP(0x48)`, off on `CPP(0x40)` or connect;
status bit 6 high while armed and disconnected; unit byte `0x80|unit<<4` when pending), **untested**.
On the real machine `[0C0D] = 01` skips it.

### The transfer engine - read in full, 2026-09-25 afternoon

**Start**, `0x1A84` -> `0x3357`, called before every PACKET data phase with `DX:AX = 0` and
`CX` = the SRB's byte count (`es:[di+0Ah]`); `SI = 0` for SRB flag `08h` (in), `1` for `10h` (out):

| step | writes |
|---|---|
| `[09E9]` bit 6 only | reg `0A` = `10`, reg `0C` &= `EF` |
| unless `DX = FFFF` | internal `06` = blocks low, internal `07` = blocks bits 9-8 (`DX:AX / 512`, so `0` here) |
| always | reg `14` = `CX / 512 - 1`, **not** bytes/2 - 1 as the handoff said: `FF` below 512 bytes, `00` for one sector |
| always | reg `12` = read value, bit 0 **set** for in / clear for out, bit 1 set (start); `[09E9]` bit 9 adds bit 7 |

Then `0x1AB9` sets the direction in the chip view: internal `0C` = 1, reg `18` = `26` in / `22` out
on the `E2` chip (`03`/`02` otherwise), internal `0C` = 0.

**Collect**, read path `0x18F2`: `0x383C` reads internal `06`/`07` back as a count of 512-byte
blocks, and `0x1B3E` then block-reads `min(count x 512 + 512, requested)` bytes through `0x1FB7`
-> `0x329E`, the same block read the polled path uses. Between chunks `0x1FB7` calls `0x3619`.
Writes go out through `0x1B85` -> `0x1F58` -> `0x31F0`.

**Status**, `0x3619`: reg `12` bits 4 and 5 both set -> `1` (done); bit 3 -> `2`; else it checks
reg `09` (`F0`/`50`/`A0` -> `18`/`08`/`10`) when `[0C16]` is set, else folds reg `08` bits 0-1 into
reg `0D` and returns `4000`.

**Stop**, `0x1B35` -> `0x33E4`: reg `12` &= `FC`; with `[09E9]` bit 6, reg `0A` = `18`, reg `0C` |= `10`.
**Abort**, `0x368C` (no direct caller): if reg `12` bit 1, set reg `13` bit 1, poll `0x3619` up to
`0x800` times for `1`, clear reg `13` bit 1 and reg `12` bits 0-1.

**What the bed shows.** The `/di` run B15 already starts the engine 266 times (`W12=33`,
`W14`, `W18=26`) and reads correctly, because every READ(10) there is one sector and the model
serves the block read from the drive directly. So the engine does not need modelling for
single-sector transfers. In the default-line run B13 the driver starts READ CAPACITY, disconnects,
sends `CPP(0x48)` and sets control bit 4 - and never sends `CPP(0x08|u)`: **no interrupt arrived.**

**Cause.** The model raises the port interrupt the moment `CPP(0x48)` is decoded, because its drive
has already finished. 86Box's `lpt_irq()` discards a raise while control bit 4 is clear and does not
re-check when bit 4 is later set, so the raise is lost. On hardware the drive completes after the
disconnect and the nACK edge lands with bit 4 already set. The fix belongs in the model: forward
INTRQ to the port after a drive latency, not at the instant of arming.

**Run B16 (default line, latency fix).** The interrupt now reaches the driver, which then scans
`CPP 08|u` / `50|u` for u = 0..8 and re-arms, forever: unit 0 always answers `00`.

### The interrupt path, read in full

| address | what it does |
|---|---|
| `0x20DE`-`0x2114` | four IRQ entries, `BP` = slot 0-3, all into `0x2116` |
| `0x2116` | IRQ number from `[0A6E+BP]`; `0x2323` checks the PIC in-service bit (OCW3 `0B`); calls the slot handler `[0A86+2BP]`; on `AX=1` far-calls `[0A76]` |
| `0x1E84` -> `0x1DF3` | `0x356A` must pass (non-ECP: nACK high, control bit 4 set); with `[0983]=1` it is "ours" at once and `0x1EA6` calls `[0981]` |
| `0x80C3` | API function 1 registers `0x621B` as that handler, and a completion callback per unit in `[0A03+4u]` for each unit in `[09F4]`, sending `CPP 50\|u` to every unit as it goes |
| `0x621B` | `CPP 40`; fn 5 (`0x239D`); for u = 0..8: **`CPP 40`, `CPP 08\|u`, `CPP 50\|u`**, and if the `08\|u` byte has bit 7 set, far-call unit u's callbacks and stop; then fn 8 = `CPP 48` |
| `0x26D9` | the CPP frame. `08\|u` has no commit strobe: status is read once, straight after the command byte. Control bit 4 is cleared by every frame and set again only after `48` |

The `0x1284` API (`0x1D13`, table at `0x1D35`) maps function 8 to `CPP 48`, 9 to `CPP 40`,
10 to `CPP 50|u`, 11 to `CPP 08|u`. The wrappers are at `0x604B`, `0x6034`, `0x6093`, `0x6079`.

**What the model got wrong.** It answered `08|u` with `80|u<<4` only while forwarding was armed.
The scan disarms with `CPP 40` before every query, so the answer was always `00`. The byte has to
report the unit's pending interrupt regardless of the arm; `0x621B` reads only bit 7. `CPP 50|u`
follows every query and is sent to every unit at registration: an acknowledge with no reply. INTRQ
is a level cleared by the status read, which the unit's completion callback performs, so the
`CPP 48` re-arm after the scan cannot fire again on the same completion.

**Run B17, default line:** passes - drive letter mapped, 539 interrupts, all single-sector reads.
**Run B17, `/di`:** froze on the first WRITE(10), after the write itself completed (status `40`).
The engine setup for OUT writes the direction code `22` to register `18` in the chip view. The
model fed that value to its unlock recogniser (`22` is unlock byte 0), dropped it, took the next
address byte `6F` as the value, and lost the `0F = 00` that closes the chip view - so every later
task-file write went to the chip view and no command reached the drive. The read direction writes
`26`, which is why no run met it before.

The register-write routines, every one read: modes 0/1/2/7/8 (`0x4847`, `0x4873`, `0x48CD`,
`0x48A0`, `0x48FD`) all write `60|reg`, control `01`, the value, control `04`, each byte doubled
or trebled. Modes 3/4 (`0x499F`) use the EPP ports and mode 5 is a far call; neither passes the
data port. Mode 6 (`0x4932`) did not decode from the sweep and is unread. An unlock frame is seven
data bytes with no control write between them, always preceded by a control write (`0x2718`).
So a data byte arriving while a register value is pending is that value, never frame content.

**Still open.** Internal `06`/`07` read back whatever was written (`0`), which serves one block per
collect. A multi-sector READ(10) under the engine has never run in the bed, with or without `/di`.

The AT bed's port is 86Box's standalone parallel port - plain SPP (`ext`, `epp`, `ecp` all 0) - so
the driver choosing nibble modes and this interrupt path there is correct for that hardware, not a
fault. The real 5160's Intek21 is ECP and takes the ECP path.

## CPP commands the driver can send (complete, from every call site of `0x26D9`)

`00` init, `08|u` unit status, `10|u` unit id, `30` disconnect, `40`, `48` arm interrupt,
`50`, `E0|u` connect. `40`/`50` need no reply.

## What remains unread

- `ASPIHDRM.SYS` itself (its commands use the same PACKET path).
- The ECP Read/Write handlers **as the real machine selects them** against the model's ECP
  callbacks - this is now the main line of work, not the nibble path.
