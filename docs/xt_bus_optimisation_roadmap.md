# Closing the XT/AT gap: the optimisation stack

Everything here is measured on the real 5160 unless marked otherwise. Measurements and method:
`inboard-hw-debug` techniques 109, 109b, 109c, 109d, 109e, 109f.

**These are not alternatives. They attack different terms of the same equation and they compose.**

```
time = commands x 4.17ms  +  sectors x (data + think)  +  host gap
         ^                          ^         ^               ^
      merge, queue            word xfer   paced poll        queue
                                          cross-device
       ... and a cache hit skips the whole expression
```

## The cost model, measured

| quantity | value | how |
|---|---|---|
| fixed per-I/O-access sync | **~3.90 us** | two-point solve, technique 109e |
| per byte inside an access | **~1.87 us** | same |
| one byte-wide access | 5.77 us | direct, twice, 0.03% apart |
| per command | **4.17 ms** | two-point INT 13h fit, technique 109b |
| think time per sector | ~0.6 ms | same fit, minus measured data phase |

**The key structural fact:** two thirds of an access is the Inboard synchronising to the 4.77 MHz
bus. So **access count is what matters, not byte count** - that is the design rule for every driver
in this project.

## The stack

| # | lever | attacks | status | 8-sector read |
|---|---|---|---|---|
| 0 | baseline | - | - | 124 KB/s |
| 1 | **word transfers** | data phase: halves access count | ✅ **shipped** | **166 KB/s** |
| 2 | **merge adjacent requests** | command count (1.8x today) | needs 4 | ~192 KB/s |
| 3 | **queue depth** | host gap | needs 4 | ~200 KB/s (gap unmeasured) |
| 4 | **async completion** | frees CPU; trunk for 2, 3, 5 | not built | - |
| 5 | **cross-device overlap** | think-time serialisation | needs 4 | copies only |
| 6 | **paced polling** | bus occupancy during think | ✅ shipped | frees bus, not throughput |
| 7 | **VCACHE tuning** | whether the I/O happens at all | not tried | multiplies everything |
| 8 | **Trantor rewrite** | same levers, SCSI path | not started | unmeasured |

**Cumulative on raw disk throughput: roughly 1.5-1.6x**, before cache effects, before cross-device
overlap on copies. Levels 2 and 3 are projections from measured components, not end-to-end results.

## Dependencies - the only reason there is an order

Independent, can be done any time:

- word transfers (done), paced polling (done), VCACHE tuning, Trantor rewrite

One trunk gates three wins:

```
async completion
  |- queue depth
  |    `- merge adjacent requests      <- the 4.17 ms, biggest single number
  `- cross-device overlap
```

That trunk is also the highest-risk change in the driver - the completion model is what wedged
Windows shutdowns for four sessions on the retired `.PDR` (technique 88). It belongs in the 86Box
bed with the control/fast binary pair for A/B, not on the card.

## Ruled out, each by a cheap capture rather than an implementation

| | why | cost to find out |
|---|---|---|
| READ MULTIPLE | IDENTIFY word 47 = 1, one sector per block | one DEBUG run |
| SCSI as a faster boot path | identical 6880 ticks per access, plus protocol phases | one DEBUG run |
| 32-bit data access | base+2/base+3 are the Error register | desk analysis |
| wait-state tuning | already 0 wait states, cache on | one COMrade read |
| widening the taskfile | 11.38 us vs 11.54 us for two registers | desk analysis |
| compression | real-mode INT 13h hooker - see the parked-idea note | desk analysis |

## Next

1. **A/B the shipped word transfers on hardware** - control vs current binary, one boot each. Turns
   the 34% projection into a published number.
2. **Async completion in the emulator**, then queue, then merge.
3. **VCACHE** - free and reversible, and the only lever that beats the bus entirely.
