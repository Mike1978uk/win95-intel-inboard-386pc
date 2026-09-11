# Emulating the EPAT bridge in 86Box — so the LS-120 can be tested in full

Goal, in the owner's words: *"if we could get the media test in emulation also that would be
epic."* Today every LS-120 iteration costs a boot, often a hard reset, and a trip downstairs.
This ends that permanently, transport included.

## Why it is tractable — we do not model the drive

**86Box already models the drive.** `src/disk/rdisk.c` carries
`IMATION / SUPERDISK 120 ATAPI` in its table and implements the whole ATAPI command set on
top of a generic device layer:

```c
scsi_device_command_phase0(scsi_device_t *dev, uint8_t *cdb);
scsi_device_command_phase1(scsi_device_t *dev);
/* scsi_common_t: packet_status, packet_len, phase, phase_data_out */
```

The IDE controller is merely one **consumer** of that layer. We write another.

**And 86Box already has a parallel-port device plugin interface.** `include/86box/lpt.h`:

```c
typedef struct lpt_device_s {
    void    (*write_data)(uint8_t val, void *priv);      /* w0() */
    void    (*write_ctrl)(uint8_t val, void *priv);      /* w2() */
    void    (*strobe)(uint8_t old, uint8_t val, void *priv);
    uint8_t (*read_status)(void *priv);                  /* r1() - the nibble path */
    uint8_t (*read_ctrl)(void *priv);                    /* r2() */
    void    (*epp_write_data)(uint8_t is_addr, uint8_t val, void *priv);
    void    (*epp_request_read)(uint8_t is_addr, void *priv);
    void *priv;
} lpt_device_t;
```

Attached with `lpt_attach_ex(port, ...)` (`src/device/lpt.c`), the same way printers are.
There is even a `CHAR_LPT_NIBBLE` flag - 86Box already has the concept.

**So all we write is the bridge**: an EPAT protocol state machine between those port hooks
and an ATAPI device.

⚠ `RDISK_BUS_LPT = 6` exists in `include/86box/rdisk.h` and is referenced in **no `.c` file at
all** - likewise `CDROM_BUS_LPT`, `HDD_BUS_LPT`, `MO_BUS_LPT`. It is a declared placeholder.
Nobody has implemented a parallel-port bridge, so this is new work upstream would not have.

## What the bridge must implement

All of it is already specified and **verified on hardware** in
`drivers/imation_ls120/TRANSPORT_SPEC.md` - read its index first - with `reference_gpl/epat.c`
as the cross-check:

| piece | where |
|---|---|
| CPP connect / disconnect handshake (`22 AA 55 00 FF 87 78` + mode, commit on nINIT) | §3 |
| Register addressing: index/data pair `0x0E`/`0x0F`, `cont_map = {0x18, 0x10, 0}` | §4b, §4h |
| Nibble register read (two nibbles in the top four status bits, `j44`) | §6, §7 |
| Register write (`0x60`-tagged) | §7 |
| Block read/write: enter block mode once, alternating **phase bit**, announce last byte | §6 (2026-09-11) |
| ECP FIFO path at `base+0x400`/`0x402` | §4f |
| ATA task file behind it, including SRST at container offset `0x16` | §5 |

## Validation — this is the part that makes it safe

**We hold byte-level captures of the real bridge answering the real protocol.**
`docs/captures/2026-09-11_ls120/` has `INQ9.OUT` returning
`MATSHITA / LS-120 COSM   04 / 0270`, plus status, interrupt-reason and byte-count readings
at every phase.

So the emulated bridge is checked by running **our own DOS probes against it** and diffing
against hardware. If `INQ9` returns the same shape in 86Box, the model is right. That is a
far stronger test than "it seems to work".

## Order

1. Skeleton `lpt_device_t` that answers the **CPP connect handshake** only. Validate with the
   connect checkpoints (`B8 58 F0`) already captured.
2. Register read/write via the index/data pair. Validate: task file returns the ATAPI
   signature `14 EB` after SRST.
3. `scsi_device_command_phase0` wiring - a real INQUIRY. Validate against `INQ9.OUT`.
4. Block read/write with the phase bit. Validate: a sector off a mounted image.
5. ECP FIFO path.

Each step has a capture to check against before the next begins.

## Then

The LS-120 driver - including **media reads and writes** - is testable without touching the
5160. And `lpt_epat` becomes something 86Box does not have, so it is contributable.

## CORRECTION — I/O timing IS modelled, and the caveat about it was wrong

An earlier version of this plan said emulation "will not reproduce the Inboard's 5.55 us per
I/O access". **That was asserted from a general belief about emulators, not from reading the
one this project wrote.** `src/device/inboard386.c` models it explicitly:

```c
inboard386_apply_io_waitstates(void)
    if (cpu_busspeed <= 4772728.0) { io_waitstates = 0; return; }
    ratio = cpu_busspeed / 4772728.0;
    extra = (int) ((11.0 * ratio) - 11.0 + 0.5);
```

with a comment reading *"which real ISA-bus hardware paces independently of CPU speed"*, and
`inboard386_apply_mem_timing()` doing the same for memory cycles with bus-speed-ratio
scaling. **The mechanism behind the 3.90 us fixed per-access cost is deliberately modelled.**

The owner's challenge was the right one: *"we have emulated the inboard - if it's not good
for this then why have we had so much success already"*. The record supports him. Every
"emulation cannot reproduce this" in this project's history turned out to be an **incomplete
model**, not a limit of emulation:

| claimed limit | what it actually was |
|---|---|
| shutdown hang would not reproduce | `hdc_xtide.c` decoded stride 1; the card is stride 2. Fixed the model, reproduced at once (technique 90) |
| keyboard latch bug masked | `kbc_xt.c`'s self-heal, which real hardware does not have (technique 37) |
| 8259 aliasing invisible | simply not modelled (technique 75) |

### The open question is a MEASUREMENT, not an argument

Run the technique-109 timing probe in 86Box - the one that measured **5.55 us per 8-bit I/O
access** on the real 5160, twice, agreeing to 0.4% - and compare. `RMTMB4.OUT` / `RMTMAF.OUT`
and the memory-vs-I/O figures give several more points to check against.

If the emulated figure lands near 5.55, then the timing class of failure (spin-up waits,
abandoned commands, SCSIPORT timeouts) reproduces as well, and emulation covers effectively
the whole surface rather than "everything except timing".

If it does not, that is a **calibration** task on a model that already has the right shape -
technique 5's bisect-against-a-real-measurement - not a reason to go back to the bench.

## The goal is an UPSTREAM contribution, not a local hack

Owner's call, 2026-09-11: *"at the end we could push it to 86 box if it faithfully
emulates."* That is the right ambition and it is achievable - 86Box has `RDISK_BUS_LPT`
declared and unimplemented, so this fills a gap its own headers admit to.

**Recording it now because it sets the standard from the first line, not the last.**

### What upstreamable means here

| requirement | state |
|---|---|
| 86Box conventions - `device_t`, `lpt_attach_ex`, `log_open`, config selection | ✅ followed from the start; `net_plip.c` was the template |
| No project-specific hacks, no LS-120-only shortcuts | ✅ it is a **bridge**, and the drive behind it is 86Box's existing `rdisk` |
| ⚠ **`ENABLE_EPAT_LOG` must default OFF before submission** | ❌ currently `1`. **This is technique 93** - `ENABLE_XTIDE_LOG` left on cost 438 MB per boot and dropped the emulator to 2-14% of speed. Keep it on while developing, gate it before the PR |
| Attribution for the protocol work | Reverse-engineered from Imation's driver and verified on real hardware; cross-checked against Linux `epat.c`. Register maps and protocol order are not copyrightable - the implementation here is ours |
| Evidence it is faithful | **This is the strong part** - see below |

### The credibility argument writes itself

Most emulation PRs say "it seems to work". This one can say: *here is the real bridge's
response to the same byte sequence, captured on the hardware, and here is the model
reproducing it.* `docs/captures/2026-09-11_ls120/` holds the connect checkpoints
(`B8` / `58` / `F0`), a full ATAPI INQUIRY returning
`MATSHITA / LS-120 COSM   04 / 0270`, and status, interrupt-reason and byte-count readings
at every phase of a command.

**Build each step against its capture** (the order in this document) and the PR arrives with
a hardware-diffed validation trail rather than an assertion.

### And it is a real gap

`RDISK_BUS_LPT`, `CDROM_BUS_LPT`, `HDD_BUS_LPT`, `MO_BUS_LPT` are all declared in 86Box's
headers and referenced by **no source file**. Parallel-port storage - Iomega parallel ZIP,
SyQuest, the SuperDisk - is a whole class of period hardware 86Box cannot currently model.
The EPAT bridge is one of the commonest, and the same `lpt_device_t` seam takes the others.

Precedent: this project already has **three** 86Box PRs merged (#7626, #7749, #7771).
