# Who answers INT 13h AH=08h — static evidence, 2026-09-07

Answers the "FIRST ACTION" of `docs/next_session_2026_09_08.md` without a chain walk.
**Static analysis only.** The skill's core principle is live evidence over disassembly, so
every conclusion below is a hypothesis until the one measurement at the end is taken.

Measured earlier (`fdtype_int13_ah08_2026-09-07.txt`), both drives, identical:

```
AX=0000  BX=0003  CX=4F09  DX=0102  ES=F000  DI=EFA0   NC
```

## 1. The 1986 system BIOS does not implement it

`cmp ah,08` (`80 FC 08`) does not occur in either 1986 U18 chip (`F800`, where the diskette
BIOS lives) — 09MAY86 or 10JAN86. An XT BIOS predates the AT-era `AH=08h` diskette service.
The call returned **success**, so the system BIOS did not answer it.

## 2. `F000:EFA0` is the IBM 9-sector diskette parameter table

Both 1986 U18 ROMs, file offset `0x6FA0`:

```
df 02 25 02 09 2a ff 50 f6 0f 08
            ^^ bytes/sector 2 = 512
               ^^ sectors/track = 9
```

Consistent with the reported `CL=09`. It is a *valid* table for a 9-sector format — which is
why handing it back looks plausible and is not, on its own, evidence of who did.

## 3. Sergey's Multi-Floppy BIOS 2.2 DOES implement AH=08h — and is configured correctly

`roms/network/Sergey_FDD.bin`, md5 `df93d1d546c9c3dc7722754545b3997b` (identical to the copy
at `OneDrive/Desktop/XT_project/Sergey_fdd_rom/`).

- INT 13h dispatch at `0x1119`: `cmp ah,8 / jne / jmp 0x98f`.
- Handler `0x98f` branches on the configured drive type. The **type 3** branch is
  `0x9dd: mov al,0x97 / lea di,[0x13d6] / mov cx,0x4f09` — `CX=0x4F09` is exactly the
  measured value.
- **But** `0x9b3` sets `ES=CS` and the tail at `0xa15` stores `DI`, so a Sergey answer returns
  `ES:DI = <ROM segment>:13D6`, **not** `F000:EFA0`.
- Its per-drive config table (`0x1f81`, 4 bytes/drive: type, FDC, unit) reads
  **DL=0 type 4 (1.44 MB), DL=1 type 2 (1.2 MB)** — the correct drives. Answering from that
  table it would report 1.44M/1.2M, not 720K/720K.

**So Sergey's ROM is doubly exonerated**, as the owner's guard insisted: it implements the call
correctly and its configuration is right. Note the live EEPROM may differ from this image.

## 4. XTIDE Universal BIOS intercepts AH=08h for drives it does NOT own

`roms/xtcf_card/XTCF_D8000_asfound_2026_08_31.bin` (XTIDE 2.0.4, dumped off the real card) and
`IDE_XTP_configured_2026_08_31.bin` (r638 XT+) — **identical logic**, offsets differ by 5:

```
1436  jb   0x145c        ; drive is not ours
...
145c  test ah, ah
145e  je   0x1438        ; AH=00h -> OUR function table
1460  cmp  ah, 8
1463  je   0x1438        ; AH=08h -> OUR function table
1465  ...                ; everything else chains to the previous handler
```

`0x1438` builds `bx = AH*2` and jumps through the table at `0x1508`; entry `AH=08h` -> `0x1610`.

In that handler the floppy path (`test di,di / je 0x163e`) calls `0x14e2`, which is
`pushf / lcall [0] / ret` — **a call to the previous INT 13h handler** — and then rewrites the
drive count. So XTIDE passes the geometry through rather than inventing it.

**And it carries its own byte-identical copy of the F000:EFA0 table** at ROM `0x166d`:

```
XTIDE 0x166d : df 02 25 02 09 2a ff 50 f6 0f 08
1986  0x6fa0 : df 02 25 02 09 2a ff 50 f6 0f 08      IDENTICAL
```

## 5. Nothing in DOS is involved

`D:\CONFIG.SYS` loads no floppy driver — no `DRIVER.SYS`, nothing. The answer comes from the
ROM layer.

## What is left, and the one measurement that decides it

Two candidates remain, and they are distinguished by whose configuration says "720K":

| candidate | fix location |
|---|---|
| XTIDE UB's floppy handling answers or rewrites it | `XTIDECFG` — **already on the CF at `D:\xtide\xtidecfg.com`** |
| Sergey's live EEPROM differs from the ROM image here | Sergey's own utility: **F2 at boot**, `p` to print, `a`/`d` to set, `w` to write |

**Neither needs a chain walk, a shim, or a `HSFLOP.PDR` patch.** Both are read-only inspections
first. Routes 1-3 in the session briefing are all deferred until one of these two reports 720K.

⚠ Unresolved: `ES:DI = F000:EFA0` fits neither candidate cleanly — Sergey returns its own table,
and XTIDE's floppy path chains. Whoever the chain reaches is returning the system BIOS's table.
That is the loose end, and it is the reason this is a hypothesis and not a finding.
