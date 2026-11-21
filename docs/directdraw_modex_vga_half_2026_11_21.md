# DirectDraw Mode X and the VGA half (research, 2026-11-21)

Status marks: **read** (source in hand), **inferred** (reasoned from read facts), **unknown** (no source found).
Paths are under the repo root unless a URL is given.

## Summary

1. The worklist X1 premise "the program Locks straight into VGA memory" is wrong for DirectDraw's own Mode X: Microsoft documents that a Mode X primary cannot be locked, blitted to, or given a DC (read, section 1). Doom 95 must use a back buffer.
2. A driver-published linear 320x200x8 mode, set by our own ReEnable, can be locked (VGA half in mode 13h, A0000, 64,000 B, fits one 64 KB bank). Try this first; it needs no ddraw Mode X path. Evidence: velocity9x SiS run, a driver-listed 320x200 mode was accepted by DirectDraw once `wRefreshRate` was 0 (read; our modes already leave it 0).
3. Clearing `DDHALINFO_MODEXILLEGAL` (`drivers/mach8_display/src/dd16.c:342`) gives ddraw's own Mode X, but the 11-02 bed run died in SetPalette (Disabled driver) and the monitor never switched. Second choice; needs a trace of which driver entries ddraw calls.
4. Whatever route: the output switch (`m8_hw_passthrough`, `m8hw.c:382`) and "nothing draws on the Mach8" (`m8_switch_out`/`m8_switch_in`, `enable.c:282-310`) already exist for DOS boxes; reuse them.
5. Return path: ReEnable back to the 8514 mode rebuilds the PDevice and repaints (`enable.c:536-580`); needs the 1 s blank accepted.

## 1. How Windows 95 DirectDraw does Mode X and 320x200

