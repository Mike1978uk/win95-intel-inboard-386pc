; M8XMEM.COM - does either half of the Graphics Ultra reach the other's memory?
;
; Each half reaches its own memory: the CPU sees the 28800's through the A000 window, and the
; Mach8's only through the engine (pixel transfer, E2E8h). This checks for any crossing:
;   1 VGA bytes A000:0-15 = 40h-4Fh; the engine fills its row 0 (x 0-255) with A5h;
;     record A000:0-15 - did the engine's write land in VGA memory?
;   2 the engine reads its row 0, x 0-15 - control: A5h expected
;   3 the CPU writes 77h to A000:0-15; the engine reads its row 0, x 0-15 again -
;     did the VGA write land in the engine's memory?
; Writes M8XMEM.BIN: "M8XM", then 16 + 16 + 16 bytes. Mode 13h while it runs; DOS boot.

        org     100h

RES             equ     0800h

        jmp     start

oname   db      'M8XMEM.BIN', 0
m_ok    db      'M8XMEM: done, results in M8XMEM.BIN', 13, 10, '$'
m_out   db      'M8XMEM: cannot write M8XMEM.BIN', 13, 10, '$'
hdr     db      'M8XM'

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     ax, 0013h
        int     10h
        push    es
        mov     ax, 0A000h
        mov     es, ax
        xor     di, di
        mov     al, 40h
        mov     cx, 16
.v:     stosb
        inc     al
        loop    .v
        pop     es

        call    init
        call    idle
        OUTW    0A6E8h, 0A5h            ; engine: row 0, x 0-255 = A5h
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        call    idle

        mov     di, RES                 ; 1: VGA bytes after the engine write
        push    ds
        mov     ax, 0A000h
        mov     ds, ax
        xor     si, si
        mov     cx, 16
        rep     movsb
        pop     ds

        call    rd16                    ; 2: engine row 0

        push    es                      ; 3: CPU writes VGA, engine reads its row 0
        mov     ax, 0A000h
        mov     es, ax
        push    di
        xor     di, di
        mov     al, 77h
        mov     cx, 16
        rep     stosb
        pop     di
        pop     es
        call    rd16

        push    di
        mov     ax, 0003h
        int     10h
        pop     di

        mov     si, di
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, si
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

; engine row 0, x 0-15 -> [di], 16 bytes
rd16:   call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 15
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     cx, 8
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
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

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
.dok:   pop     cx
        ret
