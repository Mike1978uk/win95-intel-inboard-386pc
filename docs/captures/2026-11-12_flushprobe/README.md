# Which accesses invalidate the L1, 2026-11-12

Probe: `tools/flushprobe/FLUSHP.ASM` (FLUSHP 1), real 5160, DOS prompt after `CPUSET.BAT`
(`1000h:0=92`, `1000h:1=8C`), COMrade loaded. Raw output (`C:\F.TXT`):

```
A 0C2E 2A7A 0C3C 2A78
B 0C10 24FC 0C14 24BE
C 0D12 2384 0D12 2396
D 0E84 2356 0E84 2352
E 0C10 23D8 0C0E 23C8
F 0C16 0D4A 0C12 0D34
G 0C12 0C16 0C12 0C18
H 0C10 0C3C 0C14 0C3E
I 0C12 2502 0C12 2500
J 0C10 3082 0C14 3080
K 0C12 0E6E 0C12 0E26
```

Columns: plain, with-op, plain, with-op, PIT counts (0.838 us). Per-op cost =
(op - plain) / (outer - 1); the PIT reads are port accesses, so each plain pass starts cold once.

| Test | Op | Working set | Cost per op | Reading |
|---|---|---|---|---|
| A | IN 21h | 256 B | 25 us | invalidates |
| B | IN 21h | 1 KB | 84 us | invalidates (09-21: 90.7 us) |
| C | IN 21h | 4 KB | 321 us | invalidates |
| D | IN 21h | 8 KB | 638 us | invalidates; 78 ns per byte of working set |
| E | OUT 21h | 1 KB | 81 us | invalidates |
| F | read B800:1000 | 1 KB | 4.1 us | **no invalidation**, bus time only |
| G | write B800:1000 | 1 KB | ~0 | **no invalidation**; the write is posted |
| H | read F000:FFF0 | 1 KB | 0.6 us | local, no invalidation |
| I | IN 22h | 1 KB | 85 us | invalidates |
| J | 8 x IN 21h | 1 KB | 124 us | **once per burst**: each extra IN adds 5.6 us (bus time) |
| K | register delay | 1 KB | 7.5 us | the delay itself; no invalidation |

## What it settles

- The invalidation is real and whole-cache: cost grows linearly with the working set to 8 KB,
  at 78 ns a byte (09-21: 83).
- **Only I/O instructions trigger it.** ISA memory reads and writes do not, although they also
  cross the Inboard to the XT bus. So it is not the Inboard holding the CPU off the bus.
- IN 22h (claimed inside the BL3) invalidates too: the trigger is the I/O cycle at the CPU or
  module, not an XT bus cycle. Unconfirmed that a 22h read stays off the bus.
- **The cost is per burst, not per access.** Back-to-back I/O pays one refill, then bus time.

## What it changes

- Driver: build commands in RAM, send in tight bursts, keep the code and data touched between
  bursts small. Each burst costs one refill of whatever is touched next.
- Memory writes to the card (VGA half, A000/B800) avoid the invalidation and post: the path for
  video, Mode 13h/Mode X and a FastDoom back end. Sustained write rate still needs M8VGAW (one
  byte per KB here, so posting hid the bus time).
- The Mach8 engine is I/O-only, so its sends keep paying one refill a burst.
