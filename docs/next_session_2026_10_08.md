# Next session - Mach8 model: TS1 from command 142, then the PR

Supersedes `docs/next_session_2026_10_07b.md` for order. Results and rules: `docs/captures/2026-10-07_m8txt/README.md`.

## Late 2026-10-07, read this first

Model and card findings are now in `docs/mach8_card_vs_86box.md` (the write-up for Michal and others).
Fork: `5fce405ab` fetch fix (full card EEPROM boots), `f9b466ac2` config straps, two DIAGNOSTIC commits.
Order for next session: (1) clean shadow-set probe on the 5160 (one change at a time, two reads each,
one set written per pass); (2) model three CRT sets to match, loaded from the EEPROM by the ROM;
(3) M8MONO leaves the bed's VGA blank, not the card's: log the renderer chosen when 4AE8h returns to
the VGA with M8MONO run alone; (4) TS1/TS2. vm_6695 has the full card EEPROM (needs the fork build);
REGR names each probe and pauses 3 s.

## Where it stands (2026-10-07)

- Owner's direction: understand the whole card, not only what drivers use; TEST.COM fully clean before the
  86Box PR; then ask 86Box#6695 to retest on that version. Write-up of undocumented Mach8 behaviour planned
  (Michal Necasek, OS/2 Museum, is the obvious reader; his 8514/A article is in our sources).
- Fork branch `inboard-ext-256k-diag` (86box_3c509b), today: `02c3b70cb` (monochrome host data byte order -
  fixed our own Windows 3.1 8514.DRV text bug from `a0bad65cc`), `96859258f`, `ecea8636e`, `0e9796012`
  (monochrome host-data rules from M8LINE), `8283ed2bc` (source colour compare 92EEh/EAEEh). Diagnostics
  `cdfb0657d` (MACH8_SEQ logger). Main repo and fork pushed to `8283ed2bc` mid-afternoon; later commits local.
- Card and bed agree on: M8LINE, M8MONO, M8TXT, M8SCMP, M8ROW5, M8ROW6, M8SRC4C-E, and all of TS1 (M8SEQ5, 173
  checkpoints) since fork `8193af4b3` (source 4). TEST.COM itself not yet re-run on that build; TS2 14204 0602 0004
  failed before it.
