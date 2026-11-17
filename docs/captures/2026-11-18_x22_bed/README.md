Bed vm_5160_dbg, 86box_auto build 2026-11-17 22:47 (CRT_OFFSET fix), 640x480x8, automation 9891.
1. Desktop (desk0-4): icons/taskbar normal, no shift; Explorer window open/close clean. PASS.
2. flip2d (flip2d/s1-4, grid.png): cube at a different angle each shot, frame 22/32/42, flips 75, no black or half-old frames. Before fix: all black (../2026-11-17_flip2d_bed). PASS.
   Note: s1 shows the window title bar, s2-4 show the FPS text at the top with short white ticks at the left edge (likely app drawing, not checked).
3. donuts (donuts/, 97 flips): first two shots pink desktop transition (same as before the fix, ../2026-11-17_p113_bed), then title screen. No shift. PASS with the unchanged pink frames.
4. wormhole (wormhole/, 102 flips): four distinct frames, no tearing. PASS.
5. Desktop after each exit (desk4_after_flip2d, desk_after_donuts, desk_after_wormhole): correct, not shifted. PASS.
6. Shutdown: agent shutdown reached the power-off screen (poweroff.png); stopped PID 21300. A different 86Box (PID 10556, 86box_h2 build) is running, not mine, left alone.
Not tested: 16 bpp, other beds, frame rate on the card.
