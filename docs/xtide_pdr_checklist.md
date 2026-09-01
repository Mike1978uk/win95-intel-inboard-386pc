# XT-IDE port driver (#21) — where we are, and what is left

Status at the end of 2026-09-01. Detail in `xtide_pdr_runbook.md`; the hardware gap is in
`xtide_pdr_5160_differences.md`.

## Done, and proven

| | proof |
|---|---|
| ✅ Image loads, IOS calls our AER | `Init Success port.pdr`, in 86Box and on the 5160 (phase 0) |
| ✅ IDENTIFY, sector read, sector write | phases 1–2b, writes checked from outside the guest |
| ✅ Calldown accepted by IOS | `ISP_result 0` |
| ✅ Real IOS requests serviced | READ LBA 0 / 63 / 70 / 78 / 126, correct data |
| ✅ Logical DCB and a drive letter | `ISP_ASSOCIATE_DCB 0`, drive D: |
| ✅ **Step 1 — a WRITE goes through, correctly** | `D:\WROTE.TXT`, right bytes, right block; FAT and root directory intact. Found and fixed a real corruption bug on the way — see below |
| ✅ **Step 2 — volume publishing is the default** | refactor produces a byte-identical `.PDR` (`eabc6517`) |
| ✅ **Step 3 — stride autodetect** | `-Stride 0` picks stride 1 in 86Box; read-only, so it cannot leave a drive reset |

Three consecutive clean runs of the fixed driver: healthy boot (18,676-byte log), volume mounted,
scattered writes handled, filesystem intact afterwards.

### The bug step 1 found, because it is the reason to keep doing this in emulation

`IOR_buffer_ptr` is a data buffer **only while `IORF_SCATTER_GATHER` is clear**. With it set it
points at `BlockDev_Scatter_Gather` descriptors — sector count first, buffer pointer second. We
were reading it blind and transmitting the descriptor list into the volume. VFAT's mount-time reads
are single-buffer and its writes are scattered, so the read path looked perfect and proved nothing,
and a write/read-back self-test could not catch it either. Measured after the fix: **3 scattered
writes per boot** — the walk is load-bearing, not defensive.

The old binary is not merely wrong. Rebuilt byte-identical and re-run, it killed the boot at
`Initing port.pdr` instead of corrupting the volume. Same input, different damage.

## Left to do, in cost order

### 1. A hardware probe run — safe, and the only thing that can test stride 2  (1 boot)

**Build is already made:** `dist/xtide_pdr/PORT_probe_stride2.pdr`, md5
`0174a19fe1ce88e1733ef38ff6173211`, from `build.ps1 -Stride 2 -ClaimMask 0 -NoWriteTest`.

- claims **nothing**, so no DCB, no calldown, no volume, and no request path;
- **no `-ReqMarker`** and **`-NoWriteTest`**, so nothing is written to the CF at any point. The
  second one matters: the probe's write self-test targets LBA 100 of whichever unit answered, and
  its only guard is a DEV-bit read-back on a map that has never executed;
- `XTIDE_Probe` still runs — IDENTIFY, transport autodetect, and one read of LBA 0.

86Box emulates a stride-1 XTIDE, so **stride 2 has never executed anywhere**. This run tests it,
and the register map and transport with it, at zero risk to the card.

Deploy with the card in the reader (Technique 75), `md5sum` at the destination, then read
`BOOTLOG.TXT` back. The answer is the gap between `Initing port.pdr` and `Init Success`, at roughly
33 ms per tick:

| ticks | fail code | meaning |
|---|---|---|
| ~0–1 | 0 | probe succeeded outright |
| ~30 | 1 | no I/O resource reached the DDB |
| ~60–150 | 2–5 | IDENTIFY failed: BSY stuck / rejected / no DRQ / model string not sane |
| ~330 | 11 | neither stride made Status and AltStatus agree |
| ~720+ | 24+n | **read verified, no slave to write to — this is the success we expect**, n = partition type |

### 2. The boot drive, in emulation, on a clone  (design first, then runs)

Not a bigger version of what works. Owning C: means taking the disk from `RMM.PDR` rather than
creating a volume: the boot DCB already exists when we load, so we must claim **that** DCB and IOS
must retire the mapper for it. `XTIDE_WantIop` refuses any DCB we did not claim and `ClaimMask` is
2; both change deliberately, or not at all.

Write the design down before the first run. This is the one part of the stack nothing has touched.

### 3. The real card, claiming the boot volume  (after 2)

`build.ps1 -Stride 2 -ClaimMask 1`, no `-ReqMarker`. There is no second drive in the 5160, so there
is no secondary-disk halfway house — the first claiming run on hardware is the boot volume, which
is why 2 comes first. Take a host-side image of the CF before it.

Success here is MS-DOS compatibility mode gone, which is issue #3.

### Known, not blocking, worth remembering

- **The stride autodetect has only ever picked 1.** The emulator cannot exercise the stride-2
  branch. Pin `-Stride 2` on hardware until a pinned run has worked; then `-Stride 0` is a separate,
  attributable experiment.
- **ATAPI/CD is permanently out of scope** — the XT-CF has D8–D15 unconnected.
- **The probe build is already on the card**, deployed 2026-09-01 and md5-checked at the
  destination. The 16,544-byte build that logged `Init Failure` in four ticks is backed up off-card
  as `PORT_oncard_before.pdr` (md5 `0681f388eab8173b483074aa00caa11d`) if a revert is ever wanted.
