# DOS Mach8 3D driver (P3.2): groundwork, 2026-11-18

Desk research only. Nothing installed, nothing built. Plan: `docs/open_items_review_2026_11_17.md` section 5.

## 1. DOS extender for Open Watcom 32-bit programs

What is on disk (`C:\Users\lycet\tools\openwatcom`): `binnt\wcc386.exe`, `wlink.exe`, `wstub.exe` (512 bytes), `lib386\dos\*` start-up and C libraries, `binnt\wlink.lnk` with `system` blocks `dos4g`, `causeway`, `dos32a`, `pmodew` (lines 30, 470, 491, 551). No `binw` directory. **No extender binary** (`dos32a.exe`, `pmodew.exe`, `cwstub.exe`, `dos4gw.exe`) in the tree. `wstub.exe` is only the DOS/4G loader stub: it needs `DOS4GW.EXE` on the PATH at run time. Unidentified file: `C:\Users\lycet\Downloads\pcxtbios31\dos\dos4gw.exe` (27,504 bytes; the real DOS/4GW is far larger; not checked).

| | Licence | Download | wlink system | Notes |
|---|---|---|---|---|
| DOS/32A 9.1.2 (2006-04-20) | BSD-style (Apache 1.1 derived): redistribute binary with the licence text, add "This product uses DOS/32 Advanced DOS Extender technology.", no "DOS/32A" in a derived product's name. https://scancode-licensedb.aboutcode.org/dos32a-extender.LICENSE | https://github.com/amindlost/dos32a ; `dos32a-910-bin.zip` at dos32a.narechk.net (not verified live) | `dos32a` (LE) / `dos32x` (LX), stub `dos32a.exe` | Supports Clean (INT 15h), XMS, VCPI, DPMI. Drop-in for DOS/4GW. Stub size not found. |
| PMODE/W 1.33 | Free for commercial and non-commercial use, except for applications that are themselves DOS extenders; source published. https://github.com/amindlost/pmodew (`license.txt`, not read) | repo `bin/`; originals `pmw133.zip`, http://www.sid6581.net/pmodew/ (not verified live) | `pmodew`, stub `pmodew.exe` | Smallest of the set by reputation; size not found. |
| CauseWay 5.0 | MIT per GitHub page. https://github.com/baron-von-riedesel/causeway | same repo | `causeway`, stub `cwstub.exe` | Source from Open Watcom's CauseWay; no CPU or A20 text found. |
| DOS/4GW | Not found. Ships with Watcom compilers under a restrictive licence (from memory, unchecked); not a free-to-ship choice | local `wstub.exe` only | `dos4g` | Skip. |

### A20 on an XT with an Inboard
- Inboard A20 is port 60h, write DFh (on) / DDh (off); the 8042 route does not exist (`docs/al_williams_inboard_a20_correspondence_2023.md`, `docs/windows95_on_inboard386pc_writeup.md:70-85`).
- DOS/32A: Clean and XMS paths "enable the A20 line"; how is not stated. https://www.javiergutierrezchamorro.com/wp-content/uploads/2013/07/dos32a/html/tech/7.html and `.../tech/2.html` (detection order DPMI/VCPI, XMS, then Clean INT 15h). Source would show it.
- **The larger risk is memory, not A20.** The 1986 5160 BIOS has no INT 15h AH=88h, and `INBRDPC.SYS` does not hook INT 15h (`docs/INBOARD_86BOX_PORT_PLAN.md:1326-1346`); HIMEM.SYS reports XMS 0K (`:1313`). An extender in Clean mode will therefore find no extended memory and must run in the 640K conventional area, or Windows-style host support is needed. Unproven for all four.
- Whether any of them probes/toggles A20 through 8042 ports 60h/64h and so writes DFh/DDh-style values to Inboard's port 60h by accident: not answerable from these sources.

**Recommendation: DOS/32A.** Clear licence with a file we can cite, `dos32a` already in `wlink.lnk`, source available to read the A20 and memory code, and a Liberty Edition source release. Second choice PMODE/W. First test (bed, then card): a `wcc386 -bt=dos` hello linked with `system dos32a`, run with INBRDPC.SYS loaded, print the memory the extender reports.

### Card result, 2026-11-18: the memory risk is gone

