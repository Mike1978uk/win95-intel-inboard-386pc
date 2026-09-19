# LS-120 + Shuttle EPAT on the IBM 5160 — MEASURED FACTS

**Every number here was measured on the owner's machine. None is inferred, and
none should be measured again.**

⛔ **Two rows broke that promise and were corrected on 2026-09-18** — an ECP
block-streaming "failure" from a run that predated the negotiation discovery
(§2z), and an ECP-vs-nibble speed ordering that is an instruction COUNT, not a
timing (§2b). **Anything not measured must say so in the row itself.** A file
that says "do not re-measure" launders inference into fact unless it does. Each row names the capture it came from, in
`docs/captures/`. If you are about to run a probe to establish something in
this file, read the capture instead.

Measured 2026-09-17/18 over DOS COMrade unless stated. Drive **MATSHITA LS-120
COSM 04**, firmware **0270**, bridge **SHUTTLE EPATRM**, port `0x378`.

---

## 1. The drive

| fact | value | capture |
|---|---|---|
| Vendor / product / revision | `MATSHITA` / `LS-120 COSM   04` / `0270` | `INQNOW.OUT` |
| Device type | `00` direct-access, byte 1 `80` = **removable** | `INQNOW.OUT` |
| Last LBA | **246,527** (`0x0003C2FF`) | `BUFSIZE.OUT`, READ CAPACITY |
| Block size | **512** | `BUFSIZE.OUT` |
| Capacity | **126,222,336 bytes** (246,528 × 512) | as above |

## 2. Transport — what works and what does not

| fact | value | capture |
|---|---|---|
| **Nibble / SPP** | works for registers **and** block data | every capture |
| Bus cost, nibble | **7 port accesses per byte** (5 out, 2 in) | probe's own `NIB` block |
| Cost per 8-bit I/O access | **5.55 us**, measured on this machine | technique 109 |
| ⇒ nibble throughput | **38.9 us/byte** → 19.9 ms per 512-byte sector | arithmetic on the above |
| **ECP register read** | **WORKS** — returns `EB` from register `1Dh`, matching nibble | `ECPTERM2.OUT` |
| **ECP needs NEGOTIATION** | without it, every ECP access times out (`FF`) | `ECPREG2.OUT` |
| **ECP needs TERMINATION** | without it, every later nibble read returns `F5` | `ECPREG.OUT`, `ECPTERM.OUT` |
| **ECP block streaming** | ✅ **WORKS** — the vendor DOS driver runs it on this machine, see §2y | owner, 5160 console |
| EPP | port is EPP-capable; `epat.c` mode 3 is the bulk path | `EPP7_port_is_epp_capable.OUT` |

### 2y. ECP BULK WORKS — observed, and it is not ours that proves it

`SD120PPD.SYS`, loaded from `CONFIG.SYS` with `/port:378 /IRQ:7 /de /db /ni
/sf /dpc /dp /fp`, reports **`Read Mode : ECP Read`** and **`Write Mode : ECP
Write`** on this machine, and the drive enumerates, mounts at `D:` and serves
a 36 MB file. Owner-observed at the console, stated repeatedly.

Mechanism, from the driver's own help text: `/de` disables the **Epp** check
and `/db` the **Eppbios** check. **Neither suppresses ECP detection**, and
nothing else in that line does either — so ECP is what autodetection lands on.

**What this retires:** every "ECP is closed" claim in this project was measured
against a different artefact — our own miniport (`ls120-ecp-bulk-delivers-nothing`)
or the vendor *Windows* miniport with `ECP=1` (`3a318bf`, three configurations).
The vendor **DOS** driver's ECP path had never been tested. So our empty buffers
are an implementation defect, not a property of this bridge or this bus.

**What it does NOT say:** nothing here times ECP. Working and fast are separate
claims and only the first is established.

### 2z. RETRACTED: "the bridge does not do ECP block streaming"

**That row was wrong and is withdrawn.** `ECPNOW.OUT` was captured at 23:45 on
09-17. The four-run proof that ECP requires 1284 negotiation is `ECPREG.OUT`
through `ECPTERM2.OUT`, 00:08-00:14 on 09-18 — **23 minutes later**.
`ECPNOW.SCR` contains no negotiation at all: no `al,10`, no `or al,05`, no
`0C`/`0E` terminate, where `ECPTERM2.SCR` has every one of them.

