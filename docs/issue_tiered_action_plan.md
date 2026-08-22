# Issue-tiered action plan

Maps every open GitHub issue to its **real current status**, its **next concrete action**, and what
that action **unblocks elsewhere**. Companion to
[`outstanding_work_plan_2026_08_23.md`](outstanding_work_plan_2026_08_23.md) — that file is ordered
by *execution sequence*, this one by *issue*, so nothing falls between them.

Written 2026-08-23 after re-reading every issue thread in full. **Two issues turned out to be in a
materially different state than the earlier notes assumed** — see #5 and #8.

---

## Tier 1 — actionable now, real chance of a fix

### #5 — Sound Blaster Pro causes `vmad` BSOD
`A fatal exception 0E has occurred at 0028:C002F330 in VXD VDMAD(01) + 00001660`

**Status is further along than "unsolved", and further back than "fixed".** The history matters:

1. Original theory (extra I/O at 330h / MPU-401) — **disproved**. A port-330h trace hook showed
   *zero hits*; plain SB Pro (`*PNPB002`) never requests it.
2. Second finding — a **false positive in this project's own `patch_vdmad.py`**: it mistook a
   harmless 3-byte `AND AH,0xC0` for a real `IN AL,0xC0`, corrupting 2 bytes into a 7-byte
   instruction dereferencing a garbage address. That explains the BSOD text *exactly* (page fault,
   that offset, inside VDMAD). Fixed in `VDMAD_INBOARD_FIXED.VXD`, commit `d8adb29`.
3. **But that fix was then tested on real hardware and still failed.** So the byte corruption was
   real and worth fixing, yet it was not the whole story — or not the story at all.

**Live lead (2026-08-20, @andrew-hoffman):** the XT DMA page register is only **4 bits** → 20-bit
reach → the DMA buffer **must** live under 1 MB. Corroborating detail from the port list: SB Pro is
on **DMA 1**, so its page register is **`0x83`** (the channel→port map is not numeric: ch0=87,
ch1=83, ch2=81, ch3=82). The `0080-008F` reference section documents the **AT** `74612` width — do
not read 8 bits into it here.

**Next action, in strict order:**
1. `SYSTEM.INI` `[386Enh]` → `DMABufferIn1MB=Yes`, `DMABufferSize=32`. Reboot. Zero code; the
   real-hardware equivalent is the identical edit on the CF card.
2. If unchanged → force a Win3.1-era 8-bit `.drv` via `[drivers]`, bypassing the protected-mode VxD
   audio stack entirely.
3. Only then re-disassemble `vmad.vxd` — **from scratch**, not by patching the failed attempt.

**Why this fits:** a page fault inside VDMAD's *virtual DMA buffer handling* is exactly what a
buffer the controller cannot physically address would produce.

---

### #9 — Revto486.sys TSR halts during boot
**Reclassified: harder than it looked, and blocked.**

The vogons clue is specific — the Evergreen driver only works when *"the CPU is in 386 protected
mode before loading the driver and DOS is running in virtual x86 mode."* Inspecting the CF card
confirms the precondition is absent: `CONFIG.SYS` has

```
REM DEVICE=C:\revto486.sys /BL /CN /CCM /2
```

commented out, and **no EMM386 at all** — only `HIMEM.SYS`. So there is no V86 provider on this
machine, which fits the clue exactly.

**But** adding EMM386 walks straight into the unresolved `error #04 at 0128:009B` halt (a wild jump
into a FAT directory-entry buffer entered from IO.SYS's own low-memory boot code, 2026-08-03).

**So the real first task is the EMM386 bug, not the driver.** Do not schedule #9 as a quick test.

---

## Tier 2 — one action unblocks two issues

### #4 — ATI Mach8 stock driver not working  ·  #7 — Setup hangs to black screen before Help files

**These share a lead and a next step**, arrived at independently — which is the main reason to run
it early. @TC1995 suggested the EEPROM angle on #7; #4's own investigation reached the same place.

**#4 established so far:**
- `MSDISP.INF` `[ATI8]` (PnP `*PNP090B`), `atim8.drv` + `ati.vxd`, resource requirements — all
  completely standard, no conflict with our hardware model
- Manual driver selection (bypassing auto-detection) with **two** driver variants — both failed. So
  it is *not* merely a detection problem.
- **Theory A:** the card's EEPROM "power up configuration" is set for 8-bit XT-bus operation (which
  is what makes the Win3.11 driver work), while Win95's `atim8.drv`/`ati.vxd` assume 16-bit AT-class
  addressing for accelerator registers.
