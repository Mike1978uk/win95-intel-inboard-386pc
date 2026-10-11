; M8SHOW.COM - does the 8514 side ever reach the monitor? (worklist C1, after M8FLIP/M8FLIP2 stayed black)
;
; Fills 1024x768 of the 8514 buffer with 16 vertical bars, colours 0-15, then shows it through
; ADVFUNC alone, as TEST.COM does: 5 s on shadow set 2 (ADVFUNC 7, 1024x768), 5 s on shadow set 1
; (ADVFUNC 3, 640x480), then the VGA again (ADVFUNC 2). No CLOCK_SEL write, no CRTC write.
; THE OWNER WATCHES: report what each 5 s shows - bars, black, or a monitor message. No file.

        org     100h

        jmp     start

m_go    db      'M8SHOW: 5 s of 1024x768 bars, then 5 s of 640x480 bars', 13, 10, '$'
m_ok    db      'M8SHOW: done', 13, 10, '$'
bar     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        mov     dx, m_go
        mov     ah, 9
        int     21h
        mov     cx, 36
        call    ticks

        OUTW    42E8h, 9000h            ; engine reset, as the other probes
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
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0

        mov     word [bar], 0
.b:     call    idle
        mov     dx, 0A6E8h              ; colour = bar number
        mov     ax, [bar]
        out     dx, ax
        OUTW    0BAE8h, 27h
        mov     ax, [bar]
        mov     cl, 6
        shl     ax, cl                  ; x = bar * 64
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 0
        OUTW    96E8h, 63
        OUTW    0BEE8h, 767
        OUTW    9AE8h, 40F3h
        inc     word [bar]
        cmp     word [bar], 16
        jb      .b
        call    idle

        OUTW    4AE8h, 7                ; 8514 on, shadow set 2
        mov     cx, 91
        call    ticks
        OUTW    4AE8h, 3                ; 8514 on, shadow set 1
        mov     cx, 91
        call    ticks
        OUTW    4AE8h, 2                ; VGA

        mov     dx, m_ok
        mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; wait CX BIOS ticks (55 ms each)
ticks:  push    es
        xor     ax, ax
        mov     es, ax
.t0:    mov     ax, [es:046Ch]
.t1:    cmp     ax, [es:046Ch]
        je      .t1
        loop    .t0
        pop     es
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
