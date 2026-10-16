; M8RAST.COM - the 28800's vertical line counter (extended registers A8h low, A9h high; vgadoc ATI.TXT) as a
; raster clock for the driver: palette changes per band and tear-free flips need to know where the beam is.
; Read-only: indexes 1CEh, reads 1CFh, and reads 3DAh. Text mode as found.
;
; Interrupts off; PIT channel 0 latched before and after each loop (counts as M8TIME: raw PIT difference).
;   loop 0  512 samples of the pair: A8h, then A9h -> stored as words (A9h << 8 | A8h)
;   loop 1  512 reads of A9h alone
;   loop 2  512 reads of 3DAh (input status 1) - the standard VGA way to find retrace
; Writes M8RAST.BIN: "M8R2", three PIT differences (words), 512 sample words, then the 512 3DAh bytes.

        org     100h

        jmp     start

oname   db      'M8RAST.BIN', 0
m_ok    db      'M8RAST: done, results in M8RAST.BIN', 13, 10, '$'
m_out   db      'M8RAST: cannot write M8RAST.BIN', 13, 10, '$'
hdr     db      'M8R2'
res     times 3 dw 0
samp    times 512 dw 0
st3da   times 512 db 0
t0      dw      0

%macro  TSTART 0
        cli
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        mov     [t0], ax
%endmacro

%macro  TEND 1
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        sti
        mov     bx, [t0]
        sub     bx, ax                  ; PIT counts down
        mov     [res + %1 * 2], bx
%endmacro

start:
        cld
        ; loop 0: the pair
        mov     di, samp
        mov     cx, 512
        mov     dx, 1CEh
        TSTART
.p:     mov     al, 0A8h
        out     dx, al
        inc     dx
        in      al, dx
        mov     bl, al
        dec     dx
        mov     al, 0A9h
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        mov     ah, al
        mov     al, bl
        stosw
        loop    .p
        TEND    0

        ; loop 1: A9h alone
        mov     cx, 512
        mov     dx, 1CEh
        TSTART
.a:     mov     al, 0A9h
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        loop    .a
        TEND    1

        ; loop 2: 3DAh
        mov     di, st3da
        mov     cx, 512
        mov     dx, 3DAh
        TSTART
.s:     in      al, dx
        stosb
        loop    .s
        TEND    2

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 6 + 1024 + 512
        mov     dx, hdr
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
