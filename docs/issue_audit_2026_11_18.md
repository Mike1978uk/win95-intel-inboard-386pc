# Issue accuracy audit, 2026-11-18

Read-only audit. Nothing posted, edited or closed. Sources: `gh issue list/view`, `docs/next_session_2026_11_19.md`,
`docs/contributor_input_ledger.md`, `docs/issue10_vpicd_buffered_2026_11_18.md`, `docs/issue10_ibvpicd_2026_11_18.md`,
`docs/dos_driver_groundwork_2026_11_18.md`, `git log -40`. Not verified: GitHub state beyond issue bodies and last
comment (earlier comments skimmed only); the 5160 results are taken from the handoff, not re-measured.

## 1. Open issues (17)

| # | Title | GitHub says | Now true | Verdict |
|---|---|---|---|---|
| 10 | 1982 ROM shim idea | Status 10-21: cause on 5150 unknown; last comment 10-04 | Buffered-mode VPICD built, holds on 5160 (twist 6.73 fps), sent to Simon (`b20b0ce`); H1 ruled out (PICCHK), H2 weak (only VGA BIOS touches 100h-1FFh); IBVPICD writes ICW4 09h vs ours 01h; Supersoft IRQ0 failure = rate check (`issue10_old_bios_2026_09_27.md` s15-19, ledger l.65-69) | needs update |
| 18 | Floppy garbage after media change | Parked; bed not built | Unchanged; reply to Andrew still listed owed in `next_session_2026_11_18.md` l.53 | current (reply owed) |
| 41 | Pace polling in spinning drivers | XT-CF done; T130/ELNK3 unread | No later work in log | current |
| 43 | IOCHRDY cost | Not started beyond fit | No later work | current |
| 47 | FastDoom SB sound | Not started as sweep | Unchanged; owner reply owed (`_11_18.md` l.53); X12 VGA-half path is the related lever (worklist l.356) | current (reply owed) |
| 48 | Optimisation timeline | Last entry 10-11 | No entries since: Mach8 ~1.27x ATIM8, twist/flip2d numbers, 16 bpp all later | needs update |
| 49 | Mach8 driver | Body status 10-21 (35 tests in 86Box, "not yet run on the real 5160"); last comment 10-22 (1.27x ATIM8 on card) | Status block contradicts the 10-22 comment. Since: 16 bpp, DirectDraw, 8 bpp pages flip in bed (X22, local `26fd9ec4c`, not upstream), twist 6.73 fps, flip2d 4.33 fps, DOS 8 bpp mode (`bcec7f9`) | needs update |
| 50 | BlueSCSI/DaynaPort idea | Last comment 11-17: L1 on/off no difference, 3.93 ms/KB | Same (ledger l.419) | current |
| 51 | Filesystem vs bus bytes | Last comment 10-23 (CH376) | No change | current |
| 52 | xtide_mpd from WINDISK | Last comment 11-04 | No change; worklist l.436 still open | current |
| 53 | Leads from linked repos | Last comment 10-23 | No change | current |
| 54 | Change detection by hash | Comment 11-04 audit | Later: overlay hash without imul (`a2f862c`), mask build/hash without imul (`312e5aa`) | needs update (minor) |
| 55 | 16 bpp restart hangs, SB loops sound | No comments; "not reproduced in bed" | Startup (not shutdown) sound loop locked the card at first boot this morning on the unchanged stack (`next_session_2026_11_19.md`, "2026-11-19 morning"). Second occurrence, different boot point. Next: one boot with sound scheme off | needs update |
| 56 | Hang during dxdiag + screenshot | No comments | Nothing recorded since | current |
| 57 | Apply Mach8 learnings to other drivers | Last comment 11-06 | No later driver-audit work seen in log | current (low activity) |
| 58 | DirectX on the 5160 | Last comment 11-06 | Later: 8 bpp two-page flip in bed, DDraw BSOD after mode change, Foxbear crawls, Doom 95 findings, flight-demo survey, DOS route (`next_session_2026_11_19.md`) | needs update |
| 59 | Cloud-session TODO tests | Comment 11-06: no box ticked | Not re-checked; handoff does not appear to carry the "#59" line (grep not run on all docs) | needs update (check) |

Closed in the last 14 days: none (100 closed searched, closedAt after 2026-11-01). Nothing to reopen. #23, #28, #29, #31, #35, #45 closed 2026-10-21.

## 2. Draft comments (owner edits and posts)

#10:

