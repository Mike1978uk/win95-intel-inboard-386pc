# Design: interrupt-driven transfer engine (the freeze killer)

Status: **design ready — with one hard rule learned the expensive way.**
The 2.0D experiment (hook the IRQ at boot alongside the polled engine)
**triple-faulted the machine on the first boot with a mounted bus**:
the SL811's address latch is write-only, so an IRQ handler's own chip
access corrupts whatever port sequence it preempts — and a hardware
interrupt preempts even the holder of the `xbusy` guard.  Ironclad
rule: **the chip has exactly one master at a time.**  The polled engine
runs with the card IRQ parked at the PCIC (2.0F does this from boot);
the async engine may enable interrupts only while ALL chip access goes
through its handler, and must park them again when handing back.
IRQ delivery itself is still unverified (the earlier "kill = delivery"
inference was confounded by a laptop suspend).  Stage 1 below verifies
it safely.

## Why

Every transfer today is synchronous ring-0 PIO: the request routine
spin-polls the SL811 for each packet and returns only when the whole
IOR is done. Consequences, all observed on hardware:

* the UI freezes for the duration of any copy (mitigated to ~15 ms
  bursts by the 4 KB `max_xfer_len` cap, but a mount-time FAT scan is
  ~1,850 back-to-back IORs ≈ 30+ s of a frozen machine);
* the COMrade serial bridge starves and wedges during disk I/O;
* CPU is 100 % busy moving ~500 KB/s.

## The unlock

Freeing IRQ7 (LPT1 disabled) made the card's interrupts actually
deliver — proven by Windows killing the socket over *unserviced* SOF
interrupts the moment USB traffic started (2026-07-13, twice). VxD
2.0D services the IRQ from boot; its `irqcount` doubles as the
delivery meter. Interrupt-per-packet operation is therefore possible.

## Architecture

Split every block IOR into an **IRQ-advanced state machine**; the
request routine only *starts* it.

```
IOR arrives (task time)
  validate; stash {LBA, count, buffer/SGD cursor, dir, phase=CBW}
  xbusy=1 (owns EP A for the whole IOR)
  arm first packet on EP A; INTENA |= 01h (USB-A done)
  return WITHOUT completing        <- machine is free to run

CFU1_HwInt (per packet, ~1 ms cadence)
  INTSTAT bit0?  ->  advance:
    CBW phase:  next CBW packet, else -> DATA phase
    DATA phase: copy 64B chip buffer <-> user/SGD buffer, next packet
                (honour SGD walk: {sectors, linear ptr} pairs)
    CSW phase:  read CSW, set IOR_status, IOP callback unwind
                (IOS explicitly permits completion at interrupt time -
                it is the normal port-driver model), xbusy=0,
                start next queued IOR if any
  NAK/error:    re-arm same packet, bounded; on budget exhaustion use a
                Set_Global_Time_Out retry to avoid IRQ-spinning;
                hard-fail -> IOR_status = 15h, complete
  INTSTAT bit4 (SOF): ack only (0xFE mask already preserves bit0 for
                any residual polled paths)
```

Keep the polled engine for control transfers/enumeration (rare, short,
task-time) and as a whole-engine fallback when `irqhandle == 0`
(machines where delivery genuinely fails).

## Sizing

* One 4 KB IOR = 64 data packets ≈ 64 interrupts ≈ 64 × ~30 µs handler
  work spread over ~70 ms of bus time — the machine stays alive.
* Queue depth 1 is enough (IOS serialises per-DCB unless told
  otherwise; we declared no demand bits for concurrency).

## Risks / notes

* Buffer access at IRQ time: IOR buffers are locked for the duration
  of the request per the IOS contract, and our SGD pointers are linear
  — safe to copy at interrupt time.
* The `xbusy` guard must now be held across the *whole* IOR (set at
  kick, cleared at completion) — supervisor/HID ticks already yield on
  it. Add the watchdog (planned with the detach fix) so a stuck IOR
  cannot wedge the machine's input forever.
* VKD keyboard injection stays task-time (Schedule_Global_Event), as
  today; VMD mouse posting is IRQ-safe, as today.
* HID interrupt-IN polling can migrate to the same per-packet IRQ
  model later (poll becomes an armed IN that completes on interrupt);
  not required for storage.

## Build order

1. Verify delivery SAFELY: hook + steer + enable only while the bus is
   otherwise idle (no volume mounted or I/O quiesced), count for 1s,
   park again — an ioctl-triggered test, not a boot-time state.
2. Stage A: `do_xact` waits on an IRQ-set memory flag instead of
   port-polling (same synchronous shape, proves handler/engine
   handshake, trivially revertible).
3. Stage B: the full async IOR state machine above (storage only).
4. Stage C: measure; then consider HID poll migration and lifting
   `max_xfer_len` back to 64 KB (bigger IORs amortise better once
   nothing blocks).
