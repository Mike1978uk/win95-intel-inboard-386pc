# Fable pack F1: what INBRDPC.SYS does to cache and CPU config

Static read, 2026-11-13. No diagnosis. Listing: `%TEMP%\inbrdpc_disasm\stock.lst` (not in repo).

## 1. Binary and tool

- `vxd-patches/osr1/INBRDPC_stock.SYS`, 50,837 bytes, md5 `c25b951a0a6093dcfa5138d89159cbf6`.
- That is NOT the card's md5 `d3c458017c296fe01a13bceb19f34106`. The card's md5 is
  `vxd-patches/osr1/INBRDPC_selftest_skip.SYS`. The two differ in two bytes, 0x6BA-0x6BB
  (the operand of `call 0x6f8` at 0x6B9: stock `3C 00`, skip `FF FF`). Selftest-text code, nothing
  below depends on it. The other three `INBRDPC_*.SYS` in `vxd-patches/` were not read.
- Tool: `C:\msys64\mingw64\bin\ndisasm.exe -b16`, linear sweep from 0. File offset = driver offset.
- Linear sweep misreads data tables as code (see section 5). Every finding below was read in
  context, not taken from a byte-pattern match.

## 2. Port 670h

Only seven instruction sites write 670h (`mov dx,0x670` then `out dx,al`). Bits per
`MINBRDPC.ASM` `set_wait` (line 238: `not al; and al,1eh; or al,ram_enable; out dx,al`):
bits 4-1 = wait states / 2, inverted (1Eh = 0 wait states); bit 0 = RAM/shadow enable.

| Offset | Instruction | Value written | Meaning | Confidence |
|---|---|---|---|---|
| 0xB24-0xB37 | `cmp word [0x2b7],0xffff` / `jz` / `mov dx,0x670` / `not al` / `and al,0x1e` / `or al,0x1` / `out dx,al` | `(~[2ac] & 1Eh) \| 1`; then `or byte [0x2bf],1` | Shadow ON, skipped if flag [2b7] = FFFF (NOCACHE) | high |
| 0xB60-0xB72 | same guard, `and al,0x1e` / `out dx,al` | `~[2ac] & 1Eh` (bit 0 clear); `mov byte [0x2bf],0` | Shadow OFF | high |
| 0xB78-0xBBE | INT 13h hook: `cmp dl,0x80` / `mov ax,[cs:0x2b1]` or `[cs:0x2af]` / `cmp al,[cs:0x2ac]` / `jna` / `not al` / `and al,0x1e` / `or al,[cs:0x2bf]` / `out dx,al`, then `call word far [cs:0x4b]`, then `mov al,[cs:0x2ac]` / `not al` / `and al,0x1e` / `or al,[cs:0x2bf]` / `out dx,al` | per-call wait value, then the stored value | Raise wait states for the disk call, restore after (same as `intr13` in MINBRDPC.ASM) | high |
| 0xC0F-0xC1D | `mov [0x2ac],al` (0) / `not al` / `and al,0x1e` / `or al,[0x2bf]` / `out dx,al` | 1Eh | Init to full speed | high |
| 0xC76-0xC84 | `mov [0x2ac],al` / `not al` / `and al,0x1e` / `or al,[0x2bf]` / `out dx,al` | from table `[bx+0x2a8]` | Runtime speed change; then `call 0xb60` if level index 1, else `call 0xb24` | high |
| 0xA701-0xA70A | `mov dx,0x670` / `mov al,0` / `not al` / `and al,0x1e` / `out dx,al` | 1Eh | Init before the self-test | high |

`[2bf]` holds bit 0 (the shadow bit) and is OR-ed into every later write, so bit 0 follows B24/B60.
Bits 7-5 are never written. No other 67xh port is touched (port 674h, the AT variant, is absent).

## 3. Command line

Option table at 0xC63F (strings, space-separated): `SPEED FDWAIT HDWAIT SYSWAIT EGACACHE NOCACHE
NOSCRUB NODIAGS NOPAUSE`. Flag words at `[2ad]` (SPEED), `[2af]` (FDWAIT), `[2b1]` (HDWAIT),
`[2b3]` (SYSWAIT), `[2b5]` (EGACACHE), `[2b7]` (NOCACHE), `[2b9]` (NOSCRUB), `[2bb]` (NODIAGS).
Assignment by option order and by use sites; the parser dispatch table (0xC5xx-0xC6xx) was not
decoded entry by entry. Medium confidence on 2b9/2bb, high on the rest.

- Range checks at 0xA4D5-0xA533: SPEED 1-4, the three waits at most 1Eh, then masked to 1Eh.
- `[2b7]` = FFFF (NOCACHE): B24 and B60 return without touching 670h; 0xAE1 returns without
  copying the BIOS. Without NOCACHE, 0xAE1-0xAFF does `int 15h` AH=87h, 0x8000 bytes, from the driver
  image at 0x9058 to extended memory, then `call 0xb24`. This is the BIOS shadow (RAM, not CPU cache).
- `[2b5]` = FFFF (EGACACHE): copies the `C000:0` ROM into the driver and moves INT 10h there
  (0x9849-0x9872, 0xA762); details in `docs/egacache_static_read_2026_09_28.md`. No port write.
- `[2bb]` NODIAGS gates the memory test at 0x9913; no cache effect.
- Hotkeys: INT 9 hook at 0x770 reads port 60h and, with the modifier state `[2c2]` = 70h
  (Ctrl+Alt+Shift), takes scancodes 2-6 as speed level 0-4, then `call 0xc5b` (the 0xC76 row).

## 4. Hooked vectors

Install by `call 0xc27` / `call 0xc41` (save and set vector). Handlers at the far-pointer slots
`[0x53]`=0x603, `[0x57]`=0x770, `[0x5f]`=0xB78, `[0x63]`=0x851.

| Vector | Handler | What it does | Cache effect |
|---|---|---|---|
| INT 6 (0xA53B) | 0x603 | Exception text and register dump | none |
| INT 9 (0xA55D) | 0x770 | Hotkey speed change (row 5 above); beep | 670h only |
| INT 13h (0xB3E) | 0xB78 | 670h wait states around the call | 670h only |
| INT 15h (0x99DE) | 0x851 | AH=87h: protected-mode block move via 0x86C; AH=88h: 0x9BF | none, see CR0 |

Nothing is hooked per I/O, per DMA or on INT 8. Port writes other than 670h are 60h (A20: 0xDF/0xDD),
A0h (NMI mask 80h/00h), 61h/62h/42h/43h (speaker, parity check), 20h (EOI).

## 5. CR0, INVD, WBINVD, MSRs, ports 22h/23h

| Offset | Instruction | Meaning | Confidence |
|---|---|---|---|
| 0x872 / 0x915-0x91F | `o32 mov eax,cr0` / `or eax,1` / `o32 mov cr0,eax` | INT 15h/87h: save CR0, set PE | high |
| 0x962-0x96C | `and eax,0xfffffffe` / `o32 mov cr0,eax` | clear PE | high |
| 0x998-0x99A | `pop eax` / `o32 mov cr0,eax` | restore the saved CR0 whole, so any CD/NW bit is preserved, not set | high |
| 0x9B63-0xA2F1 (14 pairs) | `o32 mov eax,cr0` / `or eax,1` ... `and eax,0xfffffffe` / `mov cr0,eax` | memory-test routines: PE on/off only | high |
| 0x622-0x62A | `0F 20 00` / `and eax,0x80000011` | exception dump: reads CR0 (PG, ET, PE), no write | medium |

Masks written are 01h and FFFFFFFEh only. No code sets or clears CD (bit 30) or NW (bit 29).

Not found:
- No INVD or WBINVD instruction. `0F 08` / `0F 09` byte pairs exist at 0x94DF, 0x960A, 0x9555,
  and the `0F 20 0F 3C` pairs at 0x94DF/0x960A, all inside 5-byte data records (`01 0D 20 0D 3C 07 ...`)
  in zero-padded tables near 0x94D0-0x9560 and 0x95F0-0x9620 (extent approximate). Ambiguous only by byte pattern; no
  control flow reaches them and ndisasm shows `db 0x0f`. Data, high confidence.
