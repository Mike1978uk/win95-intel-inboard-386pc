; REFR18.COM - set the DRAM refresh rate: PIT channel 1, mode 2, divisor 18.
;
; The Inboard has no refresh of its own; every bank refreshes on the XT's DMA refresh cycle, which
; channel 1 paces. 18 is IBM's (15.08 us); 64 (53.6 us) frees 3-6% of the bus and held in a cold
; soak (docs/mach8_card_coverage.md, "DRAM refresh"). Same OUTs as the measured probe.

        org     100h
        mov     al, 54h                 ; channel 1, LSB only, mode 2
        out     43h, al
        mov     al, 18
        out     41h, al
        mov     ax, 4C00h
        int     21h
