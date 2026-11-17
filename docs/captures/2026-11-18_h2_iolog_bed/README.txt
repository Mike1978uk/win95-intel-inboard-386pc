Issue #10 H2 bed I/O log: Win95 boot to desktop on vm_5160_now copy, h2 exe, ISSUE10_IOLOG=1.
Log kept the first 64 per port+direction, then every 4096th. Desktop reached; stopped by kill, not guest shutdown.
Port    dir  count     h  first CS:EIP      pm  caller
0102    out  1         0  C000:00000410     0   VGA BIOS (POS/setup register write; no handler)
01CE    out  >=4096    1  C000:0000253D     0   ATI VGA BIOS, index writes (B9/BE/B3), byte and word
01CF    in   64..4095  1  C000:00002541     0   ATI VGA BIOS (2541, 2556, 256F, 257D, 05C0)
All 130 logged lines are pm=0, C000 (VGA BIOS). No F000, D000, DOS-driver or pm=1 line appears.
INCONCLUSIVE for Windows: 01CE/01CF fill their 64 logged slots in the BIOS init, so a later pm=1 access to those
ports is counted but not printed (only every 4096th is). 01CF in stayed under 4096; 01CE out hit 4096 (C000:3A3E).
Ports other than 0102/01CE/01CF were never touched at all (each would have printed its first 64): no other 100-1FF use.
Also in log: '[0117:0000B929] Illegal instruction 00008B55' (protected-mode selector), unrelated to I/O.
Next if needed: rerun with a per-pm counter (log first 64 pm=1 separately) to settle VxD reads of 1CFh.
Raw lines: io10_run1.txt (130).
--- Run 2: counters per port+direction+pm; desktop reached, no Display tab (v9x agent unreachable on port 9869); killed ~3 min after desktop. io10_run2.txt (258 lines)
pm=1 only (128 lines; 0102 and all other ports: none in pm=1):
Port  dir   count(max n)  first CS:EIP     selector reading
01CE  out   64 b / 23 w   C000:0000256D    CS=C000 under pm=1 = V86 mode: VGA BIOS ROM run by Windows (mode set / DOS box)
01CF  in    64 (cap hit)  C000:0000256F    same; callers 2541/2556/256F/257D, the same BIOS probe loop as pm=0
No pm=1 access came from a protected-mode selector (no 32-bit VxD EIP). 01CF pm=1 reads hit the 64 cap, so later reads go unprinted.
Does Windows (pm=1) read 1CFh: YES, but only as the VGA BIOS in V86 mode; a native VxD/driver read is not shown (cap hit, none seen).
