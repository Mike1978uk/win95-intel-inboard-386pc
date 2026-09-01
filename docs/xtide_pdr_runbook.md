# Runbook — finishing the XT-IDE port driver (issue #21)

State at the start: the driver loads, services real Windows reads, publishes its own logical
volume and gets drive D:. Full detail and the reasoning is in `next_session_2026_09_02.md`; this
file is just the steps. Do them in order — each one de-risks the next.

**Standing rules for every step**
- `-Restore` on every run. It re-clones the boot image, so a bad run costs a file copy.
- Rebuild the fixture before every run: `python tools/mkfatimg.py vm_xtide_pdr/scratch.img`.
  It also clears the marker, so a stale one cannot be read as a result.
- Read the marker after every run: `python tools/pdr_reqmarker.py vm_xtide_pdr/scratch.img`.
- **Read the filesystem too, not just the marker.** The marker says a WRITE reached the
  transport. Only `fatls.py` and the raw sectors say the right bytes reached the right block.
  On 2026-09-01 the first of those was true while the second was not.
- **A null result is only a result if the code was verified present.** `grep` the built
  `build/PORTREQ.ASM` / `PORTAER.ASM`, or take a listing (`ML ... -Fl<file>.lst`), before
  believing that something did not happen. Three claims on 2026-09-01 were wrong for want
  of this.
- **No BOOTLOG.TXT is not a result about the driver.** Read `screen_<tag>.png` first: a POST
  prompt (`162-System Options Not Set`) means the run is void. The harness now says so.
- **The window is the measurement.** Default `-Seconds 400`. The volume is published from an
  appy-time callback and StartUp items run after that; 150 s hid a working volume once, and
  260 s published it on one run and not the next.
- **Do not run builds or other 86Box work on the host during a run.** Two of the differences
  chased on 2026-09-01 were how far the guest got inside a fixed wall-clock window.

---

## Step 1 — prove a WRITE goes through the request path  ✅ DONE 2026-09-01, and it found a bug

**Result: the write path was writing the wrong bytes, and the read path could never have shown
it.** `IOR_buffer_ptr` is a data buffer only while `IORF_SCATTER_GATHER` is clear; with it set the
DDK says it points at a list of `BlockDev_Scatter_Gather` descriptors, count first and pointer
second. VFAT's mount-time reads are single-buffer and its writes are scattered, so the read path
looked correct and proved nothing about the write path - and a write/read-back self-test cannot
catch it either, because both halves move the same wrong bytes.

Symptom: `WRITE lba=126` reached the transport and put ring-0 heap in the root directory. The
bytes on the disk began `01 00 00 00 00 E4 69 C1` - `BD_SG_Count = 1`, `BD_SG_Buffer_Ptr =
C169E400`. A descriptor, byte for byte.

Fixed by walking the list (null-terminated, separately bounded by `IOR_xfer_count`, last segment
clamped), the same shape as `zikolas/cfu1-win9x`'s `rr_sg`/`rw_sg`. Confirmed on two runs:

```
scatter/gather requests 0 read, 3 write     <- the walk is load-bearing, not dead code
D:\WROTE.TXT  21 bytes  "HELLO-FROM-STARTUP"
LBA 64  f8 ff ff 7f ff ff ff ff             <- FAT intact
LBA 126 "XTIDE VOL" / HELLO.TXT / WROTE.TXT <- root directory intact
```

The old binary is not merely wrong, it is unsafe: rebuilt byte-identical and re-run, it killed the
boot at `Initing port.pdr` instead of corrupting the volume. Same input, different damage.

### The original step, kept because it is the procedure to repeat

The only thing Windows has ever asked this driver to do is read. The write path is proven by the
driver's own self-test, not by VFAT.

```bash
cd drivers/xtide_pdr
pwsh -File build.ps1 -Stride 1 -ClaimMask 2 -ReqMarker -PublishVolume
cd ../..
python tools/mkfatimg.py vm_xtide_pdr/scratch.img
```

Put a batch file in the StartUp group that writes to D: and forces a flush:

```bash
printf '@ECHO OFF\r\nECHO hello > D:\\WROTE.TXT\r\nEXIT\r\n' > /tmp/w.bat
python tools/fatcp.py vm_xtide_pdr/xtide_base.img \
  "C:\WINDOWS\STARTM~1\PROGRAMS\STARTUP\W.BAT" /tmp/w.bat --yes
pwsh -File tools/pdr_loadtest.ps1 -Restore -Seconds 260 -Tag write1
```

