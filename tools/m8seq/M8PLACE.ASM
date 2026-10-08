; M8PLACE.COM - does a write to ADVFUNC_CNTL (4AE8h) leave the Mach8 drawing engine with a different
; pitch or offset from GE_PITCH 80h / GE_OFFSET 0 written explicitly? The ATI guide (p. 8-6, 9-20): only
; the mach8 resets CRT_PITCH, GE_PITCH, CRT_OFFSET and GE_OFFSET when ADVFUNC_CNTL is written. TEST.COM runs
; TS1 in that reset state; M8TSY forcing 80h/0/0 instead reproduced the bed's wrong TS1 table.
;
; Each pass: engine reset, explicit pitch 80h / offset 0, clear rows 0-15 x 0-2047 to 00h, then (pass
; dependent) write 4AE8h, draw four marker pixels, write pitch 80h / offset 0 again and read rows 0-15 x
; 0-2047 back. Markers: (1,1) 11h, (2,0) 22h, (0,2) 33h, (5,3) 44h.
;   pass 0   control: no 4AE8h write before the markers
;   pass 1   4AE8h = 0002h before the markers
;   pass 2   4AE8h = 0007h before the markers (TEST.COM's value)
;   pass 3   MEM_CNTL 5006h (BEE8h) before the markers, no 4AE8h write
; Ends with 4AE8h = 2. Writes M8PLACE.BIN: "M8P1", pass count, then per pass: idle timeouts, data
; timeouts, then up to 32 hits of (x word, y word, colour byte, pad byte), 0FFFFh x-terminated.
; Reads 16 x 2048 pixels per pass, 1024 words per row; only non-zero pixels are kept.

        org     100h

NPASS           equ     4
ROWS            equ     16
REC             equ     2 + 32 * 6 + 2
RES             equ     4000h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8PLACE.BIN', 0
m_ok    db      'M8PLACE: done, results in M8PLACE.BIN', 13, 10, '$'
m_out   db      'M8PLACE: cannot write M8PLACE.BIN', 13, 10, '$'
hdr     db      'M8P1'
hcount  dw      NPASS
pass    dw      0
recp    dw      0
hitp    dw      0
nhit    dw      0
row     dw      0
col     dw      0
to_idle db      0
to_data db      0
mk_x    dw      1, 2, 0, 5
mk_y    dw      1, 0, 2, 3
mk_c    dw      11h, 22h, 33h, 44h

start:  cld
        mov     di, RES                 ; clear the results
        mov     cx, NPASS * REC / 2 + 1
        xor     ax, ax
        rep     stosw
        mov     word [recp], RES
        mov     word [pass], 0
.p:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    setup
        call    explicit
        call    clear
        mov     ax, [pass]
        cmp     ax, 1
        jne     .p2
        OUTW    4AE8h, 2
        call    delay
        jmp     .mk
.p2:    cmp     ax, 2
        jne     .p3
        OUTW    4AE8h, 7
        call    delay
        OUTW    4AE8h, 7
        jmp     .mk
.p3:    cmp     ax, 3
        jne     .mk
        OUTW    0BEE8h, 5006h
.mk:    call    markers
        call    idle
        call    explicit
        call    readback
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        add     word [recp], REC
        inc     word [pass]
        cmp     word [pass], NPASS
        jb      .p

        call    idle
        OUTW    4AE8h, 2
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      outerr
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, NPASS * REC
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     say
outerr: mov     dx, m_out
say:    mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Engine reset and a known drawing state: full scissors, masks open, replace mixes.
setup:  OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        OUTW    0EEEEh, 0
        OUTW    92EEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 37FFh
        OUTW    0BEE8h, 47FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BAEEh, 7
        OUTW    0B6EEh, 7
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        ret

explicit:
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        ret

; Rows 0-15, x 0-2047, filled with 00h: one rectangle.
clear:  call    idle
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 2047
        OUTW    0BEE8h, ROWS - 1
        OUTW    9AE8h, 40B3h
        call    idle
        ret

markers:
        xor     si, si
.m:     call    idle
        mov     dx, 0A6E8h
        mov     ax, [mk_c + si]
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, [mk_x + si]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [mk_y + si]
        out     dx, ax
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        add     si, 2
        cmp     si, 8
        jb      .m
        ret

; Read rows 0-15 x 0-2047 a row at a time; keep up to 32 non-zero pixels.
readback:
        mov     ax, [recp]
        add     ax, 2
        mov     [hitp], ax
        mov     word [nhit], 0
        mov     word [row], 0
.r:     call    idle
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, [row]
        out     dx, ax
        OUTW    96E8h, 2047
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     word [col], 0
.w:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        or      al, al
        jz      .hi
        call    hit
.hi:    inc     word [col]
        mov     al, ah
        or      al, al
        jz      .nx
        call    hit
.nx:    inc     word [col]
        cmp     word [col], 2048
        jb      .w
        OUTW    0BAE8h, 27h
        inc     word [row]
        cmp     word [row], ROWS
        jb      .r
        mov     di, [hitp]
        mov     word [di], 0FFFFh
        ret

hit:    cmp     word [nhit], 32
        jae     .full
        push    ax
        mov     di, [hitp]
        mov     bx, [col]
        mov     [di], bx
        mov     bx, [row]
        mov     [di+2], bx
        mov     [di+4], al
        mov     byte [di+5], 0
        add     word [hitp], 6
        inc     word [nhit]
        pop     ax
.full:  ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.dw:    loop    .dw
        pop     cx
        ret
