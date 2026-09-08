# LS-120 parallel-port transport — specification from `SD120PPD.SYS`

Offline reverse-engineering of the vendor DOS driver, for a clean-room MIT reimplementation
as a Windows 95 `.MPD` (issue [#22]). **No code is transcribed from the vendor binary** —
register facts and protocol sequences only.

Source: `SD120PPD.SYS.orig`, 56,198 bytes, md5 `cbb42e8eb7847869e274e45f258cf718`,
dated April 1997. A DOS `.SYS` is a raw binary image, so **file offset == CS offset** throughout.

Tools used, all in `tools/`: `dump_mode_tables.py`, `sysdis.py`, `xref.py`.

---

## 1. The two dispatchers — verified, not inherited

Re-derived from the code rather than taken from the handoff note (they agree):

```asm
244B  READ  reg:  push dx/bx, pushf, pushf, cli
                  bx = [0BD9] << 3          ; read-method selector * 8
                  al = dl                   ; register number
                  dx = [0BFA]               ; LPT base
                  call word ptr cs:[bx + 4E9Dh]
                  ; restore, and STI only if the caller's IF was set (test bh,2)

2472  WRITE reg:  same prologue
                  bx = [0BDB] << 3          ; write-method selector * 8
                  ah = al                   ; value
                  al = dl                   ; register number  (xchg dl,al)
                  dx = [0BFA]
                  call word ptr cs:[bx + 4F15h]
```

Both are interrupt-safe: `cli` across the port sequence, IF restored from the caller's own
flags. Any reimplementation must do the same — these sequences cannot tolerate an interrupt
mid-handshake.

## 2. Mode tables — decoded in full

**This corrects the handoff**, which recorded "only 10 entries of each were dumped; there are
more". There are 15 read entries and 9 real write entries, and all of them now resolve.

Entry layout is `{ handler_offset:word, handler_length:word, ... }` and **the mode-name string
sits immediately at `handler + length`**. That is what makes the table self-validating: 13 of 15
read entries and 9 of 9 write entries produce a sensible name.

### READ table, `0x4E9D`, 15 entries × 8 bytes

| sel | handler | len | mode |
|---|---|---|---|
| 0 | `3A1B` | `26` | NIBBLE Normal |
| 1 | `3A55` | `27` | NIBBLE Slow |
| 2 | `3A90` | `2C` | NIBBLE Slow(-) |
| 3 | `3915` | `29` | UNIDIR Normal |
| 4 | `3952` | `2A` | UNIDIR Slow |
| 5 | `3990` | `30` | UNIDIR two wait |
| 6 | `3B17` | `27` | TOSHIBA Normal |
| 7 | `3B52` | `2D` | PS/2 Fast |
| 8 | `3B93` | `1E` | PS/2 Normal |
| 9 | `3BC5` | `23` | EPP BIOS(F) |
| 10 | `3C26` | `75` | EPP BIOS(N) |
| 11 | `3C26` | `75` | EPP BIOS(N) |
| **12** | **`3CAF`** | **`0B`** | **ECP Read** |
| 13 | `3CCE` | — | unresolved, fields do not follow the pattern |
| 14 | `3AD0` | `33` | TOSHIBA Fast |

### WRITE table, `0x4F15`, 9 entries × 8 bytes

| sel | handler | len | mode |
|---|---|---|---|
| 0 | `4847` | `18` | WRITE Normal |
| 1 | `4873` | `19` | WRITE Fast(+) |
| 2 | `48CD` | `1C` | WRITE Slow(-) |
| 3 | `499F` | `64` | EPP BIOS(N) |
| 4 | `499F` | `64` | EPP BIOS(N) |
| 5 | `4A17` | `0F` | unnamed — bridge fn `0Ch`, `al |= 0x40` |
| 6 | `4932` | — | EPP Normal |
| 7 | `48A0` | `19` | WRITE Slow |
| **8** | **`48FD`** | **`21`** | **ECP Write** |

Entries 9-14 of the write table decode as garbage, so the table's real length is 9 —
`4F15 + 9*8 = 4F5D`, after which the region is something else.

## 3. `ECP Write` — complete, self-contained

Write table selector 8, `0x48FD`, 33 bytes. Entry: `DX` = LPT base, `AL` = register number,
`AH` = value. Pure port I/O, no external dependency:

```
al = reg | 0x60
out  base+0, al        x3      ; register select
out  base+2, 0x01      x6      ; control: strobe low
al = value
out  base+0, al        x3      ; data
out  base+2, 0x04      x3      ; control: latch
ret
```

The repeated `out` to the same port with the same value is **ISA bus timing stretch**, not a
protocol repeat — the standard idiom for widening setup/hold on a slow parallel-port bridge.
Whether all three/six are needed at 4.77 MHz XT bus speed is an open question; keeping them
is the safe default.

## 4. ⚠ `ECP Read` is NOT port I/O — and this changes the size estimate

Read table selector 12, `0x3CAF`, 11 bytes:

```asm
3CAF  mov dl, byte ptr [0BC9]     ; LPT unit number
3CB3  mov ah, 0Bh                 ; function code
3CB5  lcall [0BE1]                ; far call to an EXTERNAL module
3CB9  ret
```

`[0BE1]`/`[0BE3]` is a far pointer that **this driver does not contain the target of**. Verified:

- The image's own BSS at `0BE1` is **all zero** — there is no built-in default.
- An exhaustive search for every store form to `0BE1` (`mov [0BE1],bx/ax/cx/dx/si/di`,
  `mov word [0BE1],imm`, `mov di/si,0BE1`) finds exactly **two** sites, and only one writes:
  - `0xC068` — the installer.
  - `0x4C8B` — `mov di,0BE1` then `lcall [di]`, a *reader* (block transfer, fn `0Eh`).

`0xC068` installs the pointer only after an **`INT 17h` magic handshake**:

```asm
C03C  al = 0 ; ch = 45h 'E' ; bx = 5050h 'PP' ; ah = 2 ; dx = LPT index
C046  mov [0BC9], dl
C04A  cmp byte ptr [0BC1], 1
C04F  jne C054
C051  jmp C1EF                    ; <-- skip the probe entirely, install nothing
C054  int 17h
C056  cmp cl, 50h 'P'  /  cmp al, 45h 'E'  /  cmp ch, 50h 'P'
C068  mov [0BE1], bx  ;  mov [0BE3], dx     ; entry point returned in DX:BX
```

This is the driver's **"EPP BIOS" check** — the `/db` switch is documented as *"Disables Eppbios
check"*. `C1EF` is the containing function's plain exit path; taking it leaves `[0BE1]` null.

**Consequence:** every mode that goes through `lcall [0BE1]` — `EPP BIOS(N)`, `EPP BIOS(F)`,
write selector 5, the block-transfer routine at `0x4C83`, **and `ECP Read`** — depends on an
external `INT 17h` responder. Reimplementing those means reimplementing a module that is not in
this file.

`ECP Write`, by contrast, has no such dependency. The two directions are asymmetric.

## 5. ⚠ OPEN — the contradiction that must be settled before writing transport code

The working configuration on the machine (`D:\CONFIG.SD1` line 17) is:

```
DEVICEHIGH=C:\SD120PPD\SD120PPD.SYS /port:378 /IRQ:7 /de /db /ni /sf /dpc /dp /fp
DEVICEHIGH=C:\SD120PPD\ASPIHDRM.SYS
```

That carries **`/db`**. If `/db` is the switch that sets `[0BC1]`, the EPP-BIOS probe never runs,
`[0BE1]` stays null, and read selector 12 (`ECP Read`) would far-call a null pointer. Yet the
recorded measurement for this machine is that the driver's free choice was **`ECP Read` /
`ECP Write`**.

Both cannot be true. One of these is wrong:

1. the recorded mode measurement was taken with a different switch set;
2. `/db` is not the switch behind `[0BC1]`;
3. something on the machine (`ASPIHDRM.SYS`?) answers the handshake by another route.

**Why it decides the whole job.** If the read path genuinely needs the external module, our
miniport has to reimplement code that is not in this binary, and the 8-12 KB estimate is wrong.
If the machine actually settles on a self-contained read mode (NIBBLE / PS/2 / UNIDIR — all of
which are 30-45 bytes of plain port I/O in this table), the transport is small and fully
decodable from what we already have.

**Cheapest resolution, in order:**

1. *Offline, partly done* — two routes were tried and neither closed it in this pass:
   - **Switch → bit.** The bit→flag map is decoded (§6) but the switch→bit half is not. The
     fan-out routine `0xAAA1` has exactly one caller, `0x8640`, which is reached from `0x8625`
     taking flag masks in both `AX` and `DX`; the parser is a further hop back. Keyword table
     at `0x6470`: `r w de ded db dp di fe fev fp ni i irq z PORT P sf pd dpc`.
   - **Selector → mode.** `[0BD9]`/`[0BDB]` have **no direct store anywhere in the image**
     except their initialisation to `0` at `0x8842`/`0x8848`, so the detection loop assigns
     them indirectly (through a pointer). Locating that loop would show directly whether
     selector 12 is guarded on `[0BE1] != 0`, which is the cleanest possible answer.
2. *One measurement at the machine* — load with the exact `CONFIG.SD1` line and read the mode
   the driver prints, then re-read it with `/db` removed. Two boots, no code, and it settles
   the question outright regardless of how the selection code is written.

## 6. Switch bitmask → flag byte, `0xAAA1`

The parser builds a mask in `AX`; this routine fans it out. Decoded in full:

| AX bit | flag byte |
|---|---|
| 0 | `[0BC1]` — gates the EPP-BIOS `INT 17h` probe (§4) |
| 2 | `[0BC3]` |
| 3 | `[0BF9]` |
| 4 | `[0D87]` |
| 5 | `[0C5D]` |
| 7 | `[0BE0]` |
| 9 | `[0BC2]` |

All seven are zeroed first, so an unset bit means "feature enabled" in each case.

## 7. Driver state variables

| address | meaning |
|---|---|
| `[0BC9]` | LPT unit number (0-based), set at `0xC046` |
| `[0BD7]` | port type — `0x0C` = ECP-capable |
| `[0BD9]` | READ method selector (index into `0x4E9D`) |
| `[0BDB]` | WRITE method selector (index into `0x4F15`) |
| `[0BE1]` / `[0BE3]` | EPP-BIOS far entry, offset / segment |
| `[0BFA]` | LPT I/O base |
| `[0BC1]` | EPP-BIOS probe disable |

All are BSS — zero in the file image, filled during `INIT`.

## 8. Still to do

- **Settle §5.** Nothing else should be written until it is settled.
- The connect sequence before `0x25C1` — the `0x2DA5` cluster and the callers of `0x25A0`.
  Not started this pass.
- ATAPI packet issue / data phase.
- Target hardware, already established: bridge SHUTTLE EPATRM, drive Matsushita LS-120 COSM 04,
  port `0x378`, IRQ 7, `dmaEn = 0` (technique 62 — DMA must stay off on this machine), and no
  chipset init at all (the `/ni` finding: those writes alias onto the 8259 and killed the
  keyboard in #22, and the drive works without them).

## Licence

Stay MIT. Register maps, port sequences and protocol facts are not copyrightable; the vendor's
code is. Nothing here is transcribed, and the implementation must be written fresh from this
specification. Linux's `drivers/block/paride/epat.c` is GPL-2.0 and **must not** be used as a
source.

[#22]: https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/22
