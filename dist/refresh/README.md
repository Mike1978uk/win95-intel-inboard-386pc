# REFR64.COM - DRAM refresh at divisor 64

Sets PIT channel 1 (DRAM refresh) to mode 2, divisor 64 (53.6 us between refreshes; IBM's is 18,
15.08 us). The Inboard refreshes every bank on the XT's DMA refresh cycle, so each refresh holds the
bus; fewer of them leave it free for everything else. `REFR18.COM` puts IBM's value back.

Run it from `AUTOEXEC.BAT` before Windows (see `tools/bootmenu/AUTOEXEC.BAT`). Windows does not
reprogram channel 1 (checked in the emulator, not yet on the 5160). Source: `tools/refresh/REFR64.ASM`, `REFR18.ASM` (13 bytes each).

Measured on the real 5160: 3-6% faster bus transfers; dword VGA writes 1.98 -> 1.86 us/byte; a cold
soak clean. Not yet confirmed: a warm soak from a clean boot without COMrade loaded, and floppy read
and format at divisor 64. Your DRAM may need refresh more often than ours - if anything misbehaves,
use REFR18. Details: `docs/mach8_card_coverage.md`, "DRAM refresh"; issue #33.
