# Mach8 byte cycles on the 8-bit bus - 5160, 2026-10-08

Probes `tools/m8seq/M8BYTE*.ASM`, decoder `m8byte_decode.py`. Card: 113-11504-002, 1 MB, JU1 8-bit, in the 5160.

| Probe | Finding |
|---|---|
| M8BYTE 1,3,4 | A low byte alone to a word register (86E8, 96E8, A6E8) has no effect. |
| M8BYTE 7, M8BYTE2 7 | Low then high byte = the word. The high (odd) byte commits. |
| M8BYTE 5,6 | Command 9AE8: neither byte alone (after word writes) draws. |
| M8BYTE2 3,4 | The held low byte is ONE latch shared by all ports: A6E8=20h then 86E9=01h gives CUR_X 120h; AAE8=B3h then 9AE9=40h draws. |
| M8BYTE2 5,6 | ATI extended register BAEE follows the same rule. |
| M8BYTE 8 | 64 x IN E2E8h (low byte): the same pixel every time, no advance, engine still busy with data ready. |
| M8BYTE 9 | 64 x IN E2E9h (high byte): one WORD per read (02, 04 ... 40), dry after 32. The high read pops the word. |
| M8BYTE3 | Open: after a mix of word writes the latch held 00h or the earlier fill colour, not the last low byte written. Only reachable with lone high-byte writes, which software on this bus does not make. |

Consequence: a word access, split into low+high by the 8-bit bus, has the effect of one 16-bit access. The card
differs from a 16-bit model only on lone byte accesses to word registers.

Bed write log `vm_6695/run_card_eeprom_w02b0clear.log` (322,321 writes, ROM + TEST.COM): byte-width writes go only
to 4AEE (80), 22E8 (49), CRT 02E8-1EE8, 36EE (3), 7AEE (1), 56EE/56EF (1 each) - none to a drawing register. So the
bus width does not explain TS1. Still to measure: which of those byte-written ports commit on the low byte on the card.

## Bed check, fork 442d373df (vm_6695, REGR, 2026-10-08)

- M8BYTE4 byte-identical to the card. M8BYTE 1-2: bed matches the card on every rule above; it differs only where
  the latch's leftover contents matter (M8BYTE 2 and 6, M8BYTE2 0-2: bed keeps an earlier lone low byte).
- M8BYTE 9: same data, but the bed timed out 31 times to the card's 32 - after the last word is popped the bed
  still reports data ready (GP_STAT 0100h) once. Same signature as the TS1 bed bug (data ready left set after
  32 words). Next lead.
- REGR against the previous run: 27 probes byte-identical, M8TSX/M8TS1 included. M8SEQ5 and M8SEQ differ only in
  record pad bytes the probes never write (memory left by the previous program); all checkpoints unchanged.
  M8REGS differs in extended register A9h (bed 97h before, CBh now; card 01h) - already wrong before, varies.
- TEST.COM hung after REGR (owner); log `vm_6695/run_bytebus.log` ends in its clock-select loop at 067C:17D3.

## FIFO_OPT (36EEh) bit 1, HOST_8_ENA - M8BYTE5, 5160

The ATI guide (p. 9-8) gives 36EEh bit 1 as 8-bit (1) or 16-bit (0) host data I/O. M8BYTE5 runs the three read
tests above with 36EEh = 0, 1, 2, 3 written first: all twelve results are the same as M8BYTE 8-10 (low byte peeks,
high byte pops a word, words read correctly). In an 8-bit slot the bit does not change PIX_TRANS reads.
