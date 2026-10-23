# ATI documentation sweep: findings

Root: `C:\Users\lycet\OneDrive\Desktop\XT_project\ATI\` (abbreviated `ATI\` below). 337 files found. Line numbers are of the file as `cat -n` shows it (CRs stripped) unless stated. For HLP files the cite is the help-topic number printed inside the file, or the line in the extracted text file in the scratchpad.

Scratchpad extractions (same dir as this report): `str_*.txt` (printable-text runs of each HLP/WRI/COM), `mach8hlp_text.txt` (partly decompressed MACH8.HLP; phrase table only partly resolved, so some words are missing), `hlp.py` (the decompressor).

## 1. Files

### Documentation read (each was read in full unless noted)

| Path | Size | Status |
|---|---|---|
| ATI\ATIMACH8\V2\MACH8W\README.ATI | 36345 | read (the main one: FlexDesk v2.0 [Mach] keys, troubleshooting, VESA, errors) |
| ATI\ATIMACH8\V3\VVESA.TXT (= ATI\vv222\VVESA.TXT, identical) | 7058 | read |
| ATI\ATIMACH8\README (= V3\README, identical) | 2228 | read |
| ATI\ATIMACH8\V2\README (= M8W20\README, identical) | 1893 | read |
| ATI\M8W23\README | 1610 | read |
| ATI\ATIMACH8\V1\README.1ST (= GFXULTRA\README.1ST, identical) | 1645 | read |
| ATI\GFXULTRA\README.TXT | 3969 | read |
| ATI\ATIMACH8\M8UTL\README | 1416 | read |
| ATI\8514ut\README (= ati8tool\README, identical) | 1350 | read |
| ATI\ATIMACH8\LOADER.TXT (= loader, V2, V3 copies, identical) | 1428 | read |
| ATI\hdload\README | 1848 | read |
| ATI\ativtool\README | 21595 | read |
| ATI\VGAWOND\README | 10636 | read (older VGA Wonder README, text-mode utilities only) |
| ATI\ativtool\README.GAM | 862 | read |
| ATI\M93NT1\README | 5845 | read (Windows NT beta driver) |
| ATI\ati graphics ultra win31\ati graphics ultra win31\m8wr30.txt | 5081 | read |
| ATI\ATIDESK\MACH8.WRI (= ATIMACH8\ATIDESK\MACH8.WRI, identical) | 16640 | read via text extraction (Write format) |
| ATI\ATIDESK\MACH8.HLP (= ATIMACH8\ATIDESK\MACH8.HLP) | 104370 | read via decompressor; FlexDesk+ control panel UI help only |
| ATI\ATIMACH8\V3\ATIDESK\CORRUPT.MACH8.HLP | 104423 | strings only; named "corrupt"; same UI help |
| ATI\ATIDESK\ATIDESK.HLP (= ATIMACH8\ATIDESK) | 144133 | strings only; generic ATI desktop-shell UI help, no hardware content seen |
| ATI\ATIMACH8\V2\MACH8W\MXXPANEL.HLP | 11544 | read via strings (control panel help; Resolution/Colors/Environment/Palette/Dithering/Device Bitmaps) |
| ATI\8514ut\INSTALL.HLP (= ati8tool, ATIMACH8\V3 copies) | 12144 | read (8514/ULTRA INSTALL context help) |
| ATI\ATIMACH8\M8UTL\INSTALL.HLP | 25924 | read (Graphics ULTRA/VANTAGE INSTALL context help) |
| ATI\ativtool\VINSTALL.HLP | 14976 | read (VGA Wonder VINSTALL help) |
| ATI\ATIMACH8\WININST.HLP, V2\WININST.HLP, M8W23\WININST.HLP, M8W20\WININST.HLP | 2130 / 909 / 2048 / 909 | read (WININST dialog help, nothing about hardware) |
| ATI\ATIMACH8\README.COM (= V2, V3, loader copies) | 22815 | strings only: a README viewer program, not a document |
| ATI\8514ut\MONITOR.INF (= ATIMACH8\V3\MONITOR.INF) | 5715 | read |
| ATI\8514ut\@8088.ADF | 604 | read |
| ATI\8514ut\DRIVER.INF, ATIMACH8\M8UTL\DRIVER.INF, V3\DRIVER.INF | 115 / 153 / 115 | read |
| ATI\ATIDESK\MACH8.INF, DEFAULTS.FLX; ATIMACH8\MACHW\ATIDESK.INI | 819 / 435 / 72 | read |
| ATI\ATIMACH8\LOADER.VSF, V2\LOADER.VSF, M8W20\LOADER.VSF, M8W23\LOADER.VSF | 702-788 | read (two read; others same shape) |
| ATI\ATIMACH8\MACHE\OEMMACH.INF (= V3 copy) | 3094 | read (Win3.1 RADICAL driver INF) |
| ATI\M8W23\OEMMACH.INF (= MACHE copy, 4198) | 4198 | read (v2.3 INF, writes [Mach] keys) |
| ATI\ATIMACH8\V2\MACHS\OEMM8.INF | 7501 | read (v2.0 INF, VGA drivers and svgamode) |
| ATI\GFXULTRA\OEMSETUP.INF | 3322 | read |
| ATI\M93NT1\OEMSETUP.INF, TXTSETUP.OEM | 20578 / 7031 | read (NT setup scripts; registry value names only) |
| ATI\GFXULTRA\INSTALL.BAT, ATIMACH8\V1\INSTALL.BAT | 4549 / 4449 | read (extract-to-floppy batch files, nothing technical) |
| ATI\Microsoft Windows 2.10 / 2.11 ...\winworldpc.com.txt | 691 / 737 | skipped: Microsoft Windows 2.x site notes, not ATI |

### Binary utility strings checked (not docs, looked at for utility behaviour)
TEST.COM (M8UTL, 8514ut, V3, ati8tool), C-INFO.EXE, 8-INFO.EXE, SCRN_ADJ.EXE: printable strings only.

### Skipped, by reason

| Group | Reason |
|---|---|
| ATI\Programmers Guide to the EGA VGA ... Ferraro.txt (3083176), ATI\Advanced_Programmers_Guide_to_SuperVGAs.pdf (26320470) | Told to skip |
| *.DRV, *.386, *.3GR, *.2GR, *.DLL, *.EXE (other than the four above), *.COM, *.FON, *.SYS, *.GRP, *.FNT, *.BIN ROM images (ATI_MACH8.bin, ati-graphics-ultra-mach-8-no-mouse-vm1.BIN, Ardent roms\*.BIN), *.VGA etc. | Binary, no documentation text. About 240 files. ROM images not searched for text. |
| ATI\Microsoft Windows 2.10 (386) (3.5)\Disk0*.img and 2.11 \*.img, media\*.jpg | Windows 2.x floppy images and box art, not ATI |
| ATI\Mach8-Win30\VideoTestEarly.img (245 MB), nvr\*.nvr, 86box.cfg | Test VM disk and our own config, not ATI docs |
| UNREAD ARCHIVES (not extracted alongside): ATI\M8W23\MACHW.LZH (70311) | Contents unknown; may hold a v2.3 MXXPANEL / README.ATI. Not extracted per instructions. |
| UNREAD ARCHIVES: ATI\ATIMACH8\V3\ATIDESK.LZH (65536, looks truncated; only CORRUPT.MACH8.HLP sits beside it) | not extracted |
| UNREAD ARCHIVES: ATI\M8W20\MACH8W.ZIP, MACHS.ZIP | Probably the same as V2\MACH8W and V2\MACHS (inference from names; sizes differ so packing differs) |
| UNREAD ARCHIVES: ATI\ati graphics ultra win31\...\loader.zip, m8wr30.zip | m8wr30.txt lists the contents (same files as ATIMACH8\); not extracted |
| UNREAD ARCHIVES: ATI\Mach8-Win30.7z, ATI\ativtool.zip | Both already extracted alongside (Mach8-Win30\, ativtool\) |
| Other .LZH (ATIDESK.LZH, MACHE.LZH, MACHS.LZH, MACHW.LZH under ATIMACH8\, V2\, V3\, M8W23\) | Extracted alongside (MACHS\, MACHE\, MACHW\, ATIDESK\), except those listed above |

## 2. Findings by topic

### 2.1 WIN.INI [Mach] keys (FlexDesk v2.0 driver, MACH.DRV)

Source: `ATI\ATIMACH8\V2\MACH8W\README.ATI`, section 5.0. Quoted keys and defaults.

- Line 269-272: "CacheCharacters = on | off  Default on ... Allow for caching characters into the off-screen memory area." Off-screen font cache is a feature of ATI's driver.
- Line 291-295: "DeviceBitmap = on | off  Default on. Allow for realizing bitmaps in the off-screen memory area. For some applications (for example BitStream Facelift), this feature must be turned off." Off-screen device bitmaps; known application conflict.
- Line 297-301: "DeviceBitmapDraw = on | off  Default off. Allow the drawing engine to draw into realized bitmaps. DeviceBitmap=on must be set for this to have any effect." Note M8W23\OEMMACH.INF line 72 writes `DeviceBitmapDraw=on`, contradicting the README default.
- Line 303-305: "EngineBlt = on | off  Default on. Use the blit engine."
- Line 323-326: "Pixel = on | off  Default off. Enable put and get pixel through the engine. Can improve performance when aperture is disabled." Implies ATI's default is direct (aperture) pixel access, engine pixel access is the alternative. M8W23\OEMMACH.INF line 69 writes `Pixel=on`.
- Line 333-341: "Polygon ... Default on", "PolyLine ... Default on". "When set on, Polygons are accelerated by the drawing engine."
- Line 348-351: "ScanLine = on | off  Default on. Use the mach8 engine to assist in scanline drawing."
- Line 353-357: "SlowDither ... When on, VGA style dithering is used (when in 256 color palette manager enabled mode). When off, a faster dithering algorithm is used."
- Line 359-362: "StretchBlt = on | off  Default on. Enable driver StretchBlt when on. If set to off, GDI StretchBlt will be used instead." Driver does its own StretchBlt.
- Line 364-367: "VGADAC = on | off  Default off. Program VGA DAC in synch with 8514/A DAC registers." M8W23\OEMMACH.INF line 60 writes `VGADAC=on`; line 59 writes `VGAShared=on` (VGAShared is not described in any README). Shows the 8514/A DAC and VGA DAC are separate register sets that ATI's driver keeps in step.
- Line 315-321: "PaletteManager ... When the PaletteManage is off, the color mode will use a fixed 332 (3 bits Red, 3 bits Green, 2 bits Blue) palette."
- Line 376-382: "CrystalFonts ... When CRYSTAL Fonts is enabled, the palette is fixed in 8 bpp non-programmable mode. 5 gradients of red, blue and green (125 colors) are available in the palette. Also the usual 20 Windows system colors are supported, and 16 shades of gray (including black and white). The remainder of the palette is unused." Layout of ATI's fixed palette.
- Line 403-406: "TextGamma ... 20..300 Default 100", line 390-394 "MinFontSize ... Default 8" (anti-alias threshold).
- Line 371-374: "WIFE ... Do not turn this switch on with North American Windows. WIFE will just slow down your system." (DBCS font hooks).
- Line 343, 274-283: ScreenSize/DesktopSize 640|800|1024|1280; "Virtual Desktop" panning when screen is smaller than desktop. "1280x1024 is a 5:4 pixel ratio".
- Other v2.3 INF-only keys with no README text: `StyledLines=off`, `ColourWeight16=565` (M8W23\OEMMACH.INF lines 62, 76), `Version=2.3`.

### 2.2 Win 3.1 "RADICAL" driver (July 1994) SYSTEM.INI [display] keys

Source: `ATI\ATIDESK\MACH8.WRI` lines 82-106 of `str_ATIDESK_MACH8.WRI.txt`; same keys in `ATI\ATIDESK\DEFAULTS.FLX` lines 22-31 and `ATI\ATIMACH8\MACHE\OEMMACH.INF` lines 41-49.

- "[display]  Mach8=1  DevBmp=1  dib2devengine=1  EngineOnly=1  vad=0  dpi=96  y_resolution=480  x_resolution=640  bpp=8"
- Line 129-132: "If you are running applications that have unexplained problems, set DevBmp equal to zero: (in system.ini) [display] DevBmp=0". Keys `dib2devengine` (DIBs through engine) and `EngineOnly` are undocumented elsewhere. `vad=0` is not explained anywhere.
- Lines 83-92: "386grabber=machw3.3gr ... display.drv=machw3.drv ... aspect=100, 96, 96 ... [386Enh] display=machw3v.386".
- Line 141: "386MAX ... the 386MAX.PRO file must be modified to remove the "VGASWAP" flag. The VGA area is used to provide aperture services by the driver, and thus cannot be moved by 386MAX." The "aperture" of this driver is the VGA A000h area.
- Line 115-127: GPFs with QuickTime, Macromedia Action and Adobe Illustrator caused by "direct manipulation of memory bitmaps is incompatible with ATI's device bitmap implementation."
- Line 137: "Modes 14, and above cannot be entered while in a DOS Window." (ATI mode number 0x14+ forces full-screen DOS box.)
- Lines 25, 31-35 of `m8wr30.txt` (and WRI lines 25-37): v3.0 driver "runs on Mach8 cards with 1MB of accelerator memory", drops 16-colour mode ("Incorporating 16 color mode into the driver would seriously affect performance"), drops 1280x1024 ("only supported on later versions of the Mach8 family"), drops Crystal fonts. Operates 256 colours at 640x480, 800x600, 1024x768, 386 enhanced only.
- Line 157-183 of WRI text and `m8wr30.txt` lines 145-147: Mach8 family named as "Graphics Ultra, Graphics Vantage, 8514/Ultra and VGAWonder GT".

### 2.3 Memory sizes and mode requirements

- `README.ATI` lines 164-172: table "Video Memory Required: 16 color / 256 color / Crystal Fonts: 640x480 512KB / 1MB / 1MB; 800x600 512KB/1MB/1MB; 1024x768 512KB/1MB/1MB; 1280x1024 1MB / --- / ---". Accelerator memory 512 KB or 1 MB only; 1280x1024 is 16 colours only.
- `README.ATI` line 59: "This driver supports the ATI 38800 (mach8) with 512KB or 1MB of memory." Line 156-158: "FlexDesk requires an ATI 38800 (mach8) based graphics controller."
- `GFXULTRA\README.TXT` lines 31-33: "These drivers will work with either 512K or 1Mbyte of accelerator memory. With 512K, 16 colors will be displayed. On 1Mbyte boards, 256 colors are displayed."
- `GFXULTRA\README.TXT` lines 35-37: "For 800x600 operation, the Graphics ULTRA/VANTAGE ROM version must be 1130111513 (v1.3) or newer. For 8514 ULTRA ROMs, the version must be 1330111330 (v1.3) or newer." The ROM file names in `ATI\Ardent roms\` are 11301113131, 11301113140, 1130111315, so the README's number may be a transposition of 1130111315 (inference).
- `README.ATI` lines 489-493: VGA half "has access to a bank of DRAM video memory with a maximum size of 512k ... 640x480 @ 16 or 256 colours, 800x600 @ 16 or 256 colours and 1024x768 @ 16 colours."
- `README.ATI` lines 505-508: "Both IBM 8514/a and ATI mach8 interfaces use a bank of memory devoted to accelerator functions, which has a maximum size of 1 megabyte."
- `README.ATI` line 460-462: "The mach8 accelerators have onboard VGA subsystems which cannot be disabled."

### 2.4 I/O ports, IRQ, ROM, EEPROM and bus width

- `README.ATI` lines 466-472: "Ensure that the 2E0-2EF I/O ports are free for use by the mach8 accelerator. This range includes the standard COM4 base address of 2E8 ... The mach8 cards will generally report a "Subsystem failure" at boot time if the 2E0-2EF ports are in use by another peripheral."
- `M93NT1\README` lines 125-127: "Is there another adapter card in the system that CONFLICTS with I/O locations in the 350-35F hex range? Relocate the I/O to 300 to 31F range." Appears in a Windows NT note for Mach32/Mach8 OEM cards; Mach8 relevance not stated (inference: Mach32 range).
- `M8UTL\INSTALL.HLP` topic 26: bus mouse port "Primary Address, Secondary Address or Disabled ... factory default ... Disabled ... Secondary address is used only if there is an address conflict at 23Ch - 23Fh ... If your system has cards at both port addresses 23Ch - 23Fh and 238h - 23Bh, the mouse port must then be disabled." Topic 7: IRQ "PC: Primary 5, Secondary 4; AT/MC: Primary 2, Secondary 4". Mouse port is on the card in the 238h-23Fh range.
- `8-INFO.EXE` strings: "IRQ is set to level 2 / 3 / 5", "IRQ jumper is not set", "Passthrough is connected", "EEPROM is %s". The card has an IRQ jumper for interrupt generation (TEST.COM: "Testing Interrupt Generation").
- `M8UTL\INSTALL.HLP` topic 9: bus I/O auto: "The graphics adapter will check the CPU and set Bus I/O to: 8 bits on 80286 processor systems, and 16 bits on 80386 or higher processor systems. This option may cause some computer systems to lock up."
- `M8UTL\INSTALL.HLP` topic 40: "When the card is installed in an 8 bit slot, 8 bit operation will automatically be selected; and when installed in a 16 bit slot, the video RAM will always use a 16 bit RAM access, but the ROM ... may be accessed with an 8 or 16 bit datapath." Topic 41: "select the 16 bit option for ROM operation ... The INSTALL program will perform a test for 16 bit operation".
- `M8UTL\INSTALL.HLP` topic 64, 65: separate "I/O Bus width for the VGA portion" ("select the 16 bit option for VGA Bus I/O operation") besides the coprocessor Bus I/O (topics 8, 60, 61: "Bus I/O mode to 16 bits ... may cause some computer systems to lock up. To correct the problem, power down the system, reboot and re-configure the Bus I/O mode to 8 bits").
- `8514ut\INSTALL.HLP` topics 17-18: coprocessor speed modes "Default" ("the card's 'accelerated' mode"), "16 bits", "8 bits". Default is tried first.
- `8514ut\INSTALL.HLP` topic 28: "Factory Default is memory location C880:0000." `@8088.ADF`: "mem 0C6800h - 0C7FFFh" (Micro Channel). Topic 38: "De-installation menu option ... resets the EEPROM and disables the ROM ... If you have not de-installed this graphics adapter and use it in another computer, the adapter may cease to function!" (8514/ULTRA.)
- `VINSTALL.HLP` topics 24, 26, 27 (VGA Wonder only): ROM 8/16-bit, zero wait state option ("Normally wait states are added to peripheral ROM/RAM access cycles ... enabling zero wait state will improve your system performance"), I/O 8/16-bit ("to speed up operations like I/O read/write and image transfer").
- `README.ATI` lines 447-458: shadow/cache of C000-C7FF breaks boot; "All ROMs in the same block (generally the C000-DFFF region is affected) must be addressed in either 8-bit mode or 16-bit mode ... easily result in system lockup."
- `README.ATI` lines 522-527 / WRI 166: driver needs the video ROM at C000-C7FF unmoved; the driver calls the BIOS ROM QUERY and mode-set services at start (WRI text lines 166-189).

### 2.5 CRT parameter registers (MONITOR.INF)

Source: `ATI\8514ut\MONITOR.INF`.

- Lines 14-16: "Only the first 13 entries will be accepted: first 11 are the monitor parameters, followed by LOCK and FIFO_DEPTH ... H_TOTAL, H_DISP, H_SYNC_STRT, H_SYNC_WID, V_TOTAL, V_DISP, V_SYNC_STRT, V_SYNC_WID, DISP_CNTL, CRT_PITCH, CLOCK_SEL, LOCK, FIFO_DEPTH".
- Line 18: "The composite sync bit is bit #4 in the FIFO_DEPTH register".
- Lines 32-35: "Writing 3f to the LOCK register will lock the CRT parameter set, while writing 00 will unlock it. NOTE: All IBM-Compatible modes (i.e 640x480x8 60 Hz, 1024x768 87 Hz i. must be left UNlocked -> LOCK == 00". Row at line 53, `63 4F 52 2C 418 3BF 3D6 22 23 50 50 3F 18  ;640x480x8 60 Hz n.i.`, has LOCK=3F in the file, contradicting that note.
- Row values (hex): 640x480 60 Hz `63 4F 52 2C 418 3BF 3D6 22 23 50 50 3F 18`; 800x600 56 Hz `7F 63 66 09 4E0 4AB 4B0 02 23 70 0C 3F 1E`; 1024x768 60 Hz `A7 7F 85 08 63B 5FF 600 04 23 80 3C 3F 1E`; 1024x768 87 Hz i `9D 7F 81 16 660 5FF 600 08 33 80 1C 3F 18`; 1280x1024 87 Hz i `c7 9f a9 0a 8f8 7ff 861 0a 33 a0 2d 3f 16`. All rows 1-based as in file lines 53-80.
- Inference (not stated): DISP_CNTL 0x23 for non-interlaced and 0x33 for interlaced rows; CRT_PITCH 0x50/0x70/0x80/0xA0 for 640/800/1024/1280 widths, so the 800-wide mode has pitch 0x70 (=896 pixels if the unit is 8 pixels), not 0x64.
- `SCRN_ADJ.EXE` strings: "Mach-8 CRT Controller Registers for %dx%d %dHz mode: H_TOTAL =0x%02X H_DISP =0x%02X H_SYNC_STRT=0x%02X H_SYNC_WID=0x%02X V_TOTAL =0x%04X V_DISP =0x%04X V_SYNC_STRT=0x%04X V_SYNC_WID=0x%02X DISP_CNTL=0x%02X CRT_PITCH=0x%02X CLOCK_SEL =0x%02X%02X"; mode file name "RES_%d.COP". Same register order as MONITOR.INF.
- `MONITOR.INF` lines 87-155: monitor presets map to modes. Default "IBM 8514 or compatible" = 640x480 60 Hz n.i., 800x600 95 Hz i, 1024x768 87 Hz i.
- `README.ATI` lines 555-558: supported signals "640x480: IBM default (31.5/60), 37.7/72, 46.6/72; 800x600: 31.5/89i, 33.8/95i, 35.5/56, 37.9/60, 44.2/70, 48.0/72; 1024x768: 35.5/87i, 48.0/60, 64.0/66, 56.0/70, 58.5/72, 61.0/76; 1280x1024: 50.0/87i". Line 552: "1280x1024 is not supported on all mach8 cards."
- `README.ATI` lines 531-533: monitor type must match; default "VGA" "allows only the 640x480 resolution" (lines 567-569).

### 2.6 Drivers and file naming

- `README.ATI` lines 227-231: MACH-xy.DRV table. "4 bpp: mach-46 (96 ldpi), mach-40 (120), mach-48 (128); 8 bpp no palette: mach-36/30/38; 8 bpp palette: mach-86/80/88; Crystal: mach-g6/g0/g8". `M8W23\OEMMACH.INF` lines 33-38 also lists mach-h6/h0/h8 and mach-i6/i0/i8 with no README description (undocumented variants; meaning not given).
- `README.ATI` lines 201-214: v2.0 uses `386grabber=mach.3gr`, `display=machvdd.386`, `device=mach.386`, `display.drv=mach-80.drv`.
- `OEMM8.INF` lines 84-122: VGA-side Windows drivers with `svgamode=` in decimal: m12 -> 18 (640x480/16), m54 -> 84 (800x600/16), m55 -> 85 (1024x768/16, 512K), m62 -> 98 (640x480/256, 512K), m63 -> 99 (800x600/256, 512K). Inference: decimal 84/85/98/99 = BIOS modes 54h/55h/62h/63h.
- `ativtool\README` lines 93-100: "1024x768 @ 16 colours is implemented as mode 65h ("packed-pixel" mode) on early VGAWONDER products (eg V3M series cards). Later revisions use mode 55h (a "planar" mode)."
- `GFXULTRA\OEMSETUP.INF` lines 27-30: 1992 Win 3.1 drivers ULTRA.DRV (1024x768), ULTRABIG.DRV, ULTRA800.DRV, ULTRA640.DRV with VDD `vddultra.386` and `vgadib.3gr`. `GFXULTRA\README.TXT` line 96: "Crystal Font drivers" missing from that release.
- `README.ATI` lines 52, 66-68, 855-860: FlexDesk is 386 Enhanced only; "WIN /3" forces enhanced mode, "required on machines with less than 4 Mb of system memory".

### 2.7 DOS-side behaviour (8514/A AI, VESA, VGA)

- `hdload\README` lines 16-21: "The ATI Turbo AI is a programming interface written exclusively for ATI mach8 and mach32 accelerator chips ... 100% compatible with AI programs written for the IBM 8514/A." Lines 32-35: "HDILOAD supports an UNLOAD (or simply U) command line parameter". Line 27: install in `\HDIPCDOS`. DEMOAI.EXE tests an already-loaded interface.
- `README.ATI` lines 495-503: VESA SVGA software "will often require that the VVESA.COM TSR be executed first"; 8514/A software "will require the Turbo AI ... \HDIPCDOS\HDILOAD.EXE".
- `VVESA.TXT` lines 56-78: keywords "OFF", "PAN" (linear address; "Standard VGA will wrap around at the end of 64K in planar mode"), "55rw" ("only display controller 28800-5 or higher is default to dual page mode, i.e. the writeable and readable windows can be programmed independently"), "DUALPAGE" (separate read and write windows for 100h, 101h, 103h, 105h, 110h, 112h and 113h).
- `VVESA.TXT` lines 96-102: changing VGA Memory Size in Install requires "vvesa off" and reload.
- `VVESA.TXT` lines 48-49: "To run MS Windows 3.1 with VESA 800x600 16-color driver in the 7-bit video mode 6A, VVESA is not required." Line 170-171: "Running VVESA.COM under Microsoft Windows is not recommended and can corrupt the display."
- `VVESA.TXT` lines 151-157: on Mach32 "modes require the colour registers (i.e. DAC registers) to be set through video BIOS calls only. Some applications set these registers directly, which results in incorrect colour settings in these modes."
- `VVESA.TXT` lines 125-143: mode table. Mach8-relevant rows: 100h-105h and 108h-10Ah. 107h and 110h-117h are Mach32 (and XL/XL24) only. 104h (1024x768/16) "all except V3".
- `ativtool\README` lines 163-170, `VGAWOND\README` section III: RAMBIOS.SYS / ATIVIDEO.SYS is a RAM copy of the video BIOS, about 32K; "RAMBIOS is not generally recommend, although it may increase performance on early PC's" (PC, XT, AT without ROM shadow). Must load before ANSI.SYS.
- `ativtool\README` lines 499-500: `VINSTALL MASK BA FF 40` / `VINSTALL MASK BA BF 0` disable or enable the TTL monochrome grey-scale optimisation (a VGA-side register write via mask command; port/register BAh not otherwise described).
- `ativtool\README.GAM`: GRPTXT.BAT sets "emulated graphics mode" for CGA games.

### 2.8 Utilities (what they do)

- `8514ut\README` lines 3-9 and `M8UTL\README` lines 4-10: stripped INSTALL; "Only the following main menu entries will function: Set Power Up Configuration, Set Current Video Mode, Test Graphics Adapter". "INSTALL is NOT meant to function under Windows". Line 16-19: clean boot, no memory manager; TSRs "may also interfere with INSTALL's access to your ATI hardware".
- `M8UTL\INSTALL.HLP` topic 29 (8514/A diagnostics): "Adapter memory size, Adapter ROM, Adapter registers, Adapter FIFO, RAMDAC, Video RAM, Adapter subsystems, Test patterns of applicable video modes". `8514ut\INSTALL.HLP` topic 29 adds "Pass-through connection, Interrupt generation".
- `TEST.COM` strings: "ATI Accelerator Series, POST ROM 1.46V1", "Extended Test Sequence 1.46V1", "ROM Checksum", "ROM Paging", "FIFO Overflow", "Data Not Ready", "EEPROM Self-boot", "Testing Register Integrity", "Testing RAMDAC Integrity", "RAM Speed Test", "Speed Test", "PCLK : 18810-", "Passthrough must not be attached w/digital monitor", test patterns incl. "1280x1024 95 Hz Interlaced", "640x480x4 Bank 0 / Bank 1 Test Pattern". Not documentation, listed for what it tests.
- `C-INFO.EXE` strings: "Graphics Vantage / Graphics ULTRA", "With 512K / With 256K Video Memory installed", "%d bit bus operation", "ROM found at %s0h", "8514 Section is not detected", "Coprocessor BUS I/O set to ... Auto-select", "VGA Bus I/O set to", "(%dK assigned to VGA)". Reports bus type, memory split, EEPROM state.
- `LOADER.TXT` lines 3-14: LOADER installs downloaded driver modules; must not be put in \MACH32, \COMBUTIL, \VGAUTIL or \8514UTIL.
- `README.ATI` line 81-84: INFO utilities "C-INFO, V-INFO, S-INFO, 8-INFO" report the BIOS part number; "Show System Information" does the same.
- `ATIDESK\MACH8.INF` lines 9-11: "ATIGRFXL.DLL will return one of "Mach64", "Mach32" or "Mach8" to indicate which product info section".
- `ATIDESK\DEFAULTS.FLX` lines 1-31: [Mach] defaults `ScreenSize=640x480, DesktopSize=640x480, PixelDepth=8, PaletteManager=on, ColourWeight16=565, SlowDither=off, DeviceBitmap=on, BGR=on`; [Macx] section (Mach32/64) adds `24BPPMode=24`, `VGADither=off`.

### 2.9 Mach8 vs Mach32 / revisions

- `m8wr30.txt` lines 103-109: 1280x1024 "was only supported on later versions of the Mach8 family".
- `README.ATI` lines 836-843: errors name "ATI 68800 or 38800 based video board"; 38800 = Mach8 ASIC, 68800 = Mach32 ASIC (also `M93NT1\README` lines 21-22).
- `ativtool\README` lines 33-60: 28800/18800 chips = VGA half (VGAWONDER family); `VVESA.TXT` line 67: "28800-5 or higher" supports dual-page windows (VGA-half revision difference).
- `README.ATI` lines 830: "This standard [VESA Local Bus] DOES NOT apply to mach8 based cards."
- `M93NT1\README` lines 31-34: NT beta: "Graphics Ultra Pro and Graphics Ultra+: 65535 colors if the APERTURE is disabled", "Graphics Ultra ... 1280 by 1024 16 colors" not supported. NT note says "640 by 480, 16.7 million colors IF aperture is enabled". Those are Mach32 cards.
- `hdload\README` lines 23-26: Mach32 has a "VGA Memory Size" option; "Some 8514/A applications may require that the VGA memory size be set to a value other than 'Shared'". Mach8 has no such option (not in INSTALL.HLP).

## 3. Undocumented, surprising, or contradicting

1. MONITOR.INF says IBM-compatible modes "must be left UNlocked -> LOCK == 00" but its own 640x480 60 Hz row has LOCK=3F (line 32-35 vs line 53).
2. README.ATI says DeviceBitmapDraw and Pixel default to off (lines 297, 323) while ATI's own v2.3 INF installs both as on (`M8W23\OEMMACH.INF` lines 69, 72). VGADAC likewise off in README, on in INF (lines 60, 364).
3. `VGAShared=on` is installed by v2.0 and v2.3 INFs (`OEMM8.INF` line 74, `OEMMACH.INF` line 59) and is not described in any README.
4. 800x600 CRT_PITCH in MONITOR.INF is 0x70, not 0x64; 1024 is 0x80 and 1280 is 0xA0 (inference on units).
5. The Windows 3.1 RADICAL driver's "aperture" is the VGA A000h area (`MACH8.WRI`, 386MAX note), and its docs say Pixel-through-engine helps "when aperture is disabled". No README says how the aperture is switched.
6. Driver variants mach-h*/mach-i* exist in v2.3 with no description (`M8W23\OEMMACH.INF` lines 33-38).
7. SYSTEM.INI [display] keys `dib2devengine`, `EngineOnly`, `vad`, `Mach8` have no description anywhere (`MACH8.WRI`, `DEFAULTS.FLX`).
8. `GFXULTRA\README.TXT` ROM number "1130111513" does not match the ROM image names in `Ardent roms\` (1130111315); likely a typo in the README (inference).
9. `M93NT1\README` points at I/O 350-35F conflicts, not 2E0-2EF; the Win 3.1 README points at 2E0-2EF. Different card families (inference).
10. `M8UTL\INSTALL.HLP` topic 9 and 60: "16 bits ... may cause some computer systems to lock up". The 8-bit coprocessor bus mode is the stated fallback.
11. `m8wr30.txt` lines 91-99 give 16-colour mode as the reason the v3.0 driver is slower to write; the 4 bpp path is described only as a performance cost, with no detail.

## 4. Not found (do not look again)

- No file mentions: MemMapRegs, BlockWrite, NoMemMap, hardware cursor, colour compare / colour-compare, short stroke, line-draw command, pattern registers, scissors, "mix", "bit-blit" register names, errata, "known bug" for the engine itself.
- No register map of the 8514/A or Mach8 engine ports (no 82E8h, 9AE8h, 9AEE, 42E8h, 4AE8h, 02E8h, 06E8h, 0AEEh etc. anywhere in the text files). The only register names are the CRT set in MONITOR.INF/SCRN_ADJ and the EEPROM-held options.
- No ATI extended-register (1CE/1CF, 1CEEh, 5EEEh) documentation in any readme/HLP.
- No timing or speed data for the engine or the ISA bus (nothing like wait states per access for the 8514 side), no 8-bit-slot performance advice other than "Pixel=on when aperture disabled" and bus width settings.
- No Windows 95 (or 4.0) notes. No 16 bpp/24 bpp notes for Mach8 (only Mach32 VESA modes and NT Mach32 colour depths).
- No DAC chip name, DAC register protocol or palette-write timing in any readme; only "Program VGA DAC in synch with 8514/A DAC registers" and the 332 palette note.
- No explanation of what "vad" or "VGAShared" mean.
- No M8W23 MACHW.LZH, M8W20 zips, V3 ATIDESK.LZH, loader.zip, m8wr30.zip contents read (see section 1).
- ATIDESK.HLP and CORRUPT.MACH8.HLP were only skimmed as strings (UI help). MACH8.HLP's text was recovered with a partial phrase table, so a few words are missing. Neither appeared to hold hardware facts.
