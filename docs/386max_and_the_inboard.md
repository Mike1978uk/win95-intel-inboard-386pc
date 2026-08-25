# 386MAX and the Inboard — what the source turned out to contain

Background to the two-line credit for **Bob Smith** in the README. Preserved here because
the finding is load-bearing evidence, not just an acknowledgement.

## The correspondence

**[Bob Smith](https://github.com/sudleyplace)** (Qualitas) wrote **386MAX**, released as open
source at **[sudleyplace/386MAX](https://github.com/sudleyplace/386MAX)** (see also
[sudleyplace.com](http://www.sudleyplace.com)). He corresponded with this project in
February 2023 about the early history of 386 memory management.

**On attribution, his own position is clear:** *"As far as Inboard is concerned, I had
nothing to do with it"*, and he described his recollection of that era as hazy. That
statement stands and should not be softened.

## What the source contains anyway

The 386MAX source carries genuine, first-class Inboard support:

- `@SYS_INBRDPC` / `@SYS_INBRDAT` machine-type flags
- An `INBOARD` command-line switch
- A20 routines commented *"A20 Enable for Inboard/PC"*
- Inboard-specific INT 09 and I/O port handling

Two things follow that mattered to this project:

1. **The Inboard A20 path writes `0DFh`/`0DDh` to port `60h`** — independently matching both
   Al Williams' 1990 code and this project's own emulation. Three sources, one answer.
2. **Its `MARK_XT` path is primary-source evidence that the XT DMA ceiling is 640 KB**,
   not 1 MB. That is the thread that ends in the `maxPhys` fixes.

## The Intel connection

Bob also pointed us at the Intel OEM build of 386MAX (`@OEMSYS_ILIM`, "INTEL Limulator").
That turns out to be **`ILIM386.SYS` — the memory manager in the Inboard's own Intel
software bundle**. Its strings read `Copyright (C) 1987-9 Qualitas, Inc.` and
`Intel memory boards only.`

So while Bob did not work on the Inboard, the memory manager Intel shipped with it is his.

## Credit for the route

Credit for the DMA direction that led to the 386MAX source belongs to
**@andrew-hoffman**, who cited it on
[issue #5](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/5).
