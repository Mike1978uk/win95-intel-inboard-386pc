# Next session - from 2026-11-20 (late afternoon)

## START HERE: X33 lead - the Blue Lightning setting does not always hold

- At one boot today the card's screen showed `CPU NOT SET: ...` from `C:\CTCHIP\CPUCHK.COM`
  (run by `CPUSET.BAT` from `AUTOEXEC.BAT`, added 11-19 in `ab1db53`). The owner saw it; the
  failing register and value were not recorded.
- Warm restart out of Windows (boot 78, 17:05): `CPU OK: 2x, L1 on, 1-16 MB cached`.
- CPUCHK only reads (RDMSR). Reverting `ab1db53` would hide the message, not change the CPU.
  Card `CPUSET.BAT` and `CPUCHK.COM` are byte-identical to the repo.
- Owner: the boot-option changes came AFTER the slowdown was seen, so they are not the cause.
  A backup of the earlier card image exists if needed.
- Fits X33 (`docs/worklist.md`): fast after a warm restart out of Windows, slow from cold.
  Candidate: CPUSET's MSR writes sometimes do not take on a cold boot (clock 1x, cache off or
  range wrong), so the CPU runs half speed or uncached.

**Next, in order (each cheap):**
1. Card is in the DOS agent now, warm boot, CPU OK: bench it (`C:\X33.BAT` detached, which runs
   `C:\DOS3D\M8DX33.EXE bench 50 40 200 nv`). Expect the fast figure, 54.35 fps.
2. Owner powers off and on. At the DOS agent, run `C:\CTCHIP\CPUCHK.COM` through `exec` (with
   `-Json` to see Stdout) and note the register that differs, then bench again.
3. If cold = NOT SET + slow, and warm = OK + fast: find why CTCHIP34's write fails cold (order,
   a register that needs the cache flushed first, the 1002h:3 clock switch needing a delay).
   CPUSET could loop until CPUCHK passes (exit code 1 = not set).
4. Also re-check the earlier X33 finding "lost at every m8dos exit": run CPUCHK before and after
   an m8dos run. If the MSRs survive, the exit finding needs another look.

Do not touch ports 22h/23h (they alias the 8259 here). Never set SNP (1000h:0 bit 3): it hangs the machine.

## Done today
- Fine dither (16 bpp idle refinement) fixed for window moves: `be62dab`. Bed `vm_5160_mcp` runs
  `M8EF.DRV`; FB16CHK reads 0 after moves at odd offsets, with M8FINE fully refined (was up to
  7,404). Card not yet updated: build from `drivers/mach8_display/src/build.ps1`, and do NOT
  use the staged `%TEMP%\card_fine\M8FINE.DRV` (old, buggy).
- `_TEXT` of M8DISP.DRV is 0xFFC9: 54 bytes left. The next driver change must move code out.
- X33 card-state findings: `ff22efe`, `docs/captures/2026-11-20_x33_cold_card/`.

## Agent problems (worklist X32)
- My clock BAT (`C:\CLKSET.BAT`: `DATE %1`, `TIME %2`, `SMWCLOCK C`) hung the DOS agent:
  DATE rejected `20-11-26` and sat at the "Enter new date" prompt. Never pass DATE/TIME
  arguments through the agent without checking the format by hand first; prefer setting the
  time from a program, not the DOS prompt commands. SmartWatch is still about 59 min ahead.
- Detaching a bare command (`WIN`) wedges the agent; detach a full-path BAT only.
- The owner thinks the DOS agent drops when idle; not confirmed (today's drop was the DATE prompt).

## Still pending
- Put the fine dither and the DXTWALL wallpaper on the card (staging `%TEMP%\card_fine\`,
  rebuild the driver first).
- Agent-repo docs commit 6e58ea7 waits for the owner's OK; Esc at the agent console.
- Then the approved plan: X34/X35, L6, DirectXT X29/X26 D14, BRender X24.