```
Status 2026-11-18. Thanks to Cimon for the PICCHK run and the photos.

PICCHK on the 5150: the 8259 and IRQ0 delivery work at DOS, so a bad PIC setup there is ruled out. The Supersoft "Interrupt level 0" failure is a rate check. In the emulator only the VGA BIOS touches ports 100h-1FFh, so that lead is weak.

New lead: Intel's IBVPICD.386 programs the 8259 single and buffered (ICW1 13h, ICW4 09h), like the BIOS. Our Windows 95 VPICD writes cascade, non-buffered (11h, ICW4 01h). A buffered-mode VMM32 is built and holds on the real 5160 (desktop, NIC, T130B, keyboard, mouse, floppy, twist). It has been sent to Cimon; the question is whether Windows 95 reaches the desktop on his 5150. Not tested on any 5150 yet.

Details: docs/issue10_vpicd_buffered_2026_11_18.md and docs/issue10_ibvpicd_2026_11_18.md. Thank you Cimon for the patient testing.
```

#48:

```
Status 2026-11-18. Entries since 2026-10-11 not yet added: Mach8 driver at about 1.27x ATIM8 on the card (#49), 16 bpp mode, change detection by hash without imul (#54), 8 bpp two-page flip in the emulator, twist 6.73 fps and flip2d 4.33 fps on the card. To be written up from docs/worklist.md and docs/next_session_2026_11_19.md.
```

#49:

```
Status 2026-11-18. The status block above is out of date: the driver has run on the real 5160 since 2026-10-22 (about 1.27x ATIM8, pixels identical in all 35 tests, see the comment of that date). Since then: 16 bpp mode, DirectDraw at 8 and 16 bpp, 8 bpp pages flipped by CRT_OFFSET (emulator model fix not yet upstream), a 640x480x8 mode set from DOS. Direct3D twist demo 6.73 fps on the card. Open: DirectDraw blue screen after a mode change, startup sound loop (#55). Plan: docs/worklist.md section L.
```

#54:

```
Update 2026-11-18. The lessons were applied: the overlay hash (a2f862c) and the mask build and hash (312e5aa) no longer use imul. Card timings are in docs/next_session_2026_11_19.md.
```

#55:

```
Update 2026-11-18. Second occurrence, at a different point in the boot: the card locked at the first boot to the desktop with the Sound Blaster Pro looping the Windows startup sound. The owner reset it; the next boot was normal. The stack was unchanged (before the buffered VPICD), so this is not new code. The shutdown-sound hang above and this one may be the same fault. Next recurrence: one boot with the sound scheme set to none. Still not reproduced in the emulator.
```

#58:

```
Status 2026-11-18. DirectDraw: 8 bpp two-page flip works in the emulator (not yet upstream); a blue screen after a DirectDraw mode change is under investigation. Foxbear loads and then crawls: DirectDraw's software emulation on surfaces that do not fit the card, no free RAM. Doom 95: windowed 8 bpp runs with sound but slowly; on 16 bpp it asks for 320x200x8 and our driver draws a quarter-size image; full screen on the card wedged. Direct3D: twist 6.73 fps; no 486-class flight demo exists (docs/flight_titles_2026_11_18.md). DOS: 640x480x8 from a DOS prompt and about 4.2 MB found by DOS/32A (docs/dos_driver_groundwork_2026_11_18.md). Native DirectSound not started.
```

#59: no draft; owner to confirm whether the "#59" line is carried in the handoff.

#18 and #47: replies to Andrew are listed as owed in `docs/next_session_2026_11_18.md` l.53; no new result exists, so no draft.

## 3. README.md stale lines

- Lines 535-551, "Open issues" table: #23, #28, #29, #31, #35, #45 are closed (2026-10-21); remove those rows. Open issues missing: #50-#59 (add one line each: #50 BlueSCSI bridge idea; #51 filesystem vs bus bytes; #52 xtide_mpd from WINDISK; #53 leads from linked repos; #54 change detection by hash; #55 16 bpp restart and startup sound loop; #56 dxdiag hang with agent screenshot; #57 apply Mach8 learnings to other drivers; #58 DirectX on the 5160; #59 cloud-session test list).
- #10 row, correct text: "1982-era ROMs. The 08NOV82 XT ROM runs Windows 95, on a real 5160 and in the emulator (offered in 86Box since #8217); a real 5150 still stops when Windows starts. A buffered-mode VPICD (matching Intel's IBVPICD) holds on the 5160 and is with Cimon for the 5150."
- #49 row: "the first timed run against ATIM8 on the 5160 is still to come" is wrong. Correct: "measured on the 5160 at about 1.27x ATIM8 overall, pixels identical in all 35 tests; 16 bpp and DirectDraw work".
- Line 516: "Most open issues carry a Status block" is untrue now; only #10, #18, #41, #43, #47, #48, #49 do. Reword to "Some".
- Line 274, V9x PR row: "Open, 2026-11-17; Michael Dale offered to take it" is stale. Correct: "Shipped by Michael Dale in 0.19.0 (2026-11-18, co-author credit)" (ledger l.418). Line 487 not checked for the same claim.
- Line 237 "Twenty-two PRs": table row count matches (22). Line 279 "every Inboard change is now merged except one": not verified.
