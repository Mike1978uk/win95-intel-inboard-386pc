; M8DOS8.COM - 640x480 at 8 bpp on the 8514 side from a plain DOS prompt (P3.2 groundwork).
;
; The DOS driver needs a mode it can set itself. Shadow set 1 shows 640x480 (M8SHOW), but only
; colours 0-15 have been seen there. This loads a 256-entry palette through the 8514 DAC ports
; and fills a 16 x 16 grid of 40 x 30 blocks, colour = row * 16 + column, then shows it for 10 s.
; Palette: red = column (0-15), green = row (0-15), blue 0 on even rows and full on odd rows,
; so every block differs from its neighbours. 8 bpp holds = 256 distinct blocks; 4 bpp would
; repeat one row of 16. The owner watches; INT 10h mode 3 restores the text screen.
;
;   C:\msys64\mingw64\bin\nasm.exe -f bin -o M8DOS8.COM M8DOS8.ASM

        org     100h

        jmp     start

m_go    db      'M8DOS8: 10 s of a 16 x 16 colour grid at 640x480', 13, 10, '$'
m_ok    db      'M8DOS8: done', 13, 10, '$'
blk     dw      0

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

        mov     dx, 02EAh               ; DAC pixel mask: all 8 bits
        mov     al, 0FFh
        out     dx, al
        mov     dx, 02ECh               ; write index 0, then 256 x (R, G, B), 6-bit values
        xor     al, al
        out     dx, al
        mov     dx, 02EDh
        xor     bx, bx
.p:     mov     al, bl
        and     al, 0Fh
        shl     al, 2                   ; red: column x 4
        out     dx, al
        mov     al, bl
        shr     al, 2
        and     al, 3Ch                 ; green: row x 4
        out     dx, al
        mov     al, bl
        and     al, 10h
        jz      .pb
        mov     al, 3Fh                 ; blue: full on odd rows
.pb:    out     dx, al
        inc     bx
        cmp     bx, 256
        jb      .p

        mov     word [blk], 0
.b:     call    idle
        mov     dx, 0A6E8h              ; colour = block number
        mov     ax, [blk]
        out     dx, ax
        OUTW    0BAE8h, 27h
        mov     ax, [blk]
        and     ax, 0Fh
        mov     cx, 40
        mul     cx                      ; x = column * 40
        mov     dx, 86E8h
        out     dx, ax
        mov     ax, [blk]
        mov     cl, 4
        shr     ax, cl
        mov     cx, 30
        mul     cx                      ; y = row * 30
        mov     dx, 82E8h
        out     dx, ax
        OUTW    96E8h, 39
        OUTW    0BEE8h, 29
        OUTW    9AE8h, 40F3h
        inc     word [blk]
        cmp     word [blk], 256
        jb      .b
        call    idle

        OUTW    4AE8h, 3                ; 8514 on, shadow set 1 (640x480)
        mov     cx, 182
        call    ticks
        OUTW    4AE8h, 2                ; VGA
        mov     ax, 0003h
        int     10h

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

; wait for the engine, at most 65536 status reads
idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     cx
        ret
