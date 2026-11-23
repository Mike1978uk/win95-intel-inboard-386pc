# T5 step 1: where emulated DirectSound's cost goes, card, 2026-11-23

Card boot 517, `M8CW.DRV`, M8DDB with `modexdf`/`modexdp` (this commit). Runs d1 f1 p1 d2 f2 p2, each
through the agent's `exec`, all exit 0. Silent and PlaySound columns are T4's (`../2026-11-23_t4_sound/`).

- `modexd`: two 11 kHz mono secondaries; DSOUND's own primary is **22 kHz 8-bit stereo** (logged).
- `modexdf`: primary set to 22 kHz 8-bit mono, secondaries 22 kHz mono: PlaySound's bytes, no conversion.
- `modexdp`, first pair (p): meant as write-primary. Emulated DSOUND refuses `DSSCL_WRITEPRIMARY`
  (`80004001`, E_NOTIMPL), so it ran at the default level: the 22 kHz stereo primary playing, no secondaries.
- `modexdp`, second pair (q, rebuilt): priority level, primary set to 22 kHz 8-bit mono and played, no
  secondaries: nothing to mix, PlaySound's bytes.
- `modexdi` (i): DSOUND open at the default 22 kHz stereo, both secondaries created, **nothing played**.
- `modex` x3: silent again, same session as q and i; within 1-4 % of T4's silent runs.

Fraction of silent, best of two (`mx_fill` is noisy in every set and left out). q and i against x3:

| Test | PlaySound | d: default | f: mono, 2 mixed | q: mono, none mixed | p: stereo, none mixed | i: idle, stereo |
|---|---|---|---|---|---|---|
| `mx_frame` | 0.94 | 0.81 | 0.84 | 0.91 | 0.88 | 0.86 |
| `mx_box` | 0.94 | 0.80 | 0.87 | 0.91 | 0.88 | 0.84 |
| `mx_same` | 0.94 | 0.77 | 0.85 | 0.94 | 0.88 | 0.88 |
| `mx_lock` | 0.94 | 0.78 | 0.86 | 0.89 | 0.88 | 0.85 |
| `mx_flip` | 0.94 | 0.77 | 0.84 | 0.88 | 0.88 | 0.85 |
| `mx_flipblt` | 0.94 | 0.79 | 0.86 | 0.90 | 0.89 | 0.85 |

Split of the ~21 % that emulated DirectSound costs a game (d):

- **~10 %: the emulation running at all** (q). PlaySound moves the same bytes for ~6 %.
- **~15 % with nothing played** (i): an idle DirectSound object costs more than a playing mono
  primary. The emulation keeps the stereo stream and its mixer thread running in silence.
- **~2 %: stereo over mono** (p against q), nothing mixed.
- **~4 %: mixing two sounds** (f against q), same bytes.
- **~7 %: DSOUND's default format** (d against f): stereo plus 11 to 22 kHz conversion.

A driver keeps the mixing (DSOUND mixes in software for a card with no on-board memory, DDK guide
`references/dxddk/ds_ddk_decoded/`), and can remove the emulation's fixed cost and stop the DMA when
nothing plays.