`tools/dosx/memprobe.c` (built `-l=dos32a` as `M32A.EXE`, `-l=pmodew` as `MPMW.EXE`; extenders from the Open Watcom
snapshot's `binw`), run at a plain DOS prompt after a normal boot with INBRDPC.SYS. Photos: owner's `XT_project/photos/dos_1.jpg`,
`dos_2.jpg`, `mem_1.jpg`.

| | DOS/32A 9.1.2 | PMODE/W |
|---|---|---|
| DPMI version, CPU | 0.90, 486 | 0.90, 486 |
| largest free block | 4200 KB (total 4280) | 4279 KB |
| allocated and written in 64 KB chunks | 61 = 3904 KB, first at 125E38h | 62 = 3968 KB, first at 112044h |

Both extenders find about 4.2 MB above 1 MB and write all of it, while MEM shows Extended (XMS) 0K. How they find it
(the bullet above expected none) is not determined. DOS/32A warnings, both harmless here: 9004 mouse init failed (no
DOS mouse driver), 9003 real-mode vector INT C2h modified during the run (owner of C2h not identified). Next: the
mode-set `.COM` below, then the core out of `m8d3d.c` with a DOS back end.

## 2. Getting 640x480x8 on the 8514 side from DOS

| Item | Fact | Source |
|---|---|---|
| What our `.COM` probes do | Do not set a mode. Engine reset `42E8h` 9000h then 5000h, then `BEE8h` 5006h, 76EEh 0080h, 72EEh 0, 6EEEh 0, `BEE8h` 1000h/2000h/33FFh/43FFh, `AAE8h`/`AEE8h` 00FFh, `BEE8h` A000h, `EEEEh` 0 | `tools/m8seq/M8PF.ASM:141-157` |
| Showing the 8514 side | `ADVFUNC_CNTL` (4AE8h) = 3 shows shadow set 1 (640x480), 7 shows set 2 (1024x768), 2 returns to VGA. No CLOCK_SEL or CRTC write. The shadow sets are already loaded by the card's POST/EEPROM | `tools/m8seq/M8SHOW.ASM:1-6`, `M8FLIP4.ASM:78-82`; owner saw both stripe sets, `docs/mach8_levers.md:202` |
| After a BIOS mode set | INT 10h mode 12h then ADVFUNC=3 also shows white. `DISP_CNTL` (22E8h)=60h blanks the 8514 output (D arm); 23h is enabled | `docs/mach8_levers.md:220-222` |
| Switching halves | Costs over a second of monitor re-sync | `docs/mach8_levers.md:222-228` |
| ROM entry | `C000:0064h` AL 0 = accelerator mode load (services 1201h, 5502h, BX parameter); `68h` AL 0 = engine bring-up (writes 22E8h=60h); `6Ch`/`70h` are `stc; retf`. ROM segment = C000h + (SCRATCH_PAD_0 `52EEh` bits 0-6) x 80h. Only TEST.COM and VVESA call it | `docs/mach8_levers.md:93-109` |
| XFree86 | Programs the CRTC/shadow set itself; the Mach8 server source is local | `docs/resources_and_sources.md:149-150` (`references/xfree86_336_mach8/`) |

Exact sequence the probes use: the init block above, then `OUT 4AE8h, 3`. **Unproven:** that this gives 8 bpp (the stripes were colours 0-15 in 640x480; 8 bpp is implied by the pitch-1024, 1 MB layout, not stated by the probes), a set-up from a cold DOS prompt without the card's POST mode, ROM 64h AL 0 from DOS (design doc: "ROM API static only", `docs/mach8_driver_design.md:169`), and the 640x480 refresh the EEPROM supplies (EEPROM file is 56 Hz 800x600, `roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr`). Next cheap test: a `.COM` that runs the init block and `4AE8h`=3 after a 640x480 fill, watched by the owner.

### Card result, 2026-11-18: 640x480 at 8 bpp from DOS works

`tools/m8seq/M8DOS8.COM` at a plain DOS prompt (after the boot that ran the extender probes): the init block above,
256 palette entries through 2ECh/2EDh, a 16 x 16 grid of 40 x 30 engine fills, then `OUT 4AE8h, 3`. The owner's photo
(`XT_project/photos/dostest.png`) shows all 256 blocks distinct, full screen, for the 10 s, then text mode back. So
from a cold DOS prompt, with no ROM call and no CRTC or clock write, shadow set 1 is 640x480 at 8 bpp and the 8514
DAC is live. Next: the DOS back end of the engine (P3.2) on DOS/32A.

## 3. GE-idle interrupt (Z22)

**Silicon: yes. On this card: unknown, and not usable as found.**
- `SUBSYS_CNTL` (42E8h) has GE_IDLE_ACK bits 7:4 and GE_IDLE_ENA bits 15:14, "Interrupt enabled - Engine idle", GE_IDLE generated when the FIFO is empty and the engine goes idle; status bit GE_IDLE_INT. `references/ati_mach32/guide.txt:7951-7972, 8013, 8038`.
- The card has an IRQ jumper: 8-INFO strings "IRQ is set to level 2 / 3 / 5" or "not set"; TEST.COM "Testing Interrupt Generation" (`docs/ati_readme_findings_2026_10_23.md:121`). Setting on our card not recorded (`docs/mach8_frame_rate_plan.md:150`).
- Every XT IRQ is taken (2 NIC, 3 T130B, 4 COM1, 5 SB, 6 floppy, 7 LPT; `docs/mach8_frame_rate_plan.md:150`); the card's levels 2/3/5 all collide.
- The bed does not model it (`docs/mach8_card_coverage.md:39`: ENA bits never raise an IRQ on ISA).
- The jumper position needs a look at the card; no program can read it.

## 4. Z27 correction

`tools/m8probe/M8TEX.C:25-30` includes `<windows.h>`, `<ddraw.h>`, entry `WinMain` (line 528); `M8SHAPE.C:47` includes `<windows.h>`. Both are Windows EXEs. Z27's "already drive the card from DOS" is wrong. DOS drivers of the card are the 88 `tools/m8seq/*.ASM` (16-bit `.COM`, engine only, no extender). Confirms `docs/open_items_review_2026_11_17.md` row 35.

## Sources did NOT contain
- Extender binaries, stub sizes, or an A20 method for any of the four; PMODE/W and CauseWay licence text not read.
- Any statement of a 386-class or XT problem for DOS/32A, PMODE/W or CauseWay.
- The A20/INT 15h behaviour under INBRDPC.SYS for a DOS program.
- 8 bpp confirmation for the probes' 640x480 mode; ROM 64h arguments beyond AL 0 (`mach8_levers.md:109`).
- The card's IRQ jumper setting.
