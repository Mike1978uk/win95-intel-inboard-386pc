# FLUSHP 1 with the module's SW1 both ways, 2026-11-13

Real 5160, DOS prompt after `CPUSET.BAT`, `tools/flushprobe/FLUSHP.COM` (`FLUSHP 1`), run by the
owner. `F_ON.TXT`: SW1 as fitted (acts ON). `F_OFF.TXT`: SW1 flipped, power cycled between.
Columns and test letters as in `../2026-11-12_flushprobe/README.md`.

Result: the two tables match to within 2 PIT counts (IN 1 KB 84 us, 8 KB 638 us, OUT 81 us,
8-IN burst 124 us, ISA memory read 4 us, write posted). SW1 does not change the flush on I/O.
With the Inboard driving no cache pin and INBRDPC.SYS not touching the cache
(`docs/fable_pack_f1_*.md`), the flush is fixed in the I-O Data PK-A486BL module.