**Pass:** the marker's first-requests list contains a `WRITE`, **and**
`python tools/fatls.py vm_xtide_pdr/scratch.img` shows `WROTE.TXT`. Both, not either — a WRITE
that reached the transport but landed in the wrong place is the exact failure Technique 79 exists
for.

**If no WRITE appears at all:** VFAT is caching it and the VM is killed before the flush. Add
`EXIT` after a `DIR D:` (forces a directory read-back) or raise `-Seconds`. Do **not** conclude
the write path is broken until the driver's own counters show a WRITE arriving.

---

## Step 2 — make volume publishing the default  ✅ DONE 2026-09-01, no run needed

`--publishvolume` is gone from `patch_sample.py` and `-PublishVolume` from `build.ps1`; the edit is
unconditional and `want` went 19 → 20.

**Verified by md5, which is stronger than a re-run**: `build.ps1 -Stride 0 -ClaimMask 2 -ReqMarker`
now produces `eabc6517b7517e47a9a73026f6aa81c6` — byte-identical to the `-PublishVolume` build that
produced the clean run above. A refactor that cannot change the artefact cannot change the result.
Use this trick wherever a change is meant to be a no-op.

---

## Step 3 — stride autodetect  ✅ WRITTEN 2026-09-01, `-Stride 0`

`XTIDE_DetectStride` tries stride 1 then stride 2, reading Status (`base+07*S`) and Alternate
Status (`base+14*S`) and keeping the candidate where they agree — one register on any correct map,
measured `92h` against `50h` on the wrong one. All-ones and all-zeroes are rejected so two
floating reads cannot pass.

**It writes nothing, which is the whole safety argument.** The old objection to autodetection was
real and still stands for a probe that *writes*: stride 2 on stride-1 hardware puts `0ECh` into
DEVICE CONTROL and leaves SRST asserted. Two `in` instructions cannot do that. Stride 1 is tried
first so stride-1 hardware never gets read outside its own decode.

The chosen stride is reported in the marker. `-Stride 1` / `-Stride 2` still pin it and skip
detection entirely; `build.ps1` still defaults to a pinned 2.

The emulator can only ever confirm it picks 1. **The first hardware run should still pin
`-Stride 2`** — autodetect is one more thing that has never run on the card, and a failure needs
to be attributable.

---

## Step 4 — the boot drive, in emulation  (design first, do not improvise)

**This now comes before any hardware run.** There is no second drive in the 5160, so a secondary
volume cannot be tested there at all - the boot disk is the only target, and it is not the thing to
learn on. Prove it in the emulator on a clone first.

It is not a bigger version of step 3. Owning C: means taking the disk from the Real Mode Mapper
(`RMM.PDR`) rather than creating a volume: the boot DCB already exists when we load, so the driver
must claim **that** DCB and IOS must retire the mapper for it. `XTIDE_WantIop` currently refuses
any DCB we did not claim and `ClaimMask` is 2 — both would have to change deliberately.

Work it in the emulator, on a **clone** of the boot image, and write the design down before the
first run. This is the part of the stack nothing this session touched, and unmeasured is where
every surprise came from.

---

## Step 5 — the real card  (5160, first hardware run)

```bash
pwsh -File build.ps1 -Stride 2 -ClaimMask 1          # NO -ReqMarker
```

- **`-ReqMarker` must be off.** The marker writes a sector to the claimed unit, and on the 5160
  that unit is your CF.
- `-ClaimMask 1` (master): there is no second drive in the 5160, so the boot CF is the only
  target. This is why step 4 has to come first.
- Deploy per Technique 75: copy from the host with the card in the reader, `md5sum` **at the
  destination**, and CRLF anything text.
- Read `BOOTLOG.TXT` afterwards for `Init Success port.pdr`.
- Success here is the boot volume in 32-bit mode - i.e. MS-DOS compatibility mode gone, which is
  issue #3. There is no secondary-disk halfway house available on this machine.

---

## Known-good reference points

| | |
|---|---|
| `build.ps1 -Stride 1 -ClaimMask 2 -ReqMarker` | boots to desktop, `Init Success port.pdr` |
| ...`-PublishVolume`, `-Seconds 260` | drive D:, VFAT reads LBA 126/70/78 |
| bootlog 18,676 bytes | healthy boot |
| bootlog 11,826 bytes | died at `Initing port.pdr` — the protection-error signature |
