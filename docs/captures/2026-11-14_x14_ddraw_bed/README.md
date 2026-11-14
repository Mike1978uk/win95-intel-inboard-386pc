# DirectDraw samples on the Mach8 bed, 2026-11-14

Bed: Win95 VM, V9x agent 127.0.0.1:9869, Mach8 display driver 800x600x16 (census header: M8CENSUS). Run: tools\m8cardscripts\sample.ps1, 30 s, no -Keys. Data files from src\<sample> passed with -Files.

Screenshots: every run returned 'Guest error 11 (Win32 1460): screenshot helper failed' (shot exit 43); no png exists, screen column is unknown. Census counters are cumulative (flipwhy/calls lines shown are the 'during-' census).

| program | started (put/start exit) | screen | closed with Alt+F4, agent answers | census |
|---|---|---|---|---|
| flip2d | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 2 slow 11 unknown 11 cardunknown 11 boxes 0 colour 0; calls all 0; no error/fail lines |
| donuts | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 3 slow 23 unknown 23 cardunknown 23 boxes 0 colour 0; calls all 0; no error/fail lines |
| memtime | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 4 slow 28 unknown 28 cardunknown 28 boxes 0 colour 0; calls all 0; no error/fail lines |
| stretch | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 4 slow 28 unknown 28 cardunknown 28 boxes 0 colour 0; calls all 0; no error/fail lines |
| wormhole | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 4 slow 28 unknown 28 cardunknown 28 boxes 0 colour 0; calls all 0; no error/fail lines |
| font | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 4 slow 28 unknown 28 cardunknown 28 boxes 0 colour 0; calls all 0; no error/fail lines |
| fswindow | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 4 slow 28 unknown 28 cardunknown 28 boxes 0 colour 0; calls all 0; no error/fail lines |
| switcher | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 6 slow 102 unknown 102 cardunknown 102 boxes 0 colour 0; calls all 0; no error/fail lines |
| mosquito | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 6 slow 102 unknown 102 cardunknown 102 boxes 0 colour 0; calls all 0; no error/fail lines |
| ddovrly | 0/0 | not captured (screenshot failed) | yes | flipwhy gdi 6 slow 102 unknown 102 cardunknown 102 boxes 0 colour 0; calls all 0; no error/fail lines |

Not run: none. Agent answered info after all 10 programs (BootCounter 228 throughout).
