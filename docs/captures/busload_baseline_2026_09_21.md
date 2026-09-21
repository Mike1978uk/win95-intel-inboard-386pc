# BUSLOAD baseline — instrument validation, 2026-09-21

Stage 1 of the A4/A6 harness (`tools/gen_busload_com.py`). **No device involved** — this
measures only whether the instrument itself can be trusted, before anything is asked of it
(technique 123: build the readback channel first).

## Method

`BUSLOAD.COM`, 172 bytes, md5 `88c07106d3a4fbbefd6f6fb94e223184`, generated with
`--passes 4 --iters 0x4000` and self-verified against capstone before deployment.

Four passes of a **register-only** loop (`nop` / `loop`), each bracketed by a PIT channel-0
latch. The loop touches no memory operand, so on a machine with the cache enabled it runs out
of L1 and occupies **no bus cycle at all** — which is exactly what makes it useful as a probe
for bus contention later.

Deployed and run over COMrade at a DOS prompt; results read back from the BIOS
Intra-Applications Communications Area at `0040:00F0`.

## Result

| pass | ticks |
|---|---|
| 1 | 3930 |
| 2 | 3934 |
| 3 | 3930 |
| 4 | 3930 |

**Spread: 4 ticks in 3930 = 0.10%.** At 0.8381 us/tick that is 3293.7 us per pass, or
**0.201 us per loop iteration**.

## Why this is worth recording

- ⭐ **The instrument is trustworthy to 0.1%.** Any effect smaller than that is not measurable
  with this harness, and any effect larger than it is real.
- ⭐ **It independently reproduces technique 109.** That measured a register-only loop pass at
  **0.22 us** on 2026-09-10, by a different method. 9% apart with different loop bodies — two
  instruments agreeing.
- ⭐ **It restates the asymmetry the whole plan rests on**: ~0.2 us for a cached instruction
  against **5.55 us** for one 8-bit I/O access. A 27x gap, and the reason "spend CPU to avoid
  transactions" is the right strategy on this machine specifically.

## Gotchas paid for here

- ⛔ **`file_write` with `src_path` timed out** (`op=0x5 timed out after 8.0s`) on a **172-byte**
  file and left nothing on disk — `file_stat` said `exists: false`. Sending the same bytes
  **inline as base64** worked first time. For anything small, prefer inline.
- ⚠ **The first `run_command` timed out and the command never ran** — `screen_read` showed the
  prompt untouched, and the IACA was all zeros. A retry ran it in 1.16 s. ⭐ **Check the screen
  AND the result area before concluding anything about a COMrade timeout**: "timed out" covers
  both "it ran and we lost the reply" and "it never started", and those need opposite responses.

## Next

Stage 2 adds a Sound Blaster Pro **playback** DMA transfer running concurrently. Playback
*reads* memory, so it cannot corrupt anything — the only side effect is noise. If these same
passes slow down while DMA is in flight, the delta is bus cycles stolen per DMA byte, which
answers **A4** and **A6** together.
