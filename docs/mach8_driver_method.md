# Mach8 driver: the method for every change

What to work on is `docs/worklist.md`; this is how each item gets done. Learned on 2026-10-20/21,
when a change that passed the bed hung the real card, and the guide already held every rule it broke.

## 1. Read before writing (static, cheap)

For the path being changed, in this order:

1. **The guide** - `references/ati_mach32/guide.txt` (Mach32 Register Reference, REG688000-15). Every
   register the change writes or relies on: its page, its aliases, what else writes it. Add each rule
   found to `docs/mach8_guide_rules.md` (rule, page, what the driver must do). Check `Mach 32` markings:
   the Mach8 lacks some registers (EXT_GE_STATUS reads 0).
2. **ATIM8.DRV** - `docs/atim8_disassembly.md`, listings in `references/atim8_disasm/` (local only). How
   ATI does the same operation on this card: register order, waits, recovery. It is the floor to beat
   and proof that a sequence works on the hardware.
3. **XFree86 3.3.6 Mach8 server** - `references/xfree86_336_mach8/` (Kevin E. Martin, MIT-style). The
   other working driver: `mach8fs.c` spans, `mach8frect.c` fills and patterns, `mach8line.c`,
   `mach8dsln.c` dashes, `mach8plypt.c` points, `mach8gtimg.c` reads, `mach8im*` image writes and
   stipples, `mach8fcach.c` glyph cache, `regmach8.h` `WaitQueue(n)`.

Write down what was taken, with the file and line or address, in the commit or the doc.

## 2. Write the change

- Follow the rules in `docs/mach8_guide_rules.md`; a cached register must be one the guide says nothing
  else writes.
- Comments state the constraint and cite the page (`guide 9-41`, `ATIM8 seg08:03a5`), never the history.
  Re-read every comment you wrote before committing (CLAUDE.md).
- Build: `drivers\mach8_display\src\build.ps1 -Out $env:TEMP\m8plain`.

## 3. Bed: pixels only

The bed (`vm_5160_mcp`, 86Box `auto-irq9`) checks that pixels did not change. Its speeds do not count:
it charges a port write ~1 us (card 7.64 us), memory 14 cycles a read, and finishes engine work
instantly. It also does not model all of the guide's rules: a bed pass is necessary, not sufficient.

- Write the build into `vm_5160_mcp/card.img` (`tools/fatcp.py`, VM stopped), boot, run
  `C:\M8BENCH.EXE <label>`, fetch `C:\M8BENCH.TXT`, compare with the last good run:
  `python tools/m8_bench_compare.py <log> <good> <label>` - pixels must be `same` unless the change
  means them to move (towards ATI's hash).

## 4. Card: the measurement

`tools\m8_card_cycle.ps1 -Drv $env:TEMP\m8plain\M8DISP.DRV -Name M8XXXX -Label card_xxxx` installs the
driver beside the running one, selects it, reboots (unattended: Logged + CONFIG `FULL`), benches twice,
and compares the second run with ATI's card runs. The first run after a boot is unreliable: never use it.

- Pass: pixels equal ATI's in every test (or the known exceptions shrink), no engine resets.
- A wedge: the bench call times out. Say "it's hung, can you restart" to the owner; then
  `-FetchOnly` reads the log (its last `started` line names the test). The agent still answers in a
  wedged session: `C:\M8STAT.EXE` reads the engine status (GP_STAT 0200 + SUBSYS_STAT bit 2 = a
  locked FIFO).
- Unexplained slowness or wrong pixels: a diagnostic build that writes its engine resets to
  SCRATCH_PAD_1; M8BENCH prints it per test as `pad D<routine><wait><count>` (see
  `docs/captures/2026-10-21_card/README.md`).

## 5. Keep it

- Card log to `docs/captures/<date>_card/`, a row per run in its README.
- Commit when the card passes (`m8_cycle.ps1` style: short subject, what and why), push, tick the
  worklist item in the same commit.
- A rule learned goes into `docs/mach8_guide_rules.md`; a method lesson into this file.

## Levers known to beat ATI (from step 1)

| Lever | Source | Status |
|---|---|---|
| Wait for free slots (`WaitQueue(n)` on EXT_FIFO_STATUS), not for an empty FIFO, so the CPU sets up while the engine draws | XFree86 `regmach8.h:286` | to do |
| Batch: set up once, then many primitives (5 writes per span) | XFree86 `mach8fs.c:162` | to do |
| Inline port writes (`-oi`): no call per `outpw` | Watcom; ATIM8 writes `out` inline | to do |
| Engine advances PATT_INDEX itself on patterned lines: 2 writes per segment, not 3 | guide 9-33 | to test on the card |
| 8x8 mono pattern (PATT_LENGTH bit 7): any hatch in one blit | guide 9-60 | to do |
