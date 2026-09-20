# The EPP dword gate in SD120PPD.MPD, decoded

Static analysis only, on `drivers/imation_ls120/eppfast_deploy/SD120PPD.STK`
(stock miniport, md5 `08104ffb559ae4b47b84377daee473bc`). Nothing here has been
booted. RVAs throughout; ImageBase `0x10000`, so a `[0x2xxxx]` operand is
RVA `0x1xxxx`.

Prize: the dword path amortises the Inboard's ~3.90 us per-access sync over four
bytes instead of one. Against the measured 10.86 us/byte for a Windows EPP
write that is **~27%, about 11 s on the 4 MB test**. The same argument, at 2:1,
is what `rep insw` was worth on XT-IDE: 35%, measured.

---

## 1. Mode selection, `0x8035`

Entry takes a word from the stack: `dh` = requested read mode, `dl` = requested
write mode.

```
0x805f  if dh == 0Ch          -> dh = 0Dh          ; EPP BIOS request becomes ECP Read
0x806d  [0x20BFD] = (dh == 0Bh)                    ; the printed-name "Fast" flag
0x8079  if dh not in {0Ah,0Bh} -> skip to 0x80C1
0x8083  dh=0Ah dl=3                                ; byte-wide default
0x8087  if [0x20C7C] == 1      -> refuse           ; gate 1, one-way downgrade latch
0x8090  if [0x20C7B] == 1      -> refuse           ; gate 2
0x8099  if [0x20C7B] == 2      -> refuse           ; gate 3
0x80a2  if [0x20C7B] >= 3      -> allow outright
0x80ab  else call 0xC8A9; if AL != 2 -> refuse     ; gate 4
0x80b4  call 0xFCED; if AL < 3      -> refuse      ; gate 5, CPU class
0x80bd  dh=0Bh dl=4                                ; DWORD
0x80c6  if [0x20BD7]==1 && [0x20BD8]!=1 -> dh=0Ch dl=5   ; overrides dword
```

`[0x20C7B]` and `[0x20C7C]` both initialise to `00` in `.data`, and the only
writes to `0x20C7B` are a save/restore pair at `0xB95A`. So gates 1-3 pass and
**gate 4 is the operative one**. `patch_sd120ppd_eppfast.py` targets it
correctly.

`0xC8A9` is four instructions: return 0 if `[0x20BD4]==1`, else return
`[0x20D9D]`. Gate 4 therefore needs **`[0x20BD4] != 1` and `[0x20D9D] == 2`**.

### ⛔ Two corrections to `next_session_2026_09_20b.md` §8

- **The mode-12 hypothesis is wrong.** An incoming `0Ch` is rewritten to `0Dh`
  at `0x805F`, before the range check. Mode 12 is an *output* of this function,
  assigned at `0x80D8`, and it overwrites a dword selection made twelve
  instructions earlier. The risk is real; the mechanism stated there is not.
- **The DOS banner is not a readout of the selection.** `[0x20BFD]` is set from
  the *incoming requested* `dh` at `0x806D` and never updated. The banner
  reports what was asked for. A boot to read it would have produced a third
  uninterpretable result.

A truthful banner is a same-length in-place edit: `cmp byte [0x20BFD],1` is
7 bytes, `cmp dh,0Bh` is 3 plus four `nop`. `dh` is live at `0x811B`, `dl` at
`0x818B`. Adds no port access.

---

## 2. Where `[0x20D9D] = 2` comes from

Set in the per-chipset probe arms (`0xE8EE`, `0xE933`, `0xE977`, `0xE9BA`,
`0xE9FD`) and in one other place: `0xD04F`, inside the routine that ends
`[0x20BD2] = 1` — chipset id 1, **"Hard Config. Epp"**. That is a *declared*
EPP port, not a probed one.

Chipset id table at `0x1F1A0`, 8 bytes per entry, name function first:

| id | name | id | name |
|---|---|---|---|
| 0x00 | Compatible | 0x09, 0x0A | IBM PS/2 |
| **0x01** | **Hard Config. Epp** | 0x0B | IBM PS/1 |
| 0x02 | Epp Bios | 0x0D | Winbond W |
| 0x03, 0x0C, 0x10 | VLSI | 0x11 | Intel AIP |
| 0x04, 0x05, 0x0E, 0x0F | SMC | 0x12 | UMC |
| 0x06 | WD | 0x07 | NS PC |
| 0x08 | 386/486 SL | | |

### The dispatcher, `0xCAF1`

```
0xcaf9  if [0x20BD3]==1 -> bail                    ; <- where we are today
0xcb0c  [0x20BD7] = [0x20BD8] = 0                  ; also removes the 0x80C6 stomp
0xcb1a  if [0x20C2A] in {0,1,6} -> call 0xD030
```

`[0x20BD3]==1` aborts the whole function four instructions in, so the
declaration never happens and `[0x20D9D]` stays 0. **That is why gate 4
refuses.** It is not that detection fails; it is that detection never runs.

### `0xD030`, the declared-EPP path