- 86Box#6695 does not reproduce on master or ours (Windows/386 2.11 bed `vm_6695_w2`). No comment posted:
  owner's order is build, PR, then ask them to retest. Draft kept in the 10-07 conversation; rewrite then.
  Andrew (#49): the program is ZSoft PC Paintbrush for Windows, not Windows Paint - our no-repro was with the
  wrong program. Owner has 1.05 in `Downloads/`; install it in `vm_6695_w2` and retest before saying anything.
- More ATI ROMs and software are in `XT_project/ATI/` (resources_and_sources.md): DEMOAI under HDILOAD is a
  possible second conformance test.

## TEST.COM TS1 still fails in the bed - cause narrowed (end of 2026-10-07)

**Lead, found last:** ATI_MACH8.bin (owner's copy, XT_project/ATI) at 70B4h: out 26EEh,A0h then out 76EEh,AL -
GE_PITCH = A0h (1280 px), the ROM's only GE pitch write; it never writes GE_OFFSET. TEST.COM never sets the pitch,
so the card runs TS1 at 1280. Check first: M8TSY with 76EEh = A0h forced should give the expected table on the
card; then why the bed (M8TSX straight after boot) is not at A0h - the jne before 70B4h (EEPROM? memory size?)
or how the model applies GE_PITCH. The model takes byte writes to 76EEh (vid_ati_mach8.c case 0x76ee).
ROM also writes (mov dx count): 7AEEh EXT_GE_CONFIG x2 right after the pitch, EAEEh once, 26EEh CRT_PITCH,
52EEh/56EEh (EEPROM interface?), 12EEh x6 (CONFIG_STATUS reads), 3AEE, 46EE, 5AEE, 6AEE. Read each value at boot.
Other ROMs: three more versions on ardent-tool (ATI_8514_Ultra page, in resources_and_sources.md); Michael's
card carries 113-01115-150 (our old 64 KB file), same board otherwise, 1 MB. Compare pitch writes across all.
The jne is now read (11504-002, 709Ch-70BEh): CL = ((in 42E8h >> 4) & 7) | 8; only if CH bit 0 is set AND
in B2EEh == 9Fh: CL |= 10h, out 26EEh,A0h, out 76EEh,A0h; then out 7AEEh,CL either way. 9Fh = 1280/8 - 1,
A0h = 1280/8, so B2EEh is probably a horizontal-displayed readback: pitch 1280 only when the mode is 1280 wide.
Check in the model: what B2EEh returns and what CH holds there. 113-11503-004 (theretroweb) lacks the
A0h block entirely - a card with that ROM would run TEST.COM at the default pitch.

Correction, same evening: TEST.COM DOES write the pitch. Its mode-set routine (7BD9h-7C41h; AL=1: 4AE8h=3,
CL=0; AL=2: 4AE8h=7, CL=2) reads 56EEh (ScratchPad1, set by the ROM), shifts AH right by CL, and only if bit 0
is set reads EEPROM words 1Dh and 02h, writes 7AEEh = (w02 & 0Fh) | 10h | (20h if w1D high byte = 4Fh), then
76EEh = 26EEh = A0h. The ROM's own A0h block (709Eh) is dead on the AL=0 entry (CH = 2, bit 0 clear).
5160 at DOS, read by COMrade: 56EEh = 0820h (AH = 08h: bit 0 clear for both CL values, so A0h NOT written),
52EEh = 0000h, 42E8h = 00ABh, B2EEh = 694Fh (low byte = H_DISP readback 4Fh, 640; B2EFh = H_TOTAL per
TEST.COM 1F93h). Next: read 56EEh in the bed. If it has bit 0 (CL=0) or bit 2 (CL=2) of AH set, the bed takes
the A0h path and the card does not - the reverse of the earlier theory, so recheck which side M8TSY's 80h matched.
Bed read (vm_5160_now, build_log d4024748c, MACH8_WLOG, POST only; log vm_5160_now/wlog_56ee.log): the ROM
writes 56EEh = 04h (byte), 7AEEh = 000Ah (same CL as the card's 42E8h gives). AH = 00h, so the bed skips A0h
too - the pitch gate is closed on both sides; the 17 bytes come from elsewhere. Open: card 0820h vs bed 0004h.
The card was read after an unknown session, not cold - read 56EEh on the 5160 straight after a cold boot.
Cold boot to DOS, 5160: 56EEh = 0820h again (52EEh 0000h, B2EEh 694Fh, 42E8h 00ABh). The ROM (0783h) fills
56EEh/56EFh from EEPROM words 8 and 9 (low byte each, read routine 3A4Eh). Bed EEPROM word 8 = 0004h, word 9 =
0000h; card: word 8 low = 20h, word 9 low = 08h. The bed EEPROM (INSTALL-written, rom_and_eeprom_dumps.md) is
not the card's. Next: dump the card's EEPROM (an EEDUMP built on ROM routine 3A4Eh) and give the bed the real one.
Done (M8EEDUMP, roms/video/mach8/eeprom_card_5160_2026_10_07.nvr, 32 of 64 words differ). With it the bed hangs
at POST: EEPROM word 02 bit 0 (card 0919h, INSTALL file 0700h) makes the ROM (04C0h) copy 12Bh bytes of code to
B800:1000, far-call it (a ROM/bus timing check via ext regs B7h/B9h/A3h/A0h), then the CPU loops in the system
BIOS's unexpected-interrupt handler (F000:FF2F, IF=0, IMR=FF). Card word 02 with bit 0 cleared
(eeprom_card_5160_w02b0clear.nvr) boots. Model gap: trace why that routine fails in 86Box and works on the card.
Card EEPROM block E (words 31h-37h) is MONITOR.INF's 1280x1024 87 Hz interlaced set (H_TOTAL C7, H_DISP 9F,
CRT_PITCH A0, V_TOTAL 8F8, V_DISP 7FF, V_SYNC_STRT 861, DISP_CNTL 33, CLOCK_SEL 2D, FIFO_DEPTH 16); blocks A, C, D
not yet decoded. Block B (18h-1Dh) is what INSTALL wrote; 1Ch/1Dh differ from the card.
Result with eeprom_card_5160_w02b0clear.nvr in vm_6695 (now in both beds, old file kept as mach8.nvr.flexview_install):
M8TSX.BIN and M8TS1.BIN byte-identical to the FlexView-EEPROM run (M8TSX_bed_d4024748c.BIN); TEST.COM TS1/TS2 fail as
before (owner). So the EEPROM does not cause the TS1 gap - expected for M8TSX, which replays TEST.COM's writes; the
17 bytes and the data-ready flag are engine bugs. Pixel-placement probe next.
Black screen after REGR (bed and 5160): M8TSX replays 4AEEh writes ending at 1631h (bit 0 = Mach8 owns the display)
and restores only 4AE8h. Proposed, not made: restore 4AEEh to the ROM's 1850h at exit. REGR still runs behind it.
ROOT CAUSE of the full-EEPROM hang (INBOARD_RINGAT=F000:FF23,B800, a new diagnostic ring in 386_dynarec.c):
the ROM far-calls B800:1000; byte, word and dword reads there return the copied code correctly, but the CPU
executes garbage (PC 1000 -> 1002 -> 1005 ...). The interpreter fetches code through getpccache() (mem.c),
which only serves pages with an exec pointer; the VGA mapping has none, so it returns ff_pccache (4 bytes of FF)
indexed as a whole page - out of bounds. 86Box cannot execute code from video memory on this CPU path. Same code
in upstream 4f18c5b98. Fix to discuss: fall back to readmembl() in fastreadb/w/l when a page has no exec pointer,
as the 2386 path already does. Upstream-worthy on its own.
FIXED in the fork, 5fce405ab (cpu: fetch code from pages with no exec pointer): the full card EEPROM boots.
TS1/TS2 still fail; M8TSX/M8TS1 byte-identical to before (M8TSX_bed_fullee_fetchfix.BIN).
M8TSX now restores 4AEEh to 1850h at exit, so REGR no longer leaves a black screen.
MACH8_SPLIT8=1 (diagnostic, fork): 16/32-bit Mach8 I/O as byte cycles. Results moved AWAY from the card
(M8TSX 789 bytes off vs 492) and later probes crawl to their timeouts: the model's byte paths for 16-bit
registers are not faithful (V_DISP read back 96), so this run says nothing about the bus. Next: AT control -
TEST.COM in an AT bed with the card at 16-bit; pass there points at the XT/8-bit path, fail at the engine.

TS1 code is trivial (3CDDh: play 29A4h, fold 76D2h, read 7735h, compare 27F0h vs 32 words at 2964h).
The card passes TEST.COM even after our probes. Probes (all in `tools/m8seq/`, card BINs in the captures dir):
- M8TS1 (M8SEQ5's setup): card is 17 bytes off TEST.COM's expected table; every M8SEQ capture since 10-06 is
  too. `docs/ati_test_com_notes.md` saying the replay equalled the table was wrong.
- M8TSX (TEST.COM's own 1E21h setup, plus its 692 earlier writes from `M8PRE.DAT`, made by
  `m8pre_from_log.py` from a `MACH8_WLOG=1` bed log): card = expected exactly. Bed = 17 bytes off, AND
  GP_STAT 0100h (data ready) still set after the 32 words - the card reads 0000h. Two model bugs.
- M8TSY (card): forcing GE_PITCH 80h / GE_OFFSET 0 (76EEh, 72EEh, 6EEEh) is the only change that gives the
  17 bytes; 92EE/EAEE, DEST_CMP_FN, MEM_CNTL do not. The next pass recovers, so something in setup or TS1
  sets the pitch/offset on the card by a path the model lacks. Next: find it (read back 76EE/72EE/6EEE if
  readable; check which TS1/17E7 writes could alias them), fix, then the data-ready bug, then TEST.COM.
- COMrade read-back, 5160: 76EEh reads 0000h even after writing 80h (write-only); 72EEh reads F800h,
  6EEEh 0000h, unchanged. So bisect by writes (M8TSY-style), not by reading the registers.
  Infer it instead: draw pixels at (0,0) and (0,1) under the unknown state, force pitch 80h / offset 0,
  read rows 0-1 back - where they land gives the effective pitch and offset. Check after 17E7h and TS1 chunks.
  Software clues: ATIM8.DRV writes 76EEh 3 times per session (mode set), MACHW3 ~3,000; read what values,
  and the option ROM (ATI_MACH8.bin) mode-set code for 76EEh/72EEh/6EEEh - the card may run TS1 on the ROM's pitch.
  Also XFree86 3.x XF86_Mach8 server source (owner's lead): open code for this chip's GE_PITCH/GE_OFFSET setup.
  First, cheapest: diff the bed's EEPROM (roms/video/mach8/eeprom_flexview2x_56hz_800x600.nvr) against the real
  card's EEPROM data, and trace the option ROM from its EEPROM read to its 76EEh/72EEh/6EEEh writes.
- Measured: 42E8h reset clears PATT_INDEX, keeps LINEDRAW_OPT/PATT_DATA_INDEX; fixed `d4024748c`.
- Bed run of REGR after M8TSX ended on a black screen (owner closed it); M8TSX/M8TS1 had finished. Check.

1. Run TEST.COM on `8193af4b3` in `vm_6695`: done - TS1 and TS2 still fail; see above.
2. Re-read M8ROW7's third area (X >= 1024) with a wider scissor; and model reads outside the scissor as FFh.
3. Bisect TS2 the same way (M8SEQ5-style replay of its table). Goal: TEST.COM all stages pass.
4. Regression after every fix: `vm_6695` boots straight into `C:\M8SEQ\REGR.BAT` (all probes); compare each
   probe with the previous build's results; only the intended probe may change. Check Windows 3.1 text.
5. Then: clean upstream branch (drop DIAGNOSTIC commits), regress Mach32 and plain 8514/A, TC1995 first, PR.
6. Then the driver (handoff 07b: velocity9x vs NOT_FRAMEBUFFER). Source 4 (hardware 4:1 coverage count) is a
   candidate for antialiased text; see the captures README.
7. Idea (owner, 2026-10-07): the engine is fixed-function, nothing to dump, so write a sweep probe - every
   foreground source x mono source x ALU function on a fixed pattern, card vs bed - to cover what TEST.COM does not.
   Also open: source 4 with mono source "always 1" waits for host data on the card (M8SRC4/4B), not modelled.

TS1 progress, 2026-10-07: all 172 commands match the card. Fork commits after the morning: `554cfeddc`
(source 7), `de789efb6` (mono host bits), `4e9ef658d` (POLY_MODE lines), `8193af4b3` (source 4).

## Beds and tools

- `vm_6695`: Win 3.11 + Microsoft 8514.DRV (ATI's SYSTEM.INI kept as `WINDOWS\SYSTEM.ATI`), boots to DOS,
  probes in `C:\M8SEQ` with TEST.COM. `vm_6695_master`: same on upstream (`86box_master`). All four 6695 beds
  now have the FlexView EEPROM.
- `tools/m8seq/`: M8TXT, M8MONO, M8LINE, M8SCMP, M8SRC4-4E, M8ROW4-7, M8SEQ5; `m8txt_decode.py` (read-back is low byte
  first), `m8seq_diff.py`, `REGR.BAT` (DOS batch lines must stay under 127 characters).
- COMrade: file_write and hash sometimes time out at 8 s but the copy lands; verify by reading the file back
  BEFORE running it (a truncated M8ROW6 ran once because the two were done in parallel).
  `run_command` cannot use `&&`.
- Window capture of a bed for checking the screen: a PrintWindow script (worked even with the bed behind other
  windows); not yet in `tools/`.

## Rules

- Ask before every VM launch; owner judges screens. Alert, don't fix, for committed work. Re-read comments in
  generated files before committing.
