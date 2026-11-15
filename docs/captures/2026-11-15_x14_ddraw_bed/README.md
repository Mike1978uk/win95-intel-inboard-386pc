# DirectDraw samples on the Mach8 bed, with pictures, 2026-11-15

Bed `vm_5160_mcp`, boot 236, 800x600x16 desktop, driver `33b1104` (8 bpp full screen from a 16 bpp
desktop, 8 bpp shadow freed on return). DX 6.1 SDK binaries (`references/dx61samples/ddraw/bin`),
`.bmp`/`.wav` from `src\<sample>` beside each. `tools\m8cardscripts\sample.ps1`, 25 s, picture,
census, Alt+F4. Pictures from 86Box's automation socket (`tools\bed_shot.ps1`; bed started with
`tools\bed_launch.ps1 -Automation 9890`): the agent's own screenshot fails at 16 bpp.

| sample | picture | reading |
|---|---|---|
| wormhole | tunnel, 640x480x8 full screen | **works** (11-14: page fault, no 8 bpp mode) |
| switcher | 640x480 full screen, text and ball | **works** |
| font | windowed, text drawn | works |
| stretch | windowed, donut drawn | works; white is the window past the image |
| memtime | windowed, waiting for "Time All" | not exercised (needs a menu click) |
| mosquito, ddovrly | "needs overlays" box | fail cleanly, as required |
| flip2d | 800x600, black, hourglass | **nothing drawn after 25 s**: open |
| donuts | 640x480 window, black, hourglass | **nothing drawn after 25 s**: open (loads 12 .wav: DirectSound or load time) |
| fswindow | 640x480, white, dialog frame with an empty grey body | **dialog controls not drawn**; did not close on Alt+F4 (as 11-14): open |

Agent answered after every sample; desktop clean at the end (`desktop_after.png`), fswindow's
taskbar entry left.

Next: flip2d first (no sound, no input), then fswindow (GDI over an exclusive 8 bpp mode), then
donuts with sound off.