So the block attempt ran without the prerequisite this very section proves is
mandatory, and by the table above an un-negotiated ECP access times out by
construction. **A run missing a prerequisite is not a negative result — it is
not a result** (technique 110).

**Honest state: ECP bulk transfer is UNTESTED with negotiation.** It is also
the only place ECP can pay, since §2b measures a register read as slower than
nibble — so the task file stays nibble either way, and bulk is what the 1284
work was for.

⇒ **Superseded by §2y**: ECP bulk is no longer untested. The vendor DOS driver
does it on this machine. This section stands for its method — a run missing a
prerequisite is not a result — not for its verdict.

### 2a. The 1284 sequence, exactly

Entering 1284 puts the **peripheral** into ECP too. Restoring the host's ECR
and control port does **not** bring it back.

```
negotiate   (LS_Neg1284 / SD120PPD.SYS 0x2993)
   ... ECP work ...
terminate:  control = 0Ch   nSelectIn + nInit   -> wait nAck LOW
            control = 0Eh   add nAutoFeed       -> wait nAck HIGH
            control = 0Ch -> forward
```

**Four runs, one variable apart — this is the whole proof:**

| negotiate | terminate | nibble | ECP | nibble again |
|---|---|---|---|---|
| yes | no | `00` | `00` | **`F5`** |
| no | n/a | `EB` | **`FF`** | `EB` |
| yes | wrong bits | `EB` | `EB` | **`F5`** |
| **yes** | **yes** | `EB` | **`EB`** | **`EB`** |

⛔ **This is why every ECP-capable build failed and `a925038` worked.**
`a925038` has no `LS_DetectEcp`, so it never enters 1284 and never poisons the
port. Shipped as `LS_Term1284`, falling through from `LS_EcpLeave`.

### 2b. The vendor's ECP register read (SD120PPD.SYS `3CCEh`)

Register number in `AL`, **one byte out**, every phase gated on the ECR. It is
the REGISTER path, not a block streamer.

```
control = 04 / ECR = 74h        wait ECR bit0 == 1 (FIFO empty)  budget FFFFh
DATA = regnum (ECP address)     wait ECR bit0 == 1               budget FFFFh
ECR = 34h / control = 20h (rev) / ECR = 74h
                                wait ECR bit0 == 0 (data avail)  budget 8000h
in al, base+400h                the byte
control = (ctrl & 10h) | 04h / ECR = 34h
```

⚠ **COUNTED, NOT MEASURED.** The vendor's per-register sequence is ~12 accesses
plus ECR polls against nibble's 7, so on that pattern a SINGLE register read
costs more. **ECP and nibble have never been TIMED against each other on this
hardware** — treat the ordering as an inference about the vendor's
enter/leave-per-read pattern, not a property of ECP. Holding one negotiation
open across several register reads is untested and could reverse it.
**For bulk, ECP is expected to win outright** and that is what it is for.

⚠ The vendor's bulk path at `4458h` writes **`out 0x22`**, which **aliases onto
the 8259 on this XT**. Never copy it verbatim; that is the issue #22
keyboard-killer.

## 3. Transfer sizing

| fact | value | capture |
|---|---|---|
| **Burst ceiling** | **3,584 bytes (7 sectors)**, offered by the drive in one go | `SWEEP7.OUT`, slot `+6` = `0E00` |
| Drive buffer capacity | **UNKNOWN — the drive refuses to say** | `BUFSIZE.OUT` |
| `READ BUFFER` mode 3 | **ILLEGAL REQUEST** (error `54h`, sense key 5) | `BUFSIZE.OUT` slot `+B8` |
| `READ BUFFER` mode 2 | works, 512 bytes byte-exact | `CENSUS.OUT` |
| Reset settle | **seconds**. `--spin 2000` (~0.29 s) aborts every command | `RW4_spin2000_abrt.OUT` |

### 3a. The ceiling is hard, and exceeding it is NOT an error you can ignore

Asked for 8 sectors (4,096) against the 3,584 ceiling, `SWEEP8.OUT`:

```
write +90:  58 00 58 00 01 EE 00 0E   offered 0E00 = 3584, NOT 4096
                 ^^ completion 58, never reached a clean 50
read  +98:  58 01 51 B4 01 EE 00 10   completion 51 ERR, error B4 = ABORTED COMMAND
tail 8FE0:  all zeros - the last 512 bytes were never written
```

Against 7 sectors (3,584), `SWEEP7.OUT`: both `50`, error `00`, ramp intact
head and tail.

**So the drive caps the burst at 3,584 and the excess is stranded**, the write
never completes cleanly, and the NEXT command comes back ABORTED because the
bus was left mid-transfer. `LS_MAX_XFER = 3584` is measured, not chosen.

**The burst ceiling and the drive buffer are different numbers.** 3,584 is what
the EPAT carries in one burst. The buffer is what the LS-120 absorbs before it
must commit, and it cannot be read out of the drive — bound it empirically by
`WRITE BUFFER` length, or find the throughput knee against transfer size.

## 4. Command census — all clean

`CENSUS.OUT`. Per command: status at completion, error register, and the byte
count **the drive offered**.

| command | completion | error | offered |
|---|---|---|---|
| REQUEST SENSE (1st after SRST) | `51` | **`64`** UNIT ATTENTION — correct | 512 |
| READ(10) | `50` | `00` | 512 |
| WRITE BUFFER | `50` | `00` | 512 |
| READ BUFFER (mode 2) | `50` | `00` | 512 |
| REQUEST SENSE ×4 | `50` | `00` | 18 |
| START/STOP UNIT | `50` | `00` | 0 |
| READ CAPACITY | `50` | `00` | 8 |
| READ BUFFER (mode 3) | `51` | **`54`** ILLEGAL REQUEST | 4 |

**Unit attentions drain one per REQUEST SENSE**, ASC `29` (reset) then `28`
(medium changed), then clean. INQUIRY is exempt from them — which is why
bring-up looked healthy for weeks while every read was refused.

## 5. Media — written and validated

| fact | capture |
|---|---|
| `WRITE(10)` 512 B at LBA 100,000, read back byte-exact | `MEDIAWR.OUT` |
| **Same data still there after a fresh SRST, no write that run, poisoned buffer** | `POSTSRST.OUT` |
| 7-sector (3,584 B) write + read-back byte-exact | `SWEEP7.OUT` |
| Read at LBA 230,000 of 246,528 | `FAR_lba230000_ok.OUT` (09-14) |

The post-SRST run is the one that matters: no write was issued, the drive was
reset first, and the buffer was poisoned — so the data is **on the platter**,
not in the drive's cache.

## 6. Windows never issues FORMAT UNIT

Explorer's format on a removable is **filesystem-level** — boot sector, both
FATs, root directory. CDB census of a full format: **3,266 `WRITE(10)`, 330
`READ(10)`, and ZERO `04h`.** So the recorded `FORMAT UNIT` refusal
(`FMT2_illegal_request.OUT`, ILLEGAL REQUEST, `FmtData=0`) **does not block the
workflow** and should not be treated as a blocker.

## 7. ⚠ Instrument rules, learned the hard way

- **Poison every buffer AND every slot before reading it.** An unpoisoned
  region holds the previous run's data at the same address. On 2026-09-17 an
  ECP run "matched" an SPP run across five regions and was read as a pass; only
  the one poisoned buffer told the truth. `SLOTS` was unpoisoned until late and
  showed a write result on a run that issued no write.
- **A buffer that cannot show "untouched" is not evidence.**
- **`rbuf = BUF_SECTOR2 = 0x2800`** for a single sector; `0x2600` is unused, so
  `EE` there means nothing.
- **`DEBUG` is the right command** - it is on the PATH. Both
  `C:\WINDOWS\COMMAND\DEBUG.EXE` (20,522 bytes) and `C:\DOS\DEBUG.EXE` are
  present; an earlier note here said the former was absent, and that is wrong.
- **`run_command` returns `idle` in ~1 s while DEBUG still runs.** Reading the
  `.OUT` immediately gives 0 bytes — that is *read too early*, not an empty
  result.
- **`dos_status` RTT is the load meter**: ~30 ms idle, 177 ms while DEBUG
  grinds.
- **Keystroke injection garbles command lines under load.** Use
  `delay_ms=120`, and read the screen before assuming a command ran.
