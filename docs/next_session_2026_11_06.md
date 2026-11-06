# Next session, 2026-11-06

Order of work: `docs/mach8_frame_rate_plan.md`, "Order of work (2026-11-06)". **The cloud session's tests are in #59**:
carry this line into new handoffs until every box there is ticked.

## Card

Boot 224, `m8d3d8.drv`, M8DD32.DLL md5 62D00C05C518970D4B866192E9949C8A (50,688 B), 800x600x16.

Owner, 11-06, with the agent running: both dxdiag tests ran without fault, **static gone** on `M8D3D8`. Occasional colour
blocks remain. Hardware far ahead of software. The 11-05 DirectDraw hang on this build did not recur in that run.

## V10 and probes (11-06, afternoon)

Card: boot 227, `m8d3d9.drv`, M8DD32.DLL md5 8975BCCE. Way back: `C:\V9XREMOTE\REG\BACKV10.REG` and `C:\V9XREMOTE\REG\M8DD32.D8` by WININIT.INI. Results and the open hang: `docs/captures/2026-11-06_v10/README.md`.

Probes on the card in `C:\M8P\` (`GO.BAT`, Command Prompt Only), sources in `tools/m8seq/`, decoder `m8vga_decode.py`. None run. Sweeps, not committed, scratchpad only: VGA half and engine source selects; findings to carry into the plan once the probes report. No memory path between the halves (M8XMEM, card). BDh bit 2 untested; "VGA half cannot flip" has no source.

`AGDISK` (with-agent disk baseline) is staged, not run: `C:\V9XREMOTE\TEMP\AGDISK.EXE quiet`, detached.

## Running without the agent

Desktop: `AGENTOFF`, `AGENTON`, `NOAGWTCH`, `NOAGDISK` (all `tools/m8noag/M8NOAG.EXE`, md5 56F011F2, job picked by file
name) and `NOAGENT.TXT`, the steps. Lists: `C:\V9XREMOTE\*.LST`. Log: `C:\V9XREMOTE\NOAG.TXT`, forced to disk before each
step. Off and on proven on the card by registry read-back. Runner proven with a one-line list.

REGEDIT on 4.00.950 a ignores `"value"=-` and `[-key]` and exits 0; `regedit /d` also left the key. So the agent's own
`REMOVE.REG` does nothing here. Tell its author: owner's call.

Tray: Settings was greyed because `C:\V9XREMOTE\SETUP.EXE` was not on the card (agent docs, `guest-agent.md`). Now there,
sha256 matches the package; not yet seen in the menu. Saving restarts the agent. The menu has no Stop item.

## After the owner's runs

1. Fetch `NOAG.TXT`, `M8D3DF.TXT`, `M8PAGE.TXT`, `M8BENCH.TXT`, `XTSECT.TXT`, `IDLECOST.TXT`, `LZ4B*.TXT`, `PAGETIME.TXT`,
   `PERFLOG.CSV` into `docs/captures/2026-11-06_noagent/`.
2. With-agent comparison: restart, `NOAGDISK` under another name with the label changed from `noag`. Start it detached.
3. Stage 1 onward.

Owner to bring: the CF in the host reader once, for `sectors.py` over `WIN386.SWP` (Windows holds the file open). Never commit it.

## XT-CF test builds (#59 item 1), built 11-06, not run

From `origin/claude/xtcf-tight-taskfile` c06907a, in `%TEMP%\xtcf_builds\<name>\XTIDEMP.MPD`, 11,264 B each. Rebuild from the
branch if `%TEMP%` has been cleared: the ledger rows were in a discarded worktree.

| Build | md5 | Flags beyond `-PollBackoff -FastXfer -WaitFirst -YieldCommit` |
|---|---|---|
| control | 6f0903dcd8039b1787a3ccfe12226c4b | none |
| tight | 5df3afac4245bcf7fa9f6e063e322790 | `-TightTaskfile` |
| skip | b417984d637eb0543300ab60674686df | `-SkipDrvHd` |
| both | daef68302c13295f7d9447711c7d6d4b | both |

Control against the card's `a67fefc4`: 183 bytes differ, all in the timestamps, the header checksum and the embedded build path.
The code is the same, so the card's file stays the baseline. An md5 cannot be compared across build folders.

## Open

- Bed `vm_5160_mcp` still has the error-tagged `DDRAW.DLL`: restore `C:\WINDOWS\SYSTEM\DDRAW.DLL`, md5 EEDE14A8FB89A725C903E33A6A0F9F28.
- Sound-loop locks (#55): four reports, cause open. Agent-free runs bear on the owner's theory that the agent contributes.
- Stills `triangle1-2, 4-7, 9` unread.
- #55 and #59 updates not posted.
