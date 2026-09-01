# Runbook — finishing the XT-IDE port driver (issue #21)

State at the start: the driver loads, services real Windows reads, publishes its own logical
volume and gets drive D:. Full detail and the reasoning is in `next_session_2026_09_02.md`; this
file is just the steps. Do them in order — each one de-risks the next.

**Standing rules for every step**
- `-Restore` on every run. It re-clones the boot image, so a bad run costs a file copy.
- Rebuild the fixture before every run: `python tools/mkfatimg.py vm_xtide_pdr/scratch.img`.
  It also clears the marker, so a stale one cannot be read as a result.
- Read the marker after every run: `python tools/pdr_reqmarker.py vm_xtide_pdr/scratch.img`.
- **A null result is only a result if the code was verified present.** `grep` the built
  `build/PORTREQ.ASM` / `PORTAER.ASM`, or take a listing (`ML ... -Fl<file>.lst`), before
  believing that something did not happen. Three claims today were wrong for want of this.
- `-Seconds 260`. The default 150 hid a working result for one run.

---

## Step 1 — prove a WRITE goes through the request path  (1 run)

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

## Step 2 — make volume publishing the default  (no run)

Once step 1 passes, `-PublishVolume` has earned its way in. Move the `publish the volume once the
calldown is in` edit in `tools/patch_sample.py` out of the `--publishvolume` block into the main
`aer` list, drop the switch from `build.ps1`, and adjust the `want` count. Rebuild and re-run
step 1 once to confirm nothing moved.

---

## Step 3 — stride autodetect  (1 run, and it is what makes the driver general)

Currently one binary per card variant. The method is already measured (Technique 78 addendum):
**Status (`base+07`) and Alternate Status (`base+0E`) are the same register on any correct map.**
On the wrong stride they disagree — measured `92h` vs `50h` on the real card.

In `XTIDE_Probe`, before IDENTIFY: set stride 1, read both, compare; if they differ set stride 2
and compare again; keep whichever agrees; fail if neither does. Record the chosen stride in the
marker. Verify in the emulator (which is stride 1) that it still picks 1 — that is the whole test
available here.

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
