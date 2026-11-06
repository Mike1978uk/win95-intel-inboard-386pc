; M8BNDRY.COM - does MEM_BNDRY (42EE) do anything on the Mach8?
;
; The Mach32 guide documents it as the line between memory the VGA may use and memory the engine
; may use. Reading the Mach8 card shows 0000 and its ROM never writes it, but nobody has written it
; here. MEM_PAGE_BNDRY is bits 3:0 in 256 KB units, MEM_BNDRY_ENA bit 4.
;
; Row 100 lies below 256 KB at pitch 1024, row 300 above it. Each arm fills both rows with its own
; two values through the engine and reads them back, and reads A000:0 before and after:
;   arm 0  42EE = 0: control, must read back what it wrote
;   arm 1  42EE = 0011h: boundary at 256 KB, enabled
;   arm 2  42EE = 0: put back, must read back what it wrote
; A row in arm 1 that stops following its value, or a change at A000:0, means the register acts.
; Writes M8BNDRY.BIN: "M8BD", then for each arm: 42EE read back (word), A000:0 before, row 100
; value, row 300 value, A000:0 after. Mode 13h while it runs; DOS boot.
;
;   nasm -f bin -o M8BNDRY.COM M8BNDRY.ASM

        org     100h

MEM_BNDRY       equ     42EEh

        jmp     start

oname   db      'M8BNDRY.BIN', 0
m_ok    db      'M8BNDRY: done', 13, 10, '$'
m_out   db      'M8BNDRY: cannot write M8BNDRY.BIN', 13, 10, '$'
obuf    db      'M8BD'
res     times 18 db 0
OUTLEN  equ     $ - obuf

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .ok
        loop    .i
.ok:    pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .ok
        loop    .d
.ok:    pop     cx
        ret

init:   OUTW    42E8h, 9000h
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
        ret

; BX = row, CL = value: x 0-255 filled
fillrow:
        call    idle
        xor     ax, ax
        mov     al, cl
        mov     dx, 0A6E8h
        out     dx, ax
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, bx
        out     dx, ax
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        jmp     idle

; BX = row -> AL = the pixel at x 8
rdrow:  call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 8
        mov     dx, 82E8h
        mov     ax, bx
        out     dx, ax
        OUTW    96E8h, 1
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        push    ax
        call    idle
        OUTW    0BAE8h, 27h
        pop     ax
        ret

; AX = value for 42EE, CH = the arm's first fill value, DI -> results
arm:    mov     dx, MEM_BNDRY
        out     dx, ax
        in      ax, dx
        mov     [di], ax
        add     di, 2
        mov     ax, 0A000h
        mov     es, ax
        mov     al, [es:0]
        mov     [di], al
        inc     di
        mov     bx, 100
        mov     cl, ch
        call    fillrow
        mov     bx, 300
        mov     cl, ch
        inc     cl
        call    fillrow
        mov     bx, 100
        call    rdrow
        mov     [di], al
        inc     di
        mov     bx, 300
        call    rdrow
        mov     [di], al
        inc     di
        mov     al, [es:0]
        mov     [di], al
        inc     di
        ret

start:
        cld
        mov     ax, 0013h
        int     10h
        mov     ax, 0A000h
        mov     es, ax
        mov     byte [es:0], 5Ah
        call    init

        mov     di, res
        xor     ax, ax
        mov     ch, 21h
        call    arm
        mov     ax, 0011h
        mov     ch, 43h
        call    arm
        xor     ax, ax
        mov     ch, 65h
        call    arm

        OUTW    MEM_BNDRY, 0
        mov     ax, 0003h
        int     10h
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .noout
        mov     bx, ax
        mov     ah, 40h
        mov     cx, OUTLEN
        mov     dx, obuf
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.noout: mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