- **Theory B:** the Win95 driver assumes address lines an 8-bit XT slot simply does not carry. Not
  mutually exclusive with A.

**#7:** display stalls (CPU still executing) exactly as Setup hands off from the generic VGA driver
to whatever comes next. Leading theory is a stall in the real Mach8 driver's init path. Reproduces
in **both** emulator and real hardware.

**The single unblocking action:** boot the real machine to a **plain DOS prompt** (the utility's own
README says it must not run under Windows) and run `C-INFO.EXE`, or `INSTALL.EXE` → "Show System
Information", from `ATI/ATIMACH8/M8UTL`. That reports the actual chip IDs/revision and the current
power-up configuration — confirming or killing Theory A directly, for both issues at once.

New supporting reference material now available: the
[ardent-tool 8514 Ultra page](https://www.ardent-tool.com/video/ATI_8514_Ultra.html) and the
8514/A register ranges in
[`xt_io_port_reference_annotated.md`](xt_io_port_reference_annotated.md) — note the registers are
**sparse across four ranges** (`02E8`, `06E8`, `0AE8`, `0EE8`), easy to miss when tracing.

---

## Tier 3 — low priority, likely to fall out of Tier 2

### #8 — ATI Mach8 ROM RAM test on boot in emulator
**Explicitly a nice-to-have** (user, 2026-08-23), and it sits naturally downstream of #4: both are
Mach8, and both benefit from the ardent-tool reference and the 8514/A port map that arrived with it.
Reasonable expectation that working #4 either fixes this or explains it.

**But note what was actually asked for, because it is not what we currently do.** @TC1995 in #4:

> do you plan on implementing the ram banks used by the Mach8 self test to correct the ram
> addressing error? … any help making it not say "RAM addressing" in the emulator and, instead,
> making it say "Ok." in the POST/self-test would be greatly appreciated.

So the ask is an **emulation-fidelity fix in 86Box's `vid_ati_mach8.c`** — implementing the VRAM
banking the self-test probes so the test genuinely passes. That is **a different thing from what
this project currently ships**: our `AX = BX+1` at `C000:7B16/7B23/7B37` unsticks the option ROM's
**PIT-readback delay loop**, which is a separate defect. Neither fixes the other.

If pursued, this is a **second upstream PR** against 86Box's Mach8 device, not a change to our POST
fix-ups — and it is a request from an upstream contributor, so worth doing properly rather than
hacking.

**Hypotheses, in cost order** (two are AI-sourced and unverified — treat with suspicion):
1. declared VRAM size vs what the ROM scans (cheapest to check)
2. EEPROM "video test on boot" toggle not persisting in the virtual EEPROM
3. emulator's uninitialised-VRAM fill pattern differing from a real cold boot

**First concrete probe:** read `02E8` (8514/A **display status**) and compare emulator against real
hardware via COMrade's DOS port I/O (skill Technique 6).

**Hex-patching the option ROM is last resort** — it diverges emulator from hardware, against the
project's fidelity goal, and would not satisfy the actual request.

---

## Tier 4 — cleanup and closure

### #2 — Keyboard mapping, `#` in place of `\`
The port list says where **not** to look: `0060`–`0063` are PPI and **port `0x64` does not exist on
this machine**, so this is a **layout/scancode** problem, not a controller one. `AUTOEXEC.BAT`
already runs `keyb uk,,C:\WINDOWS\COMMAND\keyboard.sys`. Start with the Windows-side layout, not the
hardware.

### #3 — Browse button in "Have Disk" causes a fault
GUI-stage; **gated on COMrade `desktop_screenshot`** (i.e. on repointing the MCP servers). No lead
yet.

### #6 — PS/2 mouse appearing in Device Manager
Cosmetic but wrong — there is no 8042 on this machine. Likely a Win95 detection default rather than
anything hardware-side.

### #7 (closure option)
If the reboot workaround remains the honest answer after the Tier 2 EEPROM check, **close it as
documented-workaround** rather than leaving it open indefinitely.

---

## Closing the loop with contributors

Several items above came from named people rather than from our own investigation —
Michal Necasek, @andrew-hoffman, @TC1995, @QuantumByteRider, @Fenix770, @OBattler. Who gave what,
whether it paid off, and **whether they have been told**, is tracked in
[`contributor_input_ledger.md`](contributor_input_ledger.md).

Reporting outcomes back is treated as a deliverable, not a courtesy — including **honest negative
results**, which keep the exchange open where silence closes it. Currently owed: Michal Necasek
(the verified `F000:FF53` result, and his unanswered RAM question), @andrew-hoffman (after the
`DMABufferIn1MB` test, either way), and @TC1995 (that the RAM-banks request is understood as a
separate defect from what we currently ship, and is not yet done).

## COMrade is a ground-truth source, not just a screenshot tool

Worth stating plainly, because it changes how several issues above should be worked: COMrade's
value is that it **opens a window onto what the real hardware actually does**, and that behaviour
can then be **translated into emulation**. It is not merely a way to see the Win95 desktop remotely.

The DOS agent supports **real-mode memory access and port I/O** directly on the real 5160. That
turns "what does this register actually read on real hardware?" from an unanswerable question into
a one-line query — and every one of those answers is a candidate emulation fix.

This is skill **Technique 6**, and it is the single most under-used capability in the project. The
comparisons worth running once the bridge is live:

| Read on real hardware | Compare against | Feeds |
|---|---|---|
| `02E8` — 8514/A display status | 86Box `vid_ati_mach8.c` | #8 (and #4) — the actual RAM-addressing question |
| `0AE8`, `06E8`, `0EE8` — rest of the sparse 8514/A set | emulator equivalents | #8, #4 |
| `0x00A0` behaviour — NMI mask vs Inboard `port_a0` | our `inboard386.c` shadow | the new NMI-mask lead |
| ATI EEPROM / chip ID via `C-INFO.EXE` | what the emulated card reports | #4, #7 |
| `0x83` DMA page register during audio init | VDMAD's buffer placement | #5 |
| `0220-022F` SB Pro register reads | `sb_pro_v2_device` | #5 |

**The direction of travel matters:** real hardware is the reference, the emulator is the thing being
corrected — consistent with the project's standing hardware-fidelity priority. Where the two
disagree, the emulator is wrong until proven otherwise, and the fix belongs upstream in 86Box rather
than as another address-gated workaround in our POST fix-ups.

This is also exactly why #8 is worth doing *properly* if it is done at all: @TC1995 asked for the
RAM banks to be implemented, and COMrade is the tool that can tell us what those banks actually do
on a real card.

## The OSR2 question

Attempting OSR2 is realistic **once Tier 1 and Tier 2 are resolved**, and the groundwork already
exists — see "Forward notes for a future OSR2 attempt" in the skill file. The key points:

- **Emulator/code fixes are OS-version-independent.** The entire "Complete Windows 95 boot fix
  inventory" applies to OSR2 unchanged. Start from that list; do not re-derive it.
- **The BIOS constraint carries over absolutely.** 1986 ROM only — it is an `INBRDPC.SYS` property,
  not a Windows one, so it bites OSR2 identically. Now enforced in code.
- **Disk-image fixes need re-deriving.** The `INBRDPC.SYS` self-test-skip byte should be identical
  (same driver), but `VKD.VXD` and `KEYBOARD.DRV` are *different builds* in OSR2, so patch offsets
  will differ. Use Technique 35 (read the real DDK source, don't guess offsets) and Technique 28
  (assert `Patched: N` with N>0).
- **Do not repeat the archived OSR2 track's mistake.** The 2026-07-31→08-03 attempt chased a
  wild-jump chain (`650B` → `0128`/EMM386 → `0048:00A8`) that was never resolved. The
  `INT 68h`/`patchint68` fix found later on OSR1 addresses the `650B` link in that same chain —
  **try that fix first on OSR2** before reopening the investigation.

⚠️ **Note the overlap with #9:** that archived chain runs through the same `0128`/EMM386 halt that
now blocks Revto486. Resolving the EMM386 bug would plausibly move **both** #9 and the OSR2 attempt.
That makes it a higher-leverage target than its issue count suggests.
