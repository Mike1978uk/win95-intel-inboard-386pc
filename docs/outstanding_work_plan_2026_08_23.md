# Outstanding work plan — as of 2026-08-23

Captures every open thread from correspondence, external clues, and the GitHub issue tracker, with
a recommended order of attack. Written after the 86Box POST 101 work closed out
([#7749](https://github.com/86Box/86Box/pull/7749) open, `86box_full/` ported, release rebuilt).

**Ordering principle:** time-sensitive first, then infrastructure that makes everything else cheaper
to debug, then real bugs ordered by *strength of the lead* and *cost of the next test*, then
cleanup. Within each item, the cheapest falsifying test comes before any code change — the SB Pro
`vmad.vxd` patch was written and failed on real hardware without a cheap test first, and that is the
mistake to avoid repeating.

**Source-quality warning:** several clues below came from an AI assistant and carry citation markers
but no primary source. They are recorded as **hypotheses to test**, not facts. Each is marked.

---

## Phase 0 — while PR #7749 is still in review (time-sensitive)

### 0a. Adopt Michal Necasek's `F000:FF53` suggestion — VERIFIED, do this first

Michal Necasek (2026-08-09) asked: *"if you need an IRET instruction for an interrupt vector to point to,
why not use the one at F000:FF53?"*

**Checked, and he is right.** Byte at `F000:FF53` is `CF` (IRET) in **both** supported ROMs:

| ROM | file offset `0x7F53` in the U18/F800 chip | bytes |
|---|---|---|
| 09MAY86 | `BIOS_5160_09MAY86_U18_…_F800.BIN` | `cf 1e e8 b9 fa …` |
| 10JAN86 | `BIOS_5160_10JAN86_U18_…_F800.BIN` | `cf 1e e8 ba fa …` |

Our current `[patchint68]` fix writes an IRET stub to physical `0x3C0` — **borrowing INT F0h's own
4-byte vector-table slot as scratch code space** — then points INT 68h at it. Michal Necasek's version is
strictly better on every axis:

- no injection of executable bytes into guest memory at all, just a vector write
- no assumption that INT F0h is unused (that assumption was always the weakest part of the fix)
- points at the BIOS's own canonical dummy-interrupt handler, which is what the BIOS itself uses
- 4 bytes written instead of 5

**Action:** change the vector to `0000:01A0 = F000:FF53` (`53 FF 00 F0`), drop the `0x3C0` write,
update the comment to credit Michal Necasek, rebuild, re-verify one Win95 boot, push to #7749. Do it while
the PR is under review rather than as a later follow-up.

**Caveat to check during the rebuild:** the fix currently fires when `CS` first becomes `0x0EAF`.
That timing logic is unchanged — only the vector target changes — so a single boot-to-desktop run is
sufficient re-validation.

### 0b. Reply to Michal Necasek — treat as a deliverable of 0a, not an optional courtesy

He gave us a correct, checkable improvement to a fix that was already public in a PR. Closing the
loop with the actual outcome is the whole point of the exchange, so **0a is not done until 0b is
sent.** Include the concrete evidence, not just "thanks, done":

- the byte check (`CF` at `0x7F53` in the U18/F800 chip of both 1986 ROMs)
- what the fix looked like before (IRET stub written into INT F0h's vector slot at `0x3C0`) and why
  his version is better (no injected code, no assumption about INT F0h being unused)
- the PR link, so he can see it in context

Two things owed in total:
1. **Credit + confirmation** on `F000:FF53`, once 0a is pushed.
2. **His actual question**, still unanswered: *"back in the day Win9x could never support the
   Inboard 386/PC because there was not enough RAM to run Win9x in the first place?"* — worth
   answering properly. The card tops out well below what Win95 wants, and our own working setup runs
   5 MB, which is not a period-plausible Inboard configuration. His framing is probably right and
   it is a genuinely interesting point about *why* nobody hit these bugs in 1995.

He also observed that the keyboard being the biggest blocker is unsurprising, since the 8042 was
baked into the PC/AT design and Win9x had no reason to abstract it — which matches exactly what the
custom `VKD.VXD` work found.

---

## Phase 1 — infrastructure that unblocks the rest

### 1a. Get COMrade onto the CF card — do before any real-hardware debugging

Both binaries are **already present locally**, no need to fetch from GitHub:

```
C:\Users\lycet\RiderProjects\Open-Source-PC110\Software\COMrade\dist\COMRADE.EXE   (DOS)
C:\Users\lycet\RiderProjects\Open-Source-PC110\Software\COMrade\dist\COMR95.EXE    (Windows 95)
```

Put **both** at the root of `C:` on the CF card for easy invocation. On Win95:

```
COMR95 /com1 /baud 115200
```

COMR95 gives HELLO/status, text screen, **desktop thumbnails**, file read/write, directory listing,
attributes, CRC hashing, and keyboard input — i.e. it makes the GUI-stage bugs (issues #3, #4, #5,
#6, #7) debuggable *in place* on real hardware instead of by photographing the screen. That is why
it comes before them.

Reminder: COMrade is **real-hardware-only** — it is not a route for emulator debugging.

---

## Phase 2 — the real bugs, strongest lead first

### 2a. Sound Blaster Pro 2 (CT1600) → `vmad` BSOD — issue #5

**Status:** a `vmad.vxd` patch was written on the theory that Win95 was touching the non-existent
secondary 8237 (ports `0xC0`–`0xDF`). It **failed on real hardware** (photo evidence). Do not
re-patch on the same theory.

**The stronger new lead**, from andrew-hoffman:

> The PC DMA controller only has a 4-bit page register so you will need to make sure any buffer the
> sound blaster DMAs from is in the first 1mb of RAM that the DMA controller can see.

This is a much better fit for the symptom than the secondary-controller theory, and — importantly —
**Windows already has a documented setting for exactly this**, so it can be tested with no code at
all. Try, in `SYSTEM.INI` `[386Enh]`:

```ini
DMABufferIn1MB=Yes
DMABufferSize=32
```

**Do this before touching `vmad.vxd` again.** If the buffer placement is the cause, this either
fixes it or changes the failure mode, and either outcome is informative for the cost of an INI edit.
Requires the real-hardware-equivalence note per project convention: this is a `SYSTEM.INI` edit, so
the real-hardware equivalent is identical — the same edit on the CF card.

**Fallback if that does not resolve it** — drop out of the protected-mode VxD audio stack entirely
and force the Windows 3.1-era 8-bit driver via `SYSTEM.INI` `[drivers]` (Sound Blaster 1.5/2.0 style
`.drv`). *(Hypothesis, AI-sourced, unverified.)* This yields simple unmixed 8-bit audio that respects
native XT interrupt routing. Lower fidelity, but "sound works at all" is the milestone.

**Only after both cheap routes fail** should the `vmad.vxd` disassembly be redone — and per the
existing triage note, redone from scratch rather than patched further.

Card choice is not the problem: the CT1600 is natively 8-bit ISA and is the right part for this bus.

### 2b. Revto486.sys TSR halts during boot — issue #9

**The clue** (vogons, Evergreen `revto486.sys` on a Model 30):

> It only works when I make sure the CPU is in 386 protected mode before loading the Evergreen
> driver and DOS is running in virtual x86 mode.

So the driver expects to be loaded *into* an already-established V86 environment, not on bare real
mode. **Cheap test:** change CONFIG.SYS load order so a V86 provider (EMM386 or equivalent) is
active *before* `revto486.sys` loads, rather than after. No patching, one reboot.

If that works on DOS, the Win95 question becomes how to reproduce that ordering during Win95's own
boot — which is a second, separate step, and should not be attempted until the DOS-level ordering is
confirmed to matter.

### 2c. ATI Mach8 ROM RAM test on boot in emulator — issue #8

**Current state:** worked around, not root-caused. `inboard_post_fixups()` forces `AX` past `BX` at
`C000:7B16` / `7B23` / `7B37` so the option ROM's PIT-readback delay loop resolves. That hack is now
in upstream #7749, honestly labelled as such.

**Hypotheses to test** *(all AI-sourced and unverified — treat with suspicion, especially the
architecture claims)*:

1. **Video memory size mismatch.** If the emulator profile declares a different VRAM size than the
   ROM scans for, the ROM may loop trying to isolate "missing" banks. Cheapest test of the three:
   compare the declared size in the 86Box Mach8 device against what the ROM expects.
2. **EEPROM / config override.** The real card stores config in an onboard EEPROM written by ATI's
   `INSTALL.EXE` / `SETMACH.EXE`. If there is a "Video Test on Boot" / "Co-processor Test" toggle,
   and 86Box is not persisting the virtual EEPROM cleanly, the ROM would re-run the test every boot.
   Testable by running the ATI DOS utility under the emulator and seeing whether the setting sticks
   across a restart.
3. **Uninitialized VRAM pattern.** Claim is that emulators may map the framebuffer with a different
   fill pattern than a real cold boot, so the ROM sees "changed/corrupt" hardware. Plausible, and it
   would explain why real hardware slips past the test while the emulator does not.
4. **Last resort:** hex-patch the dumped Mach8 option ROM to NOP the diagnostic call. This is
   explicitly the *worst* option — it diverges the emulator from real hardware, which cuts against
   the project's hardware-fidelity goal. Only if 1–3 are exhausted.

**Reference material to mine first:**
- <https://www.ardent-tool.com/video/ATI_8514_Ultra.html> — detailed Mach8 documentation
- `C:\Users\lycet\OneDrive\Desktop\XT_project\inboard_files\win95_attempts_files\XT, AT and PS2 IO port addresses.txt`
  — has Mach8 ROM addresses plus the I/O and ROM addresses Windows 95 uses

Note the architecture description in the source ("28800 VGA core + 38800 coprocessor") should be
checked against the ardent-tool page before being relied on.

---

## Phase 3 — remaining issue triage and cleanup

Work these once COMrade is on the CF card (Phase 1), since most are GUI-stage and become far cheaper
to diagnose with a live bridge.

| Issue | Title | Notes |
|---|---|---|
| #2 | Keyboard mapping, `#` in place of `\` | UK vs US layout; likely a scancode-map issue, not hardware |
| #3 | Browse button in "Have Disk" causes a fault | GUI-stage — needs COMrade |
| #4 | ATI Mach8 stock driver not working | Related to #8; the display-driver address ranges were offered as a place to trace |
| #6 | PS/2 mouse appearing in Device Manager | Cosmetic-ish but wrong — no 8042 on this machine |
| #7 | Setup hangs to black screen before Help files | Known workaround: reboot. May be a dead end worth closing as "documented workaround" |

**For every issue:** post a comment with whatever lead exists, even partial. **Close the dead ends
explicitly** rather than leaving them open — #7 is the obvious candidate if the reboot workaround is
the honest answer.

---

## Not forgotten, lower priority

- **`machine_table.c` naming not synced with upstream.** Upstream renamed the machine to
  `[386DX] IBM XT (Inboard 386/PC)` and trimmed `.cpu.package` in commit `f8a10398`. Deliberately
  not ported into `86box_full/`, because the package trim would invalidate local test configs that
  use 386SX-family CPUs. Cosmetic; revisit only if title-bar consistency starts costing time.
- **Dirty run artifacts in the repo:** `bios_f000_dump_int15.bin`, `vram_dump.txt`, and a deleted
  `vm_win311/stderr_log.txt`. Decide whether these should be tracked at all — they are regenerated
  by every run and mostly add noise to `git status`.
- **~99 untracked `live_*.txt` trace files** in the repo root from this investigation. Worth a
  `.gitignore` rule rather than repeated manual filtering at commit time.
