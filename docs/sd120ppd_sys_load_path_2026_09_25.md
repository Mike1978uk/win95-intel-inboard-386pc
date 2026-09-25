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
