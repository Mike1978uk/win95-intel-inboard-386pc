# Next session (2026-11-19, evening)

## START HERE

**Card state:** 5160 in the **DOS agent boot** (DOSNET). `tools\dosagent\bootmode.ps1 windows` returns it to
Windows LEAN. Test files are on the card in `C:\M8C3`.

**Owner's direction (11-19):** the whole card is the focus (worklist **L6**, `docs/l6_card_for_3d_2026_11_19.md`);
bed/card gaps are work (X27). Design rules from today: a background-coloured tile is not drawn; drawing an outline
twice flips the fill, so draw the cheaper colour.

## Done today (afternoon, card)

| Item | Result | Commit |
|---|---|---|
| Joined same-colour patches, `m8dos` | writes -43 / -47 %, frame -13 / -15 %, pixels identical | (commit below) |
| P5 parity fill `m8_parity`, `checkprobe` | checkered floor 3,783 -> 234 writes, 75.8 -> 13.2 ms; single-pixel edge differences | (commit below) |
| Bed fill gap evidenced (X27) | `vm_6695_w2` draws no polygon fills; earlier bed page-against-page results void for fills | (commit below) |

Captures: `docs/captures/2026-11-19_join_card/`, `docs/captures/2026-11-19_check_card/`.

## Next, in owner's order

1. **P5b** Penrose and the other tilings with 3-4 colour planes (odd-degree vertices). Probe, do not assume. - DONE 11-19 on the card (3 colours, chained, pixel-identical; see the L6 doc)
2. **P6** card dither mask plane for a pseudo high-colour look (owner's main interest).
3. **`m8_parity` strips** for full width (box limit 382 px).
4. **Overdraw audit** leftovers: `docs/overdraw_retrospective_2026_11_19.md`.
5. **Twist in the DOS native port**, then DirectXT steps 2-3.

Also: **X27** bed fill gap (bed/card gaps are work); **X32** DOS agent refuses a missing file.

## Waiting on others

- **Simon** (#10).
- **Gisle Vanem**, Watt-32 #133: he adopted our `TX_SIZE` suggestion in a diff; our reply
  (https://github.com/gvanem/Watt-32/issues/133#issuecomment-7014070247) answered his old-compiler question and
  pointed out the missed DOS4GW line (`length = TX_BUF() + TX_SIZE;`, `pcpkt.c` ~1735). When he commits: rebuild
  Watt-32 and the DOS agent, run the fast path on the 5160, report.
- **Michael**, PR #2 review.

## Mistakes today

- Bash `"C:\M8C3\$f"` became `C:\M8C3$f`, so uploads went nowhere. Rule: build guest paths from a single-quoted
  prefix; check `put` Destination and Crc32.
- The agent was then told to run a missing BAT and wedged (port refused, Esc dead). Agent weakness: it must refuse a
  missing file (X32).
