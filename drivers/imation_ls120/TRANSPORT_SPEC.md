# LS-120 parallel-port transport — specification from `SD120PPD.SYS`

Offline reverse-engineering of the vendor DOS driver, **confirmed against the live machine**, for a
clean-room MIT reimplementation as a Windows 95 `.MPD` (issue [#22]). No code is transcribed from
the vendor binary — register facts and protocol sequences only.

Source: `SD120PPD.SYS.orig`, 56,198 bytes, md5 `cbb42e8eb7847869e274e45f258cf718`, April 1997.
A DOS `.SYS` is a raw binary image, so **file offset == CS offset** throughout.

Tools in `tools/`: `dump_mode_tables.py`, `sysdis.py`, `xref.py`.

**Status: the transport is fully specified and self-contained.** The connect sequence and the
ATAPI layer are not yet done.

---

## 0. Live measurement, 2026-09-08 — this is the ground truth

Real 5160, DOS boot, `CONFIG.SD1` config, LS-120 attached with media, read over COMrade.

Driver located by walking MEM's block accounting and **verified against its own device header**
(`SCSIMGR$`, attribute `C000`, strategy `2074`, interrupt `2082`) at segment **`0575`**:

```
[0BC1] EPP-BIOS probe disabled = 01        <- /db took effect
[0BC9] LPT unit                = 00        <- LPT1
[0BCB] READ  mode name ptr     = 3CBA      -> 'ECP Read'
[0BCF] WRITE mode name ptr     = 491E      -> 'ECP Write'
[0BD7] port type               = 0C        <- ECP-capable
[0BD9] READ  selector          = 000D      <- 13
[0BDB] WRITE selector          = 0006      <- 6
[0BE1] PEP entry offset        = 0000      }  the EPP-BIOS far vector is NULL
[0BE3] PEP entry segment       = 0000      }
[0BFA] LPT base                = 0378
[0BFC] IRQ                     = 07
```

**This corrected a wrong static conclusion.** A first pass inferred selector 12 for `ECP Read` and
8 for `ECP Write`, from a model in which each table entry's second word is the handler's length and
the mode-name string follows the handler. That model reproduces 13 of 15 read and 9 of 9 write
names, which is why it looked right — but it is a **coincidence of contiguous code layout**, not
how the driver selects or names a mode. The live selectors are 13 and 6.

Selector 12's handler really is the `lcall`-based one described in §5, and it really would be
unusable with `/db`. It simply is not the one in use. The apparent contradiction was an artefact of
my own model, not a property of the machine.

## 1. The two dispatchers

Re-derived from code, and now confirmed by the live selectors resolving through them:

```asm
244B  READ  reg:  push dx/bx, pushf, pushf, cli
                  bx = [0BD9] << 3          ; read-method selector * 8
                  al = dl                   ; register number
                  dx = [0BFA]               ; LPT base
                  call word ptr cs:[bx + 4E9Dh]
                  ; restore; STI only if the caller's IF was set (test bh,2)

2472  WRITE reg:  same prologue
                  bx = [0BDB] << 3
                  ah = al                   ; value
                  al = dl                   ; register number  (xchg dl,al)
                  dx = [0BFA]
                  call word ptr cs:[bx + 4F15h]
```

`handler = word[table + selector*8]`. That formula is definitive — it is what the CPU executes.
The other three words of each entry are not uniform and are not needed.

Both are interrupt-safe: `cli` across the port sequence, IF restored from the caller's own flags.
**Any reimplementation must do the same** — these sequences cannot tolerate an interrupt mid-handshake.

Live selectors resolve as:

| | selector | handler |
|---|---|---|
| READ | 13 | `word[4E9D + 13*8 = 4F05]` = **`0x3CCE`** |
| WRITE | 6 | `word[4F15 + 6*8 = 4F45]` = **`0x4932`** |

## 2. Port map

LPT base `0x378`, so:

| address | register |
|---|---|
| `base + 0` = `0x378` | SPP data — carries the **register number** (address phase) |
| `base + 2` = `0x37A` | SPP control — `nInit`, and bit 5 = **direction** (1 = reverse/input) |
| `base + 0x400` = `0x778` | **ECP data FIFO** — carries the **value** (data phase) |
| `base + 0x402` = `0x77A` | **ECR**, extended control. Bit 0 = FIFO empty. Mode in bits 7-5 |

ECR values the driver uses: `0x74` = ECP mode, `0x34` and `0x14` = quiescent/other modes.
The earlier standalone measurement that ECR at `0x77A` answers `0x35` is consistent with this.

## 3. `ECP Write` — read register value out, complete

Handler `0x4932`, ends `0x498A` (89 bytes). Entry: `AL` = register number, `AH` = value.

```
ECR      = 0x14
control  = 0x04
ECR      = 0x74                  ; ECP mode
wait ECR bit0 == 1 (FIFO empty), cx = 0xFFFF     ; timeout -> tidy up and return
data     = register number       ; base+0, address phase
wait ECR bit0 == 1               ; timeout -> tidy up and return
FIFO     = value                 ; base+0x400, data phase
wait ECR bit0 == 1
ECR      = 0x34                  ; quiescent
ret
```

## 4. `ECP Read` — complete, and it reverses the channel

Handler `0x3CCE`, ends `0x3D58` (139 bytes). Entry: `AL` = register number. Returns the byte in `AL`.

```
control  = 0x04
ECR      = 0x74                  ; ECP mode
wait ECR bit0 == 1, cx = 0xFFFF  ; timeout -> AH = 0xFF, ECR = 0x34, return
data     = register number       ; base+0, address phase
wait ECR bit0 == 1               ; timeout -> AH = 0xFF, ECR = 0x34, return
ECR      = 0x34
control  = 0x20                  ; direction bit SET: reverse the channel to input
ECR      = 0x74                  ; ECP mode again
wait ECR bit0 == 0, cx = 0x8000  ; loopne - wait for data AVAILABLE, not empty
                                 ; timeout -> AH = 0xFF, ECR = 0x34, return
al       = in FIFO               ; base+0x400, the data byte
control  = (control & 0x10) | 0x04   ; direction cleared, IRQ-enable preserved
ECR      = 0x34                  ; quiescent
ret                              ; byte in AL
```

Three details worth carrying into the reimplementation:

- **The two waits are opposite senses.** Forward phases spin on FIFO-*empty* (`loope`); the reverse
  phase spins on FIFO-*not*-empty (`loopne`). Getting that backwards deadlocks or reads garbage.
- **The reverse timeout is `0x8000`, the forward ones `0xFFFF`.** Deliberate, not a typo.
- **Every failure path still restores `ECR = 0x34` and returns `AH = 0xFF`.** The port is never
  left in ECP mode or reversed. Preserve that; the failure paths matter more than the happy path
  on a machine where an abandoned reverse channel would wedge the port.

## 5. Selectors 12 and 8 — the EPP-BIOS variants, NOT in use

Recorded so nobody re-derives them. Read selector 12 (`0x3CAF`, 11 bytes) is:

```asm
mov dl,[0BC9] · mov ah,0Bh · lcall [0BE1] · ret
```

`[0BE1]`/`[0BE3]` is a far pointer this driver does **not** contain the target of. Verified: BSS
default is zero, and an exhaustive search of every store form finds exactly one writer, `0xC068`,
reached only after an `INT 17h` handshake (`AH=2`, `BX='PP'`, `CH='E'`, expecting `'PEP'` back and
the entry in `DX:BX`). `[0BC1] == 1` skips that probe entirely, which is what `/db` does.

Live, `[0BE1] = 0000:0000`. So every mode routed through it — `EPP BIOS(N)`, `EPP BIOS(F)`,
read selector 12, write selector 8, and the block-transfer routine at `0x4C83` — is unavailable in
our configuration, and the driver correctly avoids all of them.

**We do not need any of it.** Selectors 13 and 6 are self-contained.

## 6. Switch bitmask → flag byte, `0xAAA1`

The parser builds a mask in `AX`; this routine fans it out. `[0BC1]` measured as 1 with `/db`
present, consistent with bit 0 being `/db`.

| AX bit | flag byte |
|---|---|
| 0 | `[0BC1]` — gates the EPP-BIOS `INT 17h` probe |
| 2 | `[0BC3]` |
| 3 | `[0BF9]` |
| 4 | `[0D87]` |
| 5 | `[0C5D]` |
| 7 | `[0BE0]` |
| 9 | `[0BC2]` |

All seven are zeroed first, so an unset bit means the feature stays enabled.

## 7. Driver state variables

All BSS — zero in the file image, filled during `INIT`. Live values in §0.

| address | meaning |
|---|---|
| `[0BC1]` | EPP-BIOS probe disabled |
| `[0BC9]` | LPT unit number, 0-based |
| `[0BCB]` / `[0BCF]` | READ / WRITE mode-name string pointers (what the driver prints) |
| `[0BD7]` | port type — `0x0C` = ECP-capable |
| `[0BD9]` / `[0BDB]` | READ / WRITE method selectors |
| `[0BE1]` / `[0BE3]` | EPP-BIOS far entry, offset / segment |
| `[0BFA]` | LPT I/O base |
| `[0BFC]` | IRQ |

The driver reports its choice at load as `    Read  Mode : ` and `    Write Mode : `, and prints
`Specified mode failed. Loading with detected modes.` if a forced mode did not take — so the
printed mode is always the one in force.

## 8. Still to do

- **The connect sequence** before `0x25C1` — the `0x2DA5` cluster and the callers of `0x25A0`.
  This is the remaining unknown, and the live link is the way to settle it.
- ATAPI packet issue and data phase. The block-transfer path at `0x4C83` uses the EPP-BIOS vector
  and is therefore *not* our model; find the ECP block path instead.
- Size estimate **unchanged at 8-12 KB** — the transport is 89 + 139 bytes of port I/O plus the
  two dispatchers, and none of it needs the external module.

## 9. Known hardware target

Bridge SHUTTLE EPATRM · drive Matsushita LS-120 COSM 04 · port `0x378` · IRQ 7 · `dmaEn = 0`
(technique 62 — DMA must stay off on this machine) · **no chipset init at all** (the `/ni` finding:
those writes alias onto the 8259 and killed the keyboard in #22, and the drive works without them).

## Licence

Stay MIT. Register maps, port sequences and protocol facts are not copyrightable; the vendor's code
is. Nothing here is transcribed, and the implementation must be written fresh from this
specification. Linux's `drivers/block/paride/epat.c` is GPL-2.0 and **must not** be used as a source.

[#22]: https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22