- No `0F 30` / `0F 32` (WRMSR / RDMSR) bytes in code.
- No IN or OUT with an immediate or DX value of 22h or 23h. DX-form ports used: 60h, 670h only.
- No access to the BL3 configuration space (1000h-1004h index/data) by any means seen.
- No FLUSH-related port, no `cld`-style cache trick, no per-DMA code.
- Other `0F 20/22` matches (0x626 aside) are the PE toggles above.

## 6. Reading against inboard386.c and UniPCemu

- `86box_full/src/device/inboard386.c` line 711: "bits 4-1 = waitstates/2, bit 0 = cache/shadow
  enable". The driver agrees on bit 0 as the BIOS shadow bit (set only by B24, cleared only by B60).
  It does not control the CPU's L1.
- NOCACHE and EGACACHE are shadow-RAM options for the BIOS and the INT 10h copy, as the 86Box
  comment (line 377, 859) says.
- The whole driver therefore has no way to cause or prevent the L1 invalidation on I/O.
  That behaviour is outside what this binary writes. It is also not shown to be in the INT 13h path:
  the hook writes only 670h.

## 7. UniPCemu

Source: <https://bitbucket.org/superfury/unipcemu> (author superfury), HEAD read 2026-11-13 at
commit `e33dc9fc1ee726fb344516979415ce025fd019f0`; file `UniPCemu/hardware/inboard.c`, 257 lines
(raw: `https://bitbucket.org/superfury/unipcemu/raw/HEAD/UniPCemu/hardware/inboard.c`). Also read the
2020-11-08 GitHub mirror <https://github.com/rofl0r/uniPCemu> (139 lines, no `inboard_setROMcache`).
GitHub `superfury/UniPCemu` returned 404. Nothing was built or run.

- Port 670h: `inboard386_speed = value & 0x1F`; XT waitstates `30 - (value & 0x1E)`; AT uses bit 0
  with port 674h bit 7 and `{94,47,24,0}`. Same as our model. Port A0h on XT, port 60h A20.
- Author comments, verbatim: "This port is cache control!"; "Bit 0 might enable the cache when set
  (unconfirmed)?"; "bit 0 is cache enable? It seems to affect the ROM caching in some way, as
  observed by driver disassembling"; "Shadowing ... only seemed to be used for BIOS ROM, video ROM
  is seemingly done through the driver itself". Consistent with section 3 here.
- Not in ours: `inboard_setROMcache(enabledBIOS, enabledVideo)` also remaps the second 64 KB
  (0xC-0xF pages) and sets `BIOSROM_DisableLowMemory`; `MMU_mappingupdated()` calls
  `BIU_flushcaches()` whenever a mapping changes (mmuhandler.c:1055-1058). That flushes its
  own BIU cache model on a remap, not on I/O.
- CPU core: INVD/WBINVD (`opcodes_486.c:275-300`, `BIU_INVD` / `BIU_WBINVD` in `biu.c:994-1010`),
  CR0 CD (`biu.c:232`: CD set, or CPU below 486, invalidates the cache). The cache is write-through.
  No MSR for BL3 or 486SLC cache config; MSR tables are Pentium and later.
- Nothing on flush-on-I/O, ports 22h/23h, or SW1-style module switches. UniPCemu has nothing on the
  cache behaviour measured in `docs/fable_pack_f1_flush.md`.

## 8. Gaps

- Option parser dispatch table not decoded entry by entry (flag-to-option mapping from order and use).
- Handlers at 0x603 (INT 6), 0x9BF (INT 15h AH=88h), 0x9C4/0x9DF (descriptor helpers) read only in part.
- Card file was not read directly; `INBRDPC_selftest_skip.SYS` stands in for it (two-byte difference
  from stock, in selftest text code).
- MINBRDPC.ASM gave the names; DOLLAR386.ASM was only grepped. The grep for cache/670h/22h/23h/INVD found
  CR0 protected-mode use, no 670h, no INVD/WBINVD, and a comment at line 8471 on a Compaq caching feature (not read further).