```
0xd030  call 0xD0B0     ; CF -> fail
0xd037  call 0xD6D4     ; CF -> fail
0xd03e  call 0x7F57     ; AX must be 0
0xd043  [0x20D9D] = 0
0xd04f  [0x20D9D] = 2
0xd056  if [0x20C7E]==1 -> SKIP call 0xDD77
0xd076  if [0x20BD2]==6 -> ret
0xd07f  [0x20BD2] = 1   ; "Hard Config. Epp"
```

---

## 3. The port footprint, and the one dangerous routine

Call-graph reachability over the `0xD030` closure, 44 functions:

| port | reached from | on a 5160 |
|---|---|---|
| **0x20, 0x21, 0x2E, 0x2F** | **`0xDD77` only** | **fatal — 8259 and its `0x20-0x3F` alias** |
| 0x94, 0x102 | `0xCC72` | PS/2 POS, undecoded |
| 0x71 | `0xEF35` | bare CMOS data read |
| 0xEC, 0xED, 0xFB | `0xC36F` | undecoded |
| 0x22, 0x23 | `0x896E` — the VLSI transfer path, gated on `[0x20BD2]==3` | not on this path |

`0xDD77` is the only routine in the closure that touches the 8259 window, and
`0xD056` already has a flag that skips it. `0xCC72` is itself gated on the same
flag and returns before its first `in al,0x94`.

Path-sensitively, with a forced id of 1:

- `0xD0B0` short-circuits at `0xD11C` (`clc`) — the id is not 0, 9, 0xA or 0xB,
  so it returns success having touched **no port at all**.
- `0xD6D4` subtree: 0x71, 0xEC, 0xED, 0xFB. None in the 8259 window.
- `0x7F57` returns `AX=0` immediately when `[0x20BD9]==1`, touching nothing.

`[0x20BD9]` is set at `0xEF1B`, inside `0xEE75`, which is called only from
`0xD717` and `0xD721` — both inside `0xD6D4`, with `al` taken from config struct
fields `+0x0B` and `+0x0C`.

---

## 4. The flags word

`HwFindAdapter` parser at `0x5808` takes a config struct in `ebx`:

| field | setter | effect |
|---|---|---|
| `+0x0B`, `+0x0C` | `0xEE75` | EPP mode request -> `[0x20BD9]`, `[0x20BD2]=6` |
| `+0x10` | `0x779C` | `[0x20C29]`, `[0x20C1A]` |
| `+0x20` | `0x7E89` -> `0xCFAA` | the flags word |
| `+0x24` | `0x7E04` | `[0x20CA8]` |
| `+0x30` | `0x77C1` | |
| `+0x34` | `0x77B6` | `[0x20BD2]` and `[0x20C2A]` — forced chipset id |

Flags word bits, decoder `0xCFAA`-`0xD02F`:

| bit | sets | meaning |
|---|---|---|
| 0 `0x0001` | `[0x20BD3]` | **kills the whole detect dispatcher** |
| 2 `0x0004` | `[0x20BD5]` | |
| 3 `0x0008` | `[0x20C17]` | |
| 4 `0x0010` | `[0x20DB9]` | |
| 5 `0x0020` | `[0x20C7E]` | **skips `0xDD77` and the POS access** |
| 7 `0x0080` | `[0x20BFE]` | |
| 9 `0x0200` | `[0x20BD4]` | **hard veto on gate 4** |

---

## 5. The target state

| what | value | why |
|---|---|---|
| flags bit 0 | **0** | let the dispatcher run |
| flags bit 5 | **1** | skip `0xDD77`, the only 8259-window prober |
| flags bit 9 | **0** | leave `[0x20BD4]` clear |
| forced id `+0x34` | **1** or **6** | route to `0xD030`, short-circuit `0xD0B0` |
| `+0x0B`/`+0x0C` | EPP request | `[0x20BD9]=1`, so `0x7F57` returns 0 |

Result: `[0x20D9D]=2`, `[0x20BD4]!=1`, gate 4 passes, gate 5 passes on a 386,
`[0x20BD7]/[0x20BD8]` were zeroed at `0xCB0C` so the `0x80C6` stomp cannot
fire. **`dh=0Bh, dl=4` — EPP dword, with no binary patch and nothing written to
`0x20-0x3F`.**

`/de /db /ni` is a bigger hammer than the job needs: it sets bit 0, which kills
the declaration along with the probing.

## 6. What is not known

How the registry or INF populates the config struct. The miniport's own keyword
table is `BLK · DMA · ECP · MSN · NATN · NDPC · port · size · irq · LPT1-9 ·
Dev`; none of those is obviously the flags word, so the struct is assembled a
layer above. Until that is mapped, this is a patch target rather than a setting.

## 7. Verify in the bed before the 5160

The question this raises is "which ports does it touch", and 86Box answers that
exactly — log every I/O access and confirm nothing lands in `0x20-0x3F`. The bed
models no drive latency so it cannot time the result, but timing is not what is
in doubt.
