# X22 G11 smoke boots (build `86box_x22pr` at `c647d0ddd`)

| Run | Machine, card | Result |
|---|---|---|
| `x22_ibmvga_8514a` | asus386, IBM VGA + IBM 8514/A | Windows 3.11 at 1024x768 (s45, s70) |
| `x22_stock486_et4000ax` | ami495, ET4000AX | DOS prompt |
| `x22_stock486_gd5428` | ami495, Cirrus GD5428 ISA | DOS prompt (added 11-18 night) |
| `x22_et4000ax` | asus386, ET4000AX | **not a valid run**: that bed's guest starts Windows with `8514.DRV`, so with no 8514 the screen stays black (80x480). Not a regression and not a pass; replaced by the two ami495 runs |

G2 (`g2/`): the base build (`c68611861`) and X22 logs have the same body. The base build lacks automation, so
it could not take screenshots.
