# LS-120 on the real 5160 — 2026-09-17/18

Every result here is from the owner's machine over DOS COMrade. Procedure and
traps: `README.md` in this directory.

## ECP works. Negotiation AND termination are both required.

Four runs, one variable apart, reading task-file register `1Dh` (cyl-high,
holds `EB` of the `14 EB` ATAPI signature after SRST — a value that cannot
match by coincidence):

| capture | negotiate | terminate | nibble | **ECP** | nibble again |
|---|---|---|---|---|---|
| `ECPREG.OUT`  | yes | no  | `00` | `00` | **`F5`** port poisoned |
| `ECPREG2.OUT` | no  | n/a | `EB` | **`FF`** timeout | `EB` |
| `ECPTERM.OUT` | yes | wrong bits | `EB` | **`EB`** | **`F5`** |
| `ECPTERM2.OUT`| yes | **yes** | `EB` | **`EB`** | **`EB`** |

`ECPTERM2` also reads a real `MSWIN4.0` boot sector over SPP afterwards, where
`ECPTERM` gave `FF 77 FF 77` — the idle-port pattern.

**The terminate**, measured not guessed:

```
control = 0Ch   nSelectIn + nInit      -> wait nAck LOW
control = 0Eh   add nAutoFeed          -> wait nAck HIGH
control = 0Ch -> forward
```

⛔ **This is why every ECP-capable build failed.** `LS_Neg1284` put the
peripheral into 1284 ECP and nothing took it out, so every later nibble read
returned `F5`. `a925038` — the build that gave the owner `L:` — has no
`LS_DetectEcp` at all, so it never entered 1284. Shipped as `LS_Term1284`,
falling through from `LS_EcpLeave` so both block paths get it on **both** their
success and failure exits.

## Command census, SPP — zero errors

`CENSUS.OUT`. Per command: status after data, at completion, error register,
and the byte count the DEVICE offered.

| command | completion | error | offered |
|---|---|---|---|
| REQUEST SENSE (1st) | `51` | **`64`** UNIT ATTENTION — correct after SRST | 512 |
| READ(10) LBA 0 | `50` | `00` | 512 |
| **WRITE BUFFER** | `50` | `00` | 512 |
| **READ BUFFER** | `50` | `00` | 512 |
| REQUEST SENSE x4 | `50` | `00` | 18 |
| **START/STOP UNIT** | `50` | `00` | 0 |
| media READ(10) | `50` | `00` | 512 |
| **READ CAPACITY** | `50` | `00` | 8 |

`WRITE BUFFER` -> `READ BUFFER` round-trips 512 bytes **byte-exact** (`00 01 02
…`), media untouched. The command layer is sound; nothing to fix in either
transport at this level.

## Media written and validated

- `MEDIAWR.OUT` — `WRITE(10)` LBA 100,000, then read back: full `00`..`FF` ramp,
  head and tail.
- `POSTSRST.OUT` — **after a fresh SRST, with no write issued**, the ramp is
  still there in a **poisoned** buffer. So it is on the platter, not in the
  drive's cache.
- `FAR_lba230000_ok.OUT` (09-14) — reads at LBA 230,000 of 246,528.

## Drive identity

`MATSHITA / LS-120 COSM 04 / 0270`, direct-access, removable.

## ⚠ Instrument caveats

- **`SLOTS` (1E00-1EBF) is NOT poisoned** — only the data buffers are. A slot
  can hold the previous run's value at the same address. `POSTSRST.OUT` shows a
  write result in `+90` although no write was issued. Trust a slot only when the
  matching buffer agrees, and poison SLOTS too.
- `2600` is unused for a single-sector transfer (`rbuf = BUF_SECTOR2 = 2800`),
  so `EE` there means nothing.

## Still unmeasured, and needed for the transfer design

- **Drive buffer capacity** — `READ BUFFER` mode 3 (`3C 03 …`) returns it in
  bytes 1-3. We have the 3584-byte BRIDGE burst ceiling; the DRIVE buffer is a
  different number and nobody has asked for it.
- **MODE SENSE page 2** — the drive's own preferred transfer sizes.

Adding both needs the CDB table moved: it ends at `0x1D40` and would grow into
`SLOTS` at `0x1E00`, which the generator's layout checker refuses.
