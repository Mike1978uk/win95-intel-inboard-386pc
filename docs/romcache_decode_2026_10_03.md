# ROMCACHE: what opens the reserved block - decoded from RonnyRoy's netlist, 2026-10-03

Source: [ronnyroy111/inboard386](https://github.com/ronnyroy111/inboard386) - KiCad PCB (`inboard386.kicad_pcb`,
KiCad 10) and CUPL equations (`logic/*.pld`), cloned to `..\inboard386`. The PCB file carries a
full netlist, so connections are read from it, not inferred from signal names. It gave us the
register behind the window and the address decode; it does not name U71's inputs `i9` and `i13`.
Steer from @andrew-hoffman on #35, 2026-10-03.

## 1. `ROMCACHE` is port 670h bit 0

| U69 (74LS174) pin | net | goes to |
|---|---|---|
| Q0 (2) | `ROMCACHE` | U71 pin 14, U101 pin 7 |
| D0-D4 (3,4,6,11,13) | `D0`-`D4` | CPU data bus |
| Q1-Q4 | - | U80 (74F161) - the wait-state counter |
| D5 (14) | +5 V | |
| CP (9) | U101 `/O3` (pin 14) | registered decode of `A9` and a U84 select |
| /MR (1) | `~RESET` | |

D1-D4 load the wait-state counter and D0 is `ROMCACHE`: this is the write-only port 670h that
`INBRDPC.SYS` writes `1Fh` to (0 wait states, bit 0 set). Reset clears it, so the card boots with
`ROMCACHE` low.

Port A0h is not involved; PHYSPROBE already read the window with A0h at 00h and 80h.

## 2. What `ROMCACHE` does (U71)

| `ROMCACHE` | `580000`-`5FFFFF` | reads of `F0000`-`FFFFF` |
|---|---|---|
| 0 | the card's system bank | the ROM, over the XT bus |
| 1 | **nothing** (reads FF) | the card's RAM (writes still go to the bus) |

That is what PHYSPROBE measured after boot: `ROMCACHE` = 1, window gone.

## 3. Where the window lands - prediction, not yet measured

U71 substitutes two address lines on their way into the DRAM multiplexers (`~MA7c` into U68, a
74F153, beside `A17`; `~MA8a` into U61, a 74F258, beside `A20`/`A21`). Reading the equations:

| CPU address | `~MA7c` | `~MA8a` | system bank |
|---|---|---|---|
| `000000`-`09FFFF` | `A18` | `A19` | `00000`-`9FFFF` (conventional) |
| `100000`-`11FFFF` | `!A17` | 1 | `C0000`-`DFFFF` |
| `120000`-`13FFFF` | `!A17` | 1 | `A0000`-`BFFFF` |
| `580000`-`5FFFFF` (`ROMCACHE` = 0) | `A18` | 1 | `80000`-`FFFFF` |

So the 1 MB system bank is: conventional 0-640 KB; 256 KB at `A0000`-`DFFFF` handed out as the
first 256 KB of extended memory; and **128 KB at `E0000`-`FFFFF` reachable only through the window**.
In the window, `5E0000` is system `E0000` (the idle EGA half) and `5F0000` is system `F0000` (the
BIOS copy) - the same addresses UniPCemu and 86Box use. `580000`-`59FFFF` is conventional
`80000`-`9FFFF`, and `5A0000`-`5DFFFF` is extended `100000`-`13FFFF` again.

**Test:** ROMPROBE's write at `580100` should show up at `080100` as well, and its write at
`5E0100` nowhere else. Either result settles the table.

## 3a. ROMPROBE on the 5160: parity error, 2026-10-03

`ROMPROBE` stopped with the Inboard's parity error before printing (`RP.TXT` empty). It read
`5E0000` with bit 0 clear. That half is never written, and DRAM with parity must be written
before it is read - the same error appears when `INBRDPC.SYS` has not run. Most likely reading:
**the window opened and the read reached real, uninitialised DRAM.** Not yet proven - the probe
also read `5A0000`-`5DFFFF` (extended `100000`-`13FFFF`) in that state.

`ROMPROB2` (`tools/physprobe/ROMPROB2.S`) does not read `5E0000` until it has written it, and only
writes it if the `580100` -> `080100` alias comes out as predicted. Staged as `C:\RP2.BAT`.

**ROMPROB2 stopped the same way** - PARITY CHECK 2, no output - without reading `5E0000`, so
never-written RAM at `5E0000` is not the whole explanation. `ROMPROB3` reads only, one step at a
time, with a marker on screen per step.

**How the card raises it (netlist):** one 74F280 per byte lane (U44, U38, U40, U37) checks the
data against its parity chip; U59 (74AS21) ANDs the four; U43 gates it into U45B (74F74), clocked
on `~CAS` - so every read of card DRAM is checked, in any bank. U45B drives U87, which pulls the
bus's I/O CH CK, and the IBM BIOS reports that as PARITY CHECK 2. Any read of a location never
written since power-on can trip it. RonnyRoy's `U87_modified.pld` never enables that output.

## 4. Consequences

- **No E000 or C000 shadow in hardware.** U71's only ROM-area term is the `F0000` read. The card
  cannot map RAM at E000, so it cannot be doing EMS there; the EGA copy must be made in ordinary
  RAM with INT 10h repointed, as Andrew suggested.
- **Any driver using the block must write every page before reading it** (parity, section 3a).
- **The 128 KB is reachable only with `ROMCACHE` = 0, which turns off the BIOS shadow.** With it
  set, there is no CPU address for system `E0000`-`FFFFF`. So `SHADRAM.VXD` cannot work as built.
  A version that clears bit 0 would give Windows 32 pages, and every V86 BIOS call (the timer
  tick's INT 8 handler included) would run from the 8-bit ROM. Paging a RAM copy of the BIOS in at
  `F0000` would cost 16 of those pages back.
- **86Box** keeps `5E0000`/`5F0000` mapped whatever port 670h bit 0 says. On the card the window
  exists only while bit 0 is clear. Alerted, not fixed (owner's call).
