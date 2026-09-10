# LS-120 parallel-port transport — specification from `SD120PPD.SYS`

Clean-room specification for an MIT reimplementation as a Windows 95 `.MPD` (issue [#22]).
Derived from the vendor DOS driver plus live measurement on the real 5160. No vendor code is
transcribed — register facts and protocol sequences only.

Source: `SD120PPD.SYS.orig`, 56,198 bytes, md5 `cbb42e8eb7847869e274e45f258cf718`, April 1997.
A DOS `.SYS` is a raw binary image, so **file offset == CS offset** throughout.
Tools in `tools/`: `dump_mode_tables.py`, `sysdis.py`, `xref.py`.

Evidence is tagged **[MEASURED]** (read off the running machine), **[VERIFIED]** (replayed on the
hardware and behaved as predicted) or **[DERIVED]** (disassembly only, not yet exercised).

---

## 0. Live state, 2026-09-08 — real 5160, DOS, LS-120 attached with media

Driver located by walking MEM's block accounting and **verified against its own device header**
(`SCSIMGR$`, attribute `C000`, strategy `2074`, interrupt `2082`) at segment **`0575`**. It is an
ASPI manager; `ASPIHDRM.SYS` layers the disk on top and presents the drive as `D:`.

`DIR D:` reports **125,958,144 bytes free**, so the whole vendor stack works on this machine —
that is our control. **[MEASURED]**

```
[0BC1] EPP-BIOS probe disabled = 01      /db took effect
[0BC9] LPT unit                = 00      LPT1
[0BCB] READ  mode name ptr     = 3CBA -> 'ECP Read'
[0BCF] WRITE mode name ptr     = 491E -> 'ECP Write'
[0BD7] port type               = 0C      ECP-capable
[0BD9] READ  selector          = 000D    13
[0BDB] WRITE selector          = 0006    6
[0BE1]/[0BE3] PEP far entry    = 0000:0000   null, and unneeded
[0BFA] LPT base                = 0378
[0BFC] IRQ                     = 07
[0C02] last connect mode       = 30 -> E0 after real drive traffic
```

**Two static conclusions were overturned by this.** A first pass inferred selectors 12 and 8 from a
model where each table entry's second word is the handler length and the mode name follows the
handler. That reproduces 13 of 15 read and 9 of 9 write names — but it is a coincidence of
contiguous code layout, not how modes are selected. The live selectors are **13 and 6**.

## 1. Dispatchers **[VERIFIED]**

```asm
244B  READ  reg:  pushf, pushf, cli
                  bx = [0BD9] << 3 ; al = dl ; dx = [0BFA]
                  call word ptr cs:[bx + 4E9Dh]
                  ; STI only if the caller's IF was set (test bh,2)
2472  WRITE reg:  bx = [0BDB] << 3 ; ah = al (value) ; al = dl (reg) ; dx = [0BFA]
                  call word ptr cs:[bx + 4F15h]
```

`handler = word[table + selector*8]` — this is what the CPU executes, and the live selectors resolve
through it correctly. The other three words per entry are not uniform and are not needed.

Both run under `cli` and restore IF from the caller's own flags. **A reimplementation must do the
same** — these sequences cannot tolerate an interrupt mid-handshake.

| | selector | handler |
|---|---|---|
| READ | 13 | `word[4F05]` = **`0x3CCE`** |
| WRITE | 6 | `word[4F45]` = **`0x4932`** |

## 2. Port map

| address (base `0x378`) | register |
|---|---|
| `base+0` = `0x378` | SPP data — the **register number** (address phase) |
| `base+1` = `0x379` | SPP status — nibble returns, and the connect checkpoints |
| `base+2` = `0x37A` | SPP control — `nINIT`, bit 5 = **direction** (1 = reverse/input) |
| `base+0x400` = `0x778` | **ECP data FIFO** — the value (data phase) |
| `base+0x402` = `0x77A` | **ECR**. Bit 0 = FIFO empty. `0x74` = ECP mode, `0x34`/`0x14` quiescent |

Read cold, `ECR` returns `0x35` — quiescent with FIFO empty, matching the handlers' teardown. **[MEASURED]**

## 3. Connect — unlock handshake **[VERIFIED ON HARDWARE]**

**This is required. Register access does not work without it.**

There are two variants. The **detect** form (`0x2DBB`) carries verification checkpoints and restores
the control port afterwards; the **operational** form (`0x26D9` → `0x2710`) omits the checkpoints and
takes a mode byte. Both begin the same way.

Replayed byte-for-byte on the real machine, all three checkpoints returned exactly the predicted
values — twice, reproducibly:

```
control(0x37A) = 0x04
data(0x378):  22 22  AA AA  55 55  00 00  FF FF     (each written twice = I/O delay)
  status(0x379) & 0xF0 must == 0xB0     measured B8  -> B0  PASS
data: 87 87
  status & 0xF0 must == 0x50            measured 58  -> 50  PASS
data: 78 78
  status & 0xB0 must == 0xB0            measured F0  -> B0  PASS
```

The detect form then writes `08 08 FF FF`, restores control, and returns `AX=0` / `AX=FFFF`.

## 4. Connect — the mode byte **[MEASURED]**

The operational connect (`0x26D9`) stores `AL` at `[0C02]` and, after the magic bytes and `87`/`78`,
**writes that mode byte to the data port**, then sets control `= 0x04`:

```asm
2752  pop ax / push ax
2754  out dx,al  x2          ; the caller's mode byte
2759  add dx,2 ; al=04 ; out x2
2767  and al,0F8h ; cmp al,10h  -> read two bytes via 0x2957 into [0BBE]
277F  and al,0F8h ; cmp al,08h  -> a different path
```

**`[0C02]` was `0x30` at rest and became `0xE0` after real drive traffic**, so the operational mode
for data work is **`0xE0`** — matching the call site at `0x2C1C` (`mov al,0E0h ; or al,[0BCA]`, and
`[0BCA]=00`).

Modes observed across all call sites: `00 08 10 30 40 48 50 E0`. Thunks that set them:

| thunk | mode | callers |
|---|---|---|
| `0x3549` | `AL \| 0x08` | 1D71 1E13 1E5F |
| `0x3552` | `AL \| 0x50` | 1D66 1E7C |
| `0x355A` | `0x48` | 1D50 1E7F |
| `0x3562` | `0x40` | 1D5B 1E5C |

Callers sit in a dispatch at `0x1D4D..0x1D78`, each doing `mov al,[si+4]` → call thunk →
`mov byte [si+3],1`. So there is a request structure at `SI` with the mode at `+4` and a
"connected" flag at `+3`. **[DERIVED]**

### What was measured, and the gap that remains

Replaying unlock **without** a mode byte and then reading registers returns `FF` via ECP (with the
reverse-direction wait timing out) and `00` via nibble, across registers 0-7. **[MEASURED]** So the
unlock alone is confirmed necessary but not sufficient.

**The mode-`0xE0` replay was built but never executed** — COMrade's out-of-band file I/O wedged after
the floppy and LS-120 work, and keystroke injection was far too slow to type the stub. `tools/`
carries the generated scripts (`e0.dbg`, `modesweep.dbg` sweeping all eight modes) ready to run.
**This is the one step between here and a working transport.**


## 4b. ⭐ THE ATA TASK FILE IS BEHIND AN INDEX/DATA PAIR — `0x0E` / `0x0F`

**This is why every register probe returned zero.** The ATA registers are not bridge registers.
Bridge registers `0x0E` and `0x0F` form an index/data window, and the task file sits behind it:

```asm
3874  mov dx,0Eh ; mov al,6      ; call 2472   ; INDEX = ATA register 6
387C  mov dl,0Fh ; <value>       ; call 2472   ; DATA  = value
3883  mov dx,0Eh ; mov al,7      ; call 2472   ; INDEX = ATA register 7
388B  mov dl,0Fh ; <value & 3>   ; call 2472   ; DATA
38A0  mov dl,0Eh ; mov al,4 ... mov dl,0Fh ...              ; regs 4 and 5 likewise
```

So **read ATA register N** = write bridge `0x0E` = N, then read bridge `0x0F`.
**Write ATA register N** = write bridge `0x0E` = N, then write bridge `0x0F` = value.

Every probe run on 2026-09-08 read bridge registers `0x00`-`0x1F` directly. Bridge registers 0-7
are empty address space, which is exactly the all-zero / all-`FF` result observed in every mode,
with and without the strobe latch. The connect was correct throughout; the addressing was not.

Bridge registers the driver itself uses: `0x09`, `0x0C + [0BE8]`, `0x0D`, `0x0E`, `0x0F`, `0x12`.

### `[0C00]` is a connect-enable gate

`0x26D9` begins `cmp byte [0C00],1 / jne exit` — connect is a **no-op** when the flag is clear.
The routine at `0x98D3` clears `[0C00]` before its register accesses and restores it after, i.e.
the driver connects once and suppresses nested connects while it works. Live value is `01`.

### Register-access connect mode is `0x08`, not `0xE0`

`0x98A7` and `0x996E` both do `mov al,8 ; call 26D9` and are immediately followed by clusters of
`0x244B`/`0x2472` calls. `0xE0` is the **bulk-data** mode (it is what `[0C02]` holds after a `DIR`),
and a single-register read is not valid in it — which is why the driver's own read routine *blocked*
rather than failing fast after a mode-`0xE0` connect. The connect code has a dedicated branch for
`0x08` at `0x277F`.

### Not yet validated on hardware

`tools/indexed.dbg` is built and ready: connect at mode `0x08`, then index/data reads of ATA
registers 7, 4, 5, 6 plus bridge registers `09/0C/0D/12`. A plausible ATA status (`0x50` = DRDY|DSC)
or the ATAPI signature (`0x14`/`0xEB` in LBA mid/high) confirms the whole chain.

## 4c. Session close 2026-09-08 — the remaining gap is bridge CONFIGURATION

The ECP read replay was diffed instruction-by-instruction against handler `0x3CCE` and is
**byte-for-byte faithful**. It still never receives data: the reverse-direction wait expires on
every attempt, in every connect mode, at every register, via both ECP and nibble. The vendor's own
handler works, so the difference is **state established before the transfer**, not the transfer code.

Located, not yet decoded — `0x25EB` configures the bridge immediately after connecting:

```asm
25EB  push bx / push dx / mov bx,ax          ; BX = a flags word from the caller
25EF  test bx,2 / jz 260C
25F5  mov dx,12h ; call 244B                 ; read bridge reg 0x12
      or al,8 ; cmp [0C17],1 ; or al,80h     ; set bit 3, conditionally bit 7
      mov dx,12h ; call 2472                 ; write it back
260C  mov dx,0Dh ; call 244B                 ; read bridge reg 0x0D
      and al,0FCh
      test bx,1Bh / jz -> or al,1
      test bx,4   / jz -> or al,2            ; low two bits select the transfer mode
      ... call 2472
```

So the full bring-up is **connect → configure bridge regs `0x12` and `0x0D` → then transfer**.
Registers `0x0D` bits 0-1 select the transfer mode; `0x12` bit 3 (and bit 7, gated on `[0C17]`)
enable something further. The `BX` flags word that drives the choice comes from the caller and has
not been traced.

**Next step, offline:** decode `0x25EB`'s caller to get the `BX` flags, derive the exact values
written to `0x0D`/`0x12` for the ECP configuration on this machine, then add those two writes to
the connect replay. Bridge writes are believed to work already (only reads were ever verified as
failing), so this is testable in one run.

### Ruled out on hardware this session
Connect modes `00 08 10 30 40 48 50 E0`; with and without the strobe latch; bridge registers
`0x00`-`0x1F` direct; ATA registers via the `0x0E`/`0x0F` index/data pair; nibble and ECP transports.
All returned uniform `00` (nibble) or `FF` with an expired reverse wait (ECP).

## 4d. Replay is exhausted — the instrument needs to change

Further hardware runs, all with the connect checkpoints passing (`B8 58 F0`, reproducible in every
variant) and **all** returning `FF` with an expired reverse-direction wait:

| variant tried | result |
|---|---|
| connect + bridge config restored from the driver's own cache (`[0D34]`-`[0D37]`) | `FF` |
| ATA regs via the `0x0E`/`0x0F` index/data pair after that config | `FF` |
| no connect at all, immediately after a successful `DIR D:` | `FF` |
| `ECR` forced to SPP mode `0x14` before the magic bytes | `FF` |

**Controls that make those results trustworthy:**
- `DIR D:` still works after every probe — 125,958,144 bytes free. The probing does **not**
  destabilise the bridge, and the driver re-establishes what it needs each operation.
- The driver leaves the port quiescent (`control=0x14`, `ECR=0x35`, i.e. PS/2 mode, FIFO empty),
  so it disconnects after each operation and a connect is genuinely mandatory.
- The ECP read replay was diffed instruction-by-instruction against handler `0x3CCE`: identical.

Every element is individually verified and the composite still does not work, so the missing state
is somewhere in the driver's boot-time initialisation that has not been found by reading. **Blind
replay is the wrong instrument for the remainder.**

### Proposed next instrument: log the driver's real port sequence in 86Box

Add a minimal Shuttle EPAT bridge stub to the 86Box fork - enough to answer the magic-byte
checkpoints with `B0`/`50`/`B0` - then run `SD120PPD.SYS` in the emulator and log every access to
`0x378`-`0x37A` and `0x778`/`0x77A`. That yields the complete, exact connect-and-configure sequence
with no ambiguity, entirely offline, and no further hardware time.

It also removes a standing limitation: there is currently **no way to test an LS-120 driver in
emulation at all**, so every iteration costs real hardware. A bridge stub fixes that permanently.

## ⭐⭐ 4e. Register reads work on real hardware — 2026-09-08

> ### ⚠ CORRECTED 2026-09-10: this is verified WITH THE VENDOR DOS DRIVER RESIDENT
>
> The recipe below is right, and an independent reimplementation of it returns `0x50` on the
> real machine. But it is **not solved standalone**. Same DEBUG script, same machine, minutes
> apart, one variable:
>
> | `SD120PPD.SYS` in `CONFIG.SYS` | ATA status at `0x18+7` |
> |---|---|
> | REM'd out | **`0x00`** |
> | loaded | **`0x50`** |
>
> The CPP connect is **not** the missing piece - its checkpoints return `B8 58 F0` in both
> cases, so the handshake succeeds with or without the vendor driver. What changes is state the
> vendor driver establishes **at boot**, which our connect does not reproduce. That is exactly
> the gap section 4c identified and section 4e then declared solved.
>
> **Why it was missed:** every 2026-09-08 run kept the vendor driver loaded, using `DIR D:` as
> the control that the hardware was healthy. The control was silently a **dependency**. A
> control you never remove is indistinguishable from a prerequisite.
>
> **Consequence for the driver:** `LS120MP.MPD` implements this recipe faithfully and will
> still find nothing on a machine that has not loaded the vendor DOS driver. The next work is
> section 4c's bridge configuration - `0x25EB` writing bridge registers `0x12` and `0x0D` - or
> whatever else the vendor does at init. **Do not install the miniport expecting it to work
> until that is found.**


```
ATA status    (0x18+7) = 0x50   DRDY | DSC      <- a ready drive
ATA LBA mid   (0x18+4) = 0x00
ATA LBA high  (0x18+5) = 0x02
ATA drv/head  (0x18+6) = 0xA0   canonical master select
IDE control   (0x10+7) = 0x08
```

**The working recipe, verified on the real 5160:**

1. **CONNECT** = `CPP(0xE0)`: write `22 AA 55 00 FF 87 78 E0` to the data port (each byte twice,
   with an I/O delay between groups), then pulse control **`0x04` -> `0x05` -> `0x04`**.
2. **Address a register** by `cont_map` offset, not a bare number:

   | space | offset |
   |---|---|
   | internal bridge registers | `0x00` |
   | IDE control | `0x10` |
   | **IDE task file** | **`0x18`** |

   So ATA status = `0x18 + 7` = `0x1F`.
3. **READ** with the NIBBLE handler (`0x3A1B`): write the register number to the data port,
   control `0x01`, then `0x03` twice, read status (low nibble in bits 7-4), control `0x04` twice,
   read status again (high nibble), combine as `(second & 0xF0) | (first >> 4)`.
4. **DISCONNECT** = `CPP(0x30)`, restoring the saved control/data values.

### What was wrong all day

Every probe used bare register numbers 0-7, which is the **internal bridge** space, not the task
file. The connect was correct from the first hardware run; the addressing never was. `0xE0` is the
*connect* command and `0x30` the *disconnect* - not transfer modes, which is why `[0C02]` read `0x30`
at rest (disconnected) and `0xE0` after a `DIR` (connected).

### Independently corroborated

Linux's `epat.c` describes the identical CPP sequence - `22 AA 55 00 FF 87 78` + mode with control
toggled `4 -> 5 -> 4` - and the same `cont_map` offsets (`0x00` internal, `0x10` IDE control,
`0x18` IDE registers). Two unrelated sources agree, and the hardware confirms both. Used as a
cross-check only; no code taken.

### Still open

**ECP reads return `FF` with the reverse-direction wait expiring**, under every connect mode and
configuration tried. Nibble works, so this does not block a driver - it is a speed optimisation.
Ship nibble first.

**Nibble is also the portable answer**: it uses only the three standard SPP ports, so it works on a
plain XT parallel port with no ECP hardware at all.

## 4f. ECP IS shippable — it belongs to the DATA phase, not register access

Per-register ECP reads time out because a register read has no data phase: the reverse FIFO never
fills. That is not a defect and it does not cost throughput. The speed lives in the block path:

```asm
4458  sub dx,2 ; in al,dx ; and al,1Fh ; or al,20h ; out dx,al   ; control bit 5 = reverse
4461  add dx,2                                                   ; dx = base+0x400 = 0x778
4465  rep insb es:[di], dx        ; 512 bytes out of the ECP FIFO, hardware handshake
```

`rep outsb` at `0x4BD3` / `0x4C55` / `0x4E12` is the write equivalent.

**Architecture to build:** NIBBLE for the task file (a few bytes per command, and portable to any
SPP port), **ECP `rep insb`/`rep outsb` for sector data**. That is where every byte actually moves,
so it delivers the ECP card's benefit without needing per-register ECP to work at all.

A slower fallback exists at `0x479E` — `in ECR / test al,1 / insb` byte-at-a-time — and a variant at
`0x47B2` waits on **ECR bit 2** (`test al,4`, serviceIntr) rather than bit 0. Useful if the `rep`
form misbehaves.

### ⚠ Do NOT copy the vendor's DMA-assisted block path — it writes 0x22/0x23

```asm
4444  mov ax,8000h ; out 23h,al ; out 22h,al ; out 22h,ax ; in al,22h ; or al,1 ; out 22h,al
```

On this XT, ports `0x22`/`0x23` **alias onto the 8259** (technique 75) — this is the mechanism that
killed the keyboard in issue #22, and it lives in the *block transfer* path, not merely in chipset
init. Our driver must use the PIO `rep insb` form only, and `dmaEn` stays 0 (technique 62: the XT's
20-bit DMA reach silently truncates any buffer above 1 MB).

## 5. The transports

### `ECP Write` — handler `0x4932`, 89 bytes **[DERIVED]**
Entry `AL` = register number, `AH` = value.
```
ECR = 0x14 ; control = 0x04 ; ECR = 0x74
wait ECR bit0 == 1 (FIFO empty), cx = 0xFFFF
data(0x378) = register number
wait ECR bit0 == 1
FIFO(0x778) = value
wait ECR bit0 == 1
ECR = 0x34
```

### `ECP Read` — handler `0x3CCE`, 139 bytes **[DERIVED]**
Entry `AL` = register number; returns the byte in `AL`.
```
control = 0x04 ; ECR = 0x74
wait ECR bit0 == 1, cx = 0xFFFF        ; fail -> AH=0xFF, ECR=0x34, return
data = register number
wait ECR bit0 == 1                     ; fail -> as above
ECR = 0x34
control = 0x20                         ; reverse the channel to input
ECR = 0x74
wait ECR bit0 == 0, cx = 0x8000        ; loopne - wait for data AVAILABLE
al = in FIFO(0x778)
control = (control & 0x10) | 0x04      ; forward again, IRQ-enable preserved
ECR = 0x34
```

Three details that will bite if missed:
- **The waits are opposite senses.** Forward spins on FIFO-*empty*; the reverse phase on
  FIFO-*not*-empty. Backwards deadlocks or reads garbage.
- **Reverse timeout is `0x8000`, forward `0xFFFF`.** Deliberate.
- **Every failure path restores `ECR = 0x34` and un-reverses the port.** On this machine an
  abandoned reverse channel wedges the port, so the failure paths matter more than the happy path.

## 6. SPP / nibble — the portable fallback **[DERIVED]**

Answering "not everyone has an ECP card": the universal modes are small and use **only** the three
standard SPP ports. No ECP hardware, so they work on a plain XT parallel port.

### `NIBBLE Normal` — read selector 0, `0x3A1B`, 38 bytes
```
data = reg | 0x00
control = 0x01 ; control = 0x03 (x2)
first  = status & 0xF0            ; low nibble, in bits 7-4
control = 0x04 (x2)
second = status & 0xF0            ; high nibble
result = (second & 0xF0) | (first >> 4)
```

### `WRITE Normal` — write selector 0, `0x4847`, 24 bytes
```
data = reg | 0x60 ; control = 0x01 (x2) ; data = value ; control = 0x04
```

Note the tags: writes carry `| 0x60`, reads the bare register number. Full table in §7.

**Recommendation:** implement ECP as the fast path and NIBBLE/WRITE Normal as the portable one, and
select between them with an `AdapterSettings` string (`MODE=ECP` / `MODE=NIBBLE`) exactly as
`XTIDEMP.MPD` takes `PORT=0x300` — no detection code, which is most of what makes the vendor driver
79,872 bytes. Ship only what can be tested: ECP is testable here, nibble needs a plain SPP port.

## 7. Mode tables — decoded in full **[DERIVED]**

READ table `0x4E9D`, 15 entries × 8 bytes; WRITE table `0x4F15`, 9 real entries.

| sel | READ handler | mode | | sel | WRITE handler | mode |
|---|---|---|---|---|---|---|
| 0 | `3A1B` | NIBBLE Normal | | 0 | `4847` | WRITE Normal |
| 1 | `3A55` | NIBBLE Slow | | 1 | `4873` | WRITE Fast(+) |
| 2 | `3A90` | NIBBLE Slow(-) | | 2 | `48CD` | WRITE Slow(-) |
| 3 | `3915` | UNIDIR Normal | | 3,4 | `499F` | EPP BIOS(N) |
| 4 | `3952` | UNIDIR Slow | | 5 | `4A17` | unnamed, bridge fn `0Ch` |
| 5 | `3990` | UNIDIR two wait | | **6** | **`4932`** | **ECP Write (in use)** |
| 6 | `3B17` | TOSHIBA Normal | | 7 | `48A0` | WRITE Slow |
| 7 | `3B52` | PS/2 Fast | | 8 | `48FD` | ECP Write via EPP-BIOS |
| 8 | `3B93` | PS/2 Normal | | | | |
| 9,10,11 | `3BC5`/`3C26` | EPP BIOS(F)/(N) | | | | |
| 12 | `3CAF` | ECP Read via EPP-BIOS | | | | |
| **13** | **`3CCE`** | **ECP Read (in use)** | | | | |
| 14 | `3AD0` | TOSHIBA Fast | | | | |

## 8. Selectors 12 and 8 — EPP-BIOS variants, NOT in use **[MEASURED]**

Read selector 12 (`0x3CAF`) is `mov dl,[0BC9] ; mov ah,0Bh ; lcall [0BE1]` — a far call into a
module this driver does not contain. `[0BE1]` is written at exactly one site (`0xC068`), only after
an `INT 17h` handshake (`AH=2`, `BX='PP'`, `CH='E'`, expecting `'PEP'` back), which `/db` skips.
Live, `[0BE1] = 0000:0000`. We need none of it.

## 9. Still to do

1. **Run the mode-`0xE0` replay** (`tools/e0.dbg`, or `modesweep.dbg` for all eight modes). One
   DEBUG run settles whether the transport is complete. Needs COMrade file I/O working — a reboot
   of the box restores it.
2. Disconnect/teardown sequence — not yet located.
3. ATAPI packet issue and data phase. The block-transfer path at `0x4C83` uses the EPP-BIOS vector
   and is **not** our model; find the ECP block path.
4. Size estimate unchanged at **8-12 KB**: transport is 89 + 139 bytes, connect ~60, plus dispatch.

## 10. Hardware target

Bridge SHUTTLE EPATRM · drive Matsushita LS-120 COSM 04 · port `0x378` · IRQ 7 · `dmaEn = 0`
(technique 62 — DMA stays off on this machine) · **no chipset init** (`/ni`: those writes alias onto
the 8259 and killed the keyboard in #22, and the drive works without them).

## Licence

MIT, reverse-engineered. Register maps, port sequences and protocol facts are not copyrightable;
the vendor's code is. Nothing here is transcribed. Linux's `drivers/block/paride/epat.c` is GPL-2.0
— usable as an independent cross-check on protocol facts, but **its code must not be copied into
this repo** without relicensing that part deliberately.

[#22]: https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22

---

## 4g. 2026-09-10 - the dependency is real and COLD-CONFIRMED, and four fixes did not find it

An independent MIT reimplementation (`LS120MP.MPD`) emits the section 4e recipe and was replayed
byte-for-byte on the real 5160. Results, each on a **fully powered-down** machine:

| condition | ATA status at `0x18+7` |
|---|---|
| cold, `SD120PPD.SYS` REM'd out | **`0x00`** |
| after a boot where the vendor driver loaded, then warm-rebooted with it REM'd | `0x50` |
| vendor driver loaded | `0x50` |

**The connect itself is NOT the problem** - its checkpoints return `B8 58 F0` in every condition,
cold or warm. Something the vendor establishes **at boot** is missing, and it **survives a warm
reboot**, which is why earlier sessions read as "solved": every 09-08 run had the vendor driver
loaded as a control, and Ctrl+Alt+Del never cleared it. See technique 110.

### Tried on hardware and eliminated

| hypothesis | source | result |
|---|---|---|
| `ECR = 0x34` before connect | `SD120PPD.MPD` `0x6db4`, gated on port type `0x0C` | still `0x00` |
| magic bytes written **8x** not 2x | `SD120PPD.SYS` `0x2800`, the init-time connect | still `0x00` |
| `CPP(0)` before `CPP(0xE0)`, plus the trailing `w0(0xff)` | `epat.c` `CPP` macro | still `0x00` |
| the above **plus** `WR(8,0x10) WR(0xc,0x14) WR(0xa,0x38) WR(0x12,0x10)` | `epat.c` `epat_connect` | still `0x00` |

⚠ **The last one is not a trustworthy negative.** `WR(r,v)` is `epat_write_regr(pi,**2**,r,v)` -
**cont = 2**. If `cont_map` is `{0, 0x10, 0x18}` then `WR(8,·)` addresses `0x18+8`, not `0x08`.
That test assumed the internal space and may have written to entirely the wrong registers.
**Resolve `cont_map` before re-running it.**

### What is confirmed good

- The CPP frame, the `cont_map` offsets and the nibble read are all correct - the same code returns
  `0x50` the moment the bridge has been initialised.
- `LS120MP.MPD` is 5,632 bytes with **zero** destructive-write candidates (`xt_port_audit.py`), so
  the issue #22 keyboard-killer is absent by construction rather than by patching.

### Sources held and NOT yet read - read these before more hardware guessing

1. **`SD120PPD.SYS` `0x11dd3`** - the detect routine that gates the whole of `0xcea0` (which is
   `INIT` -> `0x81e9` -> `0xcea0`). Never disassembled. This is the boot-time path the evidence
   points at, and it is the most likely place the answer lives.
2. **`epcfw2k.sys`** - a Windows 2000 driver for a **Shuttle EPAT CF reader**, i.e. a THIRD
   independent implementation of this connect, on the same bridge family. Plain PE, readable with
   `tools/pedis.py`. On the owner's USB drive under `cf_driver/Windows 2000 Driver/`.
3. **The Win95/98 EPAT CF driver** in the same package, inside `DATA.Z` (InstallShield) - needs
   unpacking first.
4. `reference_gpl/epat.c` is now pinned locally at v6.1 (gitignored; paride was removed from
   mainline and the URL 404s). MIT/GPL boundary rules are in `reference_gpl/README.md`.

### The method lesson

Four hypotheses, four cold-boot cycles, no convergence. **Per-hypothesis hardware testing is the
wrong instrument for the remainder** - each run costs a full power-cycle and tests one guess.
Section 4d already proposed the right one and it still stands: **an EPAT bridge stub in 86Box**,
enough to answer the CPP checkpoints, then run `SD120PPD.SYS` under it and log every access to
`0x378`-`0x37A` and `0x778`/`0x77A`. That yields the exact bring-up with no guessing, offline, and
permanently removes the fact that an LS-120 driver cannot currently be tested in emulation at all.


---

## 4h. `cont_map` resolved, and the branch never tried (2026-09-10)

### `cont` numbering, settled

The ambiguity flagged in 4g is closed. From `reference_gpl/epat.c`:

```c
static int cont_map[3] = { 0x18, 0x10, 0 };
#define WR(r,v)  epat_write_regr(pi,2,r,v)     /* cont 2 -> offset 0x00 */
#define WRi(r,v) epat_write_regr(pi,0,r,v)     /* cont 0 -> offset 0x18, the ATA task file */
```

`cont 2` is offset `0x00`. Our register addressing **was** correct, so the fourth negative in 4g
(the `WR(8,0x10) WR(0xc,0x14) WR(0xa,0x38) WR(0x12,0x10)` sequence) is trustworthy after all -
it did write to the internal register space it was meant to, and the bridge still read `0x00`.

The read idiom also matches ours exactly: `w0(r); w2(1); w2(3); a=r1(); w2(4); b=r1(); j44(a,b)`.

### `epat_connect` has two branches, and we have only ever run one

```c
static int epatc8;                 /* CONFIG_PARIDE_EPATC8 - Shuttle EP1284 support */
...
	CPP(0);
	if (epatc8) {
		CPP(0x40); CPP(0xe0);
		w0(0); w2(1); w2(4);
		WR(0x8,0x12); WR(0xc,0x14); WR(0x12,0x10);
		WR(0xe,0xf);  WR(0xf,4);
		WR(0xe,0xd);  WR(0xf,0);
	}
	CPP(0xe0);
	w0(0); w2(1); w2(4);
	...
	if (!epatc8) {
		WR(8,0x10); WR(0xc,0x14); WR(0xa,0x38); WR(0x12,0x10);
	}
```

`epatc8` is a **build-time** option, not autodetected - a kernel built without it can never bring
up an EP1284-class bridge, and vice versa. Every hardware test so far has been the `!epatc8` path.

Two things in the `epatc8` branch have never been sent to this bridge:

- **`CPP(0x40)`** - an extra CPP frame with a mode byte we have never used.
- a different register set: `0x8 = 0x12` (not `0x10`), no `0xa`, and the `0xe`/`0xf` pairs
  `(0x0f,0x04)` then `(0x0d,0x00)`.

Our bridge is a **SHUTTLE EPATRM**. Whether that is EP1284-class is unknown, but this is the one
branch of the one authoritative implementation that has not been tried, and it is cheap.

The probe is committed as `drivers/imation_ls120/tools/gen_epatc8_probe.py`; it emits a
base64 DEBUG script (CRLF - technique 109c) built with DEBUG's own assembler (technique 109b, so
no hand-computed `rel16`). Run it cold with `SD120PPD.SYS` REM'd out and read `[0200]`:
`0x50` means this is the missing initialisation and `LS_Connect` gains the sequence; `0x00` means
the branch is not it either and the 86Box EPAT stub (4d) becomes the only sane instrument left.
