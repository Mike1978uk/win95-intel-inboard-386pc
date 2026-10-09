; M8CONF.COM - Mach8 conformance: one real instance of every engine operation shape ATI's drivers use,
; played on the card or the model so their pixels can be compared. gen_m8conf.py makes M8CONF.DAT from
; MACH8_WLOG=1 bed logs of the drivers in use: per shape, the driver's register state before the
; operation and the operation itself (setup, trigger, host data).
;
; For each test: engine reset, the driver's state, a known pattern over the whole 1024x1024 frame buffer
; (pixel = x XOR 5y, drawn with 8514/A rectangle fills), the state again (the fill changed it, and the
; Graphics Ultra lets the register written last pick the foreground source, so it is replayed in the
; driver's write order), the operation, then two 64x16 areas read back with the 16-bit read 53B0h as
; M8ROW7 does: around the operation's first corner and its last.
; Run from a DOS boot in the directory holding M8CONF.DAT. Writes M8CONF.BIN: "M8CF", then per test:
; idle timeouts, data timeouts, 2 pad, 2 x 1024 bytes. Each test is written as it completes.
; The 8514 display is on while it runs.
;
;   nasm -f bin -o M8CONF.COM M8CONF.ASM

        org     100h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

MAXENT  equ     4200                    ; entries per test (state + operation)
REC     equ     4 + 2048

        jmp     start

dname   db      'M8CONF.DAT', 0
oname   db      'M8CONF.BIN', 0
m_ok    db      'M8CONF: done', 13, 10, '$'
m_dat   db      'M8CONF: cannot read M8CONF.DAT', 13, 10, '$'
m_out   db      'M8CONF: cannot write M8CONF.BIN', 13, 10, '$'
m_big   db      'M8CONF: test too large', 13, 10, '$'
hdr     db      'M8CF'
dhand   dw      0
ohand   dw      0
thdr    dw      0, 0, 0, 0, 0, 0        ; state count, op count, area A x, y, area B x, y
areax   dw      0
areay   dw      0
recp    dw      0
to_idle db      0
to_data db      0

start:
        cld
        mov     ax, 3D00h
        mov     dx, dname
        int     21h
        jc      .nodat
        mov     [dhand], ax
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .noout
        mov     [ohand], ax
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h

        ; --- 8514 display on, as TEST.COM's 069Bh does ---
        mov     dx, 22E8h
        mov     ax, 53h
        out     dx, ax
        call    delay
        OUTW    4AE8h, 7
        call    delay

.test:  mov     ah, 3Fh                 ; test header
        mov     bx, [dhand]
        mov     cx, 2
        mov     dx, thdr
        int     21h
        cmp     ax, 2
        jne     .end
        cmp     word [thdr], 0FFFFh
        je      .end
        mov     ah, 3Fh
        mov     bx, [dhand]
        mov     cx, 10
        mov     dx, thdr + 2
        int     21h
        mov     ax, [thdr]
        add     ax, [thdr + 2]
        cmp     ax, MAXENT
        ja      .big
        mov     cx, ax                  ; read the entries: 6 bytes each
        shl     cx, 1
        add     cx, ax
        shl     cx, 1
        mov     ah, 3Fh
        mov     bx, [dhand]
        mov     dx, ebuf
        int     21h

        mov     byte [to_idle], 0
        mov     byte [to_data], 0
        OUTW    32EEh, 0                ; engine reset
        OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        mov     si, ebuf
        mov     cx, [thdr]
        call    play
        call    pattern
        mov     si, ebuf
        mov     cx, [thdr]
        call    play
        mov     cx, [thdr + 2]          ; the operation follows the state in the buffer
        call    play
        call    idle

        mov     ax, [thdr + 4]
        mov     [areax], ax
        mov     ax, [thdr + 6]
        mov     [areay], ax
        mov     word [recp], res + 4
        call    area_read
        mov     ax, [thdr + 8]
        mov     [areax], ax
        mov     ax, [thdr + 10]
        mov     [areay], ax
        mov     word [recp], res + 4 + 1024
        call    area_read
        mov     al, [to_idle]
        mov     [res], al
        mov     al, [to_data]
        mov     [res + 1], al
        mov     ah, 40h
        mov     bx, [ohand]
        mov     cx, REC
        mov     dx, res
        int     21h
        jmp     .test

.big:   mov     dx, m_big
        jmp     .fin
.end:   mov     dx, m_ok
.fin:   push    dx
        call    idle
        OUTW    4AE8h, 2
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al
        mov     ax, 3
        int     10h
        mov     ah, 3Eh
        mov     bx, [ohand]
        int     21h
        mov     ah, 3Eh
        mov     bx, [dhand]
        int     21h
        pop     dx
        jmp     .say
.nodat: mov     dx, m_dat
        jmp     .say
.noout: mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Play CX entries (port, value, width) from SI.
play:   jcxz    .done
.next:  call    fifo
        lodsw
        mov     dx, ax
        lodsw
        mov     bx, ax
        lodsw
        xchg    ax, bx
        cmp     bx, 1
        je      .byte
        out     dx, ax
        loop    .next
.done:  ret
.byte:  out     dx, al
        loop    .next
        ret

; pixel = x XOR 5y over 1024x1024: a one-pixel column per x, then a one-pixel XOR row per y.
pattern:
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        xor     bx, bx
.col:   call    fifo
        mov     dx, 0A6E8h
        mov     ax, bx
        out     dx, ax
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 0
        OUTW    96E8h, 0
        OUTW    0BEE8h, 3FFh
        OUTW    9AE8h, 40F3h
        inc     bx
        cmp     bx, 1024
        jb      .col
        call    idle
        OUTW    0BAE8h, 25h
        xor     bx, bx
.row:   call    fifo
        mov     ax, bx
        mov     cx, ax
        shl     ax, 2
        add     ax, cx                  ; 5y
        mov     dx, 0A6E8h
        out     dx, ax
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, bx
        out     dx, ax
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        inc     bx
        cmp     bx, 1024
        jb      .row
        call    idle
        ret

; Read [areax],[areay] + 64x16 into [recp]: full scissors, masks open, 53B0h as M8ROW7.
area_read:
        call    idle
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h
        mov     dx, 86E8h
        mov     ax, [areax]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [areay]
        out     dx, ax
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        mov     cx, 512
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret
.rz:    xor     ax, ax
        rep     stosw
        ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
idle:   push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     dx
        pop     cx
        ret

dready: push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     dx
        pop     cx
        ret

fifo:   push    cx
        push    dx
        push    ax
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     ax
        pop     dx
        pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret

res:    times   REC db 0
ebuf:
