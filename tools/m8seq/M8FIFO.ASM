; M8FIFO.COM - how deep is the Mach8's command FIFO, what happens when it is full, and how fast
; does the engine drain it?
;
; The guide (8-14) gives a 16-entry FIFO, GE_STAT bits 7:0 for eight of them and EXT_FIFO_STATUS
; (9AEEh, 9-16) for all sixteen. Over this bus a register word costs ~7.6 us (M8BUS), so the depth
; sets how many words a driver can send between status reads.
;
; Fill arms: start a 1024x512 fill (the engine is then busy for milliseconds), then write FRGD_COLOR
; 24 times, reading the PIT, GE_STAT and EXT_FIFO_STATUS after each write. A full FIFO shows as a
; write that takes milliseconds (the card holds the bus) or as SUBSYS_STAT bit 2 (INVALID_IO, the
; write was dropped). Arm A writes A6E8h; arm B writes E6E8h, the same register with A14 set,
; which the guide says waits instead of overrunning.
;
; Drain arm: time fills of 1x1, 64x64, 256x256, 1024x256 and 1024x512 from the CMD write to
; GE_BUSY clear: per-command overhead and pixel rate.
;
; Writes M8FIFO.BIN: "M8F1"; per fill arm 25 x {PIT, GE_STAT, EXT_FIFO} words then SUBSYS_STAT
; word; then 5 x {w, h, PIT before, PIT after, polls} words. Engine only; DOS boot.

        org     100h

RES             equ     0800h
NSTEP           equ     24

        jmp     start

oname   db      'M8FIFO.BIN', 0
m_ok    db      'M8FIFO: done, results in M8FIFO.BIN', 13, 10, '$'
m_out   db      'M8FIFO: cannot write M8FIFO.BIN', 13, 10, '$'
hdr     db      'M8F1'
sizes   dw      1, 1, 64, 64, 256, 256, 1024, 256, 1024, 512
wport   dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        push    cs
        pop     es
        call    init
        cli                             ; no timer tick inside a measurement
        mov     di, RES
        mov     word [wport], 0A6E8h
        call    fillarm
        mov     word [wport], 0E6E8h
        call    fillarm
        call    drain
        sti

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, di
        sub     cx, RES
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

init:
        OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        OUTW    0BAE8h, 27h
        OUTW    42E8h, 000Fh            ; clear any pending status
        ret

; Start a fill of CX x BX pixels at (0,0) with colour 0. Returns at once.
fill:   OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        mov     dx, 96E8h
        mov     ax, cx
        dec     ax
        out     dx, ax
        mov     dx, 0BEE8h
        mov     ax, bx
        dec     ax
        out     dx, ax                  ; MIN_AXIS_PCNT (index 0)
        OUTW    9AE8h, 40F3h
        ret

; One sample: PIT count, GE_STAT, EXT_FIFO_STATUS -> [di].
sample: mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        stosw
        mov     dx, 9AE8h
        in      ax, dx
        stosw
        mov     dx, 9AEEh
        in      ax, dx
        stosw
        ret

fillarm:
        call    idle
        OUTW    42E8h, 000Fh
        mov     cx, 1024
        mov     bx, 512
        call    fill
        call    sample
        mov     si, 1
.w:     mov     dx, [wport]
        mov     ax, si
        out     dx, ax
        call    sample
        inc     si
        cmp     si, NSTEP
        jbe     .w
        call    idle
        mov     dx, 42E8h
        in      ax, dx
        stosw
        OUTW    42E8h, 000Fh
        ret

drain:  mov     si, sizes
        mov     bp, 5
.s:     call    idle
        mov     cx, [si]
        mov     bx, [si + 2]
        mov     ax, cx
        stosw
        mov     ax, bx
        stosw
        call    pit
        stosw
        call    fill
        xor     cx, cx
        mov     dx, 9AE8h
.p:     inc     cx
        in      ax, dx
        test    ax, 200h
        jnz     .p
        call    pit
        stosw
        mov     ax, cx
        stosw
        add     si, 4
        dec     bp
        jnz     .s
        ret

pit:    mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        ret

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     cx
        ret