- Two Mode X modes, 320x200x8 and 320x240x8, are provided by DirectDraw for all cards; they appear only if the app set `DDSCL_ALLOWMODEX | DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE`; otherwise `SetDisplayMode` returns `DDERR_INVALIDMODE`. Surfaces carry `DDSCAPS_MODEX`. **read** https://techshelps.github.io/MSDN/DXFOUND/devdoc/good/directx/ddoverv_5upj.htm ; https://jeffpar.github.io/kbarchive/kb/139/Q139096
- No Lock, no Blt to, no GetDC on a Mode X primary; the KB says create a primary with one back buffer, render to the back buffer, `Flip`. **read** same two URLs. The back buffer is therefore system memory plus ddraw's own copy/flip. **inferred** (no source says where the back buffer lives).
- Mode 13h (standard VGA) is separate: enumerated only with `DDEDM_STANDARDVGAMODES`, set with `DDSDM_STANDARDVGAMODE`; flag `DDSCAPS_STANDARDVGAMODE`, never combined with `DDSCAPS_MODEX`; it is linear but unflippable and the primary is not locked. If a card has a linear 320x200x8 mode, ddraw enumerates that instead and a Mode 13 request uses it. **read** https://techshelps.github.io/MSDN/DXFOUND/devdoc/good/directx/ddoverv_7e7r.htm ; `references/dxddk/dx7ddk/src/F4965_ddfncs.doc` (DDSCAPS text, extracted by script); `references/dxddk/ddk9x/win98ddk/src/SRC_DISPLAY_INC_DDRAWI.H:1259-1264` ("standard VGA modes are tagged as MODEX and STANDARDVGA").
- Linear low-resolution modes: the primary can be locked and accessed directly. **read** (first URL above).
- Which component sets the mode: DDRAW.DLL is the 32-bit runtime; DDRAW16.DLL is "the 16-bit DirectDraw component that handles thunking to the DirectDraw portion of the GDI driver, receiving mode information from the GDI driver, dynamic color changing, and ModeX information". **read** `references/dxddk/dx7ddk/src/F4966_ddraw.doc` line 2276 (RTF). Whether DDRAW16 runs INT 10h itself, writes CRTC registers (Mode X is "direct programming of the VGA registers", https://jeffpar.github.io/kbarchive/kb/128/Q128804, **read**) or goes through the VDD: **unknown**. No local or web source states it.
- Related flags (**read**, `SRC_DISPLAY_INC_DDRAWI.H`): `DDHALINFO_MODEXILLEGAL` 0x2 "this hardware does not support modex modes" (:1586) copied to `DDRAWI_MODEXILLEGAL` 0x10000 (:1499); `DDRAWI_MODEX` 0x10 "driver is using modex" (:1487); `DDRAWI_STANDARDVGA` 0x100000 (:1503); `DDRAWILCL_ALLOWMODEX` 0x40 (:1476); `DDMODEINFO_MODEX` 0x2 and `DDMODEINFO_STANDARDVGA` 0x8 in a mode-table entry (:1259-1264, "mode is a modex mode", "mode is standard vga, e.g. mode 0x13"). Meaning of MODEXILLEGAL on the runtime side (hides the two built-in modes, per our 11-02 bed run: clearing it made ddraw list 320x200x8 and 320x240x8, `docs/next_session_2026_11_02.md:259-260`): **read** (that run), mechanism **inferred**.
- Does the display driver get Disable/Enable around Mode X? Evidence from our bed (11-02): `SetPalette` reached the driver with no device ("DDRAW's Mode X set its palette with the driver disabled -> null DIB device -> fatal 0E", commit `84be69f`; `docs/next_session_2026_11_02.md:254`). `m8_pdevice` is nulled only in `Disable` (`enable.c:429-447`), so a Disable-like state was in force: **inferred**. Which entries ddraw called, in what order, on entry and exit: **unknown** (the "trace build" step on `docs/next_session_2026_11_02.md:261-262` was not recorded as done).
- Bed behaviour with MODEXILLEGAL cleared, M8DDB `modex`: from the 16 bpp desktop `SetDisplayMode(320,200,8)` gave `DDERR_INVALIDMODE`; from 8 bpp it succeeded (primary caps 1020c200, pitch 320); `SetPalette` killed the program; with the palette skipped (`2aeb8f2`) the mode survived but a fill raised `DDERR_EXCEPTION`, i.e. the VGA memory access failed; the monitor stayed on the 8514 output. **read** (`docs/next_session_2026_11_02.md:259-266`, commit `2aeb8f2` message). Note the primary reported pitch 320 and a lockable-looking surface in that run, which conflicts with "cannot be locked"; whether Lock succeeded was not recorded: **unknown**.
- velocity9x on the same ddraw: 640x400 hidden because the HAL published `wRefreshRate = 60`; with 0 every driver mode (22 of 22, incl. 640x400, 320x240, 320x200 at 8 bpp) was accepted as the driver's own modes, with a packed pitch and a lockable primary (SiS 6326 guest). Reason is inferred by that author ("line rate against the monitor"). `references/velocity9x/docs/issues/2026-08-15-doom95-low-resolution-modes.md`, section "2026-10-05". **read**. Not re-run there: Doom 95 itself. Their earlier run with `DDSCL_ALLOWMODEX`: ddraw's emulated 320x240 returned `S_OK`, pitch 320, Lock succeeded, then the probe died; Doom 95 at 320x200 crashed with an access violation at `ffffffff` (same file). **read**.
- Flip and palette in Mode X: Flip is the documented way to present (KB above, **read**); palette goes through `SetPalette` (our bed shows it arrives at the driver's `SetPalette` while disabled, **read**); how ddraw writes the DAC (VGA ports vs. a driver call): **unknown**.
- Doom 95 today: owner reports `SetDisplayMode(320,200,8)` accepted with no real mode change and a quarter-size image (`docs/worklist.md:370-371`). No capture of why was found: **unknown**. (Likely a ddraw emulation on the 8 bpp desktop; **inferred** only.)

## 2. What the display driver must do

Route A, driver-published linear mode (recommended first; **inferred** from velocity9x result plus DDK structure comments):
- Add a 320x200x8 entry to the mode list; keep `wRefreshRate` 0 (our `m8dd_build` zero-fills each entry, `dd16.c:213-221`, so it already is).
- `ValidateMode` must return `VALMODE_YES` for it (`enable.c:601-612` accepts only table sizes).
- ReEnable (`enable.c:536-580`) must, for this mode: stop all drawing (as `m8_switch_out`), switch the mux to the VGA half, `INT 10h` AX=0013h (or direct 28800 programming; the BIOS call is already used by `Disable`, `enable.c:445`), and report GDIINFO for 320x200. DirectDraw's `fpPrimary` must then be the linear address of A0000 (`hi->vmiData.fpPrimary`, `dd16.c:~300`, today `m8dd_shadow_lin`) with pitch 320.
- On return (SetDisplayMode back to the desktop mode, ReEnable again): `m8_hw_set_mode`, palette, `m8_screen_resume`, `m8_request_repaint` as `m8_switch_in` does (`enable.c:300-320`). The mux flip costs over 1 s of blank each way (`docs/mach8_vga_half_findings_2026_11_06.md:8`, **read**).
- A single 64 KB page: no hardware Flip in mode 13h (**read** doc above); the program Locks the primary or Blts a system-memory surface (`DDCAPS_CANBLTSYSMEM` is set, `dd16.c:~262`). 320x240 does not fit unbanked (`docs/worklist.md:267-268`, **read**) and is not standard 13h: leave it out of the first cut.
- Pitfalls to check: a driver mode below 480 lines may be hidden (refresh 0 solves it on SiS, not run on a Mach8); `DDHALINFO_MODEXILLEGAL` can stay set in Route A (then the two built-in modes stay hidden and the driver's 320x200 is the only one): **inferred**, test; the S3 note that DirectDraw "admits no sub-480-line mode outside its own ModeX set" was later disputed by the refresh-rate result (same velocity9x file).

Route B, ddraw's Mode X (clear `DDHALINFO_MODEXILLEGAL`):
- ddraw sets the VGA mode itself (mechanism **unknown**). The driver must survive being called while disabled: `SetPalette` is guarded (`enable.c:451-454`); other entries are not audited: **unknown**.
- The driver, not ddraw, must hand the monitor to the VGA half. `Disable` already does it (`m8_hw_passthrough`, `enable.c:429-447`); in the bed the output stayed on the 8514 (11-02, **read**), cause **unknown** (does ddraw call Disable at all; does the 86Box model act on ADVFUNC bit 0).
- Programs then draw into a system-memory back buffer and Flip; the 8514's 64,000-byte copy cost is replaced by ddraw's planar copy to the VGA: speed **unknown**. This route does not give "Lock straight into VGA memory".
- Restore on exit: **unknown**; the 11-02 run saw an Internat page fault in KERNEL32 at shutdown after a failed attempt, link unproven.

## 3. Where it is in our sources (drivers/mach8_display/src)

| Item | Where | Mark |
|---|---|---|
| `DDHALINFO_ISPRIMARYDISPLAY \| DDHALINFO_MODEXILLEGAL` | `dd16.c:342` (8 bpp build); 16 bpp build has its own HAL info (`m8dd_build16`, `dd16.c:~355-480`), flags not checked | read |
| Mode table in HAL | `m8dd_build`, `dd16.c:213-221` loop over `M8_MODE_COUNT`; `dwNumModes` `dd16.c:324`; extra 16 bpp rows 327-339 | read |
| Mode count and sizes | `m8.h:22` `M8_MODE_COUNT 3`; `m8hw.c:62-70` (640x480, 800x600, 1024x768); `m8_hw_mode_size` `m8hw.c:317` | read |
| Mode validation | `ValidateMode` `enable.c:~590-612`; `enable.c:104-121` start mode from the DEVMODE | read |
| Mode change entry (DirectDraw's SetDisplayMode reaches it) | `ReEnable` `enable.c:536-580`; `dd16.c:451` comment | read |
| Hand monitor to VGA / DOS-box fence | `m8_hw_passthrough` `m8hw.c:382` (ADVFUNC 4AE8 = 0x0002); `m8_switch_out`/`m8_switch_in` `enable.c:282-310`; INT 2Fh hook `m8_hook_int2f` | read |
| Disable | `enable.c:429-447` (trapping on, `m8_screen_done`, passthrough, `INT 10h` 0003) | read |
| SetPalette guard | `enable.c:451-454` | read |
| 32-bit HAL entry for `SetDisplayMode` | none: the 32-bit HAL has no mode-set callback; mode changes arrive via ReEnable/ValidateMode (`m8dd_mode_changed` is called at `enable.c:~566`) | read |
| Escape that lends off-screen rows | `M8_ESC_DDLEND` 0x4D39 `m8.h:345`; caller `m8dd_lend` `m8dd32.c:3859`, uses at 3943, 3986, 4121, 4132, 4154, 4172, 4516, 4528, 4622; handler `m8_dd_lend` `screen.c:5201`, dispatch `enable.c:~515`. This lends 8514 heap rows; it does not apply to the VGA half (**inferred**). | read |
| Resync escape after a program draws behind the driver | `M8_ESC_RESYNC` 0x4D3E `m8.h:350`; `enable.c:~506` (`m8_switch_out` then `m8_switch_in`) | read |
| Bed probe | `tools/m8ddbench/M8DDB.C` `modex()` line 698 (`nopal` label skips the palette) | read |

## 4. Running the VGA half while the 8514 owns the desktop

- The only link between halves is the shared DAC and the output mux (ADVFUNC 4AE8 bit 0 / CLOCK_SEL 4AEE bit 0, >1 s blank per switch); both CRTCs run at once. `docs/mach8_vga_half_findings_2026_11_06.md:8,45` **read**. VGA probes already ran on the card: a VGA write about 1.07 us, flips at 69.7 Hz, latch copy 4x faster than CPU VGA-to-VGA (`docs/captures/2026-11-06_probes/README.md`, `docs/worklist.md:365-376`) **read**.
- ATI.VXD (the Win95 minivdd with ATIM8): installs I/O handlers on 4AE8-4AEB, 4AEE-4AF1, 01CE/01CF, 02E8, 0080 and turns trapping off straight after install; trapping is switched on and off as a group by callbacks from the main VDD. It does not save registers it does not know. Handler bodies not read. `docs/ati_vxd_traps.md` **read**. So an ordinary Windows-VM write to 4AE8 or A0000 is not trapped in normal running (**read**, same file), which is why switching output from the driver is cheap.
- Does VDD save or restore VGA state around a VGA mode set from the Windows VM? **unknown**. DDK has the ATI minivdd source for reference: `references/dxddk/ddk9x/win98ddk/src/SRC_DISPLAY_VDD_ATI_ATI.ASM` (not read; the Win95 DDK `DISPLAY.DOC` mentions VDD and full-screen 33 times, not read). Next local read.
- `Disable` and `m8_switch_out` show the pattern the existing driver uses: trapping on (`START_IO_TRAP` 0x4007, `enable.c:26`) before the VGA is touched, off (`STOP_IO_TRAP`, `enable.c:349`) after the mode set. For Route A, take the same bracket around the VGA mode set. **inferred**.
- GDI keeps running on the 8514 while the VGA half is shown unless fenced: set `BUSY` and suspend (as `m8_switch_out`), or GDI writes will go to a screen nobody sees and, via the cursor timer, onto the engine. **inferred** from `enable.c:282-296`.

## Dead ends

- `references/dxddk/dx7ddk/src/F4965_ddfncs.doc` / `F4967_ddstrcts.doc`: no `MODEXILLEGAL` text; only the `DDSCAPS_MODEX` and `STANDARDVGAMODE` entries ("320x200 or 320x240 Mode X surface").
- `references/jhrobotics/vmhal9x/vmhal9x.c:322`: only a TODO to set MODEXILLEGAL; no behaviour.
- `references/velocity9x` source: never sets or mentions MODEXILLEGAL; only the probe uses `ALLOWMODEX`.
- Web search for DDRAW16/ddraw.dll Mode X internals (INT 10h vs VDD, driver Disable calls): nothing found. MSDN DX3-era pages say only what is quoted above.
- Win95 DDK `DISPLAY.DOC` and `references/dxddk/dx5ddk/src/000009f4_Display.htm`: no Mode X text found by keyword (DOC is binary; keyword search only).
- `references/vgadoc/ATI.TXT`, `references/xfree86_336_ati`, `references/fastdoom`: not re-read for this note; VGA-half register facts are in the 11-06 findings.

## Next cheap steps

1. Route A on the bed: add the 320x200 row, `ValidateMode` yes, passthrough on ReEnable, `fpPrimary` = A0000, then M8DDB `modex`. Note the bed's VGA half may not reflect the mux (11-02: monitor stayed on 8514); the card is the check.
2. Alongside, a trace build of Route B (`M8_STEP` in Disable, Enable, ReEnable, SetPalette, Control) answers the "which entries" unknown.
3. Read `SRC_DISPLAY_VDD_ATI_ATI.ASM` for the VGA-state save/restore around a Windows-VM mode set.
